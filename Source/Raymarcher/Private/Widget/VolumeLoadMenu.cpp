// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "Widget/VolumeLoadMenu.h"

#include "VolumeTextureToolkitBPLibrary.h"

DEFINE_LOG_CATEGORY(VolumeLoadMenu)

bool UVolumeLoadMenu::Initialize()
{
	Super::Initialize();
	if (LoadG16Button)
	{
		LoadG16Button->OnClicked.Clear();
		LoadG16Button->OnClicked.AddDynamic(this, &UVolumeLoadMenu::OnLoadNormalizedClicked);
	}

	if (LoadF32Button)
	{
		LoadF32Button->OnClicked.Clear();
		LoadF32Button->OnClicked.AddDynamic(this, &UVolumeLoadMenu::OnLoadF32Clicked);
	}

	if (AssetSelectionComboBox)
	{
		// Add existing Volume files into box.
		AssetSelectionComboBox->ClearOptions();
		AssetSelectionComboBox->OnSelectionChanged.Clear();

		for (UVolumeAsset* VolumeAsset : AssetArray)
		{
			AssetSelectionComboBox->AddOption(GetNameSafe(VolumeAsset));
		}

		AssetSelectionComboBox->OnSelectionChanged.AddDynamic(this, &UVolumeLoadMenu::OnAssetSelected);
	}

	return true;
}

void UVolumeLoadMenu::OnLoadNormalizedClicked()
{
	PerformLoad(true);
}

void UVolumeLoadMenu::OnLoadF32Clicked()
{
	PerformLoad(false);
}

void UVolumeLoadMenu::PerformLoad(bool bNormalized)
{
	if (ListenerVolumes.Num() > 0)
	{
		UVolumeAsset* OutAsset = UVolumeTextureToolkitBPLibrary::LoadVolumeFromFileDialog(bNormalized);

		if (OutAsset)
		{
			// Add the asset to list of already loaded assets and select it through the combobox. This will call
			// OnAssetSelected().
			AssetArray.Add(OutAsset);
			AssetSelectionComboBox->AddOption(GetNameSafe(OutAsset));
			AssetSelectionComboBox->SetSelectedOption(GetNameSafe(OutAsset));
		}
		else
		{
			UE_LOG(VolumeLoadMenu, Error, TEXT("Loading Volume From file dialog failed"));
		}
	}
	else
	{
		UE_LOG(
			VolumeLoadMenu, Error, TEXT("Attempted to load Volume file with no Raymarched Volume associated with menu."));
	}
}

void UVolumeLoadMenu::OnAssetSelected(FString AssetName, ESelectInfo::Type SelectType)
{
	UVolumeAsset* SelectedAsset = nullptr;
	for (UVolumeAsset* Asset : AssetArray)
	{
		if (AssetName.Equals(GetNameSafe(Asset)))
		{
			SelectedAsset = Asset;
			break;
		}
	}
	if (!SelectedAsset)
	{
		return;
	}

	// Set Volume Asset to all listeners.
	for (ARaymarchVolume* ListenerVolume : ListenerVolumes)
	{
		ListenerVolume->SetVolumeAsset(SelectedAsset);
	}
}

void UVolumeLoadMenu::RemoveListenerVolume(ARaymarchVolume* RemovedRaymarchVolume)
{
	ListenerVolumes.Remove(RemovedRaymarchVolume);
}

void UVolumeLoadMenu::AddListenerVolume(ARaymarchVolume* NewRaymarchVolume)
{
	if (ListenerVolumes.Contains(NewRaymarchVolume))
	{
		return;
	}
	ListenerVolumes.Add(NewRaymarchVolume);
}
