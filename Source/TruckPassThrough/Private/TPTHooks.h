#pragma once

#include "CoreMinimal.h"

class AFGVehicleSubsystem;
class FOutputDevice;
class UFGVehicleAutopilotComponent;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogTruckPassThrough, Log, All);

/**
 * Native hooks for the truck autopilot. This class is named as a friend of the game classes in
 * Config/AccessTransformers.ini, so everything that touches protected game members lives in here.
 */
class FTruckPassThroughHooks
{
public:
	static void Install();

	/** TPT.Status console command */
	static void DumpStatus(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

private:
	enum class ETruckMode : uint8
	{
		/** Ignores vehicles ahead and junction reservations */
		Ghost,
		/** Vanilla, because the autopilot has no vehicle */
		NoVehicle,
		/** Vanilla, because the truck is docked */
		Docking,
		/** Vanilla, because the truck is within TPT.StationRadius of a station's docking point */
		NearStation,
	};

	/** Whether the truck ghosts or keeps vanilla queueing, optionally with the distance (cm) to the nearest station */
	static ETruckMode GetTruckMode(const UFGVehicleAutopilotComponent* Autopilot, float* OutNearestStationDistance = nullptr);

	/** True if the truck should ignore vehicles ahead and junction reservations */
	static bool ShouldGhost(const UFGVehicleAutopilotComponent* Autopilot);

	/** Runs on the game thread after all autopilots have ticked in parallel */
	static void PostTickVehicleAutopilot(AFGVehicleSubsystem* Subsystem, float DeltaTime);

	static void RefreshStationLocations(AFGVehicleSubsystem* Subsystem);
	static void ApplyVehicleCollisionGhosting(AFGVehicleSubsystem* Subsystem);
	static void RestoreVehicleCollision();

	/** Lines for TPT.Status and the periodic status log */
	static TArray<FString> BuildStatusLines(AFGVehicleSubsystem* Subsystem);
};
