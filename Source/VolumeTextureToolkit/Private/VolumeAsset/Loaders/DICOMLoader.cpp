// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).
#include "VolumeAsset/Loaders/DICOMLoader.h"

UDICOMLoader* UDICOMLoader::Get()
{
	return NewObject<UDICOMLoader>();
}

FVolumeInfo UDICOMLoader::ParseVolumeInfoFromHeader(FString FileName)
{
	return FVolumeInfo();
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
	return nullptr;
}
