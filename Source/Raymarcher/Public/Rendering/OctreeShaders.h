// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

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
		OctreeVolume2.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeMip2"), SPF_Mandatory);
		OctreeVolume3.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeMip3"), SPF_Mandatory);
		NodeSize.Bind(Initializer.ParameterMap, TEXT("NodeSize"), SPF_Mandatory);
		NumberOfMips.Bind(Initializer.ParameterMap, TEXT("NumberOfMips"), SPF_Mandatory);
	}
		
	void SetGeneratingResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const FTextureRHIRef pVolume,
		const FTexture3DComputeResource* ComputeResource, int InNodeSize, int InNumberOfMips)
	{
		FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();
		SetTextureParameter(Params, Volume, pVolume);
		SetUAVParameter(Params, OctreeVolume0, ComputeResource->UnorderedAccessViewRHIs[0]);
		SetUAVParameter(Params, OctreeVolume1, ComputeResource->UnorderedAccessViewRHIs[1]);
		SetUAVParameter(Params, OctreeVolume2, ComputeResource->UnorderedAccessViewRHIs[2]);
		SetUAVParameter(Params, OctreeVolume3, ComputeResource->UnorderedAccessViewRHIs[3]);
		SetShaderValue(Params, NodeSize, InNodeSize);
		SetShaderValue(Params, NumberOfMips, InNumberOfMips);
		RHICmdList.SetBatchedShaderParameters(ShaderRHI, Params);
	}

protected:
    // Volume texture + transfer function resource parameters
    LAYOUT_FIELD(FShaderResourceParameter, Volume);

    // OctreeVolume volume mips to modify.
    LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume0);
    LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume1);
    LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume2);
    LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume3);

    // Length of the size of the cube that creates a single leaf. (Each leaf node will have NodeSize^3 voxels)
    LAYOUT_FIELD(FShaderParameter, NodeSize);

    // Number of mips to generate.
    LAYOUT_FIELD(FShaderParameter, NumberOfMips)
};