// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FractalMarcher : ModuleRules
{
	public FractalMarcher(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp17;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
				"Engine",
				"RHI",
				"AssetRegistry",
				"VolumeTextureToolkit",
				"InputCore"
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