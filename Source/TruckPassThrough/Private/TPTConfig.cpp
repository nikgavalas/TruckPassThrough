#include "TPTConfig.h"

#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "HAL/IConsoleManager.h"
#include "TPTHooks.h"

#define LOCTEXT_NAMESPACE "TruckPassThrough"

namespace
{
	const TCHAR* ModReference = TEXT("TruckPassThrough");
	const TCHAR* StationRadiusKey = TEXT("StationRadius");
	const TCHAR* StationRadiusVariable = TEXT("TPT.StationRadius");

	// SML's Blueprint subclasses of its property classes. Only these draw an editor widget in the Mods menu.
	const TCHAR* SectionClassPath = TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/BP_ConfigPropertySection.BP_ConfigPropertySection_C");
	const TCHAR* FloatClassPath = TEXT("/SML/Interface/UI/Menu/Mods/ConfigProperties/BP_ConfigPropertyFloat.BP_ConfigPropertyFloat_C");

	FConfigId GetConfigId()
	{
		FConfigId ConfigId;
		ConfigId.ModReference = ModReference;
		return ConfigId;
	}
}

UTPTModConfiguration::UTPTModConfiguration()
{
	ConfigId = GetConfigId();
	DisplayName = LOCTEXT("ConfigDisplayName", "Truck Pass Through");
	Description = LOCTEXT("ConfigDescription", "Self-driving trucks pass through each other and do not wait at junctions.");
}

UTPTGameInstanceModule::UTPTGameInstanceModule()
{
	bRootModule = true;
	ModConfigurations.Add(UTPTModConfiguration::StaticClass());
}

void UTPTGameInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	// SML registers ModConfigurations, and loads the saved values, during INITIALIZATION in Super
	if (Phase == ELifecyclePhase::INITIALIZATION)
	{
		BuildConfigurationTemplate();
	}
	Super::DispatchLifecycleEvent(Phase);
	if (Phase == ELifecyclePhase::INITIALIZATION)
	{
		BindConfiguration();
	}
}

void UTPTGameInstanceModule::BuildConfigurationTemplate()
{
	UTPTModConfiguration* Configuration = GetMutableDefault<UTPTModConfiguration>();
	if (Configuration->RootSection != nullptr)
	{
		return;
	}
	UClass* SectionClass = LoadClass<UConfigPropertySection>(nullptr, SectionClassPath);
	UClass* FloatClass = LoadClass<UConfigPropertyFloat>(nullptr, FloatClassPath);
	if (SectionClass == nullptr || FloatClass == nullptr)
	{
		UE_LOG(LogTruckPassThrough, Error, TEXT("SML config property classes not found, the Mods menu page is missing"));
		SectionClass = UConfigPropertySection::StaticClass();
		FloatClass = UConfigPropertyFloat::StaticClass();
	}

	// Outers matter: SML copies the root section, and a property is only duplicated with it when the section is its outer
	UConfigPropertySection* RootSection = NewObject<UConfigPropertySection>(Configuration, SectionClass, TEXT("RootSection"));
	UConfigPropertyFloat* StationRadius = NewObject<UConfigPropertyFloat>(RootSection, FloatClass, StationRadiusKey);
	StationRadius->DisplayName = LOCTEXT("StationRadiusName", "Station queueing radius (cm)");
	StationRadius->Tooltip = LOCTEXT("StationRadiusTooltip",
		"0 (recommended): only a truck that is docking keeps vanilla behavior, so one truck docks at a time.\n"
		"Above 0: every truck within this distance of any station's docking point also waits for other trucks and junctions, "
		"which can cause deadlocks near stations.\n"
		"Same as the TPT.StationRadius console command. 100 cm = 1 m.");
	StationRadius->DefaultValue = 0.0f;
	StationRadius->Value = 0.0f;
	RootSection->SectionProperties.Add(StationRadiusKey, StationRadius);
	Configuration->RootSection = RootSection;
}

void UTPTGameInstanceModule::BindConfiguration()
{
	const UConfigManager* ConfigManager = GetGameInstance()->GetSubsystem<UConfigManager>();
	const UConfigPropertySection* RootSection = ConfigManager != nullptr ? ConfigManager->GetConfigurationRootSection(GetConfigId()) : nullptr;
	const TObjectPtr<UConfigProperty>* Property = RootSection != nullptr ? RootSection->SectionProperties.Find(StationRadiusKey) : nullptr;
	StationRadiusProperty = Property != nullptr ? Cast<UConfigPropertyFloat>(Property->Get()) : nullptr;
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(StationRadiusVariable);
	if (StationRadiusProperty == nullptr || Variable == nullptr)
	{
		UE_LOG(LogTruckPassThrough, Error, TEXT("Mods menu configuration not registered, TPT.StationRadius will not be saved"));
		return;
	}

	StationRadiusProperty->OnPropertyValueChanged.AddDynamic(this, &UTPTGameInstanceModule::OnStationRadiusConfigChanged);
	Variable->SetOnChangedCallback(FConsoleVariableDelegate::CreateUObject(this, &UTPTGameInstanceModule::OnStationRadiusConsoleChanged));
	OnStationRadiusConfigChanged();
}

void UTPTGameInstanceModule::OnStationRadiusConfigChanged()
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(StationRadiusVariable);
	if (StationRadiusProperty != nullptr && Variable != nullptr && Variable->GetFloat() != StationRadiusProperty->Value)
	{
		// Console priority, so a value typed in the console earlier does not block this one
		Variable->Set(StationRadiusProperty->Value, ECVF_SetByConsole);
		UE_LOG(LogTruckPassThrough, Display, TEXT("TPT.StationRadius set to %.0f from the Mods menu configuration"), StationRadiusProperty->Value);
	}
}

void UTPTGameInstanceModule::OnStationRadiusConsoleChanged(IConsoleVariable* Variable)
{
	if (StationRadiusProperty != nullptr && StationRadiusProperty->Value != Variable->GetFloat())
	{
		StationRadiusProperty->Value = Variable->GetFloat();
		// Saves the configuration file
		StationRadiusProperty->MarkDirty();
	}
}

#undef LOCTEXT_NAMESPACE
