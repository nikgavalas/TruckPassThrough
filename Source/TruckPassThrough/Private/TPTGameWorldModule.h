#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "TPTGameWorldModule.generated.h"

/** Registers the /trucks chat command. SML finds native root modules by class, so no content asset is needed. */
UCLASS()
class UTPTGameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()

public:
	UTPTGameWorldModule();
};
