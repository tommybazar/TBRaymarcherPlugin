// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "VolumeAsset/VolumeAsset.h"
#include "VolumeAsset/VolumeInfo.h"

#include "VolumeLoader.generated.h"

UINTERFACE(BlueprintType)
class VOLUMETEXTURETOOLKIT_API UVolumeLoader : public UInterface
{
    GENERATED_BODY()
};

DECLARE_LOG_CATEGORY_EXTERN(LogVolumeLoader, Log, All);

/// Interface for all volume loaders.
/// Implement this to make classes that can create UVolumeAssets from arbitrary volumetric data formats.
/// See MHDLoader.h/cpp for an implementation example.
class VOLUMETEXTURETOOLKIT_API IVolumeLoader
{
    GENERATED_BODY()
public:
    // Returns a FVolumeInfo without actually creating a volume from the file. Useful for getting info about a volume before loading
    // it.
    virtual FVolumeInfo ParseVolumeInfoFromHeader(FString FileName) = 0;

    // Creates a full volume asset from the provided data file.
    virtual UVolumeAsset* CreateVolumeFromFile(FString FileName, bool bNormalize = true, bool bConvertToFloat = true) = 0;

    // Creates a full persistent volume asset from the provided data file.
    virtual UVolumeAsset* CreatePersistentVolumeFromFile(
        const FString& FileName, const FString& OutFolder, bool bNormalize = true) = 0;

    // Creates a full volume asset from the provided FileName. Gets saved into the ParentPackage package. Used in File Factory
    // calls.
    virtual UVolumeAsset* CreateVolumeFromFileInExistingPackage(
        FString FileName, UObject* ParentPackage, bool bNormalize = true, bool bConvertToFloat = true) = 0;

    // Loads the raw bytes from the file specified in Info. Detects if file is compressed and loads returns a new uint8 array.
    // Don't forget to delete[] after using.
    static TUniquePtr<uint8[]> LoadRawDataFileFromInfo(const FString& FilePath, const FVolumeInfo& Info);

    // Tries to read the provided FileName as a file either in absolute path or relative to game folder.
    static FString ReadFileAsString(const FString& FileName);

    // Gets all files in the specified directory. Extension is optional, if provided, will only return files with the extension.
    static TArray<FString> GetFilesInFolder(FString Directory, FString Extension);

    // Takes a filename or a path and returns a valid package name.
    // Eg. "/user/somebody/img0.0.1.mhd"
    // OutPackageName = "img0_0_1" and OutFilePath = "/user/somebody/"
    static void GetValidPackageNameFromFileName(const FString& FullPath, FString& OutFilePath, FString& OutPackageName);

    // Takes a full path and returns a valid package name that's equivalent to the lowest level folder.
    // Eg. "/user/somebody.big/00000.dcm"
    // OutPackageName = "somebody_big"
    void GetValidPackageNameFromFolderName(const FString& FullPath, FString& OutPackageName);

    // Loads the raw data specified in the VolumeInfo and converts it so that it's useable with our raymarching materials.
    // This means either converting it to U8 or U16 and normalizing or a conversion to Float.
    virtual TUniquePtr<uint8[]> LoadAndConvertData(
        FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat);

    // Converts raw data read from a Volume file so that it's useable by our materials.
    // if bNormalize is true, the data gets normalized to 0.0 to 1.0 range and gets saved as a G8 or G16 texture later in the
    // process. if bConvertToFloat is true, the data gets converted to float and gets saved as a R32_Float texture later in the
    // process.
    static TUniquePtr<uint8[]> ConvertData(
        TUniquePtr<uint8[]>&& LoadedArray, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat);
};