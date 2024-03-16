// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "TextureDetailsCustomizationCustom.h"
#include "Types/SlateEnums.h"
#include "UObject/WeakObjectPtr.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

class FCurveLinearColorAtlasDetails : public FTextureDetails
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:
	FCurveLinearColorAtlasDetails()
	{}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

