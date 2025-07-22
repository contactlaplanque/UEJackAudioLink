using UnrealBuildTool;

public class UEJackAudioLinkTests : ModuleRules
{
    public UEJackAudioLinkTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "UEJackAudioLink"      // we want to test the runtime module
        });

        PrivateDependencyModuleNames.Add("AutomationController"); // helpers for tests
    }
}