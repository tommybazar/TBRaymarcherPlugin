// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureEditorCustom : ModuleRules
{
	public TextureEditorCustom(ReadOnlyTargetRules Target) : base(Target)
	{
		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"WorkspaceMenuStructure"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"UnrealEd",
                "PropertyEditor"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
                "PropertyEditor",
				"MediaUtils",
				"EditorWidgets",
				"MediaAssets",
				"DerivedDataCache",
				"DeveloperToolSettings"
			}
		);
	}
}
