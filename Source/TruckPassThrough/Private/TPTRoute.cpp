// TPT.Route <name filter>: explains why a vehicle cannot reach the stations on its route

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TPTHooks.h"
#include "WheeledVehicles/FGDockingStationIdentifier.h"
#include "WheeledVehicles/FGVehiclePathPreset.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"

namespace
{
	constexpr int32 MaxBlockedSegmentLines = 5;

	FString ShortClassName(const UClass* Class)
	{
		FString Name = GetNameSafe(Class);
		Name.RemoveFromStart(TEXT("BP_"));
		Name.RemoveFromEnd(TEXT("_C"));
		return Name;
	}

	/** Where a world location is from the player's point of view, so it can be found in game */
	FString DescribeLocation(const FVector& Location, const APlayerController* Player)
	{
		const FString Coordinates = FString::Printf(TEXT("(%.0f, %.0f, %.0f) m"), Location.X / 100.0, Location.Y / 100.0, Location.Z / 100.0);
		if (Player == nullptr)
		{
			return Coordinates;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector Delta = Location - ViewLocation;
		// Unreal yaw grows clockwise seen from above, so a positive angle is to the right
		const float Angle = FRotator::NormalizeAxis(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)) - ViewRotation.Yaw);
		const TCHAR* Side = FMath::Abs(Angle) <= 20.0f ? TEXT("ahead of you")
			: FMath::Abs(Angle) >= 160.0f ? TEXT("behind you")
			: Angle > 0.0f ? TEXT("to your right") : TEXT("to your left");
		return FString::Printf(TEXT("%.0f m away %s (%.0f deg), %+.0f m height, at %s"),
			Delta.Size2D() / 100.0, Side, Angle, Delta.Z / 100.0, *Coordinates);
	}

	FString DescribeWaypoint(const AFGVehicleSubsystem* Subsystem, const FGuid& Waypoint)
	{
		const AFGDockingStationIdentifier* Station = Subsystem->FindDockingStationIdentifierForPathNodeGuid(Waypoint);
		return Station != nullptr ? FString::Printf(TEXT("station '%s'"), *Station->GetStationName().ToString()) : FString(TEXT("waypoint"));
	}

	/** Every node reachable from Start along the network's one-way segments, only over segments Preset can drive (any segment when Preset is null) */
	TArray<bool> FindReachableNodes(const UFGVehiclePathNetwork* Network, const TArray<TArray<int32>>& OutgoingSegments, int32 Start, const UFGVehiclePathPreset* Preset)
	{
		const TArray<FVehiclePathNetworkSegmentData>& Segments = Network->GetPathSegments();
		TArray<bool> Reached;
		Reached.Init(false, Network->GetPathNodes().Num());
		TArray<int32> Queue = {Start};
		Reached[Start] = true;
		while (Queue.Num() > 0)
		{
			const int32 Node = Queue.Pop(EAllowShrinking::No);
			for (const int32 SegmentIndex : OutgoingSegments[Node])
			{
				const FVehiclePathNetworkSegmentData& Segment = Segments[SegmentIndex];
				if (Reached.IsValidIndex(Segment.ToNodeIndex) && !Reached[Segment.ToNodeIndex] &&
					(Preset == nullptr || Network->CanVehicleTraverseSegment(Segment, Preset)))
				{
					Reached[Segment.ToNodeIndex] = true;
					Queue.Push(Segment.ToNodeIndex);
				}
			}
		}
		return Reached;
	}

	void ExplainLeg(TArray<FString>& Lines, const AFGVehicleSubsystem* Subsystem, const FGuid& From, const FGuid& To,
		const UFGVehiclePathPreset* Preset, const FString& VehicleType, const APlayerController* Player)
	{
		const FString Target = DescribeWaypoint(Subsystem, To);
		const UFGVehiclePathNetwork* Network = Subsystem->FindNetworkByPathNodeGuid(From);
		if (Network == nullptr)
		{
			Lines.Add(FString::Printf(TEXT("    to %s: the start point is not on any road network"), *Target));
			return;
		}
		if (Subsystem->FindNetworkByPathNodeGuid(To) != Network)
		{
			Lines.Add(FString::Printf(TEXT("    to %s: UNREACHABLE, it is on a separate road network that is not connected to this one at all"), *Target));
			return;
		}

		TArray<FGuid> Path;
		if (Network->FindVehiclePath(From, To, Preset, Path))
		{
			Lines.Add(FString::Printf(TEXT("    to %s: reachable, %d path nodes"), *Target, Path.Num()));
			return;
		}

		const TArray<FVehiclePathNetworkNodeData>& Nodes = Network->GetPathNodes();
		const TArray<FVehiclePathNetworkSegmentData>& Segments = Network->GetPathSegments();
		const int32 StartIndex = Network->FindPathNodeIndexByGuid(From);
		const int32 TargetIndex = Network->FindPathNodeIndexByGuid(To);
		if (!Nodes.IsValidIndex(StartIndex) || !Nodes.IsValidIndex(TargetIndex))
		{
			Lines.Add(FString::Printf(TEXT("    to %s: UNREACHABLE, and its path nodes are missing from the network"), *Target));
			return;
		}

		TArray<TArray<int32>> OutgoingSegments;
		OutgoingSegments.SetNum(Nodes.Num());
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			if (OutgoingSegments.IsValidIndex(Segments[SegmentIndex].FromNodeIndex))
			{
				OutgoingSegments[Segments[SegmentIndex].FromNodeIndex].Add(SegmentIndex);
			}
		}
		const TArray<bool> ReachedByVehicle = FindReachableNodes(Network, OutgoingSegments, StartIndex, Preset);
		const TArray<bool> ReachedByAnyone = FindReachableNodes(Network, OutgoingSegments, StartIndex, nullptr);
		const FVector TargetLocation = Nodes[TargetIndex].PathNodeLocation;

		const bool bConnected = ReachedByAnyone[TargetIndex];
		Lines.Add(FString::Printf(TEXT("    to %s: UNREACHABLE for a %s. %s"), *Target, *VehicleType, bConnected
			? *FString::Printf(TEXT("The roads do connect, but some segments on the way cannot be driven by a %s (too steep, too tight a turn or too narrow)."), *VehicleType)
			: TEXT("No road leads there in this direction: a connection is missing, or a one-way road points the wrong way.")));
		Lines.Add(FString::Printf(TEXT("      station: %s"), *DescribeLocation(TargetLocation, Player)));

		// Closest point the vehicle can get to, to show where the road stops working
		int32 ClosestIndex = StartIndex;
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			if (ReachedByVehicle[NodeIndex] &&
				FVector::DistSquared(Nodes[NodeIndex].PathNodeLocation, TargetLocation) < FVector::DistSquared(Nodes[ClosestIndex].PathNodeLocation, TargetLocation))
			{
				ClosestIndex = NodeIndex;
			}
		}
		Lines.Add(FString::Printf(TEXT("      closest the %s gets: %.0f m short of it, %s"), *VehicleType,
			FVector::Dist(Nodes[ClosestIndex].PathNodeLocation, TargetLocation) / 100.0, *DescribeLocation(Nodes[ClosestIndex].PathNodeLocation, Player)));

		if (!bConnected)
		{
			return;
		}
		// Segments leading out of the area this vehicle can reach that it is not allowed to drive, nearest to the station first
		TArray<int32> BlockedSegments;
		for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
		{
			const FVehiclePathNetworkSegmentData& Segment = Segments[SegmentIndex];
			if (ReachedByVehicle.IsValidIndex(Segment.FromNodeIndex) && ReachedByVehicle.IsValidIndex(Segment.ToNodeIndex) &&
				ReachedByVehicle[Segment.FromNodeIndex] && !ReachedByVehicle[Segment.ToNodeIndex] && !Network->CanVehicleTraverseSegment(Segment, Preset))
			{
				BlockedSegments.Add(SegmentIndex);
			}
		}
		BlockedSegments.Sort([&](int32 A, int32 B)
		{
			return FVector::DistSquared(Nodes[Segments[A].ToNodeIndex].PathNodeLocation, TargetLocation) <
				FVector::DistSquared(Nodes[Segments[B].ToNodeIndex].PathNodeLocation, TargetLocation);
		});
		for (int32 Index = 0; Index < BlockedSegments.Num() && Index < MaxBlockedSegmentLines; ++Index)
		{
			const FVehiclePathNetworkSegmentData& Segment = Segments[BlockedSegments[Index]];
			Lines.Add(FString::Printf(TEXT("      blocked segment %d: starts %s"), Index + 1, *DescribeLocation(Nodes[Segment.FromNodeIndex].PathNodeLocation, Player)));
			Lines.Add(FString::Printf(TEXT("                         ends   %s"), *DescribeLocation(Nodes[Segment.ToNodeIndex].PathNodeLocation, Player)));
		}
		if (BlockedSegments.Num() > MaxBlockedSegmentLines)
		{
			Lines.Add(FString::Printf(TEXT("      ... and %d more blocked segments further from the station"), BlockedSegments.Num() - MaxBlockedSegmentLines));
		}
	}

	void ExplainRoute(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		TArray<FString> Lines;
		AFGVehicleSubsystem* Subsystem = AFGVehicleSubsystem::Get(World);
		if (Args.Num() == 0)
		{
			Lines.Add(TEXT("usage: TPT.Route <vehicle name filter>"));
		}
		else if (Subsystem == nullptr)
		{
			Lines.Add(TEXT("TPT.Route: no vehicle subsystem in this world"));
		}
		else
		{
			const FString Filter = FString::Join(Args, TEXT(" "));
			const APlayerController* Player = World->GetFirstPlayerController();
			int32 Matches = 0;
			for (const AFGWheeledVehicleIdentifier* Identifier : Subsystem->GetAllVehicles())
			{
				if (!IsValid(Identifier) || !Identifier->GetVehicleName().ToString().Contains(Filter))
				{
					continue;
				}
				++Matches;
				const AFGWheeledVehicle* Vehicle = Identifier->GetOwnerVehicle();
				const UFGVehiclePathPreset* Preset = Vehicle != nullptr ? Vehicle->GetVehiclePathPreset()
					: AFGWheeledVehicle::GetPathPathPresetForVehicleType(Identifier->GetVehicleClass());
				const FString VehicleType = ShortClassName(Identifier->GetVehicleClass());
				const TArray<FGuid>& Route = Identifier->GetVehicleRoute();

				Lines.Add(FString::Printf(TEXT("TPT.Route %s (%s): autopilot %s, status %s, %d waypoints, heading for waypoint %d"),
					*Identifier->GetVehicleName().ToString(), *VehicleType, Identifier->IsAutopilotEnabled() ? TEXT("on") : TEXT("off"),
					*StaticEnum<EVehicleAutopilotErrorStatus>()->GetNameStringByValue(static_cast<int64>(Identifier->GetAutopilotErrorStatus())),
					Route.Num(), Identifier->GetCurrentTargetWaypointIndex() + 1));
				if (Vehicle != nullptr)
				{
					Lines.Add(FString::Printf(TEXT("  vehicle: %s"), *DescribeLocation(Vehicle->GetActorLocation(), Player)));
				}
				if (Preset == nullptr)
				{
					Lines.Add(TEXT("  no path preset for this vehicle type, cannot check routes"));
					continue;
				}

				// From where the vehicle is now to the waypoint it is heading for
				const FGuid Ahead = Identifier->GetCurrentToPathNodeGUID();
				const int32 TargetWaypoint = Identifier->GetCurrentTargetWaypointIndex();
				if (!Ahead.IsValid())
				{
					Lines.Add(TEXT("  from here: the vehicle is not on a road. Drive it onto one first."));
				}
				else if (Route.IsValidIndex(TargetWaypoint))
				{
					Lines.Add(TEXT("  from here:"));
					ExplainLeg(Lines, Subsystem, Ahead, Route[TargetWaypoint], Preset, VehicleType, Player);
				}

				// Every leg of the round trip, so a problem further along the route also shows up
				Lines.Add(TEXT("  route legs:"));
				for (int32 Index = 0; Index < Route.Num() && Route.Num() >= 2; ++Index)
				{
					Lines.Add(FString::Printf(TEXT("   %d -> %d, from %s"), Index + 1, (Index + 1) % Route.Num() + 1, *DescribeWaypoint(Subsystem, Route[Index])));
					ExplainLeg(Lines, Subsystem, Route[Index], Route[(Index + 1) % Route.Num()], Preset, VehicleType, Player);
				}
			}
			if (Matches == 0)
			{
				Lines.Add(FString::Printf(TEXT("TPT.Route: no vehicle name contains '%s'"), *Filter));
			}
		}

		for (const FString& Line : Lines)
		{
			Ar.Log(Line);
			UE_LOG(LogTruckPassThrough, Display, TEXT("%s"), *Line);
		}
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdRoute(
	TEXT("TPT.Route"),
	TEXT("TPT.Route <vehicle name filter>: checks every leg of the vehicle's route and says where and why it cannot reach a station."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&ExplainRoute));
