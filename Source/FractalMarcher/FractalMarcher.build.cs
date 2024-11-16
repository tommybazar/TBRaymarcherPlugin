// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

using UnrealBuildTool;

public class FractalMarcher : ModuleRules
{
	public FractalMarcher(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		OptimizeCode = CodeOptimization.InShippingBuildsOnly;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
				"Engine",
				"RHI",
				"AssetRegistry",
				"VolumeTextureToolkit",
				"InputCore",
				"Media",
				"MediaUtils",
				"MediaAssets"
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
			}
		);
	}
}