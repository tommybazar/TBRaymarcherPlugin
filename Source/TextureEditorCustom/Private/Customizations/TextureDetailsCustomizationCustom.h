// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/WeakObjectPtr.h"
#include "Types/SlateEnums.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

class FTextureDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:
	FTextureDetails()
		: bIsUsingSlider(false)
	{}
private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	TOptional<int32> OnGetMaxTextureSize() const;
	void OnMaxTextureSizeChanged(int32 NewValue);
	void OnMaxTextureSizeCommitted(int32 NewValue, ETextCommit::Type CommitInfo);
	void OnBeginSliderMovement();
	void OnEndSliderMovement(int32 NewValue);

	TSharedPtr<IPropertyHandle> MaxTextureSizePropertyHandle;
	TSharedPtr<IPropertyHandle> VirtualTextureStreamingPropertyHandle;
	TWeakObjectPtr<UObject> TextureBeingCustomized;
	bool bIsUsingSlider;
};

