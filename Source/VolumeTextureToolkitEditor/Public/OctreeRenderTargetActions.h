// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
/**
 * 
 */
class VOLUMETEXTURETOOLKITEDITOR_API FOctreeRenderTargetAssetTypeActions : public FAssetTypeActions_Base
{
public:
	UClass* GetSupportedClass() const override;
	FText GetName() const override;
	FColor GetTypeColor() const override;
	uint32 GetCategories() override;

	void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;
};
