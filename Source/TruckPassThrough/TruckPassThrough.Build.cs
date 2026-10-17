using UnrealBuildTool;

public class TruckPassThrough : ModuleRules
{
	public TruckPassThrough(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		// FactoryGame transitive dependencies needed by the vehicle headers we include
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject",
			"Engine",
			"DeveloperSettings",
			"PhysicsCore",
			"InputCore",
			"EnhancedInput",
			"ChaosVehiclesCore", "ChaosVehicles", "ChaosSolverEngine",
			"GeometryCollectionEngine",
			"NetCore",
			"GameplayTags",
			"SlateCore", "Slate", "UMG",
		});

		// Header stubs
		PublicDependencyModuleNames.AddRange(new string[] {
			"DummyHeaders",
		});

		PublicDependencyModuleNames.AddRange(new string[] {"FactoryGame", "SML"});
	}
}
