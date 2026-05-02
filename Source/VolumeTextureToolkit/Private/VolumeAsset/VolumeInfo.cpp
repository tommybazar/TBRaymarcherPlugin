// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "VolumeAsset/VolumeInfo.h"

int64 FVolumeInfo::GetByteSize() const
{
    return Dimensions.X * Dimensions.Y * Dimensions.Z * BytesPerVoxel;
}

int64 FVolumeInfo::GetTotalVoxels() const
{
    return Dimensions.X * Dimensions.Y * Dimensions.Z;
}

float FVolumeInfo::NormalizeValue(float InValue)
{
    if (!bIsNormalized)
    {
        return InValue;
    }
    // Normalize on the range of [Min, Max]
    return ((InValue - MinValue) / (MaxValue - MinValue));
}

float FVolumeInfo::DenormalizeValue(float InValue)
{
    if (!bIsNormalized)
    {
        return InValue;
    }
    return ((InValue * (MaxValue - MinValue)) + MinValue);
}

float FVolumeInfo::NormalizeRange(float InRange)
{
    if (!bIsNormalized)
    {
        return InRange;
    }
    // Normalize the range from [Max - Min] to 1
    return ((InRange) / (MaxValue - MinValue));
}

float FVolumeInfo::DenormalizeRange(float InRange)
{
    if (!bIsNormalized)
    {
        return InRange;
    }
    // Normalize the range from [Max - Min] to 1
    return (InRange * (MaxValue - MinValue));
}

int32 FVolumeInfo::VoxelFormatByteSize(EVolumeVoxelFormat InFormat)
{
    switch (InFormat)
    {
        case EVolumeVoxelFormat::UnsignedChar:    // fall through
        case EVolumeVoxelFormat::SignedChar:
            return 1;
        case EVolumeVoxelFormat::UnsignedShort:    // fall through
        case EVolumeVoxelFormat::SignedShort:
            return 2;
        case EVolumeVoxelFormat::UnsignedInt:    // fall through
        case EVolumeVoxelFormat::SignedInt:      // fall through
        case EVolumeVoxelFormat::Float:
            return 4;
        default:
            ensure(false);
            return 0;
    }
}

bool FVolumeInfo::IsVoxelFormatSigned(EVolumeVoxelFormat InFormat)
{
    switch (InFormat)
    {
        case EVolumeVoxelFormat::UnsignedChar:     // fall through
        case EVolumeVoxelFormat::UnsignedShort:    // fall through
        case EVolumeVoxelFormat::UnsignedInt:
            return false;
        case EVolumeVoxelFormat::SignedChar:     // fall through
        case EVolumeVoxelFormat::SignedShort:    // fall through
        case EVolumeVoxelFormat::SignedInt:      // fall through
        case EVolumeVoxelFormat::Float:
            return true;
        default:
            ensure(false);
            return false;
    }
}

EPixelFormat FVolumeInfo::VoxelFormatToPixelFormat(EVolumeVoxelFormat InFormat)
{
    switch (InFormat)
    {
        case EVolumeVoxelFormat::UnsignedChar:    // fall through
        case EVolumeVoxelFormat::SignedChar:
            return EPixelFormat::PF_G8;

        case EVolumeVoxelFormat::UnsignedShort:    // fall through
        case EVolumeVoxelFormat::SignedShort:
            return EPixelFormat::PF_G16;

        case EVolumeVoxelFormat::UnsignedInt:
            return EPixelFormat::PF_R32_SINT;    // Experimental - materials will need to use asuint() to read the actual value!
        case EVolumeVoxelFormat::SignedInt:
            return EPixelFormat::PF_R32_SINT;    // Experimental - materials will need to use asint() to read the actual value!

        case EVolumeVoxelFormat::Float:
            return EPixelFormat::PF_R32_FLOAT;    // Cannot be saved.
        default:
            ensure(false);
            return EPixelFormat::PF_Unknown;
    }
}

void FVolumeInfo::UpdateMinMaxSliceNumber(int SliceNumber)
{
    if (SliceNumber < minSliceNumber)
    {
        minSliceNumber = SliceNumber;
    }
    if (SliceNumber > maxSliceNumber)
    {
        maxSliceNumber = SliceNumber;
    }
}

FString FVolumeInfo::ToString() const
{
    FString text = "File name " + DataFileName + " details:" + "\nDimensions = " + Dimensions.ToString() +
                   "\nSpacing : " + Spacing.ToString() + "\nWorld Size MM : " + Dimensions.ToString() +
                   "\nDefault window center : " + FString::SanitizeFloat(DefaultWindowingParameters.Center) +
                   "\nDefault window width : " + FString::SanitizeFloat(DefaultWindowingParameters.Width) + "\nOriginal Range : [" +
                   FString::SanitizeFloat(MinValue) + " - " + FString::SanitizeFloat(MaxValue) + "]";
    return text;
}
