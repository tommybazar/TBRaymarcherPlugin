// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
#include "VolumeAsset/Loaders/DCMTKLoader.h"

#include "TextureUtilities.h"

#pragma push_macro("verify")
#pragma push_macro("check")
#undef verify
#undef check

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dctk.h"

#pragma pop_macro("verify")
#pragma pop_macro("check")

#pragma optimize("", off)

DEFINE_LOG_CATEGORY(LogDCMTK);

UDCMTKLoader::UDCMTKLoader()
	: IVolumeLoader()
	, bReadSliceThickness(true)
	, bSetSliceThickness(false)
	, bCalculateSliceThickness(false)
	, bVerifySliceThickness(false)
	, bIgnoreIrregularThickness(false)
	, bReadPixelSpacing(true)
	, bSetPixelSpacingX(false)
	, bSetPixelSpacingY(false)
{
}

UDCMTKLoader* UDCMTKLoader::Get()
{
	return NewObject<UDCMTKLoader>();
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

	DcmDataset* Dataset = Format.getDataset();
	OFString SeriesInstanceUID;
	if (Dataset->findAndGetOFString(DCM_SeriesInstanceUID, SeriesInstanceUID).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error getting Series Instance UID!"));
		return Info;
	}

	uint32 NumberOfSlices = 1;
	{
		OFString NumberOfFramesString;
		if (Dataset->findAndGetOFString(DCM_NumberOfFrames, NumberOfFramesString).good())
		{
			NumberOfSlices = FCString::Atoi(*FString(UTF8_TO_TCHAR(NumberOfFramesString.c_str())));
		}

		if (NumberOfSlices == 1)
		{
			FString FolderName, FileNameDummy, Extension;
			FPaths::Split(FileName, FolderName, FileNameDummy, Extension);
			TArray<FString> FilesInDir = GetFilesInFolder(FolderName, Extension);

			NumberOfSlices = 0;
			for (const FString& File : FilesInDir)
			{
				DcmFileFormat FileFormat;
				if (FileFormat.loadFile(TCHAR_TO_UTF8(*(FolderName / File))).bad())
				{
					continue;
				}

				DcmDataset* FileDataset = FileFormat.getDataset();
				OFString FileSeriesInstanceUID;
				if (FileDataset->findAndGetOFString(DCM_SeriesInstanceUID, FileSeriesInstanceUID).bad())
				{
					continue;
				}

				if (FileSeriesInstanceUID == SeriesInstanceUID)
				{
					++NumberOfSlices;
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
	Info.Dimensions = FIntVector(Columns, Rows, NumberOfSlices);

	double PixelSpacingX = DefaultPixelSpacingX, PixelSpacingY = DefaultPixelSpacingY;
	if (!bSetPixelSpacingX || !bSetPixelSpacingY)
	{
		OFString OfPixelSpacingStr;
		if (Dataset->findAndGetOFString(DCM_PixelSpacing, OfPixelSpacingStr).bad())
		{
			UE_LOG(LogDCMTK, Error, TEXT("Error getting Pixel Spacing!"));
			return Info;
		}

		int ScanfResult = sscanf(OfPixelSpacingStr.c_str(), "%lf\\%lf", &PixelSpacingX, &PixelSpacingY);
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
	uint8* LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, bConvertToFloat);

	// Get proper pixel format depending on what got saved into the MHDInfo during conversion.
	const EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

	// Create the transient Volume texture.
	UVolumeTextureToolkit::CreateVolumeTextureTransient(OutAsset->DataTexture, PixelFormat, VolumeInfo.Dimensions, LoadedArray);

	delete[] LoadedArray;

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
	
	uint8* LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, false);
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
		OutAsset->DataTexture, VolumeTextureName, OutFolder, PixelFormat, VolumeInfo.Dimensions, LoadedArray, true);
	OutAsset->ImageInfo = VolumeInfo;

	delete[] LoadedArray;
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

	uint8* LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, bConvertToFloat);
	EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(VolumeInfo.ActualFormat);

	OutAsset->DataTexture =
		NewObject<UVolumeTexture>(ParentPackage, FName("VA_" + VolumeName + "_Data"), RF_Public | RF_Standalone);

	UVolumeTextureToolkit::SetupVolumeTexture(
		OutAsset->DataTexture, PixelFormat, VolumeInfo.Dimensions, LoadedArray, !bConvertToFloat);

	delete[] LoadedArray;

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

uint8* LoadMultiFrameDICOM(DcmDataset* Dataset, uint32 NumberOfSlices, uint32 DataSize)
{
	const Uint8* PixelData;
	unsigned long DataLength;
	Dataset->findAndGetUint8Array(DCM_PixelData, PixelData, &DataLength);

	if (DataLength != DataSize)
	{
		UE_LOG(LogDCMTK, Error, TEXT("DICOM Loader error, PixelData size %d is different from the expected size %d"), DataLength,
			DataSize);
		return nullptr;
	}

	uint8* Data = new uint8[DataSize];
	memcpy(Data, PixelData, DataLength);

	return Data;
}

uint8* LoadSingleFrameDICOMFolder(const FString& FilePath, const OFString& SeriesInstanceUID, FVolumeInfo& VolumeInfo,
	bool bCalculateSliceThickness, bool bVerifySliceThickness, bool bIgnoreIrregularThickness)
{
	unsigned long TotalDataSize = VolumeInfo.GetByteSize();

	FString FolderName, FileNameDummy, Extension;
	FPaths::Split(FilePath, FolderName, FileNameDummy, Extension);

	uint8* TotalArray = new uint8[TotalDataSize];
	memset(TotalArray, 0, TotalDataSize);

	TArray<double> SliceLocations;
	SliceLocations.Reserve(VolumeInfo.Dimensions.Z);
	uint32 NumberOfSlices = 0;
	const TArray<FString> FilesInDir = IVolumeLoader::GetFilesInFolder(FolderName, Extension);
	for (const FString& SliceFileName : FilesInDir)
	{
		DcmFileFormat SliceFormat;
		if (SliceFormat.loadFile(TCHAR_TO_UTF8(*(FolderName / SliceFileName))).bad())
		{
			continue;
		}

		DcmDataset* SliceDataset = SliceFormat.getDataset();
		OFString SliceSeriesInstanceUID;
		if (SliceDataset->findAndGetOFString(DCM_SeriesInstanceUID, SliceSeriesInstanceUID).bad())
		{
			continue;
		}

		if (SliceSeriesInstanceUID != SeriesInstanceUID)
		{
			continue;
		}

		OFString SliceInstanceNumberOfString;
		if (SliceDataset->findAndGetOFString(DCM_InstanceNumber, SliceInstanceNumberOfString).bad())
		{
			UE_LOG(LogDCMTK, Error, TEXT("Error getting Instance Number!"));
			delete[] TotalArray;
			return nullptr;
		}

		const FString SliceInstanceNumberString = FString(UTF8_TO_TCHAR(SliceInstanceNumberOfString.c_str()));
		const int SliceNumber = FCString::Atoi(*SliceInstanceNumberString) - 1;

		if (bCalculateSliceThickness || bVerifySliceThickness)
		{
			double SliceLocation;
			if (SliceDataset->findAndGetFloat64(DCM_SliceLocation, SliceLocation).bad())
			{
				UE_LOG(LogDCMTK, Error, TEXT("Error getting Slice Location!"));
				delete[] TotalArray;
				return nullptr;
			}

			SliceLocations.Add(SliceLocation);
		}

		const Uint8* PixelData;
		unsigned long DataLength;
		SliceDataset->findAndGetUint8Array(DCM_PixelData, PixelData, &DataLength);

		if ((DataLength * (SliceNumber + 1)) <= TotalDataSize)
		{
			memcpy(TotalArray + (DataLength * SliceNumber), PixelData, DataLength);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DICOM Loader error when attempting memcpy (SliceNumber * Data exceeds total array length), some data will be "
					 "missing"));
		}

		++NumberOfSlices;
	}

	if (bCalculateSliceThickness || bVerifySliceThickness)
	{
		SliceLocations.Sort();
		check(SliceLocations.Num() > 2);

		constexpr static const double Tolerance = 0.0001;
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
					delete[] TotalArray;
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
				delete[] TotalArray;
				return nullptr;
			}

			VolumeInfo.Spacing.Z = CalculatedSliceThickness;
			VolumeInfo.WorldDimensions = VolumeInfo.Spacing * FVector(VolumeInfo.Dimensions);
		}
	}

	return TotalArray;
}

uint8* UDCMTKLoader::LoadAndConvertData(FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat)
{
	DcmFileFormat Format;
	if (Format.loadFile(TCHAR_TO_UTF8(*FilePath)).bad())
	{
		UE_LOG(LogDCMTK, Error, TEXT("Error loading DICOM image!"));
		return nullptr;
	}

	DcmDataset* Dataset = Format.getDataset();

	int32 NumberOfFrames = 1;
	OFString NumberOfFramesString;
	if (Dataset->findAndGetOFString(DCM_NumberOfFrames, NumberOfFramesString).good())
	{
		NumberOfFrames = FCString::Atoi(*FString(UTF8_TO_TCHAR(NumberOfFramesString.c_str())));
	}

	uint8* Data;
	if (NumberOfFrames > 1)
	{
		Data = LoadMultiFrameDICOM(Dataset, NumberOfFrames, VolumeInfo.GetByteSize());
	}
	else
	{
		OFString SeriesInstanceUID;
		if (Dataset->findAndGetOFString(DCM_SeriesInstanceUID, SeriesInstanceUID).bad())
		{
			UE_LOG(LogDCMTK, Error, TEXT("Error getting Series Instance UID!"));
			return nullptr;
		}

		Data = LoadSingleFrameDICOMFolder(
			FilePath, SeriesInstanceUID, VolumeInfo, bCalculateSliceThickness, bVerifySliceThickness, bIgnoreIrregularThickness);
	}

	if (Data != nullptr)
	{
		Data = ConvertData(Data, VolumeInfo, bNormalize, bConvertToFloat);
	}

	return Data;
}

#pragma optimize("", on)
