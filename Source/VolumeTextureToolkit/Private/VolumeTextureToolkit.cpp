// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "VolumeTextureToolkit.h"

#define LOCTEXT_NAMESPACE "FVolumeTextureToolkitModule"

void FVolumeTextureToolkitModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString PluginShaderDir = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("TBRaymarcherPlugin"), TEXT("Source"), TEXT("VolumeTextureToolkit"), TEXT("Shaders"));
	// This creates an alias "Raymarcher" for the folder of our shaders, which can be used when calling IMPLEMENT_GLOBAL_SHADER to
	// find our shaders.
	AddShaderSourceDirectoryMapping(TEXT("/VolumeTextureToolkit"), PluginShaderDir);
}

void FVolumeTextureToolkitModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVolumeTextureToolkitModule, VolumeTextureToolkit)