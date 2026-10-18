#include "TPTTrucksCommand.h"

#include "Command/CommandSender.h"
#include "TPTAutopilot.h"

ATPTTrucksCommand::ATPTTrucksCommand()
{
	CommandName = TEXT("trucks");
	MinNumberOfArguments = 1;
	Usage = NSLOCTEXT("TruckPassThrough", "ChatCommand.Trucks.Usage", "/trucks on|off [name filter] - switch self-driving for every truck at once");
}

EExecutionStatus ATPTTrucksCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	bool bEnable = false;
	if (!TPTAutopilot::ParseMode(Arguments[0], bEnable))
	{
		PrintCommandUsage(Sender);
		return EExecutionStatus::BAD_ARGUMENTS;
	}
	// One line per truck would flood the chat, so those only go to the log
	Sender->SendChatMessage(TPTAutopilot::SetForAll(GetWorld(), bEnable, Arguments.Num() > 1 ? Arguments[1] : FString()));
	return EExecutionStatus::COMPLETED;
}
