// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "VolumeAssetFactory.h"

#include "Containers/UnrealString.h"
#include "Engine/VolumeTexture.h"
#include "Misc/MessageDialog.h"
#include "Runtime/Slate/Public/Framework/Notifications/NotificationManager.h"
#include "Runtime/Slate/Public/Widgets/Notifications/SNotificationList.h"
#include "VolumeAsset/Loaders/DCMTKLoader.h"
#include "VolumeAsset/Loaders/MHDLoader.h"
#include "VolumeAsset/VolumeAsset.h"
#include "VolumeImporter.h"

/* UMHDVolumeTextureFactory structors
 *****************************************************************************/

UVolumeAssetFactory::UVolumeAssetFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Formats.Add(FString(TEXT(";")) + NSLOCTEXT("UMHDVolumeTextureFactory", "FormatAny", "No Extension File").ToString());
	Formats.Add(FString(TEXT("mhd;")) + NSLOCTEXT("UMHDVolumeTextureFactory", "FormatMhd", ".mhd File").ToString());
	Formats.Add(FString(TEXT("dcm;")) + NSLOCTEXT("UMHDVolumeTextureFactory", "FormatDicom", ".dcm File").ToString());

	SupportedClass = UVolumeAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	ImportPriority = DefaultImportPriority - 10;
}

#pragma optimize("", off)
UObject* UVolumeAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	TSharedPtr<SVolumeImporterWindow> VolumeImporterWindow;
	TSharedRef<SWindow> Window =
		SNew(SWindow)
			.Title(NSLOCTEXT("VolumeAssetFactory", "VolumeImportTitle", "Volume Import"))
			.SizingRule(ESizingRule::Autosized)
			.SupportsMaximize(false)
			.SupportsMinimize(false)[SAssignNew(VolumeImporterWindow, SVolumeImporterWindow).WidgetWindow(&Window.Get())];

	FString FileNamePart, FolderPart, ExtensionPart;
	FPaths::Split(Filename, FolderPart, FileNamePart, ExtensionPart);
	if (ExtensionPart.Equals(TEXT("mhd")))
	{
		VolumeImporterWindow->LoaderType = EVolumeImporterLoaderType::MHD;
	}
	else
	{
		VolumeImporterWindow->LoaderType = EVolumeImporterLoaderType::DICOM;
	}

	FSlateApplication::Get().AddModalWindow(Window, nullptr, false);

	if (VolumeImporterWindow->bCancelled)
	{
		bOutOperationCanceled = true;
		return nullptr;
	}

	IVolumeLoader* Loader = nullptr;
	if (VolumeImporterWindow->LoaderType == EVolumeImporterLoaderType::MHD)
	{
		Loader = UMHDLoader::Get();
	}
	else
	{
		UDCMTKLoader* DCMTKLoader = UDCMTKLoader::Get();
		DCMTKLoader->bReadSliceThickness = VolumeImporterWindow->ThicknessOperation == EVolumeImporterThicknessOperation::Read;
		DCMTKLoader->bSetSliceThickness = VolumeImporterWindow->ThicknessOperation == EVolumeImporterThicknessOperation::Set;
		DCMTKLoader->DefaultSliceThickness = VolumeImporterWindow->SliceThickness;
		DCMTKLoader->bCalculateSliceThickness = VolumeImporterWindow->ThicknessOperation == EVolumeImporterThicknessOperation::Calculate;
		DCMTKLoader->bVerifySliceThickness = VolumeImporterWindow->GetVerifySliceThickness();
		DCMTKLoader->bIgnoreIrregularThickness = VolumeImporterWindow->GetIgnoreIrregularThickness();
		DCMTKLoader->bSetPixelSpacingX = VolumeImporterWindow->bSetPixelSpacingX;
		DCMTKLoader->DefaultPixelSpacingX = VolumeImporterWindow->PixelSpacingX;
		DCMTKLoader->bSetPixelSpacingY = VolumeImporterWindow->bSetPixelSpacingY;
		DCMTKLoader->DefaultPixelSpacingY = VolumeImporterWindow->PixelSpacingY;

		if (VolumeImporterWindow->bDumpDicom)
		{
			UDCMTKLoader::DumpFileStructure(Filename);
		}

		Loader = DCMTKLoader;
	}

	FVolumeInfo Info = Loader->ParseVolumeInfoFromHeader(Filename);
	UVolumeAsset* OutVolume = nullptr;
	if (Info.bParseWasSuccessful)
	{
		FString FullPath = InParent->GetName();
		FString AssetName;
		FString FolderName;
		Loader->GetValidPackageNameFromFileName(FullPath, FolderName, AssetName);

		OutVolume = Loader->CreatePersistentVolumeFromFile(Filename, FolderName, VolumeImporterWindow->GetNormalize());
		if (OutVolume)
		{
			UVolumeTexture*& VolumeTexture = OutVolume->DataTexture;
			AdditionalImportedObjects.Add(VolumeTexture);
		}
	}

	if (OutVolume == nullptr || !Info.bParseWasSuccessful)
	{
		FNotificationInfo Notification(NSLOCTEXT("VolumeAssetFactory", "VolumeImportFailed", "Volume import failed!"));
		Notification.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.ErrorWithColor.Large"));
		Notification.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Notification);
		return nullptr;
	}
	else
	{
		FNotificationInfo Notification(NSLOCTEXT("VolumeAssetFactory", "VolumeImportSuccess", "Volume import succeeded!"));
		Notification.Image = FCoreStyle::Get().GetBrush(TEXT("Icons.SuccessWithColor.Large"));
		Notification.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Notification);
	}

	return OutVolume;
}
#pragma optimize("", on)
