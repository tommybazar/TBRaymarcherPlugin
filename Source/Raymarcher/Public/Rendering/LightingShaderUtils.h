// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "RaymarchTypes.h"

#include <algorithm>	// std::sort
#include <utility>		// std::pair, std::make_pair
#include <vector>		// std::pair, std::make_pair

// Enum for indexes for cube faces - used to discern axes for light propagation shader.
// Also used for deciding vectors provided into cutting plane material.
// The axis convention is - you are looking at the cube along positive Y axis in UE.
UENUM(BlueprintType)
enum class FCubeFace : uint8
{
	XPositive = 0,	  // +X
	XNegative = 1,	  // -X
	YPositive = 2,	  // +Y
	YNegative = 3,	  // -Y
	ZPositive = 4,	  // +Z
	ZNegative = 5	  // -Z
};

// Utility function to get sensible names from FCubeFace
static FString GetDirectionName(FCubeFace Face);

// Normals of corresponding cube faces in object-space.
const FVector FCubeFaceNormals[6] = {
	{1.0, 0.0, 0.0},	 // +x
	{-1.0, 0.0, 0.0},	 // -x
	{0.0, 1.0, 0.0},	 // +y
	{0.0, -1.0, 0.0},	 // -y
	{0.0, 0.0, 1.0},	 // +z
	{0.0, 0.0, -1.0}	 // -z
};

/** Structure corresponding to the 3 major axes to propagate a light-source along with their
   respective weights. */
struct FMajorAxes
{
	// The 3 major axes indexes

	std::vector<std::pair<FCubeFace, float>> FaceWeight;

	/* Returns the weighted major axes to propagate along to add/remove a light to/from a lightvolume
	(LightPos must be converted to object-space). */
	static FMajorAxes GetMajorAxes(FVector LightPos);
};

/// Returns the dimensions of the plane cutting through the volume when going along an axis at the given indes.
FIntVector GetTransposedDimensions(const FMajorAxes& Axes, const FRHITexture3D* VolumeRef, const unsigned index);

/// Returns +1 if going along the specified axis index means increasing the index.
/// Returns -1 if going along the axis decreases the index.
/// E.G. if we're going along +X axis, will return 1, going along -X will return -1.
int GetAxisDirection(const FMajorAxes& Axes, unsigned index);

/// Returns the ReadWriteBuffer resource appropriate for the axis index.
OneAxisReadWriteBufferResources& GetBuffers(
	const FMajorAxes& Axes, const unsigned index, FBasicRaymarchRenderingResources& InParams);

// Comparison function for a Face-weight pair, to sort in Descending order.
static bool SortDescendingWeights(const std::pair<FCubeFace, float>& a, const std::pair<FCubeFace, float>& b);

FVector2D GetUVOffset(FCubeFace Axis, FVector LightPosition, FIntVector TransposedDimensions);

/// For the given axis, light position and volume dimensions and world parameters, return the StepSize and UVW offset.
///
/// StepSize is the distance the light has to travel between 2 layers (in world units) and UVW offset is the offset between a voxel
/// and the place where it should read lighting information from from the previous layer (in UVW 0-1 space).
void GetStepSizeAndUVWOffset(FCubeFace Axis, FVector LightPosition, FIntVector TransposedDimensions,
	const FRaymarchWorldParameters WorldParameters, float& OutStepSize, FVector& OutUVWOffset);

/// For the given light parameters and volume transform, returns the the Local Directional Lihgt parameters and Major Axes for illumination.
void GetLocalLightParamsAndAxes(const FDirLightParameters& LightParameters, const FTransform& VolumeTransform,
	FDirLightParameters& OutLocalLightParameters, FMajorAxes& OutLocalMajorAxes);

/// Creates a SamplerState RHI with "Border" handling of outside-of-UV reads.
/// The color read from outside the buffer is specified by the BorderColorInt.
FSamplerStateRHIRef GetBufferSamplerRef(uint32 BorderColorInt);

/// Returns the integer specifying the color needed for the border sampler.
/// Used for sampling the light outside the edge of the Read buffer.
uint32 GetBorderColorIntSingle(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index);

/// Returns clipping parameters from global world parameters.
FClippingPlaneParameters GetLocalClippingParameters(const FRaymarchWorldParameters WorldParameters);

// Returns the light's alpha at this major axis and weight (single channel)
float GetLightAlpha(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index);

// Returns a 3x3 permutation matrix depending on the current propagation axis.
// We use this matrix in the shaders to create offsets regardless of direction.
FMatrix GetPermutationMatrix(FMajorAxes MajorAxes, unsigned index);

// Returns the Loop Start index, end index and the way the loop is going along the axis.
void GetLoopStartStopIndexes(
	int& OutStart, int& OutStop, int& OutAxisDirection, const FMajorAxes& MajorAxes, const unsigned& index, const int zDimension);

// Used for swapping read/write buffers - transitions one to Readable and other to Writable.
void TransitionBufferResources(
	FRHICommandListImmediate& RHICmdList, FRHITexture* NewlyReadableTexture, FRHIUnorderedAccessView* NewlyWriteableUAV);
