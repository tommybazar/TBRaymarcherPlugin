#include "Rendering/LightingShaderUtils.h"

FString GetDirectionName(FCubeFace Face)
{
	switch (Face)
	{
		case FCubeFace::XPositive:
			return FString("+X");
		case FCubeFace::XNegative:
			return FString("-X");
		case FCubeFace::YPositive:
			return FString("+Y");
		case FCubeFace::YNegative:
			return FString("-Y");
		case FCubeFace::ZPositive:
			return FString("+Z");
		case FCubeFace::ZNegative:
			return FString("-Z");
		default:
			return FString("Something went wrong here!");
	}
}

bool SortDescendingWeights(const std::pair<FCubeFace, float>& a, const std::pair<FCubeFace, float>& b)
{
	return (a.second > b.second);
}

FMajorAxes FMajorAxes::GetMajorAxes(FVector LightPos)
{
	FMajorAxes RetVal;
	std::vector<std::pair<FCubeFace, float>> faceVector;

	for (int i = 0; i < 6; i++)
	{
		// Dot of position and face normal yields cos(angle)
		float weight = FVector::DotProduct(FCubeFaceNormals[i], LightPos);

		// Need to make sure we go along the axis with a positive weight, not negative.
		weight = (weight > 0 ? weight * weight : 0);
		RetVal.FaceWeight.push_back(std::make_pair(FCubeFace(i), weight));
	}
	// Sort so that the 3 major axes are the first.
	std::sort(RetVal.FaceWeight.begin(), RetVal.FaceWeight.end(), SortDescendingWeights);
	return RetVal;
}

FIntVector GetTransposedDimensions(const FMajorAxes& Axes, const FRHITexture3D* VolumeRef, const unsigned index)
{
	FCubeFace face = Axes.FaceWeight[index].first;
	unsigned axis = (uint8) face / 2;
	switch (axis)
	{
		case 0:	   // going along X -> Volume Y = x, volume Z = y
			return FIntVector(VolumeRef->GetSizeY(), VolumeRef->GetSizeZ(), VolumeRef->GetSizeX());
		case 1:	   // going along Y -> Volume X = x, volume Z = y
			return FIntVector(VolumeRef->GetSizeX(), VolumeRef->GetSizeZ(), VolumeRef->GetSizeY());
		case 2:	   // going along Z -> Volume X = x, volume Y = y
			return FIntVector(VolumeRef->GetSizeX(), VolumeRef->GetSizeY(), VolumeRef->GetSizeZ());
		default:
			check(false);
			return FIntVector(0, 0, 0);
	}
}

int GetAxisDirection(const FMajorAxes& Axes, unsigned index)
{
	// All even axis number are going down on their respective axes.
	return (((uint8) Axes.FaceWeight[index].first) % 2 ? 1 : -1);
}

OneAxisReadWriteBufferResources& GetBuffers(
	const FMajorAxes& Axes, const unsigned index, FBasicRaymarchRenderingResources& InParams)
{
	FCubeFace Face = Axes.FaceWeight[index].first;
	unsigned FaceAxis = (uint8) Face / 2;
	return InParams.XYZReadWriteBuffers[FaceAxis];
}


///  Returns the UV offset to the previous layer. This is the position in the previous layer that is in the direction of the light.
FVector2D GetUVOffset(FCubeFace Axis, FVector LightPosition, FIntVector TransposedDimensions)
{
	FVector normLightPosition = LightPosition;
	// Normalize the light position to get the major axis to be one. The other 2 components are then
	// an offset to apply to current pos to read from our read buffer texture.
	FVector2D RetVal;
	switch (Axis)
	{
		case FCubeFace::XPositive:	  // +X
			normLightPosition /= normLightPosition.X;
			RetVal = FVector2D(normLightPosition.Y, normLightPosition.Z);
			break;
		case FCubeFace::XNegative:	  // -X
			normLightPosition /= -normLightPosition.X;
			RetVal = FVector2D(normLightPosition.Y, normLightPosition.Z);
			break;
		case FCubeFace::YPositive:	  // +Y
			normLightPosition /= normLightPosition.Y;
			RetVal = FVector2D(normLightPosition.X, normLightPosition.Z);
			break;
		case FCubeFace::YNegative:	  // -Y
			normLightPosition /= -normLightPosition.Y;
			RetVal = FVector2D(normLightPosition.X, normLightPosition.Z);
			break;
		case FCubeFace::ZPositive:	  // +Z
			normLightPosition /= normLightPosition.Z;
			RetVal = FVector2D(normLightPosition.X, normLightPosition.Y);
			break;
		case FCubeFace::ZNegative:	  // -Z
			normLightPosition /= -normLightPosition.Z;
			RetVal = FVector2D(normLightPosition.X, normLightPosition.Y);
			break;
		default:
		{
			check(false);
			return FVector2D(0, 0);
		};
	}

	// Now normalize for different voxel sizes. Because we have Transposed Dimensions as input, we
	// know that we're propagating along TD.Z, The X-size of the buffer is in TD.X and Y-size of the
	// buffer is in TD.Y

	//// Divide by length of step
	RetVal /= TransposedDimensions.Z;

	return RetVal;
}


void GetStepSizeAndUVWOffset(FCubeFace Axis, FVector LightPosition, FIntVector TransposedDimensions,
	const FRaymarchWorldParameters WorldParameters, float& OutStepSize, FVector& OutUVWOffset)
{
	OutUVWOffset = LightPosition;
	// Since we don't care about the direction, just the size, ignore signs.
	switch (Axis)
	{
		case FCubeFace::XPositive:	  // +-X
		case FCubeFace::XNegative:
			OutUVWOffset /= abs(LightPosition.X) * TransposedDimensions.Z;
			break;
		case FCubeFace::YPositive:	  // +-Y
		case FCubeFace::YNegative:
			OutUVWOffset /= abs(LightPosition.Y) * TransposedDimensions.Z;
			break;
		case FCubeFace::ZPositive:	  // +-Z
		case FCubeFace::ZNegative:
			OutUVWOffset /= abs(LightPosition.Z) * TransposedDimensions.Z;
			break;
		default:
			check(false);
			;
	}

	// Multiply StepSize by fixed volume density.
	OutStepSize = OutUVWOffset.Size();
}

void GetLocalLightParamsAndAxes(const FDirLightParameters& LightParameters, const FTransform& VolumeTransform,
	FDirLightParameters& OutLocalLightParameters, FMajorAxes& OutLocalMajorAxes)
{
	// #TODO Why the fuck does light direction need NoScale and no multiplication by scale and clipping
	// plane needs to be multiplied?

	// Transform light directions into local space.
	OutLocalLightParameters.LightDirection = VolumeTransform.InverseTransformVector(LightParameters.LightDirection);
	// Normalize Light Direction to get unit length.
	OutLocalLightParameters.LightDirection.Normalize();

	// Intensity is the same in local space -> copy.
	OutLocalLightParameters.LightIntensity = LightParameters.LightIntensity;

	// Get Major Axes (notice inverting of light Direction - for a directional light, the position of
	// the light is the opposite of the direction) e.g. Directional light with direction (1, 0, 0) is
	// assumed to be shining from (-1, 0, 0)
	OutLocalMajorAxes = FMajorAxes::GetMajorAxes(-OutLocalLightParameters.LightDirection);

	// Prevent near-zero bugs in the shader (if first axis is very, very dominant, the secondary axis offsets are giant
	// and they just read out of bounds all the time.
	if (OutLocalMajorAxes.FaceWeight[0].second > 0.99f)
	{
		OutLocalMajorAxes.FaceWeight[0].second = 1.0f;
	}

	// Set second axis weight to (1 - (first axis weight))
	OutLocalMajorAxes.FaceWeight[1].second = 1 - OutLocalMajorAxes.FaceWeight[0].second;
}

FSamplerStateRHIRef GetBufferSamplerRef(uint32 BorderColorInt)
{
	// Return a sampler for RW buffers - bordered by specified color.
	return RHICreateSamplerState(
		FSamplerStateInitializerRHI(SF_Bilinear, AM_Border, AM_Border, AM_Border, 0, 0, 0, 1, BorderColorInt));
}

uint32 GetBorderColorIntSingle(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index)
{
	// Set alpha channel to the texture's red channel (when reading single-channel, only red component
	// is read)
	FLinearColor LightColor = FLinearColor(LightParams.LightIntensity * MajorAxes.FaceWeight[index].second, 0.0, 0.0, 0.0);
	return LightColor.ToFColor(true).ToPackedARGB();
}

FClippingPlaneParameters GetLocalClippingParameters(const FRaymarchWorldParameters WorldParameters)
{
	FClippingPlaneParameters RetVal;
	// Get clipping center to (0-1) texture local space. (Invert transform, add 0.5 to get to (0-1)
	// space of a unit cube centered on 0,0,0)
	RetVal.Center = WorldParameters.VolumeTransform.InverseTransformPosition(WorldParameters.ClippingPlaneParameters.Center) + 0.5;
	// Get clipping direction in local space
	// TODO Why the hell does light direction work with regular InverseTransformVector
	// but clipping direction only works with NoScale and multiplying by scale afterwards?
	RetVal.Direction =
		WorldParameters.VolumeTransform.InverseTransformVectorNoScale(WorldParameters.ClippingPlaneParameters.Direction);
	RetVal.Direction *= WorldParameters.VolumeTransform.GetScale3D();
	RetVal.Direction.Normalize();

	return RetVal;
}

float GetLightAlpha(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index)
{
	return LightParams.LightIntensity * MajorAxes.FaceWeight[index].second;
}

FMatrix GetPermutationMatrix(FMajorAxes MajorAxes, unsigned index)
{
	uint8 Axis = (uint8) MajorAxes.FaceWeight[index].first / 2;
	FMatrix retVal;
	retVal.SetIdentity();

	FVector xVec(1, 0, 0);
	FVector yVec(0, 1, 0);
	FVector zVec(0, 0, 1);
	switch (Axis)
	{
		case 0:	   // X Axis
			retVal.SetAxes(&yVec, &zVec, &xVec);
			break;
		case 1:	   // Y Axis
			retVal.SetAxes(&xVec, &zVec, &yVec);
			break;
		case 2:	   // We keep identity set...
		default:
			break;
	}
	return retVal;
}

void GetLoopStartStopIndexes(
	int& OutStart, int& OutStop, int& OutAxisDirection, const FMajorAxes& MajorAxes, const unsigned& index, const int zDimension)
{
	OutAxisDirection = GetAxisDirection(MajorAxes, index);
	if (OutAxisDirection == -1)
	{
		OutStart = zDimension - 1;
		OutStop = -1;	 // We want to go all the way to zero, so stop at -1
	}
	else
	{
		OutStart = 0;
		OutStop = zDimension;	 // want to go to Z - 1, so stop at Z
	}
}

void TransitionBufferResources(
	FRHICommandListImmediate& RHICmdList, FRHITexture* NewlyReadableTexture, FRHIUnorderedAccessView* NewlyWriteableUAV)
{
	// 	RHICmdList.Transition(FRHITransitionInfo(NewlyReadableTexture, ERHIAccess::UAVGraphics, ERHIAccess::UAVCompute));
	//
	// 	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, NewlyReadableTexture);
	// 	RHICmdList.TransitionResource(
	// 		EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EComputeToCompute, NewlyWriteableUAV);
}
