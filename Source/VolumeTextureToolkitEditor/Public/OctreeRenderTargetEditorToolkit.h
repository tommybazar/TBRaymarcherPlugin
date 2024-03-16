// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).

#pragma once

#include "TextureEditorCustom/Public/TextureEditorToolkitCustom.h"
#include "CoreMinimal.h"
/**
 *
 */
class VOLUMETEXTURETOOLKITEDITOR_API FOctreeRenderTargetEditorToolkit : public FTextureEditorToolkit
{
public:
	FOctreeRenderTargetEditorToolkit();
	~FOctreeRenderTargetEditorToolkit();

	void InitEditor(const TArray<UObject*>& InObjects);

	FName GetToolkitFName() const override { return "Octree Render Target Editor"; }
	FText GetBaseToolkitName() const override { return INVTEXT("Octree Editor"); }
	FString GetWorldCentricTabPrefix() const override { return "Octree"; }
	FLinearColor GetWorldCentricTabColorScale() const override { return {}; }

};
