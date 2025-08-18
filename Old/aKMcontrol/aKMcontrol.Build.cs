// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class aKMcontrol : ModuleRules
{
	public aKMcontrol(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
		
		PrivateDependencyModuleNames.AddRange(new string[] { "ImGui" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true

		// Jack Audio Library Configuration
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add Jack include directory
			PublicIncludePaths.Add("C:/Program Files/JACK2/include");
			
			// Add Jack library directory
			PublicAdditionalLibraries.Add("C:/Program Files/JACK2/lib/libjack64.lib");
			
		}
	}
}
