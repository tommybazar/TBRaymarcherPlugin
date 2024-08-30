#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

enum class EVolumeImporterLoaderType : int8
{
	MHD,
	DICOM,
};

enum class EVolumeImporterThicknessOperation : int8
{
	Read,
	Set,
	Calculate,
};

class SVolumeImporterWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVolumeImporterWindow) : _WidgetWindow()
	{}

	SLATE_ARGUMENT(SWindow*, WidgetWindow)
	SLATE_END_ARGS()

	bool bCancelled = true;

	float PixelSpacingX = 1.0f;
	float PixelSpacingY = 1.0f;

	bool bSetPixelSpacingX = false;
	bool bSetPixelSpacingY = false;

	bool bSetSliceThickness = false;
	float SliceThickness = 1.0f;
	bool bVerifySliceThickness = true;
	bool bIgnoreIrregularThickness = false;

	EVolumeImporterThicknessOperation ThicknessOperation = EVolumeImporterThicknessOperation::Read;
	EVolumeImporterLoaderType LoaderType = EVolumeImporterLoaderType::DICOM;

	bool GetNormalize() const;
	bool GetConvertToFloat() const;

	void Construct(const FArguments& InArgs);

private:
	SWindow* WidgetWindow;

	TSharedPtr<SCheckBox> NormalizeCheckBox;
	TSharedPtr<SCheckBox> ConvertToFloatCheckBox;

	FReply OnImport();
	FReply OnCancel();

	void Close();
};
