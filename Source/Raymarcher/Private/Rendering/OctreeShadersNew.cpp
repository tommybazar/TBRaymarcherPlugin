#include "Rendering/OctreeShadersNew.h"

// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "ShaderParameterStruct.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Runtime/RenderCore/Public/RenderUtils.h"
#include "Util/UtilityShaders.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_GLOBAL_SHADER(
    FGenerateLevelZeroOctreeShader, "/Raymarcher/Private/GenerateOctreeLevelZeroShader.usf", "MainComputeShader", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(
    FGenerateNextLevelOctreeShader, "/Raymarcher/Private/FGenerateNextLevelOctreeShader.usf", "MainComputeShader", SF_Compute);

// For making statistics about GPU use - Generating Octree.
DECLARE_STATS_GROUP(TEXT("Utility shaders"), STATGROU_OCTREE_SHADER, STATCAT_Advanced);

DECLARE_FLOAT_COUNTER_STAT(TEXT("GeneratingOctree_New"), STAT_GPU_GeneratingOctree_New, STATGROU_OCTREE_SHADER);
DECLARE_GPU_STAT_NAMED(GPUGeneratingOctree_New, TEXT("GeneratingOctree_New"));

// #TODO profile with different dimensions.
#define OCTREE_NUM_THREADS_PER_GROUP_DIMENSION 1    // This has to be the same as in the compute shader's spec [X, X, X]
#define LEAF_NODE_SIZE 8                            // Provided to the shader as a uniform. #TODO could be dynamic

void GenerateOctreeForVolume_RenderThread_New(FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources)
{
    check(IsInRenderingThread());
    int32 GroupSizePerDimension = OCTREE_NUM_THREADS_PER_GROUP_DIMENSION * LEAF_NODE_SIZE;

    // For GPU profiling.
    SCOPED_DRAW_EVENTF(RHICmdList, GenerateOctreeForVolume_RenderThread, TEXT("GeneratingOctree"));
    SCOPED_GPU_STAT(RHICmdList, GPUGeneratingOctree_New);

    // Start with generating the zeroth Mip (the leaves) with a compute shader specifically for that.

    TShaderMapRef<FGenerateLevelZeroOctreeShader> ZeroLevelShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
    FRHIComputeShader* ZeroLevelShaderRHI = ZeroLevelShader.GetComputeShader();
    SetComputePipelineState(RHICmdList, ZeroLevelShaderRHI);

    FTexture3DComputeResource* OctreeRTResource = Resources.OctreeVolumeRenderTarget->MippedTexture3DRTResource;
    const int32 NumMips = Resources.OctreeVolumeRenderTarget->NumMips;

    for (int32 Mip = 0; Mip < NumMips; ++Mip)
    {
        RHICmdList.Transition(FRHITransitionInfo(OctreeRTResource->UnorderedAccessViewRHIs[Mip], ERHIAccess::Unknown,
            ERHIAccess::UAVCompute));
    }

    ZeroLevelShader->SetGeneratingResources(RHICmdList, ZeroLevelShaderRHI,
        Resources.DataVolumeTextureRef->GetResource()->TextureRHI->GetTexture3D(),
        Resources.OctreeVolumeRenderTarget->MippedTexture3DRTResource, LEAF_NODE_SIZE,
        Resources.OctreeVolumeRenderTarget->GetNumMips());

    uint32 GroupSizeX = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeX, GroupSizePerDimension);
    uint32 GroupSizeY = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeY, GroupSizePerDimension);
    uint32 GroupSizeZ = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeZ, GroupSizePerDimension);
    RHICmdList.DispatchComputeShader(GroupSizeX, GroupSizeY, GroupSizeZ);

    UnsetShaderUAVs(RHICmdList, ZeroLevelShader, ZeroLevelShaderRHI);
    UnsetShaderSRVs(RHICmdList, ZeroLevelShader, ZeroLevelShaderRHI);

    // Generate the other Mips from 1 to NumMips with a different shader.

    TShaderMapRef<FGenerateNextLevelOctreeShader> NextLevelsShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
    FRHIComputeShader* NextLevelsShaderRHI = NextLevelsShader.GetComputeShader();
    SetComputePipelineState(RHICmdList, NextLevelsShaderRHI);

    for (int i = 1; i <= NumMips; ++i)
    {
        // With every higher mip, we need fewer groups, so multiply GroupSize to correspond to that.
        GroupSizePerDimension *= 2;

        const int HighMip = i;
        const int LowMip = i - 1;
        // Transition lower-level MIP UAV to read-only to make sure that all writes from previous level are finished before next level runs
        RHICmdList.Transition(FRHITransitionInfo(OctreeRTResource->UnorderedAccessViewRHIs[LowMip], ERHIAccess::UAVCompute,
            ERHIAccess::SRVCompute));
        NextLevelsShader->SetGeneratingResources(RHICmdList, NextLevelsShaderRHI,
            Resources.OctreeVolumeRenderTarget->MippedTexture3DRTResource->UnorderedAccessViewRHIs[HighMip],
            Resources.OctreeVolumeRenderTarget->MippedTexture3DRTResource->ShaderResourceViewRHIs[LowMip], LEAF_NODE_SIZE);
        GroupSizeX = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeX, GroupSizePerDimension);
        GroupSizeY = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeY, GroupSizePerDimension);
        GroupSizeZ = FMath::DivideAndRoundUp(Resources.OctreeVolumeRenderTarget->SizeZ, GroupSizePerDimension);
        
        RHICmdList.DispatchComputeShader(GroupSizeX, GroupSizeY, GroupSizeZ);
    }

    UnsetShaderUAVs(RHICmdList, NextLevelsShader, NextLevelsShaderRHI);
    UnsetShaderSRVs(RHICmdList, NextLevelsShader, NextLevelsShaderRHI);

    for (int32 Mip = 0; Mip < NumMips; ++Mip)
    {
        // #TODO Double-check this if it's alright
        RHICmdList.Transition(FRHITransitionInfo(OctreeRTResource->TextureRHI, ERHIAccess::SRVCompute, ERHIAccess::SRVGraphics));
    }
}

#undef LOCTEXT_NAMESPACE