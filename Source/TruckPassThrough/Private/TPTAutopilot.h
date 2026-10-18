#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Switches self-driving on or off for every truck at once. Used by TPT.Autopilot and the /trucks chat command. */
namespace TPTAutopilot
{
	/** Parses "on" or "off" (also 1/0). Returns false for anything else. */
	bool ParseMode(const FString& Argument, bool& bOutEnable);

	/**
	 * Turns autopilot on or off for every vehicle whose name contains Filter (all vehicles when Filter is empty).
	 * Logs one line per changed vehicle and returns a one-line summary.
	 */
	FString SetForAll(UWorld* World, bool bEnable, const FString& Filter);
}
