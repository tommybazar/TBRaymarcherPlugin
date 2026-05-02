// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "VolumeAsset/Loaders/DCMTKLoader.h"

#include "TextureUtilities.h"

// DCMTK uses their own verify and check macros.
// Also, they include some effed up windows headers which for example include min and max macros for that
// extra bit of _screw you_
#pragma push_macro("verify")
#pragma push_macro("check")
#undef verify
#undef check

#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcpixel.h"

#include <vector>

#pragma pop_macro("verify")
#pragma pop_macro("check")

DEFINE_LOG_CATEGORY(LogDCMTK);

UDCMTKLoader::UDCMTKLoader()
    : IVolumeLoader()
    , bReadSliceThickness(true)
    , bSetSliceThickness(false)
    , bCalculateSliceThickness(false)
    , bVerifySliceThickness(false)
    , bIgnoreIrregularThickness(false)
    , bSetPixelSpacingX(false)
    , bSetPixelSpacingY(false)
{
}

UDCMTKLoader* UDCMTKLoader::Get()
{
    return NewObject<UDCMTKLoader>();
}

int GetSliceNumber(DcmDataset* SliceDataset)
{
    OFString SliceInstanceNumberOfString;
    if (SliceDataset->findAndGetOFString(DCM_InstanceNumber, SliceInstanceNumberOfString).bad())
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error getting Instance Number!"));
        return -1;
    }
    const FString SliceInstanceNumberString = FString(UTF8_TO_TCHAR(SliceInstanceNumberOfString.c_str()));
    return FCString::Atoi(*SliceInstanceNumberString);
}

void Dump(DcmDataset* Dataset)
{
    std::ostringstream out;
    Dataset->print(out);

    const FString ContentString(out.str().c_str());
    UE_LOG(LogDCMTK, Log, TEXT("%s"), *ContentString);
}

void UDCMTKLoader::DumpFileStructure(const FString& FileName)
{
    DcmFileFormat Format;
    if (Format.loadFile(TCHAR_TO_UTF8(*FileName)).bad())
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error loading DICOM image!"));
    }

    DcmDataset* Dataset = Format.getDataset();
    if (Dataset == nullptr)
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error getting DICOM dataset!"));
    }

    Dump(Dataset);
}

FVolumeInfo UDCMTKLoader::ParseVolumeInfoFromHeader(FString FileName)
{
    FVolumeInfo Info;
    Info.DataFileName = FileName;

    DcmFileFormat Format;
    if (Format.loadFile(TCHAR_TO_UTF8(*FileName)).bad())
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error loading DICOM image!"));
        return Info;
    }

    // TODO - Sanity check that this DICOM is even a 2D/3D image

    DcmDataset* Dataset = Format.getDataset();
    OFString SeriesInstanceUIDOfString;
    if (Dataset->findAndGetOFString(DCM_SeriesInstanceUID, SeriesInstanceUIDOfString).bad())
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error getting Series Instance UID!"));
        return Info;
    }

    uint32 NumberOfFrames = 1;
    {
        OFString NumberOfFramesOfString;
        if (Dataset->findAndGetOFString(DCM_NumberOfFrames, NumberOfFramesOfString).good())
        {
            NumberOfFrames = FCString::Atoi(*FString(UTF8_TO_TCHAR(NumberOfFramesOfString.c_str())));
        }

        if (NumberOfFrames == 1)
        {
            FString FolderName, FileNameDummy, Extension;
            FPaths::Split(FileName, FolderName, FileNameDummy, Extension);
            TArray<FString> FilesInDir = GetFilesInFolder(FolderName, Extension);

            NumberOfFrames = 0;
            for (const FString& File : FilesInDir)
            {
                DcmFileFormat SliceDataFormat;
                if (SliceDataFormat.loadFile(TCHAR_TO_UTF8(*(FolderName / File))).bad())
                {
                    continue;
                }

                DcmDataset* SliceDataSet = SliceDataFormat.getDataset();
                OFString FileSeriesInstanceUIDOfString;
                if (SliceDataSet->findAndGetOFString(DCM_SeriesInstanceUID, FileSeriesInstanceUIDOfString).bad())
                {
                    // Series UID not matching -> different image than what we're loading.
                    continue;
                }

                if (FileSeriesInstanceUIDOfString == SeriesInstanceUIDOfString)
                {
                    ++NumberOfFrames;
                    if (int SliceNumber = GetSliceNumber(SliceDataSet); SliceNumber != -1)
                    {
                        Info.UpdateMinMaxSliceNumber(SliceNumber);
                    }
                    else
                    {
                        UE_LOG(LogDCMTK, Error, TEXT("Failed getting slice numbers when reading DICOM folder headers"));
                        return Info;
                    }
                }
            }
        }
        else
        {
            if (bCalculateSliceThickness)
            {
                UE_LOG(LogDCMTK, Error, TEXT("Cannot calculate slice thickness for multi-frame DICOM files!"));
                return Info;
            }

            if (bVerifySliceThickness)
            {
                UE_LOG(LogDCMTK, Error, TEXT("Cannot verify slice thickness for multi-frame DICOM files!"));
                return Info;
            }

            if (bIgnoreIrregularThickness)
            {
                UE_LOG(LogDCMTK, Error, TEXT("Cannot ignore irregular slice thickness for multi-frame DICOM files!"));
                return Info;
            }
        }
    }

    Uint16 Rows = 0, Columns = 0;
    if (Dataset->findAndGetUint16(DCM_Rows, Rows).bad() || Dataset->findAndGetUint16(DCM_Columns, Columns).bad())
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error getting Rows and Columns!"));
        return Info;
    }
    Info.Dimensions = FIntVector(Columns, Rows, NumberOfFrames);

    double PixelSpacingX = DefaultPixelSpacingX, PixelSpacingY = DefaultPixelSpacingY;
    if (!bSetPixelSpacingX || !bSetPixelSpacingY)
    {
        OFString OfPixelSpacingOfString;
        if (Dataset->findAndGetOFString(DCM_PixelSpacing, OfPixelSpacingOfString).bad())
        {
            UE_LOG(LogDCMTK, Error, TEXT("Error getting Pixel Spacing!"));
            return Info;
        }

        int ScanfResult = sscanf(OfPixelSpacingOfString.c_str(), "%lf\\%lf", &PixelSpacingX, &PixelSpacingY);
        if (ScanfResult == 0)
        {
            UE_LOG(LogDCMTK, Error, TEXT("Error parsing Pixel Spacing!"));
            return Info;
        }
        else if (ScanfResult == 1)
        {
            PixelSpacingY = PixelSpacingX;
        }
    }

    double SliceThickness = DefaultSliceThickness;
    if (bReadSliceThickness)
    {
        if (Dataset->findAndGetFloat64(DCM_SliceThickness, SliceThickness).bad())
        {
            UE_LOG(LogDCMTK, Error, TEXT("Error getting Slice Thickness!"));
            return Info;
        }
    }

    Info.Spacing = FVector(PixelSpacingX, PixelSpacingY, SliceThickness);
    Info.WorldDimensions = Info.Spacing * FVector(Info.Dimensions);

    Uint16 BitsAllocated = 0, PixelRepresentation = 0, SamplesPerPixel = 0;
    if (Dataset->findAndGetUint16(DCM_BitsAllocated, BitsAllocated).bad() ||
        Dataset->findAndGetUint16(DCM_PixelRepresentation, PixelRepresentation).bad() ||
        Dataset->findAndGetUint16(DCM_SamplesPerPixel, SamplesPerPixel).bad())
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error getting Pixel Data parameters!"));
        return Info;
    }

    Info.bIsSigned = PixelRepresentation == 1;
    if (SamplesPerPixel == 1)
    {
        switch (BitsAllocated)
        {
            case 8:
                Info.OriginalFormat = Info.bIsSigned ? EVolumeVoxelFormat::SignedChar : EVolumeVoxelFormat::UnsignedChar;
                break;
            case 16:
                Info.OriginalFormat = Info.bIsSigned ? EVolumeVoxelFormat::SignedShort : EVolumeVoxelFormat::UnsignedShort;
                break;
            case 32:
                Info.OriginalFormat = Info.bIsSigned ? EVolumeVoxelFormat::SignedInt : EVolumeVoxelFormat::UnsignedInt;
                break;
            default:
                UE_LOG(LogDCMTK, Error, TEXT("Error getting Bits Allocated!"));
                return Info;
        }

        Info.BytesPerVoxel = BitsAllocated / 8;
    }
    else if (SamplesPerPixel == 3)
    {
        UE_LOG(LogDCMTK, Error, TEXT("RGB DICOM files are not supported!"));
        return Info;
    }
    else
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error getting Samples Per Pixel!"));
        return Info;
    }

    Info.ActualFormat = Info.OriginalFormat;
    Info.bParseWasSuccessful = true;
    Info.bIsCompressed = false;

    return Info;
}

UVolumeAsset* UDCMTKLoader::CreateVolumeFromFile(FString FileName, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
{
    FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
    if (!VolumeInfo.bParseWasSuccessful)
    {
        return nullptr;
    }

    // Get a nice name from the folder we're in to name the asset.
    FString VolumeName;
    GetValidPackageNameFromFolderName(FileName, VolumeName);

    // Create the transient volume asset.
    UVolumeAsset* OutAsset = UVolumeAsset::CreateTransient(VolumeName);
    if (!OutAsset)
    {
        return nullptr;
    }

    // Perform complete load and conversion of data.
    TUniquePtr<uint8[]> LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, bConvertToFloat);

    // Get proper pixel format depending on what got saved into the MHDInfo during conversion.
    const EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

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

UVolumeAsset* UDCMTKLoader::CreatePersistentVolumeFromFile(
    const FString& FileName, const FString& OutFolder, bool bNormalize /*= true*/)
{
    FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
    if (!VolumeInfo.bParseWasSuccessful)
    {
        return nullptr;
    }

    // Get a nice name from the folder we're in to name the asset.
    FString VolumeName;
    GetValidPackageNameFromFolderName(FileName, VolumeName);

    TUniquePtr<uint8[]> LoadedArray(LoadAndConvertData(FileName, VolumeInfo, bNormalize, false));
    if (LoadedArray == nullptr)
    {
        return nullptr;
    }

    // Create the persistent volume asset.
    UVolumeAsset* OutAsset = UVolumeAsset::CreatePersistent(OutFolder, VolumeName);
    if (!OutAsset)
    {
        return nullptr;
    }

    const EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);
    // Create the persistent volume texture.
    const FString VolumeTextureName = "VA_" + VolumeName + "_Data";
    UVolumeTextureToolkit::CreateVolumeTextureAsset(
        OutAsset->DataTexture, VolumeTextureName, OutFolder, PixelFormat, VolumeInfo.Dimensions, LoadedArray.Get(), true);
    OutAsset->ImageInfo = VolumeInfo;

    // Check that the texture got created properly.
    if (OutAsset->DataTexture)
    {
        OutAsset->ImageInfo = VolumeInfo;
        return OutAsset;
    }

    return nullptr;
}

UVolumeAsset* UDCMTKLoader::CreateVolumeFromFileInExistingPackage(
    FString FileName, UObject* ParentPackage, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
{
    FVolumeInfo VolumeInfo = ParseVolumeInfoFromHeader(FileName);
    if (!VolumeInfo.bParseWasSuccessful)
    {
        return nullptr;
    }

    // Get a nice name from the folder we're in to name the asset.
    FString VolumeName;
    GetValidPackageNameFromFolderName(FileName, VolumeName);

    // Create the transient volume asset.
    UVolumeAsset* OutAsset = NewObject<UVolumeAsset>(ParentPackage, FName("VA_" + VolumeName), RF_Standalone | RF_Public);
    if (!OutAsset)
    {
        return nullptr;
    }

    TUniquePtr<uint8[]> LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, bConvertToFloat);
    EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

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

// Data is presumably stored in the first fragment (0th fragment is byte offsets and whatnot) of 0th frame for single frame
// images.
// TODO - test the above assumption thoroughly
bool LoadPixelData(DcmDataset* Dataset, uint8* FrameData, uint64 FrameSize, uint32 FrameIndex, uint32* InOutFragmentIndex)
{
    DcmElement* Element;
    Dataset->findAndGetElement(DCM_PixelData, Element);
    DcmPixelData* DicomPixelData = OFstatic_cast(DcmPixelData*, Element);
    OFString Dummy;
    return DicomPixelData->getUncompressedFrame(Dataset, FrameIndex, *InOutFragmentIndex, FrameData, FrameSize, Dummy).bad();
}

TUniquePtr<uint8[]> LoadMultiFrameDICOM(DcmDataset* Dataset, uint32 NumberOfFrames, const FVolumeInfo& VolumeInfo)
{
    const unsigned long FullDataSize = VolumeInfo.GetByteSize();
    const unsigned long SliceByteSize = VolumeInfo.Dimensions.X * VolumeInfo.Dimensions.Y * VolumeInfo.BytesPerVoxel;

    TUniquePtr<uint8[]> Data(new uint8[FullDataSize]);
    memset(Data.Get(), 0, FullDataSize);

    uint32 FragmentIndex = 1;
    for (uint32 FrameIndex = 0; FrameIndex < NumberOfFrames; ++FrameIndex)
    {
        if (!LoadPixelData(Dataset, Data.Get() + SliceByteSize * FrameIndex, SliceByteSize, FrameIndex, &FragmentIndex))
        {
            UE_LOG(LogDCMTK, Error, TEXT("Error Loading Pixel data from file! Most likely unsupported compression type."));
            return nullptr;
        }
    }

    return Data;
}

/// Prints 100 char values from the PixelData array. Used to roughly check array contents.
void PrintDebugData(const Uint8* PixelData, unsigned long DataLength)
{
    TArray<uint8> DebugData;
    for (unsigned long i = 0; i < DataLength; i += (DataLength / 100))
    {
        DebugData.Add((int8) PixelData[i]);
    }
    const FString DebugString(DebugData.Num(), (ANSICHAR*) DebugData.GetData());
    UE_LOG(LogTemp, Warning, TEXT("Debug data : %ls"), *DebugString);
}

TUniquePtr<uint8[]> LoadSingleFrameDICOMFolder(const FString& FilePath, const OFString& SeriesInstanceUIDOfString,
    FVolumeInfo& VolumeInfo, bool bCalculateSliceThickness, bool bVerifySliceThickness, bool bIgnoreIrregularThickness)
{
    const unsigned long FullDataSize = VolumeInfo.GetByteSize();
    const unsigned long SliceByteSize = VolumeInfo.Dimensions.X * VolumeInfo.Dimensions.Y * VolumeInfo.BytesPerVoxel;

    FString FolderName, FileNameDummy, Extension;
    FPaths::Split(FilePath, FolderName, FileNameDummy, Extension);

    TUniquePtr<uint8[]> FullData(new uint8[FullDataSize]);
    memset(FullData.Get(), 0, FullDataSize);

    TArray<double> SliceLocations;
    SliceLocations.Reserve(VolumeInfo.Dimensions.Z);
    uint32 NumberOfFrames = 0;
    const TArray<FString> FilesInDir = IVolumeLoader::GetFilesInFolder(FolderName, Extension);
    for (const FString& SliceFileName : FilesInDir)
    {
        DcmFileFormat SliceFormat;
        if (SliceFormat.loadFile(TCHAR_TO_UTF8(*(FolderName / SliceFileName))).bad())
        {
            continue;
        }

        DcmDataset* SliceDataset = SliceFormat.getDataset();
        OFString SliceSeriesInstanceUIDOfString;
        if (SliceDataset->findAndGetOFString(DCM_SeriesInstanceUID, SliceSeriesInstanceUIDOfString).bad())
        {
            continue;
        }

        if (SliceSeriesInstanceUIDOfString != SeriesInstanceUIDOfString)
        {
            continue;
        }

        const int SliceNumber = GetSliceNumber(SliceDataset);
        // Slices can be numbered from 0 or 1 (or another, random number?), so always offset from the min slice number instead of 0
        // or 1.
        const int SliceOffset = SliceNumber - VolumeInfo.minSliceNumber;

        if (bCalculateSliceThickness || bVerifySliceThickness)
        {
            double SliceLocation;
            if (SliceDataset->findAndGetFloat64(DCM_SliceLocation, SliceLocation).bad())
            {
                UE_LOG(LogDCMTK, Error, TEXT("Error getting Slice Location!"));
                return nullptr;
            }

            SliceLocations.Add(SliceLocation);
        }

        uint32 FragmentIndex = 1;
        if ((SliceByteSize * (SliceOffset + 1)) > FullDataSize)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("DICOM Loader error when attempting memcpy (SliceNumber * Data exceeds total array length), some data will be "
                     "missing"));
        }
        else if (LoadPixelData(SliceDataset, FullData.Get() + SliceByteSize * SliceOffset, SliceByteSize, 0, &FragmentIndex))
        {
            UE_LOG(
                LogDCMTK, Error, TEXT("Error Loading Pixel data from file! JPEG2000 - compressed files require custom licensing."));
            return nullptr;
        }

        ++NumberOfFrames;
    }

    if (bCalculateSliceThickness || bVerifySliceThickness)
    {
        SliceLocations.Sort();
        check(SliceLocations.Num() > 2);

        constexpr static double Tolerance = 0.0001;
        double CalculatedSliceThickness = FMath::Abs(SliceLocations[1] - SliceLocations[0]);
        double PreviousSliceLocation = SliceLocations[1];
        for (int32 i = 2; i < SliceLocations.Num(); ++i)
        {
            const double CurrentSliceLocation = SliceLocations[i];
            const double NewCalculatedSliceThickness = FMath::Abs(CurrentSliceLocation - PreviousSliceLocation);
            if (FMath::Abs(NewCalculatedSliceThickness - CalculatedSliceThickness) > Tolerance)
            {
                if (!bIgnoreIrregularThickness)
                {
                    UE_LOG(LogDCMTK, Error, TEXT("Computed slice thickness varies across the dataset! %f != %f"),
                        CalculatedSliceThickness, NewCalculatedSliceThickness);
                    return nullptr;
                }
                UE_LOG(LogDCMTK, Warning, TEXT("Computed slice thickness varies across the dataset! %f != %f"),
                    CalculatedSliceThickness, NewCalculatedSliceThickness);
            }
            PreviousSliceLocation = CurrentSliceLocation;
            CalculatedSliceThickness = NewCalculatedSliceThickness;
        }

        if (FMath::Abs(VolumeInfo.Spacing.Z - CalculatedSliceThickness) > Tolerance)
        {
            if (bVerifySliceThickness)
            {
                UE_LOG(LogDCMTK, Error, TEXT("Calculated slice thickness %f is different from the one in the header %f"),
                    CalculatedSliceThickness, VolumeInfo.Spacing.Z);
                return nullptr;
            }

            VolumeInfo.Spacing.Z = CalculatedSliceThickness;
            VolumeInfo.WorldDimensions = VolumeInfo.Spacing * FVector(VolumeInfo.Dimensions);
        }
    }

    return FullData;
}

TUniquePtr<uint8[]> UDCMTKLoader::LoadAndConvertData(
    FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat)
{
    DcmFileFormat Format;
    if (Format.loadFile(TCHAR_TO_UTF8(*FilePath)).bad())
    {
        UE_LOG(LogDCMTK, Error, TEXT("Error loading DICOM image!"));
        return nullptr;
    }

    DcmDataset* Dataset = Format.getDataset();

    int32 NumberOfFrames = 1;
    OFString NumberOfFramesOfString;
    if (Dataset->findAndGetOFString(DCM_NumberOfFrames, NumberOfFramesOfString).good())
    {
        NumberOfFrames = FCString::Atoi(*FString(UTF8_TO_TCHAR(NumberOfFramesOfString.c_str())));
    }

    TUniquePtr<uint8[]> Data;
    if (NumberOfFrames > 1)
    {
        Data = LoadMultiFrameDICOM(Dataset, NumberOfFrames, VolumeInfo);
    }
    else
    {
        OFString SeriesInstanceUIDOfString;
        if (Dataset->findAndGetOFString(DCM_SeriesInstanceUID, SeriesInstanceUIDOfString).bad())
        {
            UE_LOG(LogDCMTK, Error, TEXT("Error getting Series Instance UID!"));
            return nullptr;
        }

        Data = LoadSingleFrameDICOMFolder(FilePath, SeriesInstanceUIDOfString, VolumeInfo, bCalculateSliceThickness,
            bVerifySliceThickness, bIgnoreIrregularThickness);
    }

    if (Data != nullptr)
    {
        Data = ConvertData(MoveTemp(Data), VolumeInfo, bNormalize, bConvertToFloat);
    }

    return Data;
}