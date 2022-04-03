// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).
#pragma once

#include "VolumeLoader.h"

#include "DICOMLoader.generated.h"
/**
 * IVolumeLoader specialized for reading DICOM files.
 */
UCLASS()
class VOLUMETEXTURETOOLKIT_API UDICOMLoader : public UObject, public IVolumeLoader
{
	GENERATED_BODY()
public:
	static UDICOMLoader* Get();

	virtual FVolumeInfo ParseVolumeInfoFromHeader(FString FileName) override;

	// Creates a full volume asset from the provided data file.
	virtual UVolumeAsset* CreateVolumeFromFile(FString FileName, bool bNormalize = true, bool bConvertToFloat = true);

	// Creates a full persistent volume asset from the provided data file.
	virtual UVolumeAsset* CreatePersistentVolumeFromFile(const FString& FileName, const FString& OutFolder, bool bNormalize = true);

	virtual UVolumeAsset* CreateVolumeFromFileInExistingPackage(
		FString FileName, UObject* ParentPackage, bool bNormalize = true, bool bConvertToFloat = true);

	virtual uint8* LoadAndConvertData(FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat);
};