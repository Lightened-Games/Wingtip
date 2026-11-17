// Six Inches - game module.

using UnrealBuildTool;

public class SixInches : ModuleRules
{
	public SixInches(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"EngineCameras",
			"Json",
			"DeveloperSettings",
			"RenderCore",
			"RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "AudioMixer", "AudioExtensions" });

		// Editor-only helpers used by the content scripts (fonts, BlockingVolume brushes, mesh sockets).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
		}

		PublicIncludePaths.Add("SixInches");
	}
}
