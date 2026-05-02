// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "VolumeAsset/VolumeAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"

UVolumeAsset* UVolumeAsset::CreateTransient(FString Name)
{
    return NewObject<UVolumeAsset>(GetTransientPackage(), StaticClass(), FName("VA_" + Name), RF_Standalone | RF_Public);
}

UVolumeAsset* UVolumeAsset::CreatePersistent(FString SaveFolder, const FString SaveName)
{
    // Add slash at the end if it's not already there.
    if (!SaveFolder.EndsWith("/"))
    {
        SaveFolder += "/";
    }
    // Create persistent package if we want the Volume info to be savable.
    const FString VolumePackageName = SaveFolder + "VA_" + SaveName;
    UPackage* VolumePackage = CreatePackage(*VolumePackageName);
    VolumePackage->FullyLoad();

    UVolumeAsset* VolumeAsset =
        NewObject<UVolumeAsset>(VolumePackage, StaticClass(), FName("VA_" + SaveName), RF_Standalone | RF_Public);
    if (VolumeAsset)
    {
        FAssetRegistryModule::AssetCreated(VolumeAsset);
    }
    return VolumeAsset;
}

#if WITH_EDITOR
void UVolumeAsset::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    const FName MemberPropertyName =
        (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

    // Only called when a property other than TransferFuncCurve gets changed.
    if (MemberPropertyName != GET_MEMBER_NAME_CHECKED(UVolumeAsset, TransferFuncCurve))
    {
        OnImageInfoChanged.Broadcast();
    }
}

void UVolumeAsset::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    const FName MemberPropertyName =
        (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

    // If the curve property changed, broadcast the delegate.
    if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UVolumeAsset, TransferFuncCurve))
    {
        OnCurveChanged.Broadcast(TransferFuncCurve);
    }
}
#endif
