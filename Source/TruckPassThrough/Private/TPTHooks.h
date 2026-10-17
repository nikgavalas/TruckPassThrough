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
	/** True if the truck is docked or close to a truck station, where it keeps vanilla queueing */
	static bool IsInStationZone(const UFGVehicleAutopilotComponent* Autopilot);

	/** True if the truck should ignore vehicles ahead and junction reservations */
	static bool ShouldGhost(const UFGVehicleAutopilotComponent* Autopilot);

	/** Runs on the game thread after all autopilots have ticked in parallel */
	static void PostTickVehicleAutopilot(AFGVehicleSubsystem* Subsystem, float DeltaTime);

	static void RefreshStationLocations(AFGVehicleSubsystem* Subsystem);
	static void ApplyVehicleCollisionGhosting(AFGVehicleSubsystem* Subsystem);
	static void RestoreVehicleCollision();
};
