// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "Util/RaymarchUtils.h"

#include "Containers/UnrealString.h"
#include "GlobalShader.h"
#include "Logging/MessageLog.h"
#include "PipelineStateCache.h"
#include "RHICommandList.h"
#include "RHIDefinitions.h"
#include "RHIStaticStates.h"
#include "Rendering/LightingShaders.h"
#include "Rendering/OctreeShaders.h"
#include "Rendering/RaymarchTypes.h"
#include "SceneInterface.h"
#include "SceneUtils.h"
#include "ShaderParameterUtils.h"
#include "VolumeTextureToolkit/Public/TextureUtilities.h"

#include <Engine/TextureRenderTargetVolume.h>

#include <bit>
#include <bitset>
#include <cstdio>

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

void URaymarchUtils::AddDirLightToSingleVolume(const FBasicRaymarchRenderingResources& Resources,
    const FDirLightParameters& LightParameters, const bool Added, const FRaymarchWorldParameters WorldParameters, bool& LightAdded,
    bool bGPUSync)
{
    if (!Resources.DataVolumeTextureRef || !Resources.DataVolumeTextureRef->GetResource() ||
        !Resources.TFTextureRef->GetResource() || !Resources.LightVolumeRenderTarget->GetResource() ||
        !Resources.DataVolumeTextureRef->GetResource()->TextureRHI || !Resources.TFTextureRef->GetResource()->TextureRHI ||
        !Resources.LightVolumeRenderTarget->GetResource()->TextureRHI)
    {
        LightAdded = false;
        return;
    }
    else
    {
        LightAdded = true;
    }

    if (bGPUSync)
    {
        // Call the actual rendering code on RenderThread.
        // #todo fix GPUSync'd version of shader.
        // ENQUEUE_RENDER_COMMAND(CaptureCommand)
        //([=](FRHICommandListImmediate& RHICmdList) {
        //	AddDirLightToSingleLightVolume_GPUSync_RenderThread(RHICmdList, Resources, LightParameters, Added, WorldParameters);
        //});
    }
    else
    {
        // Call the actual rendering code on RenderThread.
        ENQUEUE_RENDER_COMMAND(CaptureCommand)
        ([=](FRHICommandListImmediate& RHICmdList)
            { AddDirLightToSingleLightVolume_RenderThread(RHICmdList, Resources, LightParameters, Added, WorldParameters); });
    }
}

void URaymarchUtils::ChangeDirLightInSingleVolume(FBasicRaymarchRenderingResources& Resources,
    const FDirLightParameters OldLightParameters, const FDirLightParameters NewLightParameters,
    const FRaymarchWorldParameters WorldParameters, bool& LightAdded, bool bGpuSync)
{
    if (!Resources.DataVolumeTextureRef || !Resources.DataVolumeTextureRef->GetResource() ||
        !Resources.TFTextureRef->GetResource() || !Resources.LightVolumeRenderTarget->GetResource() ||
        !Resources.DataVolumeTextureRef->GetResource()->TextureRHI || !Resources.TFTextureRef->GetResource()->TextureRHI ||
        !Resources.LightVolumeRenderTarget->GetResource()->TextureRHI)
    {
        LightAdded = false;
        return;
    }
    else
    {
        LightAdded = true;
    }

    // Call the actual rendering code on RenderThread. We capture by value so that if
    ENQUEUE_RENDER_COMMAND(CaptureCommand)
    (
        [=](FRHICommandListImmediate& RHICmdList)
        {
            ChangeDirLightInSingleLightVolume_RenderThread(
                RHICmdList, Resources, OldLightParameters, NewLightParameters, WorldParameters);
        });
}

void URaymarchUtils::GenerateOctree(FBasicRaymarchRenderingResources& Resources)
{
    // Call the actual rendering code on RenderThread. We capture by value so that if
    ENQUEUE_RENDER_COMMAND(CaptureCommand)
    ([=](FRHICommandListImmediate& RHICmdList) { GenerateOctreeForVolume_RenderThread(RHICmdList, Resources); });
}

// Function to sample from the Texture2D. The U and V coordinate are normalized texture coodinates.
FFloat16Color SampleFromTexture(float U, float V, UTexture2D* TF)
{
    // Ensure the Texture2D is valid.
    if (TF == nullptr)
    {
        return {};
    }

    int32 TextureWidth = TF->GetSizeX();
    int32 TextureHeight = TF->GetSizeY();
    int32 X = FMath::Clamp(FMath::RoundToInt(U * TextureWidth), 0, TextureWidth - 1);
    int32 Y = FMath::Clamp(FMath::RoundToInt(V * TextureHeight), 0, TextureHeight - 1);

    // Get the texture data.
    FTexture2DMipMap* MipMap = &TF->GetPlatformData()->Mips[0];
    const void* TextureData = MipMap->BulkData.LockReadOnly();

    // Calculate the index in the texture data.
    int32 TextureDataIndex = (Y * MipMap->SizeX) + X;

    // Sample the color.
    const FFloat16Color* ColorData = static_cast<const FFloat16Color*>(TextureData);
    FFloat16Color SampleColor = ColorData[TextureDataIndex];

    // Unlock the texture data.
    MipMap->BulkData.Unlock();

    return SampleColor;
}

FVector4 URaymarchUtils::GetBitMaskFromWindowedTFCurve(
    FWindowingParameters WindowingParams, int EdgeBits, const UCurveLinearColor* CurveTF)
{
    // Minimum alpha to consider a color non-transparent
    constexpr float MINIMUM_ALPHA = 0.001f;
    // Get min and max window values
    float MinWindowVal = WindowingParams.GetValueFromWindowPosition(0.0f);
    float MaxWindowVal = WindowingParams.GetValueFromWindowPosition(1.0f);

    // Clamp the values since we do not expect negative value in currently rendered volume.
    float MinWindowValClamped = FMath::Clamp(MinWindowVal, 0.0f, 1.0f);
    float MaxWindowValClamped = FMath::Clamp(MaxWindowVal, 0.0f, 1.0f);

    // Define the maximal number of bits in bitmask window.
    static constexpr uint32_t MaxNumberOfBits = 31;
    const float Factor = 1.0 / static_cast<float>(MaxNumberOfBits);

    auto BitPositionForValue = [&](float Val) -> uint32_t { return static_cast<uint32_t>(Val / Factor); };

    uint32_t MinWindowBit = FMath::Clamp(BitPositionForValue(MinWindowValClamped), 0, MaxNumberOfBits);
    uint32_t MaxWindowBit = FMath::Clamp(BitPositionForValue(MaxWindowValClamped), 0, MaxNumberOfBits);

    uint32_t Result = 0;

    // Handle negative window sizes
    if (MinWindowBit > MaxWindowBit)
    {
        Swap(MinWindowBit, MaxWindowBit);
    }

    if (!WindowingParams.LowCutoff)
    {
        FLinearColor MinColor = CurveTF->GetLinearColorValue(0.0f);
        if (MinColor.A > MINIMUM_ALPHA)
        {
            // Bottom of TF is not transparent and we're not clipping low values -> need to mark all the low bits as
            // bits-of-interest
            for (uint32_t BitNum = 0; BitNum < MinWindowBit; BitNum++)
            {
                Result |= (1 << BitNum);
            }
        }
    }

    if (!WindowingParams.HighCutoff)
    {
        FLinearColor MaxColor = CurveTF->GetLinearColorValue(1.0f);
        if (MaxColor.A > MINIMUM_ALPHA)
        {
            // Top of TF is not transparent and we're not clipping higher values -> need to mark all the above bits as
            // bits-of-interest
            for (uint32_t BitNum = MaxWindowBit; BitNum <= MaxNumberOfBits; BitNum++)
            {
                Result |= (1 << BitNum);
            }
        }
    }

    // if (!((MinWindowVal > 1.0f && MaxWindowVal > 1.0f) || (MinWindowVal < 0.0f && MaxWindowVal < 0.0f)))
    {
        // Sample the current value from the curve and set to relevant bit to non-zero if the curve alpha is non-zero
        for (uint32_t BitNum = MinWindowBit; BitNum <= MaxWindowBit; BitNum++)
        {
            // Sample multiple times for each bit to make sure that we don't miss a non-zero part of the range
            // (e.g. TF could have alpha 0 at time 0, but alpha 0.1 at time 0.02, that would be missed without sampling multiple
            // times per-bucket)
            uint32_t SamplesPerBit = 8;
            float SamplingOffset = Factor / SamplesPerBit;
            for (uint32_t SampleNum = 0; SampleNum < SamplesPerBit; SampleNum++)
            {
                float PositionInWindow = (Factor * BitNum) + SamplingOffset;
                FLinearColor TFColor =
                    CurveTF->GetLinearColorValue(WindowingParams.GetPositionInWindow(PositionInWindow));
                if (TFColor.A > MINIMUM_ALPHA)
                {
                    Result |= (1 << BitNum);
                    break;    // Only breaks inner loop.
                }
            }
        }

        // Make the window mask bigger based on the edge Bits.
        for (int Shake = 0; Shake < EdgeBits; Shake++)
        {
            Result |= (Result << 1);
            Result |= (Result >> 1);
        }
    }

    // Use to output mask.
    std::bitset<32> bitmask(Result);
    GEngine->AddOnScreenDebugMessage(54, 100, FColor::Orange,
        FString::Printf(TEXT("Bitmask (min = right, max = left) : %s"), (UTF8_TO_TCHAR(bitmask.to_string().c_str()))));

    // we bitcast into a float so we can pass it through the material editor. It gets "bit-cast" back to uint in the shader
    const float FloatValue = std::bit_cast<float>(Result);
    return FVector4(FloatValue, 0, 0, 0);
}

void URaymarchUtils::ClearResourceLightVolumes(const FBasicRaymarchRenderingResources Resources, float ClearValue)
{
    if (!Resources.LightVolumeRenderTarget)
    {
        return;
    }
    UVolumeTextureToolkit::ClearVolumeTexture(Resources.LightVolumeRenderTarget, ClearValue);
}

RAYMARCHER_API void URaymarchUtils::MakeDefaultTFTexture(UTexture2D*& OutTexture)
{
    constexpr unsigned SampleCount = 256;

    // Give the texture some height, so it can be inspected in the asset editor.
    constexpr unsigned TextureHeight = 1;

    FFloat16* Samples = new FFloat16[SampleCount * 4 * TextureHeight];
    for (unsigned i = 0; i < SampleCount; i++)
    {
        float Whiteness = (float) i / (float) (SampleCount - 1);
        Samples[i * 4] = Whiteness;
        Samples[i * 4 + 1] = Whiteness;
        Samples[i * 4 + 2] = Whiteness;
        Samples[i * 4 + 3] = 1;
    }

    for (unsigned i = 1; i < TextureHeight; i++)
    {
        FMemory::Memcpy(Samples + (i * SampleCount * 4), Samples, SampleCount * 4 * 2);
    }

    UVolumeTextureToolkit::Create2DTextureTransient(
        OutTexture, PF_FloatRGBA, FIntPoint(SampleCount, TextureHeight), (uint8*) Samples);

    // Don't forget to free the memory here
    delete[] Samples;
}

void URaymarchUtils::ColorCurveToTexture(UCurveLinearColor* Curve, UTexture2D*& OutTexture)
{
    constexpr unsigned SampleCount = 256;

    // Give the texture some height, so it can be inspected in the asset editor. Possibly breaks cache consistency
    constexpr unsigned TextureHeight = 16;

    // Using float16 format because RGBA8 wouldn't be persistent for some reason.
    FFloat16* samples = new FFloat16[SampleCount * 4 * TextureHeight];

    for (unsigned i = 0; i < SampleCount; i++)
    {
        float index = ((float) i) / ((float) SampleCount - 1);
        FLinearColor picked = Curve->GetLinearColorValue(index);

        samples[i * 4] = picked.R;
        samples[i * 4 + 1] = picked.G;
        samples[i * 4 + 2] = picked.B;
        samples[i * 4 + 3] = picked.A;
    }

    for (unsigned i = 1; i < TextureHeight; i++)
    {
        FMemory::Memcpy(samples + (i * SampleCount * 4), samples, SampleCount * 4 * 2);
    }

    UVolumeTextureToolkit::Create2DTextureTransient(
        OutTexture, PF_FloatRGBA, FIntPoint(SampleCount, TextureHeight), (uint8*) samples);

    delete[] samples;    // Don't forget to free the memory here
    return;
}

void URaymarchUtils::CreateBufferTextures(FIntPoint Size, EPixelFormat PixelFormat, OneAxisReadWriteBufferResources& RWBuffers)
{
	if (Size.X == 0 || Size.Y == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Warning: Creating Buffer Textures: Size is Zero!"));
		return;
	}

	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("Illumination Buffer"), Size.X, Size.Y, PixelFormat);
	Desc.Flags |= TexCreate_ShaderResource | TexCreate_UAV;
	Desc.NumMips = 1;
	Desc.NumSamples = 1;
	
	for (int i = 0; i < 4; i++)
	{
		RWBuffers.Buffers[i] = RHICreateTexture(Desc);
		RWBuffers.UAVs[i] = GetCmdList().CreateUnorderedAccessView(
			RWBuffers.Buffers[i], FRHIViewDesc::CreateTextureUAV().SetDimensionFromTexture(RWBuffers.Buffers[i]));
	}
}

void URaymarchUtils::ReleaseOneAxisReadWriteBufferResources(OneAxisReadWriteBufferResources& Buffer)
{
    for (FUnorderedAccessViewRHIRef& UAV : Buffer.UAVs)
    {
        if (UAV)
        {
            UAV.SafeRelease();
        }
        UAV = nullptr;
    }

	for (FTextureRHIRef& TextureRef : Buffer.Buffers)
	{
		if (TextureRef)
		{
			TextureRef.SafeRelease();
		}
		TextureRef = nullptr;
	}
}

void URaymarchUtils::GetVolumeTextureDimensions(UVolumeTexture* Texture, FIntVector& Dimensions)
{
    if (Texture)
    {
        // This is slightly retarded...
        Dimensions = FIntVector(Texture->GetSizeX(), Texture->GetSizeY(), Texture->GetSizeZ());
    }
    else
    {
        Dimensions = FIntVector(0, 0, 0);
    }
}

void URaymarchUtils::TransformToMatrix(const FTransform Transform, FMatrix& OutMatrix, bool WithScaling)
{
    if (WithScaling)
    {
        OutMatrix = Transform.ToMatrixWithScale();
    }
    else
    {
        OutMatrix = Transform.ToMatrixNoScale();
    }
}

void URaymarchUtils::LocalToTextureCoords(FVector LocalCoords, FVector& TextureCoords)
{
    TextureCoords = (LocalCoords / 2.0f) + 0.5f;
}

void URaymarchUtils::TextureToLocalCoords(FVector TextureCoors, FVector& LocalCoords)
{
    LocalCoords = (TextureCoors - 0.5f) * 2.0f;
}

#undef LOCTEXT_NAMESPACE