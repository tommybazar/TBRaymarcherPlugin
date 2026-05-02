// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "Rendering/RaymarchTypes.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameters.h"
#include "VolumeAsset/WindowingParameters.h"

void AddDirLightToSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources,
	const FDirLightParameters LightParameters, const bool Added, const FRaymarchWorldParameters WorldParameters);

void ChangeDirLightInSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
	FBasicRaymarchRenderingResources Resources, const FDirLightParameters OldLightParameters,
	const FDirLightParameters NewLightParameters, const FRaymarchWorldParameters WorldParameters);

// A shader implementing adding or removing a single directional light.
// (As opposed to changing [e.g. add and remove at the same time] a directional light)
// Only adds the bAdded boolean for toggling adding/removing a light.
class FAddDirLightShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FAddDirLightShader, Global, RAYMARCHER_API);

public:
	FAddDirLightShader() : FGlobalShader()
	{
	}

	~FAddDirLightShader(){};

	FAddDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
		VolumeSampler.Bind(Initializer.ParameterMap, TEXT("VolumeSampler"), SPF_Mandatory);

		TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"), SPF_Mandatory);
		TransferFuncSampler.Bind(Initializer.ParameterMap, TEXT("TransferFuncSampler"), SPF_Mandatory);

		LocalClippingCenter.Bind(Initializer.ParameterMap, TEXT("LocalClippingCenter"), SPF_Mandatory);
		LocalClippingDirection.Bind(Initializer.ParameterMap, TEXT("LocalClippingDirection"), SPF_Mandatory);

		WindowingParameters.Bind(Initializer.ParameterMap, TEXT("WindowingParameters"), SPF_Mandatory);
		StepSize.Bind(Initializer.ParameterMap, TEXT("StepSize"), SPF_Mandatory);

		PermutationMatrix.Bind(Initializer.ParameterMap, TEXT("PermutationMatrix"), SPF_Mandatory);
		// Actual light volume
		ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"), SPF_Mandatory);

		// Offsets to get location to read from from previous layer.
		PrevPixelOffset.Bind(Initializer.ParameterMap, TEXT("PrevPixelOffset"), SPF_Mandatory);
		UVWOffset.Bind(Initializer.ParameterMap, TEXT("UVWOffset"), SPF_Mandatory);

		// Multiplier for adding or removing light.
		bAdded.Bind(Initializer.ParameterMap, TEXT("bAdded"), SPF_Mandatory);
		Loop.Bind(Initializer.ParameterMap, TEXT("Loop"), SPF_Mandatory);
		// Read buffer and sampler.
		ReadBuffer.Bind(Initializer.ParameterMap, TEXT("ReadBuffer"), SPF_Mandatory);
		ReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("ReadBufferSampler"), SPF_Mandatory);
		// Write buffer.
		WriteBuffer.Bind(Initializer.ParameterMap, TEXT("WriteBuffer"), SPF_Mandatory);
	}

	void SetAllParameters(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, bool bLightAdded,
		const FTextureRHIRef pVolume, const FTextureRHIRef pTransferFunc, FWindowingParameters WindowingParams,
		FClippingPlaneParameters LocalClippingParams, FLinearColor pWindowingParameters, float pStepSize,
		FUnorderedAccessViewRHIRef pALightVolume, FMatrix PermMatrix, FVector2D PixelOffset, FVector pUVWOffset,
		const int loopIndex, const FTextureRHIRef pReadBuffer, const FSamplerStateRHIRef pReadBuffSampler,
		const FUnorderedAccessViewRHIRef pWriteBuffer)
	{
		float ZeroTFValue = WindowingParams.Center - 0.5 * WindowingParams.Width;
		FLinearColor VolumeClearColor = FLinearColor(ZeroTFValue, 0.0, 0.0, 0.0);
		const uint32 BorderColorInt = VolumeClearColor.ToFColor(false).ToPackedARGB();
		FSamplerStateRHIRef DataVolumeSamplerRef = RHICreateSamplerState(
			FSamplerStateInitializerRHI(SF_Trilinear, AM_Border, AM_Border, AM_Border, 0, 1, 0, 0, BorderColorInt));
		FSamplerStateRHIRef TFSamplerRef = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();

		// Light added/removed
		SetShaderValue(Params, bAdded, bLightAdded ? 1 : -1);

		// Raymarch resources
		SetTextureParameter(Params, Volume, pVolume);
		SetSamplerParameter(Params, VolumeSampler, DataVolumeSamplerRef);
		SetTextureParameter(Params, TransferFunc, pTransferFunc);
		SetSamplerParameter(Params, TransferFuncSampler, TFSamplerRef);

		// Raymarch parameters
		SetShaderValue(Params, LocalClippingCenter, FVector3f(LocalClippingParams.Center));
		SetShaderValue(Params, LocalClippingDirection, FVector3f(LocalClippingParams.Direction));
		SetShaderValue(Params, WindowingParameters, pWindowingParameters);

		// Step size
		SetShaderValue(Params, StepSize, pStepSize);

		// Light volume
		SetUAVParameter(Params, ALightVolume, pALightVolume);

		// Permutation matrix
		SetShaderValue(Params, PermutationMatrix, FMatrix44f(PermMatrix));

		// UV/UVW offsets
		SetShaderValue(Params, PrevPixelOffset, FVector2f(PixelOffset));
		SetShaderValue(Params, UVWOffset, FVector3f(pUVWOffset));

		// Loop parameters
		SetShaderValue(Params, Loop, loopIndex);
		SetUAVParameter(Params, WriteBuffer, pWriteBuffer);
		SetTextureParameter(Params, ReadBuffer, pReadBuffer);
		SetSamplerParameter(Params, ReadBufferSampler, pReadBuffSampler);

		RHICmdList.SetBatchedShaderParameters(ShaderRHI, Params);
	}

protected:
	// Volume texture + transfer function resource parameters
	LAYOUT_FIELD(FShaderResourceParameter, Volume);
	LAYOUT_FIELD(FShaderResourceParameter, VolumeSampler);
	LAYOUT_FIELD(FShaderResourceParameter, TransferFunc);
	LAYOUT_FIELD(FShaderResourceParameter, TransferFuncSampler);
	// Clipping uniforms
	LAYOUT_FIELD(FShaderParameter, LocalClippingCenter);
	LAYOUT_FIELD(FShaderParameter, LocalClippingDirection);
	// TF intensity Domain
	LAYOUT_FIELD(FShaderParameter, WindowingParameters);
	// Step size taken each iteration
	LAYOUT_FIELD(FShaderParameter, StepSize);
	// Permutation matrix - used to get position in the volume from axis-aligned X,Y and loop index.
	LAYOUT_FIELD(FShaderParameter, PermutationMatrix);
	// Light volume to modify.
	LAYOUT_FIELD(FShaderResourceParameter, ALightVolume);
	// Tells the shader the pixel offset for reading from the previous loop's buffer
	LAYOUT_FIELD(FShaderParameter, PrevPixelOffset);
	// And the offset in the volume from the previous volume sample.
	LAYOUT_FIELD(FShaderParameter, UVWOffset);
	// Parameter for the added/removed multiplier.
	LAYOUT_FIELD(FShaderParameter, bAdded);
	// The current loop index of this shader run.
	LAYOUT_FIELD(FShaderParameter, Loop);
	// Read buffer texture and sampler.
	LAYOUT_FIELD(FShaderResourceParameter, ReadBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, ReadBufferSampler);
	// Write buffer UAV.
	LAYOUT_FIELD(FShaderResourceParameter, WriteBuffer);
};

// A shader implementing changing a light in one pass.
// Works by subtracting the old light and adding the new one.
// Notice the UE macro DECLARE_SHADER_TYPE, unlike the shaders above (which are abstract)
// this one actually gets implemented.
class FChangeDirLightShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FChangeDirLightShader, Global, RAYMARCHER_API);

public:
	FChangeDirLightShader() : FGlobalShader()
	{
	}
	~FChangeDirLightShader(){};

	FChangeDirLightShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
		VolumeSampler.Bind(Initializer.ParameterMap, TEXT("VolumeSampler"), SPF_Mandatory);

		TransferFunc.Bind(Initializer.ParameterMap, TEXT("TransferFunc"), SPF_Mandatory);
		TransferFuncSampler.Bind(Initializer.ParameterMap, TEXT("TransferFuncSampler"), SPF_Mandatory);

		LocalClippingCenter.Bind(Initializer.ParameterMap, TEXT("LocalClippingCenter"), SPF_Mandatory);
		LocalClippingDirection.Bind(Initializer.ParameterMap, TEXT("LocalClippingDirection"), SPF_Mandatory);

		WindowingParameters.Bind(Initializer.ParameterMap, TEXT("WindowingParameters"), SPF_Mandatory);
		StepSize.Bind(Initializer.ParameterMap, TEXT("StepSize"), SPF_Mandatory);

		Loop.Bind(Initializer.ParameterMap, TEXT("Loop"), SPF_Optional);
		PermutationMatrix.Bind(Initializer.ParameterMap, TEXT("PermutationMatrix"), SPF_Mandatory);

		// Read buffer and sampler.
		ReadBuffer.Bind(Initializer.ParameterMap, TEXT("ReadBuffer"), SPF_Optional);
		ReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("ReadBufferSampler"), SPF_Optional);
		// Write buffer.
		WriteBuffer.Bind(Initializer.ParameterMap, TEXT("WriteBuffer"), SPF_Optional);
		// Actual light volume
		ALightVolume.Bind(Initializer.ParameterMap, TEXT("ALightVolume"), SPF_Mandatory);

		// Offsets to get location to read from from previous layer.
		PrevPixelOffset.Bind(Initializer.ParameterMap, TEXT("PrevPixelOffset"), SPF_Mandatory);
		UVWOffset.Bind(Initializer.ParameterMap, TEXT("UVWOffset"), SPF_Mandatory);

		// Volume texture + Transfer function uniforms
		RemovedPrevPixelOffset.Bind(Initializer.ParameterMap, TEXT("RemovedPrevPixelOffset"), SPF_Mandatory);
		RemovedReadBuffer.Bind(Initializer.ParameterMap, TEXT("RemovedReadBuffer"), SPF_Mandatory);
		RemovedReadBufferSampler.Bind(Initializer.ParameterMap, TEXT("RemovedReadBufferSampler"), SPF_Mandatory);
		RemovedWriteBuffer.Bind(Initializer.ParameterMap, TEXT("RemovedWriteBuffer"), SPF_Mandatory);
		RemovedUVWOffset.Bind(Initializer.ParameterMap, TEXT("RemovedUVWOffset"), SPF_Mandatory);
		RemovedStepSize.Bind(Initializer.ParameterMap, TEXT("RemovedStepSize"), SPF_Mandatory);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	void SetAllParameters(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI,
		const FTextureRHIRef pVolume, const FTextureRHIRef pTransferFunc, FWindowingParameters WindowingParams,
		FClippingPlaneParameters LocalClippingParams, FLinearColor pWindowingParameters,
		FUnorderedAccessViewRHIRef pALightVolume, float pAddedStepSize, float pRemovedStepSize,
		FMatrix PermMatrix, FVector2D AddedPixelOffset, FVector2D RemovedPixelOffset,
		FVector pAddedUVWOffset, FVector pRemovedUVWOffset,
		const int loopIndex,
		const FTextureRHIRef pRemovedReadBuffer, const FSamplerStateRHIRef pRemovedReadBuffSampler,
		const FUnorderedAccessViewRHIRef pRemovedWriteBuffer, const FTextureRHIRef pAddedReadBuffer,
		const FSamplerStateRHIRef pAddedReadBuffSampler, const FUnorderedAccessViewRHIRef pAddedWriteBuffer)
	{
		// TODO as now this Set gets called every loop invocation, this setup should be moved out and not re-done for every loop!
		float ZeroTFValue = WindowingParams.Center - 0.5 * WindowingParams.Width;
		FLinearColor VolumeClearColor = FLinearColor(ZeroTFValue, 0.0, 0.0, 0.0);
		const uint32 BorderColorInt = VolumeClearColor.ToFColor(false).ToPackedARGB();
		FSamplerStateRHIRef DataVolumeSamplerRef = RHICreateSamplerState(
			FSamplerStateInitializerRHI(SF_Trilinear, AM_Border, AM_Border, AM_Border, 0, 1, 0, 0, BorderColorInt));
		FSamplerStateRHIRef TFSamplerRef = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();

		// Raymarch resources
		SetTextureParameter(Params, Volume, pVolume);
		SetSamplerParameter(Params, VolumeSampler, DataVolumeSamplerRef);
		SetTextureParameter(Params, TransferFunc, pTransferFunc);
		SetSamplerParameter(Params, TransferFuncSampler, TFSamplerRef);

		// Raymarch parameters
		SetShaderValue(Params, LocalClippingCenter, FVector3f(LocalClippingParams.Center));
		SetShaderValue(Params, LocalClippingDirection, FVector3f(LocalClippingParams.Direction));
		SetShaderValue(Params, WindowingParameters, pWindowingParameters);

		// Light volume
		SetUAVParameter(Params, ALightVolume, pALightVolume);

		// Step sizes
		SetShaderValue(Params, StepSize, pAddedStepSize);
		SetShaderValue(Params, RemovedStepSize, pRemovedStepSize);

		// Permutation matrix
		SetShaderValue(Params, PermutationMatrix, FMatrix44f(PermMatrix));

		// Pixel offsets
		SetShaderValue(Params, PrevPixelOffset, FVector2f(AddedPixelOffset));
		SetShaderValue(Params, RemovedPrevPixelOffset, FVector2f(RemovedPixelOffset));

		// UVW offsets
		SetShaderValue(Params, UVWOffset, FVector3f(pAddedUVWOffset));
		SetShaderValue(Params, RemovedUVWOffset, FVector3f(pRemovedUVWOffset));

		// Loop parameters - added buffers
		SetShaderValue(Params, Loop, loopIndex);
		SetUAVParameter(Params, WriteBuffer, pAddedWriteBuffer);
		SetTextureParameter(Params, ReadBuffer, pAddedReadBuffer);
		SetSamplerParameter(Params, ReadBufferSampler, pAddedReadBuffSampler);

		// Loop parameters - removed buffers
		SetUAVParameter(Params, RemovedWriteBuffer, pRemovedWriteBuffer);
		SetTextureParameter(Params, RemovedReadBuffer, pRemovedReadBuffer);
		SetSamplerParameter(Params, RemovedReadBufferSampler, pRemovedReadBuffSampler);

		RHICmdList.SetBatchedShaderParameters(ShaderRHI, Params);
	}

protected:
	// Volume texture + transfer function resource parameters
	LAYOUT_FIELD(FShaderResourceParameter, Volume);
	LAYOUT_FIELD(FShaderResourceParameter, VolumeSampler);
	LAYOUT_FIELD(FShaderResourceParameter, TransferFunc);
	LAYOUT_FIELD(FShaderResourceParameter, TransferFuncSampler);
	// Clipping uniforms
	LAYOUT_FIELD(FShaderParameter, LocalClippingCenter);
	LAYOUT_FIELD(FShaderParameter, LocalClippingDirection);
	// TF intensity Domain
	LAYOUT_FIELD(FShaderParameter, WindowingParameters);
	// Step size taken each iteration
	LAYOUT_FIELD(FShaderParameter, StepSize);

	// The current loop index of this shader run.
	LAYOUT_FIELD(FShaderParameter, Loop);
	// Permutation matrix - used to get position in the volume from axis-aligned X,Y and loop index.
	LAYOUT_FIELD(FShaderParameter, PermutationMatrix);
	// Read buffer texture and sampler.
	LAYOUT_FIELD(FShaderResourceParameter, ReadBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, ReadBufferSampler);
	// Write buffer UAV.
	LAYOUT_FIELD(FShaderResourceParameter, WriteBuffer);
	// Light volume to modify.
	LAYOUT_FIELD(FShaderResourceParameter, ALightVolume);

	// Tells the shader the pixel offset for reading from the previous loop's buffer
	LAYOUT_FIELD(FShaderParameter, PrevPixelOffset);
	// And the offset in the volume from the previous volume sample.
	LAYOUT_FIELD(FShaderParameter, UVWOffset);

	// Same collection of parameters as for a "add dir light" shader, but these ones are the ones of
	// the removed light.

	// Tells the shader the pixel offset for reading from the previous loop's buffer
	LAYOUT_FIELD(FShaderParameter, RemovedPrevPixelOffset);

	LAYOUT_FIELD(FShaderResourceParameter, RemovedReadBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, RemovedReadBufferSampler);
	// Write buffer UAV.
	LAYOUT_FIELD(FShaderResourceParameter, RemovedWriteBuffer);
	// Removed light step size (is different than added one's)
	LAYOUT_FIELD(FShaderParameter, RemovedStepSize);
	// Removed light UVW offset
	LAYOUT_FIELD(FShaderParameter, RemovedUVWOffset);
};