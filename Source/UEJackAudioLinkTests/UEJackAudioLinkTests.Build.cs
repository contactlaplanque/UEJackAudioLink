using UnrealBuildTool;

public class UEJackAudioLinkTests : ModuleRules
{
    public UEJackAudioLinkTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "UEJackAudioLink"      // we want to test the runtime module
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "AutomationController",  // helpers for tests
            "UnrealEd",             // for automation framework
            "ToolMenus"             // required for automation
        });
    }
}