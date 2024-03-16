// Copyright Epic Games, Inc. All Rights Reserved.

#include "Models/TextureEditorViewportClientCustom.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CubemapUnwrapUtils.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureCubeArray.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTarget2DArray.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/VolumeTexture.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Slate/SceneViewport.h"
#include "Texture2DPreview.h"
#include "TextureEditorSettingsCustom.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UnrealEdGlobals.h"
#include "VolumeTexturePreview.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/STextureEditorViewportCustom.h"

static TAutoConsoleVariable<int32> CVarEnableVTFeedback(
	TEXT("r.VT.UpdateFeedbackTextureEditor"),
	1,
	TEXT("Enable/Disable the CPU feedback analysis in the texture editor."),
	ECVF_RenderThreadSafe
);

/* FTextureEditorViewportClient structors
 *****************************************************************************/

FTextureEditorViewportClient::FTextureEditorViewportClient( TWeakPtr<ITextureEditorToolkit> InTextureEditor, TWeakPtr<STextureEditorViewport> InTextureEditorViewport )
	: TextureEditorPtr(InTextureEditor)
	, TextureEditorViewportPtr(InTextureEditorViewport)
	, CheckerboardTexture(NULL)
{
	check(TextureEditorPtr.IsValid() && TextureEditorViewportPtr.IsValid());

	ModifyCheckerboardTextureColors();
}


FTextureEditorViewportClient::~FTextureEditorViewportClient( )
{
	DestroyCheckerboardTexture();
}


/* FViewportClient interface
 *****************************************************************************/

void FTextureEditorViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	if (!TextureEditorPtr.IsValid())
	{
		return;
	}

	TSharedPtr<ITextureEditorToolkit> TextureEditorPinned = TextureEditorPtr.Pin();
	const UTextureEditorSettingsCustom& Settings = *GetDefault<UTextureEditorSettingsCustom>();

	UTexture* Texture = TextureEditorPinned->GetTexture();
	FVector2D Ratio = FVector2D(GetViewportHorizontalScrollBarRatio(), GetViewportVerticalScrollBarRatio());
	FVector2D ViewportSize = FVector2D(TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY());
	FVector2D ScrollBarPos = GetViewportScrollBarPositions();
	int32 BorderSize = Settings.TextureBorderEnabled ? 1 : 0;
	float YOffset = (Ratio.Y > 1.0f) ? ((ViewportSize.Y - (ViewportSize.Y / Ratio.Y)) * 0.5f) : 0;
	int32 YPos = FMath::RoundToInt(YOffset - ScrollBarPos.Y + BorderSize);
	float XOffset = (Ratio.X > 1.0f) ? ((ViewportSize.X - (ViewportSize.X / Ratio.X)) * 0.5f) : 0;
	int32 XPos = FMath::RoundToInt(XOffset - ScrollBarPos.X + BorderSize);
	
	UpdateScrollBars();


	Canvas->Clear( Settings.BackgroundColor );

	UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	UTextureCube* TextureCube = Cast<UTextureCube>(Texture);
	UTexture2DArray* Texture2DArray = Cast<UTexture2DArray>(Texture);
	UTextureCubeArray* TextureCubeArray = Cast<UTextureCubeArray>(Texture);
	UVolumeTexture* VolumeTexture = Cast<UVolumeTexture>(Texture);
	UTextureRenderTarget2D* TextureRT2D = Cast<UTextureRenderTarget2D>(Texture);
	UTextureRenderTarget2DArray* TextureRT2DArray = Cast<UTextureRenderTarget2DArray>(Texture);
	UTextureRenderTargetCube* RTTextureCube = Cast<UTextureRenderTargetCube>(Texture);
	UTextureRenderTargetVolume* RTTextureVolume = Cast<UTextureRenderTargetVolume>(Texture);

	// Fully stream in the texture before drawing it.
	Texture->SetForceMipLevelsToBeResident(30.0f);
	Texture->WaitForStreaming();

	TextureEditorPinned->PopulateQuickInfo();

	// Figure out the size we need
	int32 Width, Height, Depth, ArraySize;
	TextureEditorPinned->CalculateTextureDimensions(Width, Height, Depth, ArraySize, false);
	const float MipLevel = (float)TextureEditorPinned->GetMipLevel();
	const float LayerIndex = (float)TextureEditorPinned->GetLayer();
	const float SliceIndex = (float)TextureEditorPinned->GetSlice();

	bool bIsVirtualTexture = false;

	TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;

	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
	{
		if (TextureCube || TextureCubeArray || RTTextureCube)
		{
			const int32 FaceIndex = TextureEditorPinned->GetFace();
			const FVector4f& IdentityW = FVector4f(FVector3f::ZeroVector, 1);
			// when the face index is not specified, generate a view matrix based on the tracked orientation in the texture editor
			// (assuming that identity matrix displays the face 0 properly oriented in space),
			// otherwise generate a view matrix which displays the selected face using DDS orientation and face order
			// (positive x, negative x, positive y, negative y, positive z, negative z)
			const FMatrix44f& ViewMatrix =
				FaceIndex < 0 ? FMatrix44f(FRotationMatrix::Make(TextureEditorPinned->GetOrientation())) :
				FaceIndex == 0 ? FMatrix44f(FVector3f::ForwardVector, FVector3f::DownVector, FVector3f::RightVector, IdentityW) :
				FaceIndex == 1 ? FMatrix44f(FVector3f::BackwardVector, FVector3f::UpVector, FVector3f::RightVector, IdentityW) :
				FaceIndex == 2 ? FMatrix44f(FVector3f::RightVector, FVector3f::ForwardVector, FVector3f::DownVector, IdentityW) :
				FaceIndex == 3 ? FMatrix44f(FVector3f::LeftVector, FVector3f::ForwardVector, FVector3f::UpVector, IdentityW) :
				FaceIndex == 4 ? FMatrix44f(FVector3f::UpVector, FVector3f::ForwardVector, FVector3f::RightVector, IdentityW) :
				FMatrix44f(FVector3f::DownVector, FVector3f::BackwardVector, FVector3f::RightVector, IdentityW);
			const bool bShowLongLatUnwrap = TextureEditorPinned->GetCubemapViewMode() == TextureEditorCubemapViewMode_2DView && FaceIndex < 0;
			BatchedElementParameters = new FMipLevelBatchedElementParameters(MipLevel, SliceIndex, TextureCubeArray != nullptr, ViewMatrix, bShowLongLatUnwrap, false);
		}
		else if (VolumeTexture)
		{
			BatchedElementParameters = new FBatchedElementVolumeTexturePreviewParameters(
				TextureEditorPinned->GetVolumeViewMode() == TextureEditorVolumeViewMode_DepthSlices,
				FMath::Max<int32>(VolumeTexture->GetSizeZ(), 1), 
				MipLevel, 
				(float)TextureEditorPinned->GetVolumeOpacity(),
				true, 
				TextureEditorPinned->GetOrientation());
		}
		else if (RTTextureVolume)
		{
			BatchedElementParameters = new FBatchedElementVolumeTexturePreviewParameters(
				TextureEditorPinned->GetVolumeViewMode() == TextureEditorVolumeViewMode_DepthSlices,
				FMath::Max<int32>(RTTextureVolume->SizeZ >> RTTextureVolume->GetCachedLODBias(), 1),
				MipLevel,
				(float)TextureEditorPinned->GetVolumeOpacity(),
				true,
				TextureEditorPinned->GetOrientation());
		}
		else if (Texture2D)
		{
			bool bIsNormalMap = Texture2D->IsNormalMap();
			bool bIsSingleChannel = Texture2D->CompressionSettings == TC_Grayscale || Texture2D->CompressionSettings == TC_Alpha;
			bool bSingleVTPhysicalSpace = Texture2D->IsVirtualTexturedWithSinglePhysicalSpace();
			bIsVirtualTexture = Texture2D->IsCurrentlyVirtualTextured();
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, bIsNormalMap, bIsSingleChannel, bSingleVTPhysicalSpace, bIsVirtualTexture, false);
		}
		else if (Texture2DArray) 
		{
			bool bIsNormalMap = Texture2DArray->IsNormalMap();
			bool bIsSingleChannel = Texture2DArray->CompressionSettings == TC_Grayscale || Texture2DArray->CompressionSettings == TC_Alpha;
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, bIsNormalMap, bIsSingleChannel, false, false, true);
		}
		else if (TextureRT2D)
		{
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, false, false, false, false, false);
		}
		else if (TextureRT2DArray)
		{
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, false, false, false, false, true);
		}
		else
		{
			// Default to treating any UTexture derivative as a 2D texture resource
			BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(MipLevel, LayerIndex, SliceIndex, false, false, false, false, false);
		}
	}

	// Draw the background checkerboard pattern in the same size/position as the render texture so it will show up anywhere
	// the texture has transparency
	if (Viewport && CheckerboardTexture)
	{
		const int32 CheckerboardSizeX = FMath::Max<int32>(1, CheckerboardTexture->GetSizeX());
		const int32 CheckerboardSizeY = FMath::Max<int32>(1, CheckerboardTexture->GetSizeY());
		if (Settings.Background == TextureEditorBackground_CheckeredFill)
		{
			Canvas->DrawTile( 0.0f, 0.0f, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0.0f, 0.0f, (float)Viewport->GetSizeXY().X / CheckerboardSizeX, (float)Viewport->GetSizeXY().Y / CheckerboardSizeY, FLinearColor::White, CheckerboardTexture->GetResource());
		}
		else if (Settings.Background == TextureEditorBackground_Checkered)
		{
			Canvas->DrawTile( XPos, YPos, Width, Height, 0.0f, 0.0f, (float)Width / CheckerboardSizeX, (float)Height / CheckerboardSizeY, FLinearColor::White, CheckerboardTexture->GetResource());
		}
	}

	FTexturePlatformData** RunningPlatformDataPtr = Texture->GetRunningPlatformData();
	float Exposure = RunningPlatformDataPtr && *RunningPlatformDataPtr && IsHDR((*RunningPlatformDataPtr)->PixelFormat) ? FMath::Pow(2.0f, (float)TextureEditorPinned->GetExposureBias()) : 1.0f;

	if ( Texture->GetResource() != nullptr )
	{
		FCanvasTileItem TileItem( FVector2D( XPos, YPos ), Texture->GetResource(), FVector2D( Width, Height ), FLinearColor(Exposure, Exposure, Exposure) );
		TileItem.BlendMode = TextureEditorPinned->GetColourChannelBlendMode();
		TileItem.BatchedElementParameters = BatchedElementParameters;

		if (bIsVirtualTexture && Texture->Source.GetNumBlocks() > 1)
		{
			// Adjust UVs to display entire UDIM range, accounting for UE inverted V-axis
			const FIntPoint BlockSize = Texture->Source.GetSizeInBlocks();
			TileItem.UV0 = FVector2D(0.0f, 1.0f - (float)BlockSize.Y);
			TileItem.UV1 = FVector2D((float)BlockSize.X, 1.0f);
		}

		Canvas->DrawItem( TileItem );

		// Draw a white border around the texture to show its extents
		if (Settings.TextureBorderEnabled)
		{
			FCanvasBoxItem BoxItem(FVector2D(XPos, YPos), FVector2D(Width + BorderSize, Height + BorderSize));
			BoxItem.SetColor( Settings.TextureBorderColor );
			Canvas->DrawItem( BoxItem );
		}

		// if we are presenting a virtual texture, make the appropriate tiles resident
		if (bIsVirtualTexture && CVarEnableVTFeedback.GetValueOnAnyThread() != 0)
		{
			FVirtualTexture2DResource* VTResource = static_cast<FVirtualTexture2DResource*>(Texture->GetResource());
			const FVector2D ScreenSpaceSize((float)Width, (float)Height);
			const FVector2D ViewportPositon(-(float)XPos, -(float)YPos);
			const FVector2D UV0 = TileItem.UV0;
			const FVector2D UV1 = TileItem.UV1;

			const ERHIFeatureLevel::Type InFeatureLevel = GMaxRHIFeatureLevel;
			ENQUEUE_RENDER_COMMAND(MakeTilesResident)(
				[InFeatureLevel, VTResource, ScreenSpaceSize, ViewportPositon, ViewportSize, UV0, UV1, MipLevel](FRHICommandListImmediate& RHICmdList)
			{
				// AcquireAllocatedVT() must happen on render thread
				IAllocatedVirtualTexture* AllocatedVT = VTResource->AcquireAllocatedVT();

				IRendererModule& RenderModule = GetRendererModule();
				RenderModule.RequestVirtualTextureTilesForRegion(AllocatedVT, ScreenSpaceSize, ViewportPositon, ViewportSize, UV0, UV1, MipLevel);
				RenderModule.LoadPendingVirtualTextureTiles(RHICmdList, InFeatureLevel);
			});
		}
	}

	// If we are requesting an explicit mip level of a VT asset, test to see if we can even display it properly and warn about it
	if (bIsVirtualTexture && MipLevel >= 0.f)
	{
		const uint32 Mip = (uint32)MipLevel;
		const FIntPoint SizeOnMip = { Texture2D->GetSizeX() >> Mip,Texture2D->GetSizeY() >> Mip };
		const uint64 NumPixels = static_cast<uint64>(SizeOnMip.X) * SizeOnMip.Y;

		const FVirtualTexture2DResource* Resource = (FVirtualTexture2DResource*)Texture2D->GetResource();
		const FIntPoint PhysicalTextureSize = Resource->GetPhysicalTextureSize(0u);
		const uint64 NumPhysicalPixels = static_cast<uint64>(PhysicalTextureSize.X) * PhysicalTextureSize.Y;

		if (NumPixels >= NumPhysicalPixels)
		{
			UFont* ErrorFont = GEngine->GetLargeFont();
			const int32 LineHeight = FMath::TruncToInt(ErrorFont->GetMaxCharHeight());
			const FText Message = NSLOCTEXT("TextureEditor", "InvalidVirtualTextureMipDisplay", "Displaying a virtual texture on a mip level that is larger than the physical cache. Rendering will probably be invalid!");
			const uint32 MessageWidth = ErrorFont->GetStringSize(*Message.ToString());
			const uint32 Xpos = (ViewportSize.X - MessageWidth) / 2;
			Canvas->DrawShadowedText(Xpos, LineHeight*1.5,
				Message,
				ErrorFont, FLinearColor::Red);
		}
		
	}
}


bool FTextureEditorViewportClient::InputKey(const FInputKeyEventArgs& InEventArgs)
{
	if (InEventArgs.Event == IE_Pressed)
	{
		if (InEventArgs.Key == EKeys::MouseScrollUp)
		{
			TextureEditorPtr.Pin()->ZoomIn();

			return true;
		}
		else if (InEventArgs.Key == EKeys::MouseScrollDown)
		{
			TextureEditorPtr.Pin()->ZoomOut();

			return true;
		}
		else if (InEventArgs.Key == EKeys::MiddleMouseButton && TextureEditorPtr.Pin()->IsUsingOrientation())
		{
			TextureEditorPtr.Pin()->ResetOrientation();
		}
	}
	return false;
}

bool FTextureEditorViewportClient::InputAxis(FViewport* Viewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		TSharedPtr<ITextureEditorToolkit> TextureEditorPinned = TextureEditorPtr.Pin();
		if (TextureEditorPinned->IsUsingOrientation() && Viewport->KeyState(EKeys::LeftMouseButton))
		{
			const float RotationSpeed = .2f;
			FRotator DeltaOrientation = FRotator(Key == EKeys::MouseY ? Delta * RotationSpeed : 0, Key == EKeys::MouseX ? Delta * RotationSpeed : 0, 0);
			if (TextureEditorPinned->IsVolumeTexture())
			{
				TextureEditorPinned->SetOrientation((FRotationMatrix::Make(DeltaOrientation) * FRotationMatrix::Make(TextureEditorPinned->GetOrientation())).Rotator());
			}
			else
			{
				TextureEditorPinned->SetOrientation(TextureEditorPinned->GetOrientation() + DeltaOrientation);
			}
		}
		else if (ShouldUseMousePanning(Viewport))
		{
			TSharedPtr<STextureEditorViewport> EditorViewport = TextureEditorViewportPtr.Pin();

			int32 Width, Height, Depth, ArraySize;
			TextureEditorPinned->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

			if (Key == EKeys::MouseY)
			{
				float VDistFromBottom = EditorViewport->GetVerticalScrollBar()->DistanceFromBottom();
				float VRatio = GetViewportVerticalScrollBarRatio();
				float localDelta = (Delta / static_cast<float>(Height));
				EditorViewport->GetVerticalScrollBar()->SetState(FMath::Clamp((1.f - VDistFromBottom - VRatio) + localDelta, 0.0f, 1.0f - VRatio), VRatio);
			}
			else
			{
				float HDistFromBottom = EditorViewport->GetHorizontalScrollBar()->DistanceFromBottom();
				float HRatio = GetViewportHorizontalScrollBarRatio();
				float localDelta = (Delta / static_cast<float>(Width)) * -1.f; // delta needs to be inversed
				EditorViewport->GetHorizontalScrollBar()->SetState(FMath::Clamp((1.f - HDistFromBottom - HRatio) + localDelta, 0.0f, 1.0f - HRatio), HRatio);
			}
		}
		return true;
	}

	return false;
}

bool FTextureEditorViewportClient::ShouldUseMousePanning(FViewport* Viewport) const
{
	if (Viewport->KeyState(EKeys::RightMouseButton))
	{
		TSharedPtr<STextureEditorViewport> EditorViewport = TextureEditorViewportPtr.Pin();
		return EditorViewport.IsValid() && EditorViewport->GetVerticalScrollBar().IsValid() && EditorViewport->GetHorizontalScrollBar().IsValid();
	}

	return false;
}

EMouseCursor::Type FTextureEditorViewportClient::GetCursor(FViewport* Viewport, int32 X, int32 Y)
{
	return ShouldUseMousePanning(Viewport) ? EMouseCursor::GrabHandClosed : EMouseCursor::Default;
}

bool FTextureEditorViewportClient::InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice)
{
	const bool LeftMouseButtonDown = Viewport->KeyState(EKeys::LeftMouseButton);
	const bool RightMouseButtonDown = Viewport->KeyState(EKeys::RightMouseButton);

	if (GestureType == EGestureEvent::Scroll && !LeftMouseButtonDown && !RightMouseButtonDown)
	{
		double CurrentZoom = TextureEditorPtr.Pin()->GetCustomZoomLevel();
		TextureEditorPtr.Pin()->SetCustomZoomLevel(CurrentZoom + GestureDelta.Y * 0.01);
		return true;
	}

	return false;
}


void FTextureEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CheckerboardTexture);
}


void FTextureEditorViewportClient::ModifyCheckerboardTextureColors()
{
	DestroyCheckerboardTexture();

	const UTextureEditorSettingsCustom& Settings = *GetDefault<UTextureEditorSettingsCustom>();
	CheckerboardTexture = FImageUtils::CreateCheckerboardTexture(Settings.CheckerColorOne, Settings.CheckerColorTwo, Settings.CheckerSize);
}


FText FTextureEditorViewportClient::GetDisplayedResolution() const
{
	// Zero is the default size 
	int32 Height, Width, Depth, ArraySize;
	TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, false);

	FText CubemapInfo;
	UTexture* Texture = TextureEditorPtr.Pin()->GetTexture();
	if (Texture->IsA(UTextureCube::StaticClass()) || Texture->IsA(UTextureCubeArray::StaticClass()) || Texture->IsA(UTextureRenderTargetCube::StaticClass()))
	{
		CubemapInfo = NSLOCTEXT("TextureEditor", "DisplayedPerCubeSide", "*6 (CubeMap)");
	}

	FNumberFormattingOptions Options;
	Options.UseGrouping = false;

	if (Depth > 0)
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolutionThreeDimension", "Displayed: {0}x{1}x{2}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options), FText::AsNumber(Depth, &Options));
	}
	else if (ArraySize > 0)
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolution", "Displayed: {0}x{1}{2}*{3}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options), CubemapInfo, FText::AsNumber(ArraySize, &Options));
	}
	else
	{
		return FText::Format(NSLOCTEXT("TextureEditor", "DisplayedResolutionTwoDimension", "Displayed: {0}x{1}{2}"), FText::AsNumber(Width, &Options), FText::AsNumber(Height, &Options), CubemapInfo);
	}
}


float FTextureEditorViewportClient::GetViewportVerticalScrollBarRatio() const
{
	int32 Height = 1;
	int32 Width = 1;
	float WidgetHeight = 1.0f;
	if (TextureEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid())
	{
		int32 Depth, ArraySize;
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		WidgetHeight = TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().Y;
	}

	return WidgetHeight / Height;
}


float FTextureEditorViewportClient::GetViewportHorizontalScrollBarRatio() const
{
	int32 Width = 1;
	int32 Height = 1;
	float WidgetWidth = 1.0f;
	if (TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		int32 Depth, ArraySize;
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		WidgetWidth = TextureEditorViewportPtr.Pin()->GetViewport()->GetSizeXY().X;
	}

	return WidgetWidth / Width;
}


void FTextureEditorViewportClient::UpdateScrollBars()
{
	TSharedPtr<STextureEditorViewport> Viewport = TextureEditorViewportPtr.Pin();

	if (!Viewport.IsValid() || !Viewport->GetVerticalScrollBar().IsValid() || !Viewport->GetHorizontalScrollBar().IsValid())
	{
		return;
	}

	float VRatio = GetViewportVerticalScrollBarRatio();
	float HRatio = GetViewportHorizontalScrollBarRatio();
	float VDistFromTop = Viewport->GetVerticalScrollBar()->DistanceFromTop();
	float VDistFromBottom = Viewport->GetVerticalScrollBar()->DistanceFromBottom();
	float HDistFromTop = Viewport->GetHorizontalScrollBar()->DistanceFromTop();
	float HDistFromBottom = Viewport->GetHorizontalScrollBar()->DistanceFromBottom();

	if (VRatio < 1.0f)
	{
		if (VDistFromBottom < 1.0f)
		{
			Viewport->GetVerticalScrollBar()->SetState(FMath::Clamp((1.0f + VDistFromTop - VDistFromBottom - VRatio) * 0.5f, 0.0f, 1.0f - VRatio), VRatio);
		}
		else
		{
			Viewport->GetVerticalScrollBar()->SetState(0.0f, VRatio);
		}
	}

	if (HRatio < 1.0f)
	{
		if (HDistFromBottom < 1.0f)
		{
			Viewport->GetHorizontalScrollBar()->SetState(FMath::Clamp((1.0f + HDistFromTop - HDistFromBottom - HRatio) * 0.5f, 0.0f, 1.0f - HRatio), HRatio);
		}
		else
		{
			Viewport->GetHorizontalScrollBar()->SetState(0.0f, HRatio);
		}
	}
}


FVector2D FTextureEditorViewportClient::GetViewportScrollBarPositions() const
{
	FVector2D Positions = FVector2D::ZeroVector;
	if (TextureEditorViewportPtr.Pin()->GetVerticalScrollBar().IsValid() && TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar().IsValid())
	{
		int32 Width, Height, Depth, ArraySize;
		UTexture* Texture = TextureEditorPtr.Pin()->GetTexture();
		float VRatio = GetViewportVerticalScrollBarRatio();
		float HRatio = GetViewportHorizontalScrollBarRatio();
		float VDistFromTop = TextureEditorViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromTop();
		float VDistFromBottom = TextureEditorViewportPtr.Pin()->GetVerticalScrollBar()->DistanceFromBottom();
		float HDistFromTop = TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromTop();
		float HDistFromBottom = TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar()->DistanceFromBottom();
	
		TextureEditorPtr.Pin()->CalculateTextureDimensions(Width, Height, Depth, ArraySize, true);

		if ((TextureEditorViewportPtr.Pin()->GetVerticalScrollBar()->GetVisibility() == EVisibility::Visible) && VDistFromBottom < 1.0f)
		{
			Positions.Y = FMath::Clamp((1.0f + VDistFromTop - VDistFromBottom - VRatio) * 0.5f, 0.0f, 1.0f - VRatio) * Height;
		}
		else
		{
			Positions.Y = 0.0f;
		}

		if ((TextureEditorViewportPtr.Pin()->GetHorizontalScrollBar()->GetVisibility() == EVisibility::Visible) && HDistFromBottom < 1.0f)
		{
			Positions.X = FMath::Clamp((1.0f + HDistFromTop - HDistFromBottom - HRatio) * 0.5f, 0.0f, 1.0f - HRatio) * Width;
		}
		else
		{
			Positions.X = 0.0f;
		}
	}

	return Positions;
}

void FTextureEditorViewportClient::DestroyCheckerboardTexture()
{
	if (CheckerboardTexture)
	{
		if (CheckerboardTexture->GetResource())
		{
			CheckerboardTexture->ReleaseResource();
		}
		CheckerboardTexture->MarkAsGarbage();
		CheckerboardTexture = NULL;
	}
}