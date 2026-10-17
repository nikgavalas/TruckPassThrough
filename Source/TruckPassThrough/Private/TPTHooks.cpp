#include "TPTHooks.h"

#include "Buildables/FGBuildableDockingStation.h"
#include "Components/PrimitiveComponent.h"
#include "HAL/IConsoleManager.h"
#include "Patching/NativeHookManager.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGVehicleAutopilotComponent.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"
#include "WheeledVehicles/FGWheeledVehicleProxyActor.h"

DEFINE_LOG_CATEGORY(LogTruckPassThrough);

static TAutoConsoleVariable<int32> CVarEnabled(
	TEXT("TPT.Enabled"), 1,
	TEXT("1: away from truck stations, trucks ignore vehicles ahead and junction reservations. 0: vanilla autopilot."));

static TAutoConsoleVariable<int32> CVarGhost(
	TEXT("TPT.Ghost"), 1,
	TEXT("1: wheeled vehicles have no physical collision with each other. 0: vanilla collision."));

static TAutoConsoleVariable<float> CVarStationRadius(
	TEXT("TPT.StationRadius"), 3000.0f,
	TEXT("Distance (cm) from a truck station's docking point within which trucks keep vanilla queueing."));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdStatus(
	TEXT("TPT.Status"),
	TEXT("Prints truck station and vehicle counts."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&FTruckPassThroughHooks::DumpStatus));

namespace
{
	constexpr float StationRefreshInterval = 5.0f;
	constexpr float CollisionRefreshInterval = 1.0f;

	// Written on the game thread after the parallel autopilot tick has joined, read by the autopilot hooks during it
	TWeakObjectPtr<UWorld> CachedWorld;
	TArray<FVector> StationLocations;
	float TimeSinceStationRefresh = 0.0f;
	float TimeSinceCollisionRefresh = 0.0f;

	// Original collision responses of every component we ghosted, so TPT.Ghost 0 can put them back
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FCollisionResponseContainer> GhostedComponents;
	TSet<ECollisionChannel> GhostChannels;
	TSet<FName> LoggedVehicleClasses;

	FString ChannelName(ECollisionChannel Channel)
	{
		return StaticEnum<ECollisionChannel>()->GetNameStringByValue(static_cast<int64>(Channel));
	}

	bool IsSharedWorldChannel(ECollisionChannel Channel)
	{
		// Ignoring these would make vehicles fall through terrain, buildings or players
		return Channel == ECC_WorldStatic || Channel == ECC_WorldDynamic || Channel == ECC_Pawn ||
			Channel == ECC_Visibility || Channel == ECC_Camera;
	}

	void CollectVehicleActors(AFGVehicleSubsystem* Subsystem, TArray<AActor*>& OutActors, TArray<AFGWheeledVehicle*>& OutVehicles)
	{
		for (const AFGWheeledVehicleIdentifier* Identifier : Subsystem->GetAllVehicles())
		{
			AFGWheeledVehicle* Vehicle = Identifier ? Identifier->GetOwnerVehicle() : nullptr;
			if (IsValid(Vehicle))
			{
				OutVehicles.Add(Vehicle);
				OutActors.Add(Vehicle);
			}
		}
	}

	void LogVehicleCollisionOnce(const AActor* Actor)
	{
		const FName ClassName = Actor->GetClass()->GetFName();
		if (LoggedVehicleClasses.Contains(ClassName))
		{
			return;
		}
		LoggedVehicleClasses.Add(ClassName);

		TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
		for (const UPrimitiveComponent* Component : Components)
		{
			UE_LOG(LogTruckPassThrough, Display, TEXT("%s.%s: collision %s, object type %s, response to Vehicle %d"),
				*ClassName.ToString(), *Component->GetName(),
				Component->IsCollisionEnabled() ? TEXT("on") : TEXT("off"),
				*ChannelName(Component->GetCollisionObjectType()),
				static_cast<int32>(Component->GetCollisionResponseToChannel(ECC_Vehicle)));
		}
	}
}

void FTruckPassThroughHooks::Install()
{
	// Vehicle avoidance: slowing down for, or stopping behind, the vehicle ahead
	SUBSCRIBE_METHOD(UFGVehicleAutopilotComponent::CalculateVehicleAvoidanceTarget,
		[](auto& Scope, const UFGVehicleAutopilotComponent* Self, float MaxLookaheadDistance, float VehicleHalfLength, TArray<FVehicleStopTarget>& OutStopTargets)
		{
			if (ShouldGhost(Self))
			{
				Scope.Cancel();
			}
		});

	// Junction reservations: stopping at the line until the junction blocks ahead are free
	SUBSCRIBE_METHOD(UFGVehicleAutopilotComponent::CalculatePathReservationStopTarget,
		[](auto& Scope, const UFGVehicleAutopilotComponent* Self, float MaxLookaheadDistance, float VehicleHalfLength, TArray<FVehicleStopTarget>& OutStopTargets)
		{
			if (ShouldGhost(Self))
			{
				Scope.Cancel();
			}
		});

	// Junction reservations: booking blocks. Ghosts book nothing, so they never hold up a truck queueing at a station.
	// With nothing referenced, the autopilot's own cleanup releases whatever the truck booked before it became a ghost.
	SUBSCRIBE_METHOD(UFGVehicleAutopilotComponent::ReserveVehiclePathBlocks_Parallel,
		[](auto& Scope, UFGVehicleAutopilotComponent* Self, const TArray<FVehicleAutopilotBlockReference>& PathBlocks, TSet<FVehiclePathBlockReference>& ReferencedPathBlocks)
		{
			if (ShouldGhost(Self))
			{
				Scope.Cancel();
			}
		});

	// Deadlock timer: a ghost never waits on a reservation, so keep its wait time at zero and it is never flagged as deadlocked
	SUBSCRIBE_METHOD(UFGVehicleAutopilotComponent::UpdateVehicleDeadlockStatus_Parallel,
		[](auto& Scope, UFGVehicleAutopilotComponent* Self, const FVehicleCommonInputs& CommonInputs, const TArray<FVehicleAutopilotBlockReference>& CurrentVehicleBlocks, float DeltaTime, FVehicleAutopilotComponentSimData& SimData)
		{
			if (ShouldGhost(Self))
			{
				Self->mTimeSpentWaitingOnCurrentFreeBlock = 0.0f;
			}
		});

	SUBSCRIBE_METHOD_AFTER(AFGVehicleSubsystem::TickVehicleAutopilot,
		[](AFGVehicleSubsystem* Self, float DeltaTime)
		{
			PostTickVehicleAutopilot(Self, DeltaTime);
		});

	UE_LOG(LogTruckPassThrough, Display, TEXT("hooks installed"));
}

bool FTruckPassThroughHooks::IsInStationZone(const UFGVehicleAutopilotComponent* Autopilot)
{
	const AFGWheeledVehicle* Vehicle = Cast<AFGWheeledVehicle>(Autopilot->GetOwner());
	if (Vehicle == nullptr)
	{
		return true;
	}

	const AFGWheeledVehicleIdentifier* Identifier = Vehicle->GetVehicleIdentifier();
	if (Identifier != nullptr && Identifier->IsCurrentlyDocking())
	{
		return true;
	}

	// In proxy mode the vehicle actor is not moved, so prefer the autopilot's own position when it has one
	const FVector Location = Autopilot->mHasAuthoritativeVehicleLocation ? Autopilot->mLastVehicleLocation : Vehicle->GetActorLocation();
	const float Radius = CVarStationRadius.GetValueOnAnyThread();
	const float RadiusSquared = Radius * Radius;
	for (const FVector& StationLocation : StationLocations)
	{
		if (FVector::DistSquared(Location, StationLocation) <= RadiusSquared)
		{
			return true;
		}
	}
	return false;
}

bool FTruckPassThroughHooks::ShouldGhost(const UFGVehicleAutopilotComponent* Autopilot)
{
	return CVarEnabled.GetValueOnAnyThread() != 0 && Autopilot != nullptr && !IsInStationZone(Autopilot);
}

void FTruckPassThroughHooks::PostTickVehicleAutopilot(AFGVehicleSubsystem* Subsystem, float DeltaTime)
{
	UWorld* World = Subsystem->GetWorld();
	if (CachedWorld.Get() != World)
	{
		// New save loaded: refresh everything on this tick
		CachedWorld = World;
		StationLocations.Reset();
		GhostedComponents.Reset();
		TimeSinceStationRefresh = StationRefreshInterval;
		TimeSinceCollisionRefresh = CollisionRefreshInterval;
	}

	TimeSinceStationRefresh += DeltaTime;
	if (TimeSinceStationRefresh >= StationRefreshInterval)
	{
		TimeSinceStationRefresh = 0.0f;
		RefreshStationLocations(Subsystem);
	}

	TimeSinceCollisionRefresh += DeltaTime;
	if (TimeSinceCollisionRefresh >= CollisionRefreshInterval)
	{
		TimeSinceCollisionRefresh = 0.0f;
		if (CVarGhost.GetValueOnGameThread() != 0)
		{
			ApplyVehicleCollisionGhosting(Subsystem);
		}
		else if (GhostedComponents.Num() > 0)
		{
			RestoreVehicleCollision();
		}
	}
}

void FTruckPassThroughHooks::RefreshStationLocations(AFGVehicleSubsystem* Subsystem)
{
	StationLocations.Reset();
	for (const AFGDockingStationIdentifier* Identifier : Subsystem->GetDockingStations())
	{
		const AFGBuildableDockingStation* Station = Identifier ? Identifier->GetStation() : nullptr;
		if (!IsValid(Station))
		{
			continue;
		}
		const AFGVehiclePathNode* DockingNode = Station->GetDockingPathNode();
		StationLocations.Add(IsValid(DockingNode) ? DockingNode->GetActorLocation() : Station->GetActorLocation());
	}
}

void FTruckPassThroughHooks::ApplyVehicleCollisionGhosting(AFGVehicleSubsystem* Subsystem)
{
	TArray<AActor*> Actors;
	TArray<AFGWheeledVehicle*> Vehicles;
	CollectVehicleActors(Subsystem, Actors, Vehicles);
	for (AFGWheeledVehicle* Vehicle : Vehicles)
	{
		if (IsValid(Vehicle->mVehicleProxyActor))
		{
			Actors.Add(Vehicle->mVehicleProxyActor.Get());
		}
	}

	// Vehicles ignore the Vehicle channel plus whatever object type their bodies actually use
	GhostChannels.Add(ECC_Vehicle);
	for (const AActor* Actor : Actors)
	{
		LogVehicleCollisionOnce(Actor);
		const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
		if (Body != nullptr && Body->IsCollisionEnabled() && !IsSharedWorldChannel(Body->GetCollisionObjectType()))
		{
			bool bAlreadyInSet = false;
			GhostChannels.Add(Body->GetCollisionObjectType(), &bAlreadyInSet);
			if (!bAlreadyInSet)
			{
				UE_LOG(LogTruckPassThrough, Display, TEXT("vehicles now ignore collision channel %s"), *ChannelName(Body->GetCollisionObjectType()));
			}
		}
	}

	for (const AActor* Actor : Actors)
	{
		TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
		for (UPrimitiveComponent* Component : Components)
		{
			if (!Component->IsCollisionEnabled())
			{
				continue;
			}
			if (!GhostedComponents.Contains(Component))
			{
				GhostedComponents.Add(Component, Component->GetCollisionResponseToChannels());
			}
			for (const ECollisionChannel Channel : GhostChannels)
			{
				if (Component->GetCollisionResponseToChannel(Channel) != ECR_Ignore)
				{
					Component->SetCollisionResponseToChannel(Channel, ECR_Ignore);
				}
			}
		}
	}
}

void FTruckPassThroughHooks::RestoreVehicleCollision()
{
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FCollisionResponseContainer>& Entry : GhostedComponents)
	{
		if (UPrimitiveComponent* Component = Entry.Key.Get())
		{
			Component->SetCollisionResponseToChannels(Entry.Value);
		}
	}
	UE_LOG(LogTruckPassThrough, Display, TEXT("restored vanilla collision on %d components"), GhostedComponents.Num());
	GhostedComponents.Reset();
}

void FTruckPassThroughHooks::DumpStatus(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	AFGVehicleSubsystem* Subsystem = AFGVehicleSubsystem::Get(World);
	if (Subsystem == nullptr)
	{
		Ar.Log(TEXT("TPT: no vehicle subsystem in this world"));
		return;
	}

	int32 AutopilotTrucks = 0;
	int32 StationZoneTrucks = 0;
	for (const AFGWheeledVehicleIdentifier* Identifier : Subsystem->GetAllVehicles())
	{
		const AFGWheeledVehicle* Vehicle = Identifier ? Identifier->GetOwnerVehicle() : nullptr;
		const UFGVehicleAutopilotComponent* Autopilot = Vehicle ? Vehicle->GetVehicleAutopilotComponent() : nullptr;
		if (Autopilot == nullptr || !Identifier->IsAutopilotEnabled())
		{
			continue;
		}
		++AutopilotTrucks;
		if (IsInStationZone(Autopilot))
		{
			++StationZoneTrucks;
		}
	}

	const FString Status = FString::Printf(
		TEXT("TPT: Enabled=%d Ghost=%d StationRadius=%.0f | stations=%d | autopilot trucks=%d (ghost %d, station zone %d) | ghosted components=%d"),
		CVarEnabled.GetValueOnGameThread(), CVarGhost.GetValueOnGameThread(), CVarStationRadius.GetValueOnGameThread(),
		StationLocations.Num(), AutopilotTrucks, AutopilotTrucks - StationZoneTrucks, StationZoneTrucks, GhostedComponents.Num());
	Ar.Log(Status);
	UE_LOG(LogTruckPassThrough, Display, TEXT("%s"), *Status);
}
