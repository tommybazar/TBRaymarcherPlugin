// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

//#pragma once
//
//#include "LightingShaders.h"
//
//void AddDirLightToSingleLightVolume_GPUSync_RenderThread(FRHICommandListImmediate& RHICmdList,
//	FBasicRaymarchRenderingResources Resources, const FDirLightParameters LightParameters, const bool Added,
//	const FRaymarchWorldParameters WorldParameters);
//
//class FAddDirLightShader_GPUSyncCS : public FAddDirLightShader
//{
//	DECLARE_EXPORTED_SHADER_TYPE(FAddDirLightShader_GPUSyncCS, Global, RAYMARCHER_API);
//
//public:
//	FAddDirLightShader_GPUSyncCS() : FAddDirLightShader()
//	{
//	}
//
//	FAddDirLightShader_GPUSyncCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//		: FAddDirLightShader(Initializer)
//	{
//		// Volume texture + Transfer function uniforms
//		Start.Bind(Initializer.ParameterMap, TEXT("Start"), SPF_Mandatory);
//		Stop.Bind(Initializer.ParameterMap, TEXT("Stop"), SPF_Mandatory);
//		AxisDirection.Bind(Initializer.ParameterMap, TEXT("AxisDirection"), SPF_Mandatory);
//		BufferBorderValue.Bind(Initializer.ParameterMap, TEXT("BufferBorderValue"), SPF_Mandatory);
//		// !!!!!!!!!!!
//		// When binding FRWShaderParameter, we must omit the "RW" from the name provided into the Bind function
//		// Otherwise it gets bound as a SRV (no RW access)! See FRWShaderParameter::Init() in ShaderParameters.h !!!
//		// !!!!!!!!!!!
//		LightBuffer.Bind(Initializer.ParameterMap, TEXT("LightBuffer"));
//	}
//
//	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
//	{
//		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
//	}
//
//	void SetReadWriteBuffer(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, FTexture2DRHIRef pTexture,
//		FRHIUnorderedAccessView* pRWBuffer)
//	{
//		LightBuffer.SetTexture(RHICmdList, ShaderRHI, pTexture, pRWBuffer);
//	}
//
//	void SetLoopParameters(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, const int pStart, const int pStop,
//		const int pAxisDirection)
//	{
//		SetShaderValue(RHICmdList, ShaderRHI, Start, pStart);
//		SetShaderValue(RHICmdList, ShaderRHI, Stop, pStop);
//		SetShaderValue(RHICmdList, ShaderRHI, AxisDirection, pAxisDirection);
//	};
//
//	void SetOutsideLight(FRHICommandListImmediate& RHICmdList, FRHIComputeShader* ShaderRHI, float OutsideLightIntensity)
//	{
//		SetShaderValue(RHICmdList, ShaderRHI, BufferBorderValue, OutsideLightIntensity);
//	};
//
//protected:
//	LAYOUT_FIELD(FShaderParameter, Start);
//	LAYOUT_FIELD(FShaderParameter, Stop);
//	LAYOUT_FIELD(FShaderParameter, AxisDirection);
//	LAYOUT_FIELD(FShaderParameter, BufferBorderValue);
//	LAYOUT_FIELD(FRWShaderParameter, LightBuffer);
//};