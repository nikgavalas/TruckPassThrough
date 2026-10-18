#include "TPTGameWorldModule.h"

#include "TPTTrucksCommand.h"

UTPTGameWorldModule::UTPTGameWorldModule()
{
	bRootModule = true;
	mChatCommands.Add(ATPTTrucksCommand::StaticClass());
}
