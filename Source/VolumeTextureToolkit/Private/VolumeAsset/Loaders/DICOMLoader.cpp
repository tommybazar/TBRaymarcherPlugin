// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).
#include "VolumeAsset/Loaders/DICOMLoader.h"

#include "TextureUtilities.h"
#include "HAL/FileManagerGeneric.h"
#include "VolumeAsset/DICOMParser/DICOMAppHelper.h"
#include "VolumeAsset/DICOMParser/DICOMParser.h"

// #pragma optimize("", off)

UDICOMLoader* UDICOMLoader::Get()
{
	return NewObject<UDICOMLoader>();
}

FVolumeInfo UDICOMLoader::ParseVolumeInfoFromHeader(FString FileName)
{
	FVolumeInfo Info;

	Info.DataFileName = FileName;

	FString FolderName, FileNameDummy, Extension;
	FPaths::Split(FileName, FolderName, FileNameDummy, Extension);

	// Get all files in the folder (series).
	TArray<FString> FilesInDir = GetFilesInFolder(FolderName, Extension);

	DICOMParser Parser;
	const std::string FileNameStd = std::string(TCHAR_TO_UTF8(*FileName));

	Parser.OpenFile(FileNameStd);
	DICOMAppHelper ParserHelper;
	ParserHelper.RegisterCallbacks(&Parser);
	ParserHelper.RegisterPixelDataCallback(&Parser);
	Parser.ReadHeader();

	if (!Parser.IsDICOMFile())
	{
		return Info;
	}

	// Get dimensions of DICOM file.
	int* Dimensions = ParserHelper.GetDimensions();
	FIntVector Dims = FIntVector(Dimensions[0], Dimensions[1], FilesInDir.Num());
	Info.Dimensions = Dims;
	// Get spacing.
	float* Spacing = ParserHelper.GetPixelSpacing();
	Info.Spacing = FVector(Spacing[0], Spacing[1], Spacing[2]);
	//	Info.Spacing.Z = S
	// Calculate world size.
	Info.WorldDimensions = Info.Spacing * FVector(Info.Dimensions);

	void* DataArray;
	DICOMParser::VRTypes DataType;
	unsigned long DataLength;

	ParserHelper.GetImageData(DataArray, DataType, DataLength);

	switch (DataType)
	{
		case DICOMParser::VR_Float:
			Info.OriginalFormat = EVolumeVoxelFormat::Float;
			Info.bIsSigned = true;
			Info.BytesPerVoxel = 4;
			break;
		case DICOMParser::VR_SignedLong:
			Info.OriginalFormat = EVolumeVoxelFormat::SignedInt;
			Info.bIsSigned = true;
			Info.BytesPerVoxel = 4;
			break;
		case DICOMParser::VR_UnsignedLong:
			Info.OriginalFormat = EVolumeVoxelFormat::UnsignedInt;
			Info.bIsSigned = false;
			Info.BytesPerVoxel = 4;
			break;
		case DICOMParser::VR_UnsignedShort:
			Info.OriginalFormat = EVolumeVoxelFormat::UnsignedShort;
			Info.bIsSigned = false;
			Info.BytesPerVoxel = 2;
			break;
		case DICOMParser::VR_SignedShort:
		case DICOMParser::VR_WordString:
			Info.OriginalFormat = EVolumeVoxelFormat::SignedShort;
			Info.bIsSigned = true;
			Info.BytesPerVoxel = 2;
			break;
		case DICOMParser::VR_Double:
			UE_LOG(LogTemp, Error, TEXT("Loading of DICOM file failed. Double format is not supported!"));
			return Info;
		default:
			UE_LOG(LogTemp, Error, TEXT("Loading of DICOM file failed. Unknown pixel format!"));
			return Info;
	}

	unsigned long TotalDataSize = DataLength * FilesInDir.Num();
	ensure(TotalDataSize == Info.GetByteSize());

	Info.ActualFormat = Info.OriginalFormat;
	Info.bParseWasSuccessful = true;
	Info.bIsCompressed = false;

	ParserHelper.Clear();
	Parser.CloseFile();

	return Info;
}

UVolumeAsset* UDICOMLoader::CreateVolumeFromFile(FString FileName, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
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

UVolumeAsset* UDICOMLoader::CreatePersistentVolumeFromFile(
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
	// Create the persistent volume asset.
	UVolumeAsset* OutAsset = UVolumeAsset::CreatePersistent(OutFolder, VolumeName);
	if (!OutAsset)
	{
		return nullptr;
	}

	uint8* LoadedArray = LoadAndConvertData(FileName, VolumeInfo, bNormalize, false);
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
	else
	{
		return nullptr;
	}
}

UVolumeAsset* UDICOMLoader::CreateVolumeFromFileInExistingPackage(
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

uint8* UDICOMLoader::LoadAndConvertData(FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat)
{
	unsigned long TotalDataSize = VolumeInfo.GetByteSize();

	FString FolderName, FileNameDummy, Extension;
	FPaths::Split(FilePath, FolderName, FileNameDummy, Extension);

	// Get all files in the folder (series).
	TArray<FString> FilesInDir = GetFilesInFolder(FolderName, Extension);

	DICOMParser Parser;
	DICOMAppHelper ParserHelper;
	ParserHelper.RegisterCallbacks(&Parser);
	ParserHelper.RegisterPixelDataCallback(&Parser);

	uint8* TotalArray = new uint8[TotalDataSize];
	memset(TotalArray, 0, TotalDataSize);

	void* DataArray;
	DICOMParser::VRTypes DataType;
	unsigned long DataLength;

	for (FString SliceName : FilesInDir)
	{
		const std::string SliceNameStd = std::string(TCHAR_TO_UTF8(*(FolderName / SliceName)));
		Parser.OpenFile(SliceNameStd);
		Parser.ReadHeader();

		ParserHelper.GetImageData(DataArray, DataType, DataLength);
		int SliceNumber = ParserHelper.GetSliceNumber();

		// Copy to our giant array at the right spot for this slice.
		// Increase TotalArray ptr by DataLength * SliceNumber to get to the start of the curren't slices memory.
		if ((DataLength * SliceNumber) < TotalDataSize)
		{
			memcpy(TotalArray + (DataLength * SliceNumber), DataArray, DataLength);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("DICOM Loader error during memcpy, some data might be missing"));
		}

		Parser.CloseFile();
	}

	// Make sure slice thickness is correct.
	std::vector<std::pair<float, std::string>> SliceLocations;
	ParserHelper.GetSliceLocationFilenamePairs(SliceLocations, true);
	if (SliceLocations.size() > 1)
	{
		if (abs(SliceLocations[1].first - SliceLocations[0].first) > 0)
		{
			VolumeInfo.Spacing.Z = abs(SliceLocations[1].first - SliceLocations[0].first);
		}
		VolumeInfo.WorldDimensions = VolumeInfo.Spacing * FVector(VolumeInfo.Dimensions);
	}

	TotalArray = IVolumeLoader::ConvertData(TotalArray, VolumeInfo, bNormalize, bConvertToFloat);
	return TotalArray;
}

// #pragma optimize("", off)
