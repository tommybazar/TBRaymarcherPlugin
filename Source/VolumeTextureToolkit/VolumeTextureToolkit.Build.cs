// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

using UnrealBuildTool;

public class VolumeTextureToolkit : ModuleRules
{
	public VolumeTextureToolkit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");

		CppStandard = CppStandardVersion.Cpp17;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
				"RHI",
				"AssetRegistry",
				"Engine"
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

		string BinPath = System.IO.Path.Combine(ModuleDirectory, "ThirdParty/dcmtk", "bin", Target.Platform.ToString());
		string LibPath = System.IO.Path.Combine(ModuleDirectory, "ThirdParty/dcmtk", "lib", Target.Platform.ToString());
		string IncludePath = System.IO.Path.Combine(ModuleDirectory, "ThirdParty/dcmtk", "include");

		string[] DLLs = {
			"oficonv.dll", "ofstd.dll", "oflog.dll", "dcmdata.dll", "ijg8.dll", "ijg12.dll", "ijg16.dll",
			"dcmimgle.dll", "dcmimage.dll", "dcmjpeg.dll", "dcmtkcharls.dll", "dcmjpls.dll"
		};
		string[] Libs =
		{
			"oficonv.lib", "ofstd.lib", "oflog.lib", "dcmdata.lib", "ijg8.lib", "ijg12.lib", "ijg16.lib",
			"dcmimgle.lib", "dcmimage.lib", "dcmjpeg.lib", "dcmtkcharls.lib", "dcmjpls.lib"
		};

		PrivateIncludePaths.Add(IncludePath);

		foreach (string DLL in DLLs)
		{
			string FullPath = System.IO.Path.Combine(BinPath, DLL);
			PublicDelayLoadDLLs.Add(DLL);
			RuntimeDependencies.Add(FullPath);
		}

		foreach (string Lib in Libs)
		{
			string FullPath = System.IO.Path.Combine(LibPath, Lib);
			PublicAdditionalLibraries.Add(FullPath);
		}
	}
}