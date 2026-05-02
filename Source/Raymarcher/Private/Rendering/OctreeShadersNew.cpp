#include "Rendering/OctreeShadersNew.h"

// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "Engine/TextureRenderTargetVolume.h"
#include "Runtime/RenderCore/Public/RenderUtils.h"
#include "Util/UtilityShaders.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_GLOBAL_SHADER(
    FGenerateLevelZeroOctreeShader, "/Raymarcher/Private/GenerateOctreeLevelZeroShader.usf", "MainComputeShader", SF_Compute);

// For making statistics about GPU use - Generating Octree.
DECLARE_FLOAT_COUNTER_STAT(TEXT("GeneratingOctree_New"), STAT_GPU_GeneratingOctree_New, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUGeneratingOctree_New, TEXT("GeneratingOctree_New"));

// #TODO profile with different dimensions.
#define OCTREE_NUM_THREADS_PER_GROUP_DIMENSION 1    // This has to be the same as in the compute shader's spec [X, X, X]
#define LEAF_NODE_SIZE 8                            // Provided to the shader as a uniform.

void GenerateOctreeForVolume_RenderThread_New(FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources)
{
    check(IsInRenderingThread());
    constexpr int32 GroupSizePerDimension = OCTREE_NUM_THREADS_PER_GROUP_DIMENSION * LEAF_NODE_SIZE;

    // For GPU profiling.
    SCOPED_DRAW_EVENTF(RHICmdList, GenerateOctreeForVolume_RenderThread, TEXT("GeneratingOctree"));
    SCOPED_GPU_STAT(RHICmdList, GPUGeneratingOctree_New);

    TShaderMapRef<FGenerateLevelZeroOctreeShader> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
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