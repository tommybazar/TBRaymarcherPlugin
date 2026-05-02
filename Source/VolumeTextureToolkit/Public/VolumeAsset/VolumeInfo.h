// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "CoreMinimal.h"

#include "VolumeInfo.generated.h"

/// Voxel format of a loaded volume.
UENUM(BlueprintType)
enum class EVolumeVoxelFormat : uint8
{
    // 1 byte
    UnsignedChar = 0,
    SignedChar = 1,
    // 2 bytes
    UnsignedShort = 2,
    SignedShort = 3,
    // 4 bytes
    UnsignedInt = 4,
    SignedInt = 5,
    // 4 bytes float
    Float = 6
    // #TODO maybe double? Unreal materials don't support them anyways...
};

/// Struct for raymarch windowing parameters. These work exactly the same as DICOM window.
USTRUCT(BlueprintType)
struct FWindowingParameters
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float Center = 0.5;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float Width = 1.0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool LowCutoff = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool HighCutoff = true;

    /** Transforms the 4 values into a FLinear color to be used in materials.**/
    FLinearColor ToLinearColor()
    {
        return FLinearColor(Center, Width, LowCutoff, HighCutoff);
    }

    // Converts a value to a normalized position within the window.
    // Corresponds to `GetTransferFuncPosition` in WindowedSampling.usf
    float GetPositionInWindow(float inValue) const
    {
        return (inValue - Center + (Width / 2.0)) / Width;
    }

    float GetValueFromWindowPosition(float inWindowValue) const
    {
        return Center + (inWindowValue - 0.5f) * Width;
    }
};

/// Contains information about the volume loaded from the Various volumetric data file formats supported.
USTRUCT(BlueprintType)
struct VOLUMETEXTURETOOLKIT_API FVolumeInfo
{
    GENERATED_BODY()
public:
    /// If true, parsing succeeded. If false, this Volume Info is unusable.
    bool bParseWasSuccessful = false;

    /// Name of the volume file that was loaded, including extension.
    UPROPERTY(VisibleAnywhere)
    FString DataFileName;

    /// Format of voxels loaded from the volume. Does NOT have to match the actual PixelFormat that the VolumeTexture is stored in!
    /// e.g. this could be UChar and VolumeTexture is saved as a EPixelFormat::Float - so don't use for calculating sizes!
    UPROPERTY(VisibleAnywhere)
    EVolumeVoxelFormat OriginalFormat = EVolumeVoxelFormat::UnsignedChar;

    /// The format we're using after load has been finished. Takes into account being normalized or converted to float.
    UPROPERTY(VisibleAnywhere)
    EVolumeVoxelFormat ActualFormat = EVolumeVoxelFormat::UnsignedChar;

    // Size of volume in voxels.
    UPROPERTY(VisibleAnywhere)
    FIntVector Dimensions = FIntVector(0, 0, 0);

    // Size of a voxel in mm.
    UPROPERTY(VisibleAnywhere)
    FVector Spacing = FVector(0, 0, 0);

    // Size of the whole volume in mm (equals Dimensions * Spacing)
    UPROPERTY(VisibleAnywhere)
    FVector WorldDimensions = FVector(0, 0, 0);

    // Default windowing parameters used when this volume is loaded.
    UPROPERTY(EditAnywhere)
    FWindowingParameters DefaultWindowingParameters;

    // If true, the values in the texture have been normalized from [MinValue, MaxValue] to [0, 1] range.
    UPROPERTY(VisibleAnywhere)
    bool bIsNormalized = false;

    // Lowest value voxel in the volume in the original volume (before normalization).
    UPROPERTY(VisibleAnywhere)
    float MinValue = -1000;

    // Highest value voxel in the volume in the original volume (before normalization).
    UPROPERTY(VisibleAnywhere)
    float MaxValue = 3000;

    bool bIsCompressed = false;

    int32 CompressedByteSize = 0;

    // Returns the number of bytes needed to store this Volume.
    int64 GetByteSize() const;

    // Returns the number of voxels in this volume.
    int64 GetTotalVoxels() const;

    // Properties not visible to blueprints (used only when loading)
    // Will reflect the ActualFormat rather than OriginalFormat.
    bool bIsSigned;

    // Will reflect the ActualFormat rather than OriginalFormat.
    size_t BytesPerVoxel;

    // Initialized to max() so that any int lowers it on first call.
    int minSliceNumber{std::numeric_limits<int>::max()};

    // Initialized to min() so that any int increases it on first call.
    int maxSliceNumber{std::numeric_limits<int>::min()};

    // Normalizes an input value from the range [MinValue, MaxValue] to [0,1]. Note that values can be outside of the range,
    // e.g. MinValue - (MaxValue - MinValue) will be normalized to -1.
    float NormalizeValue(float InValue);

    /// Converts a [0,1] normalized value to [Min, Max] range.
    float DenormalizeValue(float InValue);

    /// Normalizes a range to 0-1 depending on the size of the original data.
    float NormalizeRange(float InRange);

    /// Converts a [0,1] normalized range to the range of the original data (e.g. 1 will get converted to (MaxValue - MinValue))
    float DenormalizeRange(float InRange);

    static int32 VoxelFormatByteSize(EVolumeVoxelFormat InFormat);

    static bool IsVoxelFormatSigned(EVolumeVoxelFormat InFormat);

    static EPixelFormat VoxelFormatToPixelFormat(EVolumeVoxelFormat InFormat);

    /// Updates min + max slice numbers saved in the VolumeInfo to reflect having a slice with the provided number.
    void UpdateMinMaxSliceNumber(int SliceNumber);

    FString ToString() const;
};
