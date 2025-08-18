// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEJackAudioLink : ModuleRules
{
	public UEJackAudioLink(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"Slate",
				"SlateCore"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Engine",
				"CoreUObject",
				"DeveloperSettings",
				"Settings"
				// ... add private dependencies that you statically link with here ...	
			}
			);

		// Optional JACK SDK integration toggle. Default OFF to avoid requiring headers/libs at build time.
		PublicDefinitions.Add("WITH_JACK=0");

		// If developer defines JACK_SDK_ROOT, try to enable WITH_JACK and add includes/libs
		string JackRoot = System.Environment.GetEnvironmentVariable("JACK_SDK_ROOT");
		bool bJackLinked = false;
		if (!string.IsNullOrEmpty(JackRoot))
		{
			string Inc = System.IO.Path.Combine(JackRoot, "include");
			string LibDir = System.IO.Path.Combine(JackRoot, "lib");
			string LibName = (Target.Platform == UnrealTargetPlatform.Win64) ? "libjack64.lib" : "jack.lib";
			string LibPath = System.IO.Path.Combine(LibDir, LibName);
			if (System.IO.Directory.Exists(Inc) && System.IO.File.Exists(LibPath))
			{
				PublicIncludePaths.Add(Inc);
				PublicAdditionalLibraries.Add(LibPath);
				bJackLinked = true;
			}
		}

		// Windows convenience: try common JACK2 install dir if env var not set
		if (!bJackLinked && Target.Platform == UnrealTargetPlatform.Win64)
		{
			string DefaultRoot = @"C:\\Program Files\\JACK2";
			string Inc = System.IO.Path.Combine(DefaultRoot, "include");
			string LibPath = System.IO.Path.Combine(DefaultRoot, "lib", "libjack64.lib");
			if (System.IO.Directory.Exists(Inc) && System.IO.File.Exists(LibPath))
			{
				PublicIncludePaths.Add(Inc);
				PublicAdditionalLibraries.Add(LibPath);
				bJackLinked = true;
			}
		}

		if (bJackLinked)
		{
			PublicDefinitions.Remove("WITH_JACK=0");
			PublicDefinitions.Add("WITH_JACK=1");
		}
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelEditor",
					"EditorStyle",
					"WorkspaceMenuStructure"
				});
		}
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
