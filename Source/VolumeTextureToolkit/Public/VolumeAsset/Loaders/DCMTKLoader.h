// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
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
	static UDCMTKLoader* Get();

	virtual FVolumeInfo ParseVolumeInfoFromHeader(FString FileName) override;

	virtual UVolumeAsset* CreateVolumeFromFile(FString FileName, bool bNormalize = true, bool bConvertToFloat = true) override;

	virtual UVolumeAsset* CreatePersistentVolumeFromFile(
		const FString& FileName, const FString& OutFolder, bool bNormalize = true) override;

	virtual UVolumeAsset* CreateVolumeFromFileInExistingPackage(
		FString FileName, UObject* ParentPackage, bool bNormalize = true, bool bConvertToFloat = true) override;

	virtual uint8* LoadAndConvertData(FString FilePath, FVolumeInfo& VolumeInfo, bool bNormalize, bool bConvertToFloat) override;
};
