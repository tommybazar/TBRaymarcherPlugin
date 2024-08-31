#include "VolumeImporter.h"

#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SVolumeImporterWindow"

bool SVolumeImporterWindow::bDumpDicom = false;

float SVolumeImporterWindow::PixelSpacingX = 1.0f;
float SVolumeImporterWindow::PixelSpacingY = 1.0f;

bool SVolumeImporterWindow::bSetPixelSpacingX = false;
bool SVolumeImporterWindow::bSetPixelSpacingY = false;

float SVolumeImporterWindow::SliceThickness = 1.0f;

EVolumeImporterThicknessOperation SVolumeImporterWindow::ThicknessOperation = EVolumeImporterThicknessOperation::Read;
EVolumeImporterLoaderType SVolumeImporterWindow::LoaderType = EVolumeImporterLoaderType::DICOM;

bool SVolumeImporterWindow::bVerifySliceThickness = false;
bool SVolumeImporterWindow::bIgnoreIrregularThickness = true;

bool SVolumeImporterWindow::GetNormalize() const
{
	return NormalizeCheckBox->GetCheckedState() == ECheckBoxState::Checked;
}

bool SVolumeImporterWindow::GetConvertToFloat() const
{
	return ConvertToFloatCheckBox->GetCheckedState() == ECheckBoxState::Checked;
}

bool SVolumeImporterWindow::GetVerifySliceThickness() const
{
	return ThicknessOperation == EVolumeImporterThicknessOperation::Read && bVerifySliceThickness;
}

bool SVolumeImporterWindow::GetIgnoreIrregularThickness() const
{
	if (ThicknessOperation == EVolumeImporterThicknessOperation::Calculate)
	{
		return bIgnoreIrregularThickness;
	}
	else if (ThicknessOperation == EVolumeImporterThicknessOperation::Read && bVerifySliceThickness)
	{
		return bIgnoreIrregularThickness;
	}

	return false;
}

void SVolumeImporterWindow::Construct(const FArguments& InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;

	const FText SetPixelSpacingTooltipText =
		LOCTEXT("PixelSpacing", "Pixel Spacing is read from the files header unless setup directly in the importer.");

	// clang-format off
	ChildSlot
	[
		SNew(SBorder)
		.Padding(2.0f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FormatSection", "Format"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SSegmentedControl<EVolumeImporterLoaderType>)
				.Value_Lambda([this]() { return LoaderType; })
				.OnValueChanged_Lambda([this](EVolumeImporterLoaderType NewValue) { LoaderType = NewValue; })
				+ SSegmentedControl<EVolumeImporterLoaderType>::Slot(EVolumeImporterLoaderType::DICOM)
				.Text(LOCTEXT("LoaderTypeDICOM", "DICOM"))
				.ToolTip(LOCTEXT("LoaderTypeDICOMTooltip", "DICOM format via the dcmtk."))
				+ SSegmentedControl<EVolumeImporterLoaderType>::Slot(EVolumeImporterLoaderType::MHD)
				.Text(LOCTEXT("LoaderTypeMHD", "MHD"))
				.ToolTip(LOCTEXT("LoaderTypeMHD", "MHD format."))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ConversionSection", "Conversion"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10, 5)
			[
				SNew(SHorizontalBox)
				.ToolTip(
					SNew(SToolTip)
					.Text(LOCTEXT("Normalization",
						"Would you like your volume converted to G8 or G16 and normalized to the whole type range? This will allow it to "
						"be saved persistently as an asset and make inspecting it with Texture editor easier. Also, rendering with the "
						"provided raymarching material and transfer function will work out-of-the-box.\nIf your volume already is MET_(U)CHAR or "
						"MET_(U)SHORT, your volume will be persistent even without conversion, but values might be all over the place.")))
				+ SHorizontalBox::Slot()
				[
					SAssignNew(NormalizeCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Checked)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NormalizeCheckbox", "Normalize"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10, 5)
			[
				SNew(SHorizontalBox)
				.ToolTip(
					SNew(SToolTip)
					.Text(LOCTEXT("FloatConversion",
						"Should we convert it to R32_FLOAT? This will make the asset un-saveable." )))
				+ SHorizontalBox::Slot()
				[
					SAssignNew(ConvertToFloatCheckBox, SCheckBox)
					.IsChecked(ECheckBoxState::Unchecked)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ToFloatCheckbox", "To Float"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10, 5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PixelSpacingSection", "Pixel Spacing"))
			]

			// PixelSpacing X

			+ SVerticalBox::Slot()
			[
				SNew(SVerticalBox)
				.IsEnabled_Lambda([this]() { return LoaderType == EVolumeImporterLoaderType::DICOM; })
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 2)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						SNew(SHorizontalBox)
						.ToolTip(
							SNew(SToolTip)
							.Text(SetPixelSpacingTooltipText))	
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(5, 1)
						[
							SNew(SCheckBox)
							.IsChecked(bSetPixelSpacingX)
							.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) 
								{ 
									bSetPixelSpacingX = NewState == ECheckBoxState::Checked;
								})
							.Content()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SetXCheckbox", "Set X"))
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						.Padding(5, 1)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.MinValue(0.0001)
							.MaxValue(100)
							.Value_Lambda([this]() { return PixelSpacingX; })
							.IsEnabled_Lambda([this]() { return bSetPixelSpacingX; })
							.OnValueChanged(SNumericEntryBox<float>::FOnValueChanged::CreateLambda(
								[this](float Value)
								{
									PixelSpacingX = Value;
								}))
						]
					]
				]

				// PixelSpacing Y
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 2)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						SNew(SHorizontalBox)
						.ToolTip(
							SNew(SToolTip)
							.Text(SetPixelSpacingTooltipText))	
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(5, 1)
						[
							SNew(SCheckBox)
							.IsChecked(bSetPixelSpacingY)
							.OnCheckStateChanged_Lambda([this](ECheckBoxState State) 
								{ 
									bSetPixelSpacingY = State == ECheckBoxState::Checked;
								})
							.Content()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("SetYCheckbox", "Set Y"))
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						.Padding(5, 1)
						[
							SNew(SNumericEntryBox<float>)
							.AllowSpin(true)
							.MinValue(0.0001)
							.MaxValue(100)
							.Value_Lambda([this]() { return PixelSpacingY; })
							.IsEnabled_Lambda([this]() { return bSetPixelSpacingY; })
							.OnValueChanged(SNumericEntryBox<float>::FOnValueChanged::CreateLambda(
								[this](float Value)
								{
									PixelSpacingY = Value;
								}))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SSeparator)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SliceThicknessSection", "Slice Thickness"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5)
				[
					SNew(SSegmentedControl<EVolumeImporterThicknessOperation>)
					.Value_Lambda([this]() { return ThicknessOperation; })
					.OnValueChanged_Lambda([this](EVolumeImporterThicknessOperation NewValue) { ThicknessOperation = NewValue; })
					+ SSegmentedControl<EVolumeImporterThicknessOperation>::Slot(EVolumeImporterThicknessOperation::Read)
					.Text(LOCTEXT("ThicknessRead", "Read"))
					.ToolTip(LOCTEXT("ThicknessReadTooltip", "Read the SliceThickness value from the files header."))
					+ SSegmentedControl<EVolumeImporterThicknessOperation>::Slot(EVolumeImporterThicknessOperation::Calculate)
					.Text(LOCTEXT("ThicknessCalculate", "Calculate"))
					.ToolTip(LOCTEXT("ThicknessCalculateTooltip", "Calculate the SliceThickness value location attributes of each slice."))
					+ SSegmentedControl<EVolumeImporterThicknessOperation>::Slot(EVolumeImporterThicknessOperation::Set)
					.Text(LOCTEXT("ThicknessSet", "Set"))
					.ToolTip(LOCTEXT("ThicknessSetTooltip", "Set the SliceThickness to a fixed value."))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bVerifySliceThickness = State == ECheckBoxState::Checked; })
					.IsEnabled_Lambda([this]()
						{
							return ThicknessOperation == EVolumeImporterThicknessOperation::Read;
						})
					.IsChecked_Lambda([this]()
						{
							return bVerifySliceThickness ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
					.ToolTip(
						SNew(SToolTip)
						.Text(LOCTEXT("ThicknessVerifyTooltip", "Read the value from the header and compare it to each difference of the slice location attributes.")))
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("VerifyCheckbox", "Verify"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 5)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bIgnoreIrregularThickness = State == ECheckBoxState::Checked; })
					.IsEnabled_Lambda([this]()
						{
							return ThicknessOperation == EVolumeImporterThicknessOperation::Calculate || 
								(ThicknessOperation == EVolumeImporterThicknessOperation::Read && bVerifySliceThickness);
						})
					.IsChecked_Lambda([this]()
						{
							return bIgnoreIrregularThickness ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
					.ToolTip(
						SNew(SToolTip)
						.Text(LOCTEXT("ThicknessIgnoreIrregular", "When the importer encounters a slice that has different thickness than others it won't fail the import")))
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("IgnoreIrregularCheckbox", "Ignore Irregular"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(5, 1)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.MinValue(0.0001)
					.MaxValue(100)
					.Value_Lambda([this]() { return SliceThickness; })
					.IsEnabled_Lambda([this]() { return ThicknessOperation == EVolumeImporterThicknessOperation::Set; })
					.OnValueChanged(SNumericEntryBox<float>::FOnValueChanged::CreateLambda(
						[this](float Value)
						{
							SliceThickness = Value;
						}))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("UtilsSection", "Utils"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10, 5)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bDumpDicom ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.IsEnabled_Lambda([this]() { return LoaderType == EVolumeImporterLoaderType::DICOM; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bDumpDicom = State == ECheckBoxState::Checked; })
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DumpCheckbox", "Dump"))
					.ToolTip(
						SNew(SToolTip)
						.Text(LOCTEXT("DumpDicomTooltip", "Dump the file structure of the DICOM file to the output log.")))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5, 10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Fill)
				.Padding(5)
				[
					SNew(SButton)
					.Text(LOCTEXT("ImportButton", "Import"))
					.OnClicked(this, &SVolumeImporterWindow::OnImport)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Fill)
				.Padding(5)
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelButton", "Cancel"))
					.OnClicked(this, &SVolumeImporterWindow::OnCancel)
				]
			]
		]
	];
	// clang-format on
}

FReply SVolumeImporterWindow::OnImport()
{
	bCancelled = false;
	Close();
	return FReply::Handled();
}

FReply SVolumeImporterWindow::OnCancel()
{
	Close();
	return FReply::Handled();
}

void SVolumeImporterWindow::Close()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
