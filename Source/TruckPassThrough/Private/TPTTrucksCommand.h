#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "TPTTrucksCommand.generated.h"

/**
 * /trucks on|off [name filter], the chat version of TPT.Autopilot. SML runs chat commands on the server, so this also
 * works for a player on a dedicated server, and that player only needs SML, not this mod.
 */
UCLASS()
class ATPTTrucksCommand : public AChatCommandInstance
{
	GENERATED_BODY()

public:
	ATPTTrucksCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
