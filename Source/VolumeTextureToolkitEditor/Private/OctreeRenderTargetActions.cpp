// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).


#include "OctreeRenderTargetActions.h"
#include "VolumeTextureToolkit/Public/RenderTargetVolumeMipped.h"
#include "OctreeRenderTargetEditorToolkit.h"

UClass* FOctreeRenderTargetAssetTypeActions::GetSupportedClass() const
{
	return URenderTargetVolumeMipped::StaticClass();
}

FText FOctreeRenderTargetAssetTypeActions::GetName() const
{
	return INVTEXT("Octree Render Target");
}

FColor FOctreeRenderTargetAssetTypeActions::GetTypeColor() const
{
	return FColor::Cyan;
}

uint32 FOctreeRenderTargetAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}
void FOctreeRenderTargetAssetTypeActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	MakeShared<FOctreeRenderTargetEditorToolkit>()->InitTextureEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects[0]);
	//FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
}