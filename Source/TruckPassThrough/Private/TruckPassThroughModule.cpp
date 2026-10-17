#include "TruckPassThroughModule.h"

#include "TPTHooks.h"

void FTruckPassThroughModule::StartupModule()
{
#if WITH_EDITOR
	// The editor's FactoryGame is built from header stubs with empty function bodies, and hooking those aborts the process
	UE_LOG(LogTruckPassThrough, Display, TEXT("editor build, hooks not installed"));
#else
	FTruckPassThroughHooks::Install();
#endif
}

IMPLEMENT_MODULE(FTruckPassThroughModule, TruckPassThrough)
