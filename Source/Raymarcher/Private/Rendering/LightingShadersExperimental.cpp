// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

//#include "Rendering/LightingShadersExperimental.h"
//#include "Rendering/LightingShaderUtils.h"
//
//IMPLEMENT_GLOBAL_SHADER(
//	FAddDirLightShader_GPUSyncCS, "/Raymarcher/Private/AddDirLightShader_GPUSync.usf", "MainComputeShader", SF_Compute);
//
//#define NUM_THREADS_PER_GROUP_DIMENSION 16	  // This has to be the same as in the compute shader's spec [X, X, 1]
//// #TODO profile with different dimensions.
//
//// A GPU-synced version of a light adding shader.
//void AddDirLightToSingleLightVolume_GPUSync_RenderThread(FRHICommandListImmediate& RHICmdList,
//	FBasicRaymarchRenderingResources Resources, const FDirLightParameters LightParameters, const bool Added,
//	const FRaymarchWorldParameters WorldParameters)
//{
//	check(IsInRenderingThread());
//
//	// Can't have directional light without direction...
//	if (LightParameters.LightDirection == FVector(0.0, 0.0, 0.0))
//	{
//		GEngine->AddOnScreenDebugMessage(
//			-1, 100.0f, FColor::Yellow, TEXT("Returning because the directional light doesn't have a direction."));
//		return;
//	}
//
//	FDirLightParameters LocalLightParams;
//	FMajorAxes LocalMajorAxes;
//	// Calculate local Light parameters and corresponding axes.
//	GetLocalLightParamsAndAxes(LightParameters, WorldParameters.VolumeTransform, LocalLightParams, LocalMajorAxes);
//
//	// Transform clipping parameters into local space.
//	FClippingPlaneParameters LocalClippingParameters = GetLocalClippingParameters(WorldParameters);
//
//	// For GPU profiling.
//	SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToSingleLightVolume_RenderThread, TEXT("Adding Lights"));
//	SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);
//
//	// TODO create structure with 2 sets of buffers so we don't have to look for them again in the
//	// actual shader loop! Clear buffers for the two axes we will be using.
//	for (unsigned i = 0; i < 2; i++)
//	{
//		// Break if the axis weight == 0
//		if (LocalMajorAxes.FaceWeight[i].second == 0)
//		{
//			break;
//		}
//		// Get the X, Y and Z transposed into the current axis orientation.
//		FIntVector TransposedDimensions =
//			GetTransposedDimensions(LocalMajorAxes, Resources.LightVolumeRenderTarget->Resource->TextureRHI->GetTexture3D(), i);
//		OneAxisReadWriteBufferResources& Buffers = GetBuffers(LocalMajorAxes, i, Resources);
//
//		float LightAlpha = GetLightAlpha(LocalLightParams, LocalMajorAxes, i);
//
//		Clear2DTexture_RenderThread(
//			RHICmdList, Buffers.UAVs[0], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), LightAlpha);
//		Clear2DTexture_RenderThread(
//			RHICmdList, Buffers.UAVs[1], FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), LightAlpha);
//	}
//
//	// Find and set compute shader
//	TShaderMapRef<FAddDirLightShader_GPUSyncCS> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
//	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
//	RHICmdList.SetComputeShader(ShaderRHI);
//
//	// 	// Don't need barriers on these - we only ever read/write to the same pixel from one thread ->
//	// 	// no race conditions But we definitely need to transition the resource to Compute-shader
//	// 	// accessible, otherwise the renderer might touch our textures while we're writing there.
//	// 	RHICmdList.TransitionResource(
//	// 		EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToCompute, Resources.LightVolumeUAVRef);
//
//	// Set parameters, resources, LightAdded and ALightVolume
//	ComputeShader->SetRaymarchParameters(
//		RHICmdList, ShaderRHI, LocalClippingParameters, Resources.WindowingParameters.ToLinearColor());
//	ComputeShader->SetRaymarchResources(RHICmdList, ShaderRHI, Resources.DataVolumeTextureRef->Resource->TextureRHI->GetTexture3D(),
//		Resources.TFTextureRef->Resource->TextureRHI->GetTexture2D(), Resources.WindowingParameters);
//	ComputeShader->SetLightAdded(RHICmdList, ShaderRHI, Added);
//	ComputeShader->SetALightVolume(RHICmdList, ShaderRHI, Resources.LightVolumeUAVRef);
//
//	for (unsigned i = 0; i < 2; i++)
//	{
//		// Break if the main axis weight == 0
//		if (LocalMajorAxes.FaceWeight[i].second == 0)
//		{
//			break;
//		}
//		OneAxisReadWriteBufferResources& Buffers = GetBuffers(LocalMajorAxes, i, Resources);
//
//		// Get the X, Y and Z transposed into the current axis orientation.
//		FIntVector TransposedDimensions =
//			GetTransposedDimensions(LocalMajorAxes, Resources.LightVolumeRenderTarget->Resource->TextureRHI->GetTexture3D(), i);
//
//		FVector2D UVOffset =
//			GetUVOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection, TransposedDimensions);
//		FMatrix PermutationMatrix = GetPermutationMatrix(LocalMajorAxes, i);
//
//		FIntVector LightVolumeSize = FIntVector(Resources.LightVolumeRenderTarget->SizeX, Resources.LightVolumeRenderTarget->SizeY,
//			Resources.LightVolumeRenderTarget->SizeZ);
//
//		FVector UVWOffset;
//		float StepSize;
//		GetStepSizeAndUVWOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection, TransposedDimensions,
//			WorldParameters, StepSize, UVWOffset);
//
//		// Normalize UVW offset to length of largest voxel size to get rid of artifacts. (Not correct,
//		// but consistent!)
//		int LowestVoxelCount = FMath::Min3(TransposedDimensions.X, TransposedDimensions.Y, TransposedDimensions.Z);
//		float LongestVoxelSide = 1.0f / LowestVoxelCount;
//		UVWOffset.Normalize();
//		UVWOffset *= LongestVoxelSide;
//
//		ComputeShader->SetStepSize(RHICmdList, ShaderRHI, StepSize);
//		ComputeShader->SetPermutationMatrix(RHICmdList, ShaderRHI, PermutationMatrix);
//		ComputeShader->SetUVOffset(RHICmdList, ShaderRHI, UVOffset);
//		ComputeShader->SetUVWOffset(RHICmdList, ShaderRHI, UVWOffset);
//		ComputeShader->SetReadWriteBuffer(RHICmdList, ShaderRHI, Buffers.Buffers[i], Buffers.UAVs[i]);
//
//		int Start, Stop, AxisDirection;
//		GetLoopStartStopIndexes(Start, Stop, AxisDirection, LocalMajorAxes, i, TransposedDimensions.Z);
//
//		ComputeShader->SetLoopParameters(RHICmdList, ShaderRHI, Start, Stop, AxisDirection);
//		ComputeShader->SetOutsideLight(RHICmdList, ShaderRHI, GetLightAlpha(LocalLightParams, LocalMajorAxes, i));
//
//		uint32 GroupSizeX = FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
//		uint32 GroupSizeY = FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);
//
//		RHICmdList.DispatchComputeShader(GroupSizeX, GroupSizeY, 1);
//	}
//
//	// Unbind UAVs.
//	ComputeShader->UnbindResourcesLightPropagation(RHICmdList, ShaderRHI);
//
//	// Transition resources back to the renderer.
//	RHICmdList.Transition(FRHITransitionInfo(Resources.LightVolumeUAVRef, ERHIAccess::UAVCompute, ERHIAccess::UAVGraphics));
//}