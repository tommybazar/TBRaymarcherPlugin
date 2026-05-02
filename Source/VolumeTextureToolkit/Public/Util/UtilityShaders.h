// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "SceneUtils.h"
#include "Shader.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameters.h"

void VOLUMETEXTURETOOLKIT_API ClearVolumeTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList, FRHITexture* ALightVolumeResource, float ClearValue);

void VOLUMETEXTURETOOLKIT_API Clear2DTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList, FRHIUnorderedAccessView* TextureRW, FIntPoint TextureSize, float Value);
// void ClearVolumeTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* ALightVolumeResource, float
// ClearValue);

// Compute shader for clearing a single-channel 2D float RW texture
class FClearFloatRWTextureCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearFloatRWTextureCS, Global, VOLUMETEXTURETOOLKIT_API);

public:
	FClearFloatRWTextureCS() : FGlobalShader()
	{
	}
	FClearFloatRWTextureCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		ClearValue.Bind(Initializer.ParameterMap, TEXT("ClearValue"), SPF_Mandatory);
		ClearTexture2DRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureRW"), SPF_Mandatory);
	}

	void SetParameters(FRHICommandList& RHICmdList, FRHIComputeShader* ShaderRHI, FRHIUnorderedAccessView* TextureRW, float Value)
	{
		FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();
		SetUAVParameter(Params, ClearTexture2DRW, TextureRW);
		SetShaderValue(Params, ClearValue, Value);
		RHICmdList.SetBatchedShaderParameters(ShaderRHI, Params);
	}

	void UnbindUAV(FRHICommandList& RHICmdList, FRHIComputeShader* ShaderRHI)
	{
		// No-op: resource transitions handle state management on Vulkan/D3D12.
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	const FShaderParameter& GetClearColorParameter()
	{
		return ClearValue;
	}

	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearTexture2DRW;
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, ClearTexture2DRW);
	LAYOUT_FIELD(FShaderParameter, ClearValue);
};

// Compute Shader used for fast clearing of RW volume textures.
class FClearVolumeTextureShaderCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearVolumeTextureShaderCS, Global, VOLUMETEXTURETOOLKIT_API);

public:
	FClearVolumeTextureShaderCS() : FGlobalShader()
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FClearVolumeTextureShaderCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
		ClearValue.Bind(Initializer.ParameterMap, TEXT("ClearValue"), SPF_Mandatory);
		ZSize.Bind(Initializer.ParameterMap, TEXT("ZSize"), SPF_Mandatory);
	}

	void SetParameters(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, FRHIUnorderedAccessView* VolumeRef, float clearColor, int ZSizeParam)
	{
		FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();
		SetUAVParameter(Params, Volume, VolumeRef);
		SetShaderValue(Params, ClearValue, clearColor);
		SetShaderValue(Params, ZSize, ZSizeParam);
		RHICmdList.SetBatchedShaderParameters(ShaderRHI, Params);
	}

	void UnbindUAV(FRHICommandList& RHICmdList, FRHIComputeShader* ShaderRHI)
	{
		// No-op: resource transitions handle state management on Vulkan/D3D12.
	}
	
protected:
	// Float values to be set to the alpha volume.
	LAYOUT_FIELD(FShaderResourceParameter, Volume);
	LAYOUT_FIELD(FShaderParameter, ClearValue);
	LAYOUT_FIELD(FShaderParameter, ZSize);
};
