// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "VolumeAssetFactory.h"

#include "Containers/UnrealString.h"
#include "Engine/VolumeTexture.h"
#include "Misc/MessageDialog.h"
#include "VolumeAsset/Loaders/MHDLoader.h"
#include "VolumeAsset/VolumeAsset.h"
#include "VolumeAsset/Loaders/DICOMLoader.h"

/* UMHDVolumeTextureFactory structors
 *****************************************************************************/

UVolumeAssetFactory::UVolumeAssetFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Formats.Add(FString(TEXT("mhd;")) + NSLOCTEXT("UMHDVolumeTextureFactory", "FormatMhd", ".mhd File").ToString());
	Formats.Add(FString(TEXT("dcm;")) + NSLOCTEXT("UMHDVolumeTextureFactory", "FormatDicom", ".dcm File").ToString());

	SupportedClass = UVolumeAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	ImportPriority = 1;
}

#pragma optimize("", off)
UObject* UVolumeAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	IVolumeLoader* Loader = nullptr;

	FString FileNamePart, FolderPart, ExtensionPart;
	FPaths::Split(Filename, FolderPart, FileNamePart, ExtensionPart);
	if (ExtensionPart.Equals(TEXT("mhd")))
	{
		Loader = UMHDLoader::Get();
	}
	else
	{
		Loader = UDICOMLoader::Get();
	}

	FVolumeInfo Info = Loader->ParseVolumeInfoFromHeader(Filename);
	if (!Info.bParseWasSuccessful)
	{
		return nullptr;
	}

	bool bNormalize = false;
	bool bConvertToFloat = false;

	EAppReturnType::Type DialogAnswer = FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::Yes,
		NSLOCTEXT("Volumetrics", "Normalize?",
			"Would you like your volume converted to G8 or G16 and normalized to the whole type range? This will allow it to "
			"be saved persistently as an asset and make inspecting it with Texture editor easier. Also, rendering with the "
			"default raymarching material and transfer function will be easier.\nIf your volume already is MET_(U)CHAR or "
			"MET_(U)SHORT, your volume will be persistent even without conversion, but values might be all over the place."));

	if (DialogAnswer == EAppReturnType::Yes)
	{
		bNormalize = true;
	}
	else if (Info.OriginalFormat != EVolumeVoxelFormat::Float)
	{
		EAppReturnType::Type DialogAnswer2 = FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::Yes,
			NSLOCTEXT("Volumetrics", "Convert to R32?",
				"Should we convert it to R32_FLOAT? This will make sure the default materials can read it, but will make the "
				"texture un-saveable."));
		if (DialogAnswer2 == EAppReturnType::Yes)
		{
			bConvertToFloat = true;
		}
	}

	UVolumeAsset* OutVolume = Loader->CreateVolumeFromFileInExistingPackage(Filename, InParent, bNormalize, bConvertToFloat);
	bOutOperationCanceled = false;

	// Add created MHD file to AdditionalImportedObjects so it also gets saved in-editor.
	UVolumeTexture*& VolumeTexture = OutVolume->DataTexture;
	AdditionalImportedObjects.Add(VolumeTexture);

	return OutVolume;
}
#pragma optimize("", on)
