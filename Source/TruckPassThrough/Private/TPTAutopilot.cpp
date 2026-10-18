#include "TPTAutopilot.h"

#include "HAL/IConsoleManager.h"
#include "TPTHooks.h"
#include "WheeledVehicles/FGVehicleSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicleIdentifier.h"

static FAutoConsoleCommandWithWorldArgsAndOutputDevice CmdAutopilot(
	TEXT("TPT.Autopilot"),
	TEXT("TPT.Autopilot on|off [name filter]: switches self-driving for every truck at once."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		bool bEnable = false;
		if (Args.Num() == 0 || !TPTAutopilot::ParseMode(Args[0], bEnable))
		{
			Ar.Log(TEXT("usage: TPT.Autopilot on|off [name filter]"));
			return;
		}
		Ar.Log(TPTAutopilot::SetForAll(World, bEnable, Args.Num() > 1 ? Args[1] : FString()));
	}));

bool TPTAutopilot::ParseMode(const FString& Argument, bool& bOutEnable)
{
	if (Argument == TEXT("on") || Argument == TEXT("1"))
	{
		bOutEnable = true;
		return true;
	}
	if (Argument == TEXT("off") || Argument == TEXT("0"))
	{
		bOutEnable = false;
		return true;
	}
	return false;
}

FString TPTAutopilot::SetForAll(UWorld* World, bool bEnable, const FString& Filter)
{
	const TCHAR* Mode = bEnable ? TEXT("on") : TEXT("off");
	AFGVehicleSubsystem* Subsystem = AFGVehicleSubsystem::Get(World);
	if (Subsystem == nullptr)
	{
		return FString::Printf(TEXT("autopilot %s: no vehicle subsystem in this world"), Mode);
	}
	if (Subsystem->GetNetMode() == NM_Client)
	{
		// SetAutopilotEnabled only changes the local copy on a client
		return FString::Printf(TEXT("autopilot %s: only works on the host or server. Use the /trucks chat command instead."), Mode);
	}

	int32 Changed = 0;
	int32 Unchanged = 0;
	int32 CannotEnable = 0;
	for (AFGWheeledVehicleIdentifier* Identifier : Subsystem->GetAllVehicles())
	{
		if (!IsValid(Identifier))
		{
			continue;
		}
		const FString Name = Identifier->GetVehicleName().ToString();
		if (!Filter.IsEmpty() && !Name.Contains(Filter))
		{
			continue;
		}
		if (Identifier->IsAutopilotEnabled() == bEnable)
		{
			++Unchanged;
			continue;
		}
		if (bEnable && !Identifier->CanEnableAutopilot())
		{
			// For example a route with fewer than two stations. The vehicle UI would refuse it too.
			UE_LOG(LogTruckPassThrough, Display, TEXT("autopilot on: cannot enable %s"), *Name);
			++CannotEnable;
			continue;
		}
		Identifier->SetAutopilotEnabled(bEnable);
		UE_LOG(LogTruckPassThrough, Display, TEXT("autopilot %s: %s"), Mode, *Name);
		++Changed;
	}

	FString Summary = FString::Printf(TEXT("autopilot %s: changed %d, already %s %d"), Mode, Changed, Mode, Unchanged);
	if (CannotEnable > 0)
	{
		Summary += FString::Printf(TEXT(", cannot enable %d (see log)"), CannotEnable);
	}
	if (!Filter.IsEmpty())
	{
		Summary += FString::Printf(TEXT(", name filter '%s'"), *Filter);
	}
	UE_LOG(LogTruckPassThrough, Display, TEXT("%s"), *Summary);
	return Summary;
}
