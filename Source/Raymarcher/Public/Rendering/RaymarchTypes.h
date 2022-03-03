// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "Engine/VolumeTexture.h"
#include "RHIResources.h"
#include "RaymarchMaterialParameters.h"
#include "VolumeAsset/VolumeInfo.h"

#include "RaymarchTypes.generated.h"

class UTextureRenderTargetVolume;

// USTRUCT for Directional light parameters.
USTRUCT(BlueprintType)
struct FDirLightParameters
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") FVector LightDirection;
	UPROPERTY(BlueprintReadWrite, Category = "DirLightParameters") float LightIntensity;

	FDirLightParameters(FVector LightDir, float LightInt) : LightDirection(LightDir), LightIntensity(LightInt){};
	FDirLightParameters() : LightDirection(FVector(0, 0, 0)), LightIntensity(0){};

	// Equal operator for convenient checking if a light changed.
	inline bool operator==(const FDirLightParameters& rhs)
	{
		return (this->LightDirection == rhs.LightDirection) && (this->LightIntensity == rhs.LightIntensity);
	}

	// Inequality operator for convenient checking if a light changed.
	inline bool operator!=(const FDirLightParameters& rhs)
	{
		return !(*this == rhs);
	}
};

// USTRUCT for Clipping plane parameters.
USTRUCT(BlueprintType)
struct FClippingPlaneParameters
{
	GENERATED_BODY()

	/// Center point of the clipping plane.
	UPROPERTY(BlueprintReadWrite, Category = "ClippingPlaneParameters")
	FVector Center;

	// The direction from the center that is NOT clipped away.
	UPROPERTY(BlueprintReadWrite, Category = "ClippingPlaneParameters")
	FVector Direction;

	FClippingPlaneParameters(FVector ClipCenter, FVector ClipDirection) : Center(ClipCenter), Direction(ClipDirection){};
	FClippingPlaneParameters() : Center(FVector(0, 0, 0)), Direction(FVector(0, 0, 0)){};

	// Equality operator for convenient checking if the clip plane changed.
	friend bool operator==(const FClippingPlaneParameters& lhs, const FClippingPlaneParameters& rhs)
	{
		return ((lhs.Center == rhs.Center) && (lhs.Direction == rhs.Direction));
	}

	// Inequality operator for convenient checking if the clip plane changed.
	friend bool operator!=(const FClippingPlaneParameters& lhs, const FClippingPlaneParameters& rhs)
	{
		return !(lhs == rhs);
	}
};

// A structure for 4 switchable read-write buffers. Used for one axis. Need 2 pairs for change-light
// shader.
struct OneAxisReadWriteBufferResources
{
	// 2D Textures whose dimensions match the matching axis in the volume texture.
	FTexture2DRHIRef Buffers[4];
	// UAV refs to the Buffers, when we need to make a RWTexture out of them.
	FUnorderedAccessViewRHIRef UAVs[4];
};

/** A structure holding all resources related to a single raymarchable volume - its texture ref, the
   TF texture ref and TF Range parameters,
	light volume texture ref, and read-write buffers used for propagating along all axes. */
USTRUCT(BlueprintType)
struct FBasicRaymarchRenderingResources
{
	GENERATED_BODY()

	/// Flag that these Rendering Resources have been initialized and can be used.
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Basic Raymarch Rendering Resources")
	bool bIsInitialized = false;

	/// Pointer to the Data Volume texture.
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Transient, Category = "Basic Raymarch Rendering Resources")
	UVolumeTexture* DataVolumeTextureRef = nullptr;

	/// Pointer to the Transfer Function Volume texture.
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Transient, Category = "Basic Raymarch Rendering Resources")
	UTexture2D* TFTextureRef = nullptr;

	/// Pointer to the illumination volume texture render target.
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Transient, Category = "Basic Raymarch Rendering Resources")
	UTextureRenderTargetVolume* LightVolumeRenderTarget = nullptr;

	/// If true, Light Volume texture will be created with it's side scaled down by 1/2 (-> 1/8 total voxels!)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic Raymarch Rendering Resources")
	bool LightVolumeHalfResolution = false;

	/// Windowing parameters that dictate how a value read from the volume is transferred onto the transfer function.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FWindowingParameters WindowingParameters;

	// Following is not visible in BPs, it's too low level to be useful in BP.

	// Unordered access view to the Light Volume. Used in our compute shaders as a RWTexture.
	FUnorderedAccessViewRHIRef LightVolumeUAVRef;
	// Read-write buffers for all 3 major axes. Used in compute shaders.
	OneAxisReadWriteBufferResources XYZReadWriteBuffers[3];
};

/** Structure containing the world parameters required for light propagation shaders - these include
  the volume's world transform and clipping plane parameters. If these change, the whole light volume needs
  to be recomputed.
*/
USTRUCT(BlueprintType)
struct FRaymarchWorldParameters
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters")
	FTransform VolumeTransform;
	UPROPERTY(BlueprintReadWrite, Category = "Raymarch Rendering World Parameters")
	FClippingPlaneParameters ClippingPlaneParameters;

	friend bool operator==(const FRaymarchWorldParameters& lhs, const FRaymarchWorldParameters& rhs)
	{
		return ((lhs.VolumeTransform.Equals(rhs.VolumeTransform)) && (lhs.ClippingPlaneParameters == rhs.ClippingPlaneParameters));
	}
	friend bool operator!=(const FRaymarchWorldParameters& lhs, const FRaymarchWorldParameters& rhs)
	{
		return !(lhs == rhs);
	}
};
