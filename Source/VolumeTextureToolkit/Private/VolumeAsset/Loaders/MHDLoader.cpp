// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "VolumeAsset/Loaders/MHDLoader.h"

#include "TextureUtilities.h"
#include "sstream"
#include "string"

UMHDLoader* UMHDLoader::Get()
{
    // Maybe get a singleton going on here?
    return NewObject<UMHDLoader>();
}

FVolumeInfo UMHDLoader::ParseVolumeInfoFromHeader(FString FileName)
{
    // #TODO UE probably has a nicer string parser than istringstream...
    // And the way I'm doing this is the ugliest you could imagine.
    // But hey, this is probably literally the first C++ code I ever wrote in Unreal, so I'm keeping it this way, so
    // I can look at it and shed a tear of remembering the sweet, sweet days of yesteryear.

    FString FileString = IVolumeLoader::ReadFileAsString(FileName);

    FVolumeInfo OutVolumeInfo;
    OutVolumeInfo.bParseWasSuccessful = false;

    // #TODO stop being sentimental and use FConsole::Parse()
    {
        std::string MyStdString(TCHAR_TO_UTF8(*FileString));
        std::istringstream inStream = std::istringstream(MyStdString);

        std::string ReadWord;

        // Skip until we get to Dimensions.
        while (inStream.good() && ReadWord != "DimSize")
        {
            inStream >> ReadWord;
        }
        // Should be at the "=" after DimSize now.
        if (inStream.good())
        {
            // Get rid of equal sign.
            inStream >> ReadWord;
            // Read the three values;
            inStream >> OutVolumeInfo.Dimensions.X;
            inStream >> OutVolumeInfo.Dimensions.Y;
            inStream >> OutVolumeInfo.Dimensions.Z;
        }
        else
        {
            return OutVolumeInfo;
        }

        // Go back to beginning
        inStream = std::istringstream(MyStdString);
        // Skip until we get to spacing.
        while (inStream.good() && ReadWord != "ElementSpacing" && ReadWord != "ElementSize")
        {
            inStream >> ReadWord;
        }
        // Should be at the "=" after ElementSpacing/ElementSize now.
        if (inStream.good())
        {
            // Get rid of equal sign.
            inStream >> ReadWord;
            // Read the three values;
            inStream >> OutVolumeInfo.Spacing.X;
            inStream >> OutVolumeInfo.Spacing.Y;
            inStream >> OutVolumeInfo.Spacing.Z;

            OutVolumeInfo.WorldDimensions = OutVolumeInfo.Spacing * FVector(OutVolumeInfo.Dimensions);
        }
        else
        {
            return OutVolumeInfo;
        }

        // Go back to beginning
        inStream = std::istringstream(MyStdString);
        // Skip until we get to ElementType
        while (inStream.good() && ReadWord != "ElementType")
        {
            inStream >> ReadWord;
        }
        // Should be at the "=" after ElementType now.
        if (inStream.good())
        {
            // Get rid of equal sign.
            inStream >> ReadWord;

            inStream >> ReadWord;
            if (ReadWord == "MET_UCHAR")
            {
                OutVolumeInfo.OriginalFormat = EVolumeVoxelFormat::UnsignedChar;
            }
            else if (ReadWord == "MET_CHAR")
            {
                OutVolumeInfo.OriginalFormat = EVolumeVoxelFormat::SignedChar;
            }

            else if (ReadWord == "MET_USHORT")
            {
                OutVolumeInfo.OriginalFormat = EVolumeVoxelFormat::UnsignedShort;
            }
            else if (ReadWord == "MET_SHORT")
            {
                OutVolumeInfo.OriginalFormat = EVolumeVoxelFormat::SignedShort;
            }
            else if (ReadWord == "MET_UINT")
            {
                OutVolumeInfo.OriginalFormat = EVolumeVoxelFormat::UnsignedInt;
            }
            else if (ReadWord == "MET_INT")
            {
                OutVolumeInfo.OriginalFormat = EVolumeVoxelFormat::SignedInt;
            }
            else if (ReadWord == "MET_FLOAT")
            {
                OutVolumeInfo.OriginalFormat = EVolumeVoxelFormat::Float;
            }
            else
            {
                return OutVolumeInfo;
            }
        }
        else
        {
            return OutVolumeInfo;
        }

        OutVolumeInfo.BytesPerVoxel = FVolumeInfo::VoxelFormatByteSize(OutVolumeInfo.OriginalFormat);
        OutVolumeInfo.bIsSigned = FVolumeInfo::IsVoxelFormatSigned(OutVolumeInfo.OriginalFormat);

        // Check for compressed data size tag.

        // Go back to beginning
        inStream = std::istringstream(MyStdString);
        // Skip until we get to ElementType
        while (inStream.good() && ReadWord != "CompressedDataSize")
        {
            inStream >> ReadWord;
        }
        // Should be at the "=" after ElementType now.
        if (inStream.good())
        {
            OutVolumeInfo.bIsCompressed = true;

            // Get rid of equal sign.
            inStream >> ReadWord;

            inStream >> OutVolumeInfo.CompressedByteSize;
        }

        // Go back to beginning
        inStream = std::istringstream(MyStdString);
        // Skip until we get to ElementType
        while (inStream.good() && ReadWord != "ElementDataFile")
        {
            inStream >> ReadWord;
        }
        // Should be at the "=" after ElementType now.
        if (inStream.good())
        {
            // Get rid of equal sign.
            inStream >> ReadWord;

            inStream >> ReadWord;
        }
        else
        {
            return OutVolumeInfo;
        }
        OutVolumeInfo.DataFileName = FString(ReadWord.c_str());
        OutVolumeInfo.bParseWasSuccessful = true;
        // Return with constructor that sets success to true.
        return OutVolumeInfo;
    }
}

UVolumeAsset* UMHDLoader::CreateVolumeFromFile(FString FileName, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
{
    FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
    if (!VolumeInfo.bParseWasSuccessful)
    {
        return nullptr;
    }
    // Get valid package name and filepath.
    FString FilePath, VolumeName;
    GetValidPackageNameFromFileName(FileName, FilePath, VolumeName);

    // Create the transient volume asset.
    UVolumeAsset* OutAsset = UVolumeAsset::CreateTransient(VolumeName);
    if (!OutAsset)
    {
        return nullptr;
    }

    // Perform complete load and conversion of data.
    TUniquePtr<uint8[]> LoadedArray = LoadAndConvertData(FilePath, VolumeInfo, bNormalize, bConvertToFloat);

    // Get proper pixel format depending on what got saved into the MHDInfo during conversion.
    EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

    // Create the transient Volume texture.
    UVolumeTextureToolkit::CreateVolumeTextureTransient(
        OutAsset->DataTexture, PixelFormat, VolumeInfo.Dimensions, LoadedArray.Get());

    // Check that the texture got created properly.
    if (OutAsset->DataTexture)
    {
        OutAsset->ImageInfo = VolumeInfo;
        return OutAsset;
    }
    else
    {
        return nullptr;
    }
}

UVolumeAsset* UMHDLoader::CreatePersistentVolumeFromFile(
    const FString& FileName, const FString& OutFolder, bool bNormalize /*= true*/)
{
    FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
    if (!VolumeInfo.bParseWasSuccessful)
    {
        return nullptr;
    }
    // Get valid package name and filepath.
    FString FilePath, VolumeName;
    GetValidPackageNameFromFileName(FileName, FilePath, VolumeName);

    // Create persistent volume asset.
    UVolumeAsset* OutAsset = UVolumeAsset::CreatePersistent(OutFolder, VolumeName);
    if (!OutAsset)
    {
        return nullptr;
    }

    TUniquePtr<uint8[]> LoadedArray = LoadAndConvertData(FilePath, VolumeInfo, bNormalize, false);
    EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

    // Create the persistent volume texture.
    FString VolumeTextureName = "VA_" + VolumeName + "_Data";
    UVolumeTextureToolkit::CreateVolumeTextureAsset(
        OutAsset->DataTexture, VolumeTextureName, OutFolder, PixelFormat, VolumeInfo.Dimensions, LoadedArray.Get(), true);
    OutAsset->ImageInfo = VolumeInfo;

    // Check that the texture got created properly.
    if (OutAsset->DataTexture)
    {
        OutAsset->ImageInfo = VolumeInfo;
        return OutAsset;
    }
    else
    {
        return nullptr;
    }
}

UVolumeAsset* UMHDLoader::CreateVolumeFromFileInExistingPackage(
    FString FileName, UObject* ParentPackage, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
{
    FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
    if (!VolumeInfo.bParseWasSuccessful)
    {
        return nullptr;
    }
    // Get valid package name and filepath.
    FString FilePath, VolumeName;
    GetValidPackageNameFromFileName(FileName, FilePath, VolumeName);

    // Create the transient volume asset.
    UVolumeAsset* OutAsset = NewObject<UVolumeAsset>(ParentPackage, FName("VA_" + VolumeName), RF_Standalone | RF_Public);
    if (!OutAsset)
    {
        return nullptr;
    }

    // Perform complete load and conversion of data.
    TUniquePtr<uint8[]> LoadedArray = LoadAndConvertData(FilePath, VolumeInfo, bNormalize, bConvertToFloat);

    // Get proper pixel format depending on what got saved into the MHDInfo during conversion.
    EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

    // Create the transient Volume texture.
    OutAsset->DataTexture =
        NewObject<UVolumeTexture>(ParentPackage, FName("VA_" + VolumeName + "_Data"), RF_Public | RF_Standalone);

    UVolumeTextureToolkit::SetupVolumeTexture(
        OutAsset->DataTexture, PixelFormat, VolumeInfo.Dimensions, LoadedArray.Get(), !bConvertToFloat);

    // Check that the texture got created properly.
    if (OutAsset->DataTexture)
    {
        OutAsset->ImageInfo = VolumeInfo;
        return OutAsset;
    }
    else
    {
        return nullptr;
    }
}
