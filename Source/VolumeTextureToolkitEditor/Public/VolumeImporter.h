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

	static bool bDumpDicom;

	static float PixelSpacingX;
	static float PixelSpacingY;

	static bool bSetPixelSpacingX;
	static bool bSetPixelSpacingY;

	static float SliceThickness;

	static EVolumeImporterThicknessOperation ThicknessOperation;
	static EVolumeImporterLoaderType LoaderType;

	bool GetNormalize() const;
	bool GetConvertToFloat() const;
	bool GetVerifySliceThickness() const;
	bool GetIgnoreIrregularThickness() const;

	void Construct(const FArguments& InArgs);

private:
	SWindow* WidgetWindow;

	static bool bVerifySliceThickness;
	static bool bIgnoreIrregularThickness;

	TSharedPtr<SCheckBox> NormalizeCheckBox;
	TSharedPtr<SCheckBox> ConvertToFloatCheckBox;

	FReply OnImport();
	FReply OnCancel();

	void Close();
};
