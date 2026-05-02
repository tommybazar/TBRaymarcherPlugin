//
// This file contains a compute volume texture definition.
//

#include "RenderTargetVolumeMipped.h"

#include "Containers/ResourceArray.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Engine/VolumeTexture.h"
#include "RenderUtils.h"
#include "TextureResource.h"

void URenderTargetVolumeMipped::Init(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, int32 InMips, EPixelFormat InFormat)
{
	check((InSizeX > 0) && (InSizeY > 0) && (InSizeZ > 0));

	// set required size/format
	SizeX = InSizeX;
	SizeY = InSizeY;
	SizeZ = InSizeZ;
	OverrideFormat = InFormat;
	NumMips = InMips;
	// Recreate the texture's resource.
	UpdateResource();
}

FTextureResource* URenderTargetVolumeMipped::CreateResource()
{
	if (GetNumMips() > 0 && GSupportsTexture3D)
	{
		return new FTexture3DComputeResource(this);
	}
	else if (GetNumMips() == 0)
	{
		UE_LOG(LogTexture, Warning, TEXT("%s contains no miplevels! Please delete."), *GetFullName());
	}
	else if (!GSupportsTexture3D)
	{
		UE_LOG(LogTexture, Warning, TEXT("%s cannot be created, rhi does not support 3d textures."), *GetFullName());
	}
	return nullptr;
}

#if WITH_EDITOR
void URenderTargetVolumeMipped::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	constexpr int32 MaxSize = 2048;

	EPixelFormat Format = GetFormat();
    if (Format != PF_Unknown)
    {
        SizeX = FMath::Clamp<int32>(SizeX - (SizeX % GPixelFormats[Format].BlockSizeX), 1, MaxSize);
        SizeY = FMath::Clamp<int32>(SizeY - (SizeY % GPixelFormats[Format].BlockSizeY), 1, MaxSize);
        SizeZ = FMath::Clamp<int32>(SizeZ - (SizeZ % GPixelFormats[Format].BlockSizeZ), 1, MaxSize);
    }

	UTexture::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void URenderTargetVolumeMipped::PostLoad()
{
	UTexture::PostLoad();

	if (!FPlatformProperties::SupportsWindowedMode())
	{
		// clamp the render target size in order to avoid reallocating the scene render targets
		SizeX = FMath::Min<int32>(SizeX, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
		SizeY = FMath::Min<int32>(SizeY, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
		SizeZ = FMath::Min<int32>(SizeZ, FMath::Min<int32>(GSystemResolution.ResX, GSystemResolution.ResY));
	}
}

void URenderTargetVolumeMipped::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	UTexture::GetResourceSizeEx(CumulativeResourceSize);
	// Todo: Add the size of the mipmaps
}

FString URenderTargetVolumeMipped::GetDesc()
{
	return FString::Printf(TEXT("Mipped (%d Mip) Render Volume %dx%d%d[%s]"), NumMips, SizeX, SizeY, SizeZ,
		GPixelFormats[GetFormat()].Name);
}

bool URenderTargetVolumeMipped::CanConvertToTexture(
	ETextureSourceFormat& OutTextureSourceFormat, EPixelFormat& OutPixelFormat, FText* OutErrorMessage) const
{
	const EPixelFormat LocalFormat = GetFormat();
	// this is not actually required, TextureSourceFormat is a free choice
	// volumes will always use the 16F read path, that comes from GetReadPixelsFormat()
	auto TextureSourceFormat = TSF_RGBA16F;

	if ((SizeX <= 0) || (SizeY <= 0) || (SizeZ <= 0))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = FText::Format(NSLOCTEXT("TextureRenderTargetVolume", "InvalidSizeForConversionToTexture",
												 "Invalid size ({0},{1},{2}) for converting {3} to {4}"),
				FText::AsNumber(SizeX), FText::AsNumber(SizeY), FText::AsNumber(SizeZ), FText::FromString(GetClass()->GetName()),
				FText::FromString(GetTextureUClass()->GetName()));
		}
		return false;
	}

	OutPixelFormat = LocalFormat;
	OutTextureSourceFormat = TextureSourceFormat;
	return true;
}

TSubclassOf<UTexture> URenderTargetVolumeMipped::GetTextureUClass() const
{
	return UVolumeTexture::StaticClass();
}

EPixelFormat URenderTargetVolumeMipped::GetFormat() const
{
	return OverrideFormat;
}

bool URenderTargetVolumeMipped::IsSRGB() const
{
	return true;
}

float URenderTargetVolumeMipped::GetDisplayGamma() const
{
	return 1.0;
}

ETextureClass URenderTargetVolumeMipped::GetRenderTargetTextureClass() const
{
	return ETextureClass::Volume;
}