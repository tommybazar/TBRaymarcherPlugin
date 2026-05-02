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

// A shader that generates the first level of an octree from a volume.
class FGenerateLevelZeroOctreeShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FGenerateLevelZeroOctreeShader, Global, RAYMARCHER_API);//

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
		LeafNodeSize.Bind(Initializer.ParameterMap, TEXT("LeafNodeSize"), SPF_Mandatory);
	}

	void SetGeneratingResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const FTexture3DRHIRef pVolume,
		const FTexture3DComputeResource* ComputeResource, int InLeafNodeSize, int InNumberOfMips)
	{
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, pVolume);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume0, ComputeResource->UnorderedAccessViewRHIs[0]);
		SetShaderValue(RHICmdList, ShaderRHI, MinMaxValues, FVector2f(0.0, 1.0));
		SetShaderValue(RHICmdList, ShaderRHI, LeafNodeSize, InLeafNodeSize);
	}

	void UnbindResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI)
	{
		SetTextureParameter(RHICmdList, ShaderRHI, Volume, nullptr);
		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume0, nullptr);
	}

protected:
	// Volume texture + transfer function resource parameters
	LAYOUT_FIELD(FShaderResourceParameter, Volume);

	// OctreeVolume volume mip level 0 to modify.
	LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume0);

	// Parameter for min/max values allowed.
	LAYOUT_FIELD(FShaderParameter, MinMaxValues);

	// Length of the size of the cube that creates a single leaf. (Each leaf node will have LeafNodeSize^3 voxels)
	LAYOUT_FIELD(FShaderParameter, LeafNodeSize);
};
//
// // A shader that generates the next level of an octree based on a previous one.
// class FGenerateNextLevelOCtreeShader : public FGlobalShader
// {
// 	DECLARE_EXPORTED_SHADER_TYPE(FGenerateLevelZeroOctreeShader, Global, RAYMARCHER_API);//
//
// public:
// 	FGenerateLevelZeroOctreeShader() : FGlobalShader()
// 	{
// 	}
//
// 	~FGenerateLevelZeroOctreeShader(){};
//
// 	FGenerateOctreeShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
// 	{
// 		OctreeVolume0.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeMip"), SPF_Mandatory);
// 		OctreeVolume0.Bind(Initializer.ParameterMap, TEXT("OctreeVolumeMipNext"), SPF_Mandatory);
// 		MinMaxValues.Bind(Initializer.ParameterMap, TEXT("MinMaxValues"), SPF_Mandatory);
// 		LeafNodeSize.Bind(Initializer.ParameterMap, TEXT("LeafNodeSize"), SPF_Mandatory);
// 	}
//
// 	void SetGeneratingResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const FTexture3DRHIRef pVolume,
// 		const FTexture3DComputeResource* ComputeResource, int InLeafNodeSize, int InNumberOfMips)
// 	{
// 		SetTextureParameter(RHICmdList, ShaderRHI, Volume, pVolume);
// 		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume0, ComputeResource->UnorderedAccessViewRHIs[0]);
// 		SetShaderValue(RHICmdList, ShaderRHI, MinMaxValues, FVector2f(0.0, 1.0));
// 		SetShaderValue(RHICmdList, ShaderRHI, LeafNodeSize, InLeafNodeSize);
// 	}
//
// 	void UnbindResources(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI)
// 	{
// 		SetTextureParameter(RHICmdList, ShaderRHI, Volume, nullptr);
// 		SetUAVParameter(RHICmdList, ShaderRHI, OctreeVolume0, nullptr);
// 	}
//
// protected:
// 	// Volume texture + transfer function resource parameters
// 	LAYOUT_FIELD(FShaderResourceParameter, Volume);
//
// 	// OctreeVolume volume mip level 0 to modify.
// 	LAYOUT_FIELD(FShaderResourceParameter, OctreeVolume0);
//
// 	// Parameter for min/max values allowed.
// 	LAYOUT_FIELD(FShaderParameter, MinMaxValues);
//
// 	// Length of the size of the cube that creates a single leaf. (Each leaf node will have LeafNodeSize^3 voxels)
// 	LAYOUT_FIELD(FShaderParameter, LeafNodeSize);
// };
//
