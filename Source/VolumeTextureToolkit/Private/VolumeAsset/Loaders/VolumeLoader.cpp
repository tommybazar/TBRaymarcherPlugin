// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "VolumeAsset/Loaders/VolumeLoader.h"

#include "TextureUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY(LogVolumeLoader)

uint8* IVolumeLoader::LoadRawDataFileFromInfo(const FString& FilePath, const FVolumeInfo& Info)
{
	if (Info.bIsCompressed)
	{
		// #TODO potentially implement support for other compression formats.
		return UVolumeTextureToolkit::LoadZLibCompressedFileIntoArray(
			FilePath + "/" + Info.DataFileName, Info.GetByteSize(), Info.CompressedByteSize);
	}
	else
	{
		return UVolumeTextureToolkit::LoadRawFileIntoArray(FilePath + "/" + Info.DataFileName, Info.GetByteSize());
	}
}

FString IVolumeLoader::ReadFileAsString(const FString& FileName)
{
	FString FileContent;
	// First, try to read FileName as absolute path
	if (FFileHelper::LoadFileToString(/*out*/ FileContent, *FileName))
	{
		return FileContent;
	}

	// If that didn't work, try it as a relative path.
	FString RelativePath = FPaths::ProjectContentDir();
	FString FullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + FileName;
	if (FFileHelper::LoadFileToString(/*out*/ FileContent, *FullPath))
	{
		return FileContent;
	}
	UE_LOG(LogVolumeLoader, Error, TEXT("Cannot read file path %s either as absolute or as relative path."), *FileName);
	return "";
}


TArray<FString> IVolumeLoader::GetFilesInFolder(FString Directory, FString Extension)
{
	TArray<FString> OutPut;
	OutPut.Empty();
	if (FPaths::DirectoryExists(Directory))
	{
		FFileManagerGeneric::Get().FindFiles(OutPut, *Directory, *Extension);
	}
	return OutPut;
}

void IVolumeLoader::GetValidPackageNameFromFileName(const FString& FullPath, FString& OutFilePath, FString& OutPackageName)
{
	FString ExtensionPart;

	FPaths::Split(FullPath, OutFilePath, OutPackageName, ExtensionPart);
	OutPackageName = FPaths::MakeValidFileName(OutPackageName);
	// Periods are not cool in package names -> replace with underscores.
	OutPackageName.ReplaceCharInline('.', '_');
}

void IVolumeLoader::GetValidPackageNameFromFolderName(const FString& FullPath, FString& OutPackageName)
{
	FString NamePart, ExtensionPart;

	FPaths::Split(FullPath, OutPackageName, NamePart, ExtensionPart);

	// Get the name of the folder we're in.
	int32 LastSlash = OutPackageName.Find("\\", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	OutPackageName.RightChopInline(LastSlash);
	OutPackageName = FPaths::MakeValidFileName(OutPackageName);
	// Periods are not cool in package names -> replace with underscores.
	OutPackageName.ReplaceCharInline('.', '_');

}

uint8* IVolumeLoader::LoadAndConvertData(FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat)
{
	// Load raw data.
	uint8* LoadedArray = IVolumeLoader::LoadRawDataFileFromInfo(FilePath, VolumeInfo);
	LoadedArray = IVolumeLoader::ConvertData(LoadedArray, VolumeInfo, bNormalize, bConvertToFloat);
	return LoadedArray;
}

uint8* IVolumeLoader::ConvertData(uint8* LoadedArray, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat)
{
	VolumeInfo.bIsNormalized = bNormalize;
	if (bNormalize)
	{
		// We want to normalize and cap at G16, perform that normalization.
		uint8* ConvertedArray = UVolumeTextureToolkit::NormalizeArrayByFormat(
			VolumeInfo.OriginalFormat, LoadedArray, VolumeInfo.GetByteSize(), VolumeInfo.MinValue, VolumeInfo.MaxValue);
		delete[] LoadedArray;
		LoadedArray = ConvertedArray;

		if (VolumeInfo.BytesPerVoxel > 1)
		{
			VolumeInfo.BytesPerVoxel = 2;
			VolumeInfo.ActualFormat = EVolumeVoxelFormat::UnsignedShort;
		}
		else
		{
			VolumeInfo.ActualFormat = EVolumeVoxelFormat::UnsignedChar;
		}
	}
	else if (bConvertToFloat && VolumeInfo.OriginalFormat != EVolumeVoxelFormat::Float)
	{
		float* ConvertedArray =
			UVolumeTextureToolkit::ConvertArrayToFloat(VolumeInfo.OriginalFormat, LoadedArray, VolumeInfo.GetTotalVoxels());
		delete[] LoadedArray;
		LoadedArray = (uint8*) ConvertedArray;
		VolumeInfo.ActualFormat = EVolumeVoxelFormat::Float;
	}
	else
	{
		VolumeInfo.ActualFormat = VolumeInfo.OriginalFormat;
	}
	return LoadedArray;
}
