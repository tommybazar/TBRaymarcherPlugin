// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "Rendering/OctreeShaders.h"

#include "Engine/TextureRenderTargetVolume.h"
#include "Runtime/RenderCore/Public/RenderUtils.h"
#include "Util/UtilityShaders.h"

#if !UE_BUILD_SHIPPING
#pragma optimize("", off)
#endif

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_GLOBAL_SHADER(FGenerateOctreeShader, "/Raymarcher/Private/GenerateOctreeShader.usf", "MainComputeShader", SF_Compute);

// For making statistics about GPU use - Generating Octree.
DECLARE_FLOAT_COUNTER_STAT(TEXT("GeneratingOctree"), STAT_GPU_GeneratingOctree, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUGeneratingOctree, TEXT("GeneratingOctree_"));

// #TODO profile with different dimensions.
#define OCTREE_NUM_THREADS_PER_GROUP_DIMENSION 1	// This has to be the same as in the compute shader's spec [X, X, X]
#define LEAF_NODE_SIZE 8							// Provided to the shader as a uniform.

void GenerateOctreeForVolume_RenderThread(FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources)
{
	check(IsInRenderingThread());
	constexpr int32 GroupSizePerDimension = OCTREE_NUM_THREADS_PER_GROUP_DIMENSION * LEAF_NODE_SIZE;

	// For GPU profiling.
	SCOPED_DRAW_EVENTF(RHICmdList, GenerateOctreeForVolume_RenderThread, TEXT("GeneratingOctree"));
	SCOPED_GPU_STAT(RHICmdList, GPUGeneratingOctree);

	TShaderMapRef<FGenerateOctreeShader> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ShaderRHI);
	RHICmdList.Transition(FRHITransitionInfo(Resources.OctreeUAVRef, ERHIAccess::UAVGraphics, ERHIAccess::UAVCompute));

	ComputeShader->SetGeneratingResources(RHICmdList, ShaderRHI,
		Resources.DataVolumeTextureRef->GetResource()->TextureRHI->GetTexture3D(),
		Resources.OctreeVolumeRenderTarget->MippedTexture3DRTResource, LEAF_NODE_SIZE,
		Resources.OctreeVolumeRenderTarget->GetNumMips());

	const uint32 GroupSizeX = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeX, GroupSizePerDimension);
	const uint32 GroupSizeY = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeY, GroupSizePerDimension);
	const uint32 GroupSizeZ = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeZ, GroupSizePerDimension);
	RHICmdList.DispatchComputeShader(GroupSizeX, GroupSizeY, GroupSizeZ);

	ComputeShader->UnbindResources(RHICmdList, ShaderRHI);
	RHICmdList.Transition(FRHITransitionInfo(Resources.OctreeUAVRef, ERHIAccess::UAVCompute, ERHIAccess::UAVGraphics));
}

#undef LOCTEXT_NAMESPACE

#if !UE_BUILD_SHIPPING
#pragma optimize("", on)
#endif
