// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "Rendering/RaymarchTypes.h"
#include "ShaderParameterUtils.h"
#include "ShaderParameters.h"

void GenerateOctreeForVolume_RenderThread(FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources);

// A shader that generates a TF-independent octree accelerator structure for a volume.
class FGenerateOctreeShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FGenerateOctreeShader, Global, RAYMARCHER_API);

public:
	FGenerateOctreeShader() : FGlobalShader()
	{
	}

	~FGenerateOctreeShader(){};

	FGenerateOctreeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
		OctreeVolume.Bind(Initializer.ParameterMap, TEXT("OctreeVolume"), SPF_Mandatory);
		MinMaxValues.Bind(Initializer.ParameterMap, TEXT("MinMaxValues"), SPF_Mandatory);
	}
		
	void SetGeneratingResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const FTexture3DRHIRef pVolume,
		const FUnorderedAccessViewRHIRef pOctreeVolume)
	{
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, pVolume);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume, pOctreeVolume);
		SetShaderValue(RHICmdList, ShaderRHI, MinMaxValues, FVector2f(0.0, 1.0));
	}

	void UnbindResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI)
	{
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, nullptr);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume, nullptr);
	}

protected:
	// Volume texture + transfer function resource parameters
	LAYOUT_FIELD(FShaderResourceParameter, Volume);

	// OctreeVolume volume to modify.
	LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume);
	
	// Parameter for the added/removed multiplier.
	LAYOUT_FIELD(FShaderParameter, MinMaxValues);
};