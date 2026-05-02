// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "VolumeTextureToolkitBPLibrary.h"

#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "TextureUtilities.h"
#include "VolumeAsset/Loaders/DCMTKLoader.h"
#include "VolumeAsset/Loaders/MHDLoader.h"
#include "VolumeAsset/VolumeAsset.h"

bool UVolumeTextureToolkitBPLibrary::CreateVolumeTextureAsset(UVolumeTexture*& OutTexture, FString AssetName, FString FolderName,
    EPixelFormat PixelFormat, FIntVector Dimensions, bool bUAVTargettable /*= false*/)
{
    return UVolumeTextureToolkit::CreateVolumeTextureAsset(
        OutTexture, AssetName, FolderName, PixelFormat, Dimensions, nullptr, true, true);
}

UVolumeAsset* UVolumeTextureToolkitBPLibrary::LoadVolumeFromFileDialog(const bool& bNormalize)
{
    // Get best window for file picker dialog.
    TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(TSharedPtr<SWindow>());
    const void* ParentWindowHandle = (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
                                         ? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
                                         : nullptr;

    TArray<FString> FileNames;
    // Open the file picker for Volume files.
    bool Success = FDesktopPlatformModule::Get()->OpenFileDialog(
        ParentWindowHandle, "Select volumetric file", "", "", ".mhd;.dcm", 0, FileNames);
    if (FileNames.Num() > 0)
    {
        FString FileName = FileNames[0];
        IVolumeLoader* Loader = nullptr;
        if (FileName.EndsWith(".mhd"))
        {
            Loader = UMHDLoader::Get();
        }
        else
        {
            Loader = UDCMTKLoader::Get();
        }
        UVolumeAsset* OutAsset = Loader->CreateVolumeFromFile(FileName, bNormalize, !bNormalize);

        if (OutAsset)
        {
            UE_LOG(LogTemp, Display,
                TEXT("Creating Volume asset from filename %s succeeded, seting Volume asset into associated listener volumes."),
                *FileName);

            // Add the asset to list of already loaded assets and select it through the combobox. This will call
            // OnAssetSelected().
            return OutAsset;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Creating Volume asset from filename %s failed."), *FileName);
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Loading of Volume file cancelled. Dialog creation failed or no file was selected."));
    }
    return nullptr;
}
