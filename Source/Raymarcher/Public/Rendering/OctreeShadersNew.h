 // Copyright 2024 Tomas Bartipan
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

void GenerateOctreeForVolume_RenderThread_New(FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources);

// A shader that generates the zeroth level (the leaves) of an octree from a volume.
class FGenerateLevelZeroOctreeShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FGenerateLevelZeroOctreeShader, Global, RAYMARCHER_API);

public:
	FGenerateLevelZeroOctreeShader() : FGlobalShader()
	{
	}

	~FGenerateLevelZeroOctreeShader(){};

	FGenerateLevelZeroOctreeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		Volume.Bind(Initializer.ParameterMap, TEXT("Volume"), SPF_Mandatory);
		OctreeVolume0.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeMip0"), SPF_Mandatory);
		MinMaxValues.Bind(Initializer.ParameterMap, TEXT("MinMaxValues"), SPF_Mandatory);
		NodeSize.Bind(Initializer.ParameterMap, TEXT("NodeSize"), SPF_Mandatory);
	}

	void SetGeneratingResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const FTextureRHIRef pVolume,
		const FTexture3DComputeResource* ComputeResource, int InNodeSize, int InNumberOfMips)
	{
		FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();
		SetTextureParameter(Params, Volume, pVolume);
		SetUAVParameter(Params, OctreeVolume0, ComputeResource->UnorderedAccessViewRHIs[0]);
		SetShaderValue(Params, MinMaxValues, FVector2f(0.0, 1.0));
		SetShaderValue(Params, NodeSize, InNodeSize);
	    RHICmdList.SetBatchedShaderParameters(ShaderRHI, Params);
	}

protected:
	// Volume texture + transfer function resource parameters
	LAYOUT_FIELD(FShaderResourceParameter, Volume);

	// OctreeVolume volume mip level 0 to modify.
	LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume0);

	// Parameter for min/max values allowed.
	LAYOUT_FIELD(FShaderParameter, MinMaxValues);

	// Length of the size of the cube that creates a single leaf. (Each leaf node will have NodeSize^3 voxels)
	LAYOUT_FIELD(FShaderParameter, NodeSize);
};

// A shader that generates an upper level of an octree from a lower level.
class FGenerateNextLevelOctreeShader : public FGlobalShader
{
    DECLARE_EXPORTED_SHADER_TYPE(FGenerateNextLevelOctreeShader, Global, RAYMARCHER_API);

public:
    FGenerateNextLevelOctreeShader() : FGlobalShader()
    {
    }

    ~FGenerateNextLevelOctreeShader(){};

    FGenerateNextLevelOctreeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
    {
        OctreeVolumeHigherMip.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeHigherMip"), SPF_Mandatory);
        OctreeVolumeLowerMip.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeLowerMip"), SPF_Mandatory);
        // NodeSize.Bind(Initializer.ParameterMap, TEXT("NodeSize"), SPF_Mandatory);
    }

    void SetGeneratingResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const FUnorderedAccessViewRHIRef pHigherMip,
        const FShaderResourceViewRHIRef pLowerMip, int InNodeSize)
    {
        FRHIBatchedShaderParameters& Params = RHICmdList.GetScratchShaderParameters();
        SetSRVParameter(Params, OctreeVolumeLowerMip, pLowerMip); // Read-only lower mip
        SetUAVParameter(Params, OctreeVolumeHigherMip, pHigherMip); // Write-only higher mip
        // SetShaderValue(Params, NodeSize, InNodeSize);
        RHICmdList.SetBatchedShaderParameters(ShaderRHI, Params);
    }

protected:
    // The higher MIP that is getting filled in this shader.
    LAYOUT_FIELD(FShaderResourceParameter, OctreeVolumeHigherMip);

    // The lower MIP that is being read from and "OR-red" into the higher MIP in this shader.
    LAYOUT_FIELD(FShaderResourceParameter, OctreeVolumeLowerMip);

    // // Length of the size of the cube (in the lower mip) that creates a single node (in the upper mip).
    // // (i.e. a cube of NodeSize^3 voxels gets compressed into one voxel in the higher mip)
    // LAYOUT_FIELD(FShaderParameter, NodeSize);
    // For now just assume 2 
};
