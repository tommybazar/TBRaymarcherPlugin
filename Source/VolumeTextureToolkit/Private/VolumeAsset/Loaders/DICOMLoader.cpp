// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).
#include "VolumeAsset/Loaders/DICOMLoader.h"

#include "DesktopPlatform/Public/DesktopPlatformModule.h"
#include "HAL/FileManagerGeneric.h"
#include "VolumeAsset/DICOMParser/DICOMAppHelper.h"
#include "VolumeAsset/DICOMParser/DICOMParser.h"

#pragma optimize("", off)

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
	return nullptr;
}

UVolumeAsset* UDICOMLoader::CreatePersistentVolumeFromFile(
	const FString& FileName, const FString& OutFolder, bool bNormalize /*= true*/)
{
	return nullptr;
}

UVolumeAsset* UDICOMLoader::CreateVolumeFromFileInExistingPackage(
	FString FileName, UObject* ParentPackage, bool bNormalize /*= true*/, bool bConvertToFloat /*= true*/)
{
	FVolumeInfo Info = ParseVolumeInfoFromHeader(FileName);

	// Get a nice name from the folder we're in to name the asset.
	FString VolumeName;
	GetValidPackageNameFromFolderName(FileName, VolumeName);

	uint8* TotalArray = LoadAndConvertData(FileName, Info, bNormalize, bConvertToFloat);

	EPixelFormat PixelFormat = FVolumeInfo::VoxelFormatToPixelFormat(Info.ActualFormat);

	// Create the VolumeAsset and underlying volume texture.
	UVolumeAsset* OutAsset = NewObject<UVolumeAsset>(ParentPackage, FName("VA_" + VolumeName), RF_Standalone | RF_Public);
	OutAsset->DataTexture =
		NewObject<UVolumeTexture>(ParentPackage, FName("VA_" + VolumeName + "_Data"), RF_Public | RF_Standalone);
	
	UVolumeTextureToolkit::SetupVolumeTexture(OutAsset->DataTexture, PixelFormat, Info.Dimensions, TotalArray, !bConvertToFloat);

	delete[] TotalArray;

	if (!OutAsset || !OutAsset->DataTexture)
	{
		return nullptr;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Loading of DICOM file succeeded. Output Dims = %s."), *(Info.Dimensions.ToString()));
		OutAsset->ImageInfo = Info;
		return OutAsset;
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
		memcpy(TotalArray + (DataLength * SliceNumber), DataArray, DataLength);

		Parser.CloseFile();
	}

	// Make sure slice thickness is correct.
	std::vector<std::pair<float, std::string>> SliceLocations;
	ParserHelper.GetSliceLocationFilenamePairs(SliceLocations, true);
	if (SliceLocations.size() > 1)
	{
		VolumeInfo.Spacing.Z = abs(SliceLocations[1].first - SliceLocations[0].first);
		VolumeInfo.WorldDimensions = VolumeInfo.Spacing * FVector(VolumeInfo.Dimensions);
	}

	TotalArray = IVolumeLoader::ConvertData(TotalArray, VolumeInfo, bNormalize, bConvertToFloat);
	return TotalArray;
}

void AMofoCallback()
{
}

#pragma optimize("", on)
