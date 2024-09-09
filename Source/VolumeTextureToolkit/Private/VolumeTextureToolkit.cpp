// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "VolumeTextureToolkit.h"

#define LOCTEXT_NAMESPACE "FVolumeTextureToolkitModule"

// DCMTK uses their own verify and check macros.
#pragma push_macro("verify")
#pragma push_macro("check")
#undef verify
#undef check

#include "dcmtk/dcmdata/dcrledrg.h"
#include "dcmtk/dcmjpeg/djdecode.h"
#include "dcmtk/dcmjpls/djdecode.h"

#pragma pop_macro("verify")
#pragma pop_macro("check")


void FVolumeTextureToolkitModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString PluginShaderDir = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("TBRaymarcherPlugin"), TEXT("Source"), TEXT("VolumeTextureToolkit"), TEXT("Shaders"));
	// This creates an alias "Raymarcher" for the folder of our shaders, which can be used when calling IMPLEMENT_GLOBAL_SHADER to
	// find our shaders.
	AddShaderSourceDirectoryMapping(TEXT("/VolumeTextureToolkit"), PluginShaderDir);

	const FString DllBasePath = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("TBRaymarcherPlugin/Source/VolumeTextureToolkit/ThirdParty/dcmtk/Bin/Win64"));

	// Note that order matters here (dependencies go first)
	const FString DCMTKDlls[] = {"oficonv.dll", "ofstd.dll", "oflog.dll", "dcmdata.dll", "ijg8.dll", "ijg12.dll", "ijg16.dll",
		"dcmimgle.dll", "dcmimage.dll", "dcmjpeg.dll", "dcmtkcharls.dll", "dcmjpls.dll"};
	for (const FString& DllName : DCMTKDlls)
	{
		const FString DllPath = FPaths::Combine(DllBasePath, DllName);
		void* DllHandle = FPlatformProcess::GetDllHandle(*DllPath);
		verify(DllHandle != NULL);
		DllHandles.Add(DllHandle);
	}

	// Register codecs for DICOM decompression so that they are available throughout app lifetime
	DcmRLEDecoderRegistration::registerCodecs();
	DJDecoderRegistration::registerCodecs();
	DJLSDecoderRegistration::registerCodecs();

}

void FVolumeTextureToolkitModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	DcmRLEDecoderRegistration::cleanup();
	DJDecoderRegistration::cleanup();
	DJLSDecoderRegistration::cleanup();

	for (void* DllHandle : DllHandles)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
	}
	
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVolumeTextureToolkitModule, VolumeTextureToolkit)