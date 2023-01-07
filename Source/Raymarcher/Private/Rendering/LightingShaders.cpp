// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "Rendering/LightingShaders.h"
#include "Rendering/LightingShaderUtils.h"

#include "RenderCore/Public/RenderUtils.h"
#include "Renderer/Public/VolumeRendering.h"
#include "Rendering/LightingShaders.h"

#include "Engine/TextureRenderTargetVolume.h"
#include "Util/UtilityShaders.h"

#if !UE_BUILD_SHIPPING
#pragma optimize("", off)
#endif

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_GLOBAL_SHADER(FAddDirLightShader, "/Raymarcher/Private/AddDirLightShader.usf", "MainComputeShader", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FChangeDirLightShader, "/Raymarcher/Private/ChangeDirLightShader.usf", "MainComputeShader", SF_Compute);

// For making statistics about GPU use - Adding Lights.
DECLARE_FLOAT_COUNTER_STAT(TEXT("AddingLights"), STAT_GPU_AddingLights, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUAddingLights, TEXT("AddingLightsToVolume"));

// For making statistics about GPU use - Changing Lights.
DECLARE_FLOAT_COUNTER_STAT(TEXT("ChangingLights"), STAT_GPU_ChangingLights, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUChangingLights, TEXT("ChangingLightsInVolume"));

// #TODO profile with different dimensions.
#define NUM_THREADS_PER_GROUP_DIMENSION 16	  // This has to be the same as in the compute shader's spec [X, X, 1]

void AddDirLightToSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources,
	const FDirLightParameters LightParameters, const bool Added, const FRaymarchWorldParameters WorldParameters)
{
	check(IsInRenderingThread());

	// Can't have directional light without direction...
	if (LightParameters.LightDirection == FVector(0.0, 0.0, 0.0))
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 100.0f, FColor::Yellow, TEXT("Returning because the directional light doesn't have a direction."));
		return;
	}

	FDirLightParameters LocalLightParams;
	FMajorAxes LocalMajorAxes;
	// Calculate local Light parameters and corresponding axes.
	GetLocalLightParamsAndAxes(LightParameters, WorldParameters.VolumeTransform, LocalLightParams, LocalMajorAxes);

	// Transform clipping parameters into local space.
	FClippingPlaneParameters LocalClippingParameters = GetLocalClippingParameters(WorldParameters);

	// For GPU profiling.
	SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToSingleLightVolume_RenderThread, TEXT("Adding Lights"));
	SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);

	// TODO create structure with 2 sets of buffers so we don't have to look for them again in the
	// actual shader loop! Clear buffers for the two axes we will be using.
	for (unsigned i = 0; i < 2; i++)
	{
		// Break if the axis weight == 0
		if (LocalMajorAxes.FaceWeight[i].second == 0)
		{
			break;
		}
		// Get the X, Y and Z transposed into the current axis orientation.
		FIntVector TransposedDimensions =
			GetTransposedDimensions(LocalMajorAxes, Resources.LightVolumeRenderTarget->GetResource()->TextureRHI->GetTexture3D(), i);
		OneAxisReadWriteBufferResources& Buffers = GetBuffers(LocalMajorAxes, i, Resources);

		float LightAlpha = GetLightAlpha(LocalLightParams, LocalMajorAxes, i);

		Clear2DTexture_RenderThread(
			RHICmdList, Buffers.UAVs[0], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), LightAlpha);
		Clear2DTexture_RenderThread(
			RHICmdList, Buffers.UAVs[1], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), LightAlpha);
	}

	// Find and set compute shader
	TShaderMapRef<FAddDirLightShader> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ShaderRHI);
	
	// Transition the resource to Compute-shader.
	// Otherwise the renderer might touch our textures while we're writing to them.
	RHICmdList.Transition(FRHITransitionInfo(Resources.LightVolumeUAVRef, ERHIAccess::UAVGraphics, ERHIAccess::UAVCompute));

	// Set parameters, resources, LightAdded and ALightVolume
	ComputeShader->SetRaymarchParameters(
		RHICmdList, ShaderRHI, LocalClippingParameters, Resources.WindowingParameters.ToLinearColor());
	ComputeShader->SetRaymarchResources(RHICmdList, ShaderRHI, Resources.DataVolumeTextureRef->GetResource()->TextureRHI->GetTexture3D(),
		Resources.TFTextureRef->GetResource()->TextureRHI->GetTexture2D(), Resources.WindowingParameters);
	ComputeShader->SetLightAdded(RHICmdList, ShaderRHI, Added);
	ComputeShader->SetALightVolume(RHICmdList, ShaderRHI, Resources.LightVolumeUAVRef);

	for (unsigned i = 0; i < 2; i++)
	{
		// Break if the main axis weight == 1
		if (LocalMajorAxes.FaceWeight[i].second == 0)
		{
			break;
		}
		OneAxisReadWriteBufferResources& Buffers = GetBuffers(LocalMajorAxes, i, Resources);

		uint32 ColorInt = GetBorderColorIntSingle(LocalLightParams, LocalMajorAxes, i);
		FSamplerStateRHIRef readBuffSampler = GetBufferSamplerRef(ColorInt);

		// Get the X, Y and Z transposed into the current axis orientation.
		FIntVector TransposedDimensions =
			GetTransposedDimensions(LocalMajorAxes, Resources.LightVolumeRenderTarget->GetResource()->TextureRHI->GetTexture3D(), i);

		FVector2D UVOffset =
			GetUVOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection, TransposedDimensions);
		FMatrix PermutationMatrix = GetPermutationMatrix(LocalMajorAxes, i);

		FIntVector LightVolumeSize = FIntVector(Resources.LightVolumeRenderTarget->SizeX, Resources.LightVolumeRenderTarget->SizeY,
			Resources.LightVolumeRenderTarget->SizeZ);

		FVector UVWOffset;
		float StepSize;
		GetStepSizeAndUVWOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection, TransposedDimensions,
			WorldParameters, StepSize, UVWOffset);

		// Normalize UVW offset to length of largest voxel size to get rid of artifacts. (Not correct,
		// but consistent!)
		int LowestVoxelCount = FMath::Min3(TransposedDimensions.X, TransposedDimensions.Y, TransposedDimensions.Z);
		float LongestVoxelSide = 1.0f / LowestVoxelCount;
		UVWOffset.Normalize();
		UVWOffset *= LongestVoxelSide;

		ComputeShader->SetStepSize(RHICmdList, ShaderRHI, StepSize);
		ComputeShader->SetPermutationMatrix(RHICmdList, ShaderRHI, PermutationMatrix);
		ComputeShader->SetUVOffset(RHICmdList, ShaderRHI, UVOffset);
		ComputeShader->SetUVWOffset(RHICmdList, ShaderRHI, UVWOffset);

		uint32 GroupSizeX = FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
		uint32 GroupSizeY = FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

		int Start, Stop, AxisDirection;
		GetLoopStartStopIndexes(Start, Stop, AxisDirection, LocalMajorAxes, i, TransposedDimensions.Z);

		for (int j = Start; j != Stop; j += AxisDirection)
		{
			// Switch read and write buffers each row.
			if (j % 2 == 0)
			{
				ComputeShader->SetLoop(RHICmdList, ShaderRHI, j, Buffers.Buffers[0], readBuffSampler, Buffers.UAVs[1]);
			}
			else
			{
				ComputeShader->SetLoop(RHICmdList, ShaderRHI, j, Buffers.Buffers[1], readBuffSampler, Buffers.UAVs[0]);
			}
			RHICmdList.DispatchComputeShader(GroupSizeX, GroupSizeY, 1);
		}
	}

	// Unbind UAVs.
	ComputeShader->UnbindResourcesLightPropagation(RHICmdList, ShaderRHI);

	// Transition resources back to the renderer.
	RHICmdList.Transition(FRHITransitionInfo(Resources.LightVolumeUAVRef, ERHIAccess::UAVCompute, ERHIAccess::UAVGraphics));
}

void ChangeDirLightInSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
	FBasicRaymarchRenderingResources Resources, const FDirLightParameters RemovedLightParameters,
	const FDirLightParameters AddedLightParameters, const FRaymarchWorldParameters WorldParameters)
{
	// Can't have directional light without direction...
	if (AddedLightParameters.LightDirection == FVector(0.0, 0.0, 0.0) ||
		RemovedLightParameters.LightDirection == FVector(0.0, 0.0, 0.0))
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 100.0f, FColor::Yellow, TEXT("Returning because the directional light doesn't have a direction."));
		return;
	}

	// Create local copies of Light Params, so that if we have to fall back to 2x
	// AddOrRemoveLight, we can just pass the original parameters.
	FDirLightParameters RemovedLocalLightParams, AddedLocalLightParams;
	FMajorAxes RemovedLocalMajorAxes, AddedLocalMajorAxes;
	// Calculate local Light parameters and corresponding axes.
	GetLocalLightParamsAndAxes(
		RemovedLightParameters, WorldParameters.VolumeTransform, RemovedLocalLightParams, RemovedLocalMajorAxes);
	GetLocalLightParamsAndAxes(AddedLightParameters, WorldParameters.VolumeTransform, AddedLocalLightParams, AddedLocalMajorAxes);

	// If lights have different major axes, do a separate removal and addition.
	// (Change dir light only works if it runs on the same major axes).
	if (RemovedLocalMajorAxes.FaceWeight[0].first != AddedLocalMajorAxes.FaceWeight[0].first ||
		RemovedLocalMajorAxes.FaceWeight[1].first != AddedLocalMajorAxes.FaceWeight[1].first)
	{
		AddDirLightToSingleLightVolume_RenderThread(RHICmdList, Resources, RemovedLightParameters, false, WorldParameters);
		AddDirLightToSingleLightVolume_RenderThread(RHICmdList, Resources, AddedLightParameters, true, WorldParameters);
		return;
	}

	FClippingPlaneParameters LocalClippingParameters = GetLocalClippingParameters(WorldParameters);

	// Clear buffers for the two axes we will be using.
	for (unsigned i = 0; i < 2; i++)
	{
		// Get the X, Y and Z transposed into the current axis orientation.
		FIntVector TransposedDimensions = GetTransposedDimensions(
			RemovedLocalMajorAxes, Resources.LightVolumeRenderTarget->GetResource()->TextureRHI->GetTexture3D(), i);
		OneAxisReadWriteBufferResources& Buffers = GetBuffers(RemovedLocalMajorAxes, i, Resources);

		float RemovedLightAlpha = GetLightAlpha(RemovedLocalLightParams, RemovedLocalMajorAxes, i);
		float AddedLightAlpha = GetLightAlpha(AddedLocalLightParams, AddedLocalMajorAxes, i);

		// Clear R/W buffers for Removed Light
		Clear2DTexture_RenderThread(
			RHICmdList, Buffers.UAVs[0], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), RemovedLightAlpha);
		Clear2DTexture_RenderThread(
			RHICmdList, Buffers.UAVs[1], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), RemovedLightAlpha);
		// Clear R/W buffers for Added Light
		Clear2DTexture_RenderThread(
			RHICmdList, Buffers.UAVs[2], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), AddedLightAlpha);
		Clear2DTexture_RenderThread(
			RHICmdList, Buffers.UAVs[3], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), AddedLightAlpha);
	}

	// For GPU profiling.
	SCOPED_DRAW_EVENTF(RHICmdList, ChangeDirLightInSingleLightVolume_RenderThread, TEXT("Changing Lights"));
	SCOPED_GPU_STAT(RHICmdList, GPUChangingLights);

	TShaderMapRef<FChangeDirLightShader> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ShaderRHI);

	// Don't need barriers on these - we only ever read/write to the same pixel from one thread ->
	// no race conditions But we definitely need to transition the resource to Compute-shader
	// accessible, otherwise the renderer might touch our textures while we're writing them.
	RHICmdList.Transition(FRHITransitionInfo(Resources.LightVolumeUAVRef, ERHIAccess::UAVGraphics, ERHIAccess::UAVCompute));

	ComputeShader->SetRaymarchParameters(
		RHICmdList, ShaderRHI, LocalClippingParameters, Resources.WindowingParameters.ToLinearColor());
	ComputeShader->SetRaymarchResources(RHICmdList, ShaderRHI, Resources.DataVolumeTextureRef->GetResource()->TextureRHI->GetTexture3D(),
		Resources.TFTextureRef->GetResource()->TextureRHI->GetTexture2D(), Resources.WindowingParameters);
	ComputeShader->SetALightVolume(RHICmdList, ShaderRHI, Resources.LightVolumeUAVRef);

	for (unsigned AxisIndex = 0; AxisIndex < 2; AxisIndex++)
	{
		// Get Color ints for texture borders.
		uint32 RemovedColorInt = GetBorderColorIntSingle(RemovedLocalLightParams, RemovedLocalMajorAxes, AxisIndex);
		uint32 AddedColorInt = GetBorderColorIntSingle(AddedLocalLightParams, AddedLocalMajorAxes, AxisIndex);
		// Get the sampler for read buffer to use border with the proper light color.
		FSamplerStateRHIRef RemovedReadBuffSampler = GetBufferSamplerRef(RemovedColorInt);
		FSamplerStateRHIRef AddedReadBuffSampler = GetBufferSamplerRef(AddedColorInt);

		OneAxisReadWriteBufferResources& Buffers = GetBuffers(RemovedLocalMajorAxes, AxisIndex, Resources);
		// TODO take these from buffers.
		FIntVector TransposedDimensions = GetTransposedDimensions(
			RemovedLocalMajorAxes, Resources.LightVolumeRenderTarget->GetResource()->TextureRHI->GetTexture3D(), AxisIndex);

		FVector2D AddedPixOffset = GetUVOffset(
			AddedLocalMajorAxes.FaceWeight[AxisIndex].first, -AddedLocalLightParams.LightDirection, TransposedDimensions);
		FVector2D RemovedPixOffset = GetUVOffset(
			RemovedLocalMajorAxes.FaceWeight[AxisIndex].first, -RemovedLocalLightParams.LightDirection, TransposedDimensions);

		FVector2D AddedUVOffset = GetUVOffset(
			AddedLocalMajorAxes.FaceWeight[AxisIndex].first, -AddedLocalLightParams.LightDirection, TransposedDimensions);
		FVector2D RemovedUVOffset = GetUVOffset(
			RemovedLocalMajorAxes.FaceWeight[AxisIndex].first, -RemovedLocalLightParams.LightDirection, TransposedDimensions);

		FVector AddedUVWOffset, RemovedUVWOffset;
		float AddedStepSize, RemovedStepSize;

		GetStepSizeAndUVWOffset(AddedLocalMajorAxes.FaceWeight[AxisIndex].first, -AddedLocalLightParams.LightDirection,
			TransposedDimensions, WorldParameters, AddedStepSize, AddedUVWOffset);
		GetStepSizeAndUVWOffset(RemovedLocalMajorAxes.FaceWeight[AxisIndex].first, -RemovedLocalLightParams.LightDirection,
			TransposedDimensions, WorldParameters, RemovedStepSize, RemovedUVWOffset);

		// Normalize UVW offset to length of largest voxel size to get rid of artifacts. (Not correct,
		// but consistent!)
		int LowestVoxelCount = FMath::Min3(TransposedDimensions.X, TransposedDimensions.Y, TransposedDimensions.Z);
		float LongestVoxelSide = 1.0f / LowestVoxelCount;

		AddedUVWOffset.Normalize();
		AddedUVWOffset *= LongestVoxelSide;
		RemovedUVWOffset.Normalize();
		RemovedUVWOffset *= LongestVoxelSide;

		ComputeShader->SetStepSizes(RHICmdList, ShaderRHI, AddedStepSize, RemovedStepSize);

		ComputeShader->SetPixelOffsets(RHICmdList, ShaderRHI, AddedPixOffset, RemovedPixOffset);
		ComputeShader->SetUVWOffsets(RHICmdList, ShaderRHI, AddedUVWOffset, RemovedUVWOffset);

		FMatrix PermMatrix = GetPermutationMatrix(RemovedLocalMajorAxes, AxisIndex);
		ComputeShader->SetPermutationMatrix(RHICmdList, ShaderRHI, PermMatrix);

		// Get group sizes for compute shader
		uint32 GroupSizeX = FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
		uint32 GroupSizeY = FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

		int Start, Stop, AxisDirection;
		GetLoopStartStopIndexes(Start, Stop, AxisDirection, RemovedLocalMajorAxes, AxisIndex, TransposedDimensions.Z);

		for (int LoopIndex = Start; LoopIndex != Stop; LoopIndex += AxisDirection)
		{	 // Switch read and write buffers each cycle.
			if (LoopIndex % 2 == 0)
			{
				TransitionBufferResources(RHICmdList, Buffers.Buffers[0], Buffers.UAVs[1]);
				TransitionBufferResources(RHICmdList, Buffers.Buffers[2], Buffers.UAVs[3]);
				ComputeShader->SetLoop(RHICmdList, ShaderRHI, LoopIndex, Buffers.Buffers[0], RemovedReadBuffSampler,
					Buffers.UAVs[1], Buffers.Buffers[2], AddedReadBuffSampler, Buffers.UAVs[3]);
			}
			else
			{
				TransitionBufferResources(RHICmdList, Buffers.Buffers[1], Buffers.UAVs[0]);
				TransitionBufferResources(RHICmdList, Buffers.Buffers[3], Buffers.UAVs[2]);
				ComputeShader->SetLoop(RHICmdList, ShaderRHI, LoopIndex, Buffers.Buffers[1], RemovedReadBuffSampler,
					Buffers.UAVs[0], Buffers.Buffers[3], AddedReadBuffSampler, Buffers.UAVs[2]);
			}
			RHICmdList.DispatchComputeShader(GroupSizeX, GroupSizeY, 1);
		}
	}

	// Unbind Resources.
	ComputeShader->UnbindResourcesChangeDirLight(RHICmdList, ShaderRHI);

	// Transition resources back to the renderer.
	RHICmdList.Transition(FRHITransitionInfo(Resources.LightVolumeUAVRef, ERHIAccess::UAVCompute, ERHIAccess::UAVGraphics));
}

#undef LOCTEXT_NAMESPACE

#if !UE_BUILD_SHIPPING
#pragma optimize("", on)
#endif
