// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "VolumeAsset/Loaders/VolumeLoader.h"

#include "DCMTKLoader.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDCMTK, Log, All);

/**
 * IVolumeLoader specialized for reading DICOM files using the DCMTK Toolkit.
 */
UCLASS()
class VOLUMETEXTURETOOLKIT_API UDCMTKLoader : public UObject, public IVolumeLoader
{
    GENERATED_BODY()
public:
    /// Read the slice thickness from the DICOM file. Default is true.
    bool bReadSliceThickness : 1;

    /// Set the slice thickness in the volume info ignoring the values of the DICOM file. Default is false.
    bool bSetSliceThickness : 1;

    /// Calculate the slice thickness from the slice location. Default is false.
    bool bCalculateSliceThickness : 1;

    /// Verify the slice thickness by measurements from the slice location. Default is true.
    bool bVerifySliceThickness : 1;

    /// Ignore irregular slice thickness in the slice thickness calculation pass. Default is false.
    bool bIgnoreIrregularThickness : 1;

    /// Set the pixel spacing in the volume info ignoring the values of the DICOM file. Default is false.
    bool bSetPixelSpacingX : 1;

    /// Set the pixel spacing in the volume info ignoring the values of the DICOM file. Default is false.
    bool bSetPixelSpacingY : 1;

    /// The distance between pixels in mm in the x direction. Default is 1.0f.
    float DefaultPixelSpacingX = 1.0f;

    /// The distance between pixels in mm in the y direction. Default is 1.0f.
    float DefaultPixelSpacingY = 1.0f;

    /// The distance between slices in mm used when the bSetSliceThickness flag is set. Default is 1.0f.
    float DefaultSliceThickness = 1.0f;

    UDCMTKLoader();

    static UDCMTKLoader* Get();

    virtual FVolumeInfo ParseVolumeInfoFromHeader(FString FileName) override;

    virtual UVolumeAsset* CreateVolumeFromFile(FString FileName, bool bNormalize = true, bool bConvertToFloat = true) override;

    virtual UVolumeAsset* CreatePersistentVolumeFromFile(
        const FString& FileName, const FString& OutFolder, bool bNormalize = true) override;

    virtual UVolumeAsset* CreateVolumeFromFileInExistingPackage(
        FString FileName, UObject* ParentPackage, bool bNormalize = true, bool bConvertToFloat = true) override;

    virtual TUniquePtr<uint8[]> LoadAndConvertData(
        FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat) override;

    static void DumpFileStructure(const FString& FileName);
};
