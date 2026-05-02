// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "VolumeTextureToolkitBPLibrary.generated.h"

/*
 *	Blueprint function library for Volume Texture utilities.
 */
UCLASS()
class VOLUMETEXTURETOOLKIT_API UVolumeTextureToolkitBPLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Creates a Volume Texture asset with the given name, pixel format and
      dimensions and fills it with zeros.*/
    UFUNCTION(BlueprintCallable, meta = (Keywords = "VolumeTextureToolkit sample test testing"), Category = "VolumeTextureToolkit")
    static bool CreateVolumeTextureAsset(UVolumeTexture*& OutTexture, FString AssetName, FString FolderName,
        EPixelFormat PixelFormat, FIntVector Dimensions, bool bUAVTargettable = false);

    /** Pops up a file dialog prompting the user to select a file to load a volume from. Loads the volume with the appropriate
     * IVolumeLoader.*/
    UFUNCTION(BlueprintCallable, meta = (Keywords = "Load Volume DICOM MHD"), Category = "VolumeTextureToolkit")
    static UVolumeAsset* LoadVolumeFromFileDialog(const bool& bNormalize);
};
