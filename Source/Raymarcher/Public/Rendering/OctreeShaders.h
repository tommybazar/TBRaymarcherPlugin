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
		OctreeVolume0.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeMip0"), SPF_Mandatory);
		OctreeVolume1.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeMip1"), SPF_Mandatory);
		MinMaxValues.Bind(Initializer.ParameterMap, TEXT("MinMaxValues"), SPF_Mandatory);
		LeafNodeSize.Bind(Initializer.ParameterMap, TEXT("LeafNodeSize"), SPF_Mandatory);
	}
		
	void SetGeneratingResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const FTexture3DRHIRef pVolume,
		const FTexture3DComputeResource* ComputeResource, int InLeafNodeSize)
	{
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, pVolume);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume0, ComputeResource->UnorderedAccessViewRHIs[0]);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume1, ComputeResource->UnorderedAccessViewRHIs[1]);
		SetShaderValue(RHICmdList, ShaderRHI, MinMaxValues, FVector2f(0.0, 1.0));
		SetShaderValue(RHICmdList, ShaderRHI, LeafNodeSize, InLeafNodeSize);
	}

	void UnbindResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI)
	{
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, nullptr);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume0, nullptr);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume1, nullptr);
	}

protected:
	// Volume texture + transfer function resource parameters
	LAYOUT_FIELD(FShaderResourceParameter, Volume);

	// OctreeVolume volume to modify.
	LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume0);

	// OctreeVolume volume to modify.
	LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume1);

	// Parameter for the added/removed multiplier.
	LAYOUT_FIELD(FShaderParameter, MinMaxValues);
	
	// Length of the size of the cube that creates a single leaf. (Each leaf node will have LeafNodeSize^3 voxels)
	LAYOUT_FIELD(FShaderParameter, LeafNodeSize);
};