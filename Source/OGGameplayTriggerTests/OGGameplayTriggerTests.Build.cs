using UnrealBuildTool;

public class OGGameplayTriggerTests : ModuleRules
{
    public OGGameplayTriggerTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "GameplayTags",
                "OGGameplayTrigger",
                "UnrealEd",
            }
        );
    }
}