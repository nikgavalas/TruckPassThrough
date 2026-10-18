#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "Module/GameInstanceModule.h"
#include "TPTConfig.generated.h"

class UConfigPropertyFloat;

/**
 * The mod's page in the in-game Mods menu. SML saves it to <game>/FactoryGame/Configs/TruckPassThrough.cfg.
 * Its properties are built at runtime by UTPTGameInstanceModule, because the menu widgets come from SML's Blueprint
 * property classes and this mod has no content of its own.
 */
UCLASS()
class UTPTModConfiguration : public UModConfiguration
{
	GENERATED_BODY()

public:
	UTPTModConfiguration();
};

/**
 * Registers the Mods menu configuration and keeps it in sync with the TPT.StationRadius console variable in both
 * directions, so a value typed in the console is saved too.
 */
UCLASS()
class UTPTGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	UTPTGameInstanceModule();

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

private:
	/** Fills the configuration's root section with properties before SML registers it */
	void BuildConfigurationTemplate();

	/** Applies the loaded configuration to the console variable and starts syncing */
	void BindConfiguration();

	UFUNCTION()
	void OnStationRadiusConfigChanged();

	void OnStationRadiusConsoleChanged(IConsoleVariable* Variable);

	/** The live property SML edits and saves, not the template */
	UPROPERTY()
	TObjectPtr<UConfigPropertyFloat> StationRadiusProperty;
};
