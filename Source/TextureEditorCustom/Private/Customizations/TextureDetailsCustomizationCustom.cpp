// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/TextureDetailsCustomizationCustom.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/Texture.h"
#include "IDetailPropertyRow.h"
#include "Misc/MessageDialog.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "FTextureDetails"


TSharedRef<IDetailCustomization> FTextureDetails::MakeInstance()
{
	return MakeShareable(new FTextureDetails);
}

void FTextureDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ensure(ObjectsBeingCustomized.Num() == 1))
	{
		TextureBeingCustomized = ObjectsBeingCustomized[0];
	}

	DetailBuilder.EditCategory("LevelOfDetail");
	DetailBuilder.EditCategory("Compression");
	DetailBuilder.EditCategory("Texture");
	DetailBuilder.EditCategory("Adjustments");
	DetailBuilder.EditCategory("File Path");

	MaxTextureSizePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UTexture, MaxTextureSize));
	VirtualTextureStreamingPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UTexture, VirtualTextureStreaming));

	// Customize MaxTextureSize
	if( MaxTextureSizePropertyHandle->IsValidHandle() )
	{
		IDetailCategoryBuilder& CompressionCategory = DetailBuilder.EditCategory("Compression");
		IDetailPropertyRow& MaxTextureSizePropertyRow = CompressionCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UTexture, MaxTextureSize));
		TSharedPtr<SWidget> NameWidget;
		TSharedPtr<SWidget> ValueWidget;
		FDetailWidgetRow Row;
		MaxTextureSizePropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

		int32 MaxTextureSize = 2048;

		if (UTexture* Texture = Cast<UTexture>(TextureBeingCustomized.Get()))
		{
			// GetMaximumDimension is for current RHI and texture type
			MaxTextureSize = Texture->GetMaximumDimension();
		}

		const bool bShowChildren = true;
		MaxTextureSizePropertyRow.CustomWidget(bShowChildren)
			.NameContent()
			.MinDesiredWidth(Row.NameWidget.MinWidth)
			.MaxDesiredWidth(Row.NameWidget.MaxWidth)
			[
				NameWidget.ToSharedRef()
			]
			.ValueContent()
			.MinDesiredWidth(Row.ValueWidget.MinWidth)
			.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
			[
				SNew(SNumericEntryBox<int32>)
				.AllowSpin(true)
				.Value(this, &FTextureDetails::OnGetMaxTextureSize)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.MinValue(0)
				.MaxValue(MaxTextureSize)
				.MinSliderValue(0)
				.MaxSliderValue(MaxTextureSize)
				.OnValueChanged(this, &FTextureDetails::OnMaxTextureSizeChanged)
				.OnValueCommitted(this, &FTextureDetails::OnMaxTextureSizeCommitted)
				.OnBeginSliderMovement(this, &FTextureDetails::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &FTextureDetails::OnEndSliderMovement)
			];
	}

	// Hide the option to enable VT streaming, if VT is disabled for the project
	if (VirtualTextureStreamingPropertyHandle.IsValid())
	{
		static const auto CVarVirtualTexturesEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures")); check(CVarVirtualTexturesEnabled);
		const bool bVirtualTextureEnabled = CVarVirtualTexturesEnabled->GetValueOnAnyThread() != 0;
		if (!bVirtualTextureEnabled)
		{
			DetailBuilder.HideProperty(VirtualTextureStreamingPropertyHandle);
		}
	}
}

/** @return The value or unset if properties with multiple values are viewed */
TOptional<int32> FTextureDetails::OnGetMaxTextureSize() const
{
	int32 NumericVal;
	if (MaxTextureSizePropertyHandle->GetValue(NumericVal) == FPropertyAccess::Success)
	{
		return NumericVal;
	}

	// Return an unset value so it displays the "multiple values" indicator instead
	return TOptional<int32>();
}

void FTextureDetails::OnMaxTextureSizeChanged(int32 NewValue)
{
	if (bIsUsingSlider)
	{
		int32 OrgValue(0);
		if (MaxTextureSizePropertyHandle->GetValue(OrgValue) != FPropertyAccess::Fail)
		{
			// Value hasn't changed, so let's return now
			if (OrgValue == NewValue)
			{
				return;
			}
		}

		// We don't create a transaction for each property change when using the slider.  Only once when the slider first is moved
		EPropertyValueSetFlags::Type Flags = (EPropertyValueSetFlags::InteractiveChange | EPropertyValueSetFlags::NotTransactable);
		MaxTextureSizePropertyHandle->SetValue(NewValue, Flags);
	}
}

void FTextureDetails::OnMaxTextureSizeCommitted(int32 NewValue, ETextCommit::Type CommitInfo)
{
	MaxTextureSizePropertyHandle->SetValue(NewValue);
}

/**
 * Called when the slider begins to move.  We create a transaction here to undo the property
 */
void FTextureDetails::OnBeginSliderMovement()
{
	bIsUsingSlider = true;

	GEditor->BeginTransaction(TEXT("TextureDetails"), LOCTEXT("SetMaximumTextureSize", "Edit Maximum Texture Size"), nullptr /* MaxTextureSizePropertyHandle->GetProperty() */ );
}


/**
 * Called when the slider stops moving.  We end the previously created transaction
 */
void FTextureDetails::OnEndSliderMovement(int32 NewValue)
{
	bIsUsingSlider = false;

	GEditor->EndTransaction();
}


#undef LOCTEXT_NAMESPACE
