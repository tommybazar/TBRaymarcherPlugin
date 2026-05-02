// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).
#pragma once

#include "VolumeLoader.h"

#include "MHDLoader.generated.h"
/**
 * IVolumeLoader specialized for reading MHD files. (https://itk.org/Wiki/ITK/MetaIO/Documentation)
 */
UCLASS()
class VOLUMETEXTURETOOLKIT_API UMHDLoader : public UObject, public IVolumeLoader
{
    GENERATED_BODY()
public:
    // Getter for a dummy non-static object. Useful to have non-static so all loaders can use the same interface with virtual
    // methods.
    static UMHDLoader* Get();

    // Returns a FVolumeInfo without actually creating a volume from the file. Useful for getting info about a volume before loading
    // it.
    virtual FVolumeInfo ParseVolumeInfoFromHeader(FString FileName) override;

    // Creates a full transient volume asset from the provided data file.
    virtual UVolumeAsset* CreateVolumeFromFile(FString FileName, bool bNormalize = true, bool bConvertToFloat = true) override;

    // Creates a full persistent volume asset from the provided data file.
    virtual UVolumeAsset* CreatePersistentVolumeFromFile(
        const FString& FileName, const FString& OutFolder, bool bNormalize = true) override;

    // Creates a full volume asset from the provided FileName. Gets saved into the ParentPackage package. Used in File Factory
    // calls.
    virtual UVolumeAsset* CreateVolumeFromFileInExistingPackage(
        FString FileName, UObject* ParentPackage, bool bNormalize = true, bool bConvertToFloat = true) override;
};