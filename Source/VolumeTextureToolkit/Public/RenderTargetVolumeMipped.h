#pragma once
#include "CoreMinimal.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "UObject/ObjectMacros.h"

#include "RenderTargetVolumeMipped.generated.h"

class FTexture3DComputeResource;

UCLASS(hidecategories = (Object, Compositing, ImportSettings))
class VOLUMETEXTURETOOLKIT_API URenderTargetVolumeMipped : public UTextureRenderTargetVolume
{
	GENERATED_BODY()

public:
	void Init(uint32 InSizeX, uint32 InSizeY, uint32 InSizeZ, int32 InMips, EPixelFormat InFormat);

	// Define the number of Mips.
	// @warning: Make sure the number of mips defined here is the same as number of mips generated in shader.
	int32 NumMips;

	// The only thing we need to override so that a FTexture3DResource gets created with UAV flag.
	virtual FTextureResource* CreateResource() override;

	// Override stuff because these functions are missing ENGINE_API, so they won't link outside the engine module.
	// Implementation is exclusively calling Super::functions;

	virtual float GetSurfaceWidth() const override
	{
		return static_cast<float>(SizeX);
	}

	virtual float GetSurfaceHeight() const override
	{
		return static_cast<float>(SizeY);
	}

	virtual float GetSurfaceDepth() const override
	{
		return static_cast<float>(SizeZ);
	}

	virtual uint32 GetSurfaceArraySize() const override
	{
		return 0;
	}

	virtual EMaterialValueType GetMaterialType() const override
	{
		return MCT_VolumeTexture;
	}

	int32 GetNumMips() const
	{
		return NumMips;
	}

	FTexture3DComputeResource* GetMippedTexture3DRTResource() const
	{
		return MippedTexture3DRTResource;
	}

	///
	/// Following functions are stubs/copypasta from UTextureRenderTargetVolume, because Epic can't be bothered to export symbols
	/// with ENGINE_API on their functions.
	///
	virtual bool CanConvertToTexture(
		ETextureSourceFormat& OutTextureSourceFormat, EPixelFormat& OutPixelFormat, FText* OutErrorMessage) const override;
	virtual TSubclassOf<UTexture> GetTextureUClass() const override;
	virtual EPixelFormat GetFormat() const override;
	virtual bool IsSRGB() const override;
	virtual float GetDisplayGamma() const override;
	virtual ETextureClass GetRenderTargetTextureClass() const override;
	///
	/// end of copypasta
	///

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif	  // WITH_EDITOR
	virtual void PostLoad() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual FString GetDesc() override;

	FTexture3DComputeResource* MippedTexture3DRTResource;
};

// A bare-bones resource created for storing mipped 3D textures that should be accessible from compute shaders.
class FTexture3DComputeResource : public FTextureResource
{
public:
	/**
	 * Minimal initialization constructor.
	 * @param InOwner - The UVolumeTexture which this FComputeTexture3DResource represents.
	 */
	FTexture3DComputeResource(URenderTargetVolumeMipped* InOwner)
		: Owner(InOwner)
		, SizeX(InOwner->SizeX)
		, SizeY(InOwner->SizeY)
		, SizeZ(InOwner->SizeZ)
		, NumMips(InOwner->GetNumMips())
		, PixelFormat(InOwner->OverrideFormat)
		, TextureReference(&InOwner->TextureReference)
	{
		check(0 < NumMips && NumMips <= MAX_TEXTURE_MIP_COUNT);
		check(FMath::IsPowerOfTwo(SizeX) && FMath::IsPowerOfTwo(SizeY) && FMath::IsPowerOfTwo(SizeZ)) uint32 MinAxis =
			FMath::Min3(SizeX, SizeY, SizeZ);
		check((1U << (NumMips - 1)) <= MinAxis);

		TextureName = Owner->GetName();

		// This one line is the point of this whole file - add TexCreate_UAV to the resource creation flags so we can target the
		// texture in Compute shaders.
		CreationFlags = (Owner->SRGB ? ETextureCreateFlags::SRGB : ETextureCreateFlags::None) | ETextureCreateFlags::UAV |
						ETextureCreateFlags::ShaderResource | ETextureCreateFlags::RenderTargetable;

		SamplerFilter = SF_Trilinear;
		bGreyScaleFormat = (PixelFormat == PF_G8) || (PixelFormat == PF_G16) || (PixelFormat == PF_BC4);
	}

	/**
	 * Called when the resource is initialized. This is only called by the rendering thread.
	 */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		// Create the RHI texture.
		FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create3D(*TextureName, SizeX, SizeY, SizeZ, PixelFormat);
		Desc.SetFlags(CreationFlags);
		Desc.SetNumMips(NumMips);

		RenderTargetTextureRHI = RHICreateTexture(Desc);
		TextureRHI = RenderTargetTextureRHI;

		TextureRHI->SetName(*TextureName);
		RHIBindDebugLabelName(TextureRHI, *TextureName);

		for (int i = 0; i < NumMips; i++)
		{
			UnorderedAccessViewRHIs.Add(
				FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(TextureRHI, i));
		}

		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);

		Owner->MippedTexture3DRTResource = this;

		// Create the sampler state RHI resource.
		const FSamplerStateInitializerRHI SamplerStateInitializer(SamplerFilter, AM_Wrap, AM_Wrap, AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	virtual void ReleaseRHI() override
	{
		if (TextureReference)
		{
			RHIUpdateTextureReference(TextureReference->TextureReferenceRHI, nullptr);
		}
		RenderTargetTextureRHI.SafeRelease();
		FTextureResource::ReleaseRHI();
	}

	/** Returns the width of the texture in pixels. */
	virtual uint32 GetSizeX() const override
	{
		return SizeX;
	}

	/** Returns the height of the texture in pixels. */
	virtual uint32 GetSizeY() const override
	{
		return SizeY;
	}

	/** Returns the depth of the texture in pixels. */
	virtual uint32 GetSizeZ() const override
	{
		return SizeZ;
	}

public:
	/** The UVolumeTexture which this resource represents */
	URenderTargetVolumeMipped* Owner;

	/** The String name of the texture asset */
	FString TextureName;

	/** Dimension X of the resource	*/
	uint32 SizeX;
	/** Dimension Y of the resource	*/
	uint32 SizeY;
	/** Dimension Z of the resource	*/
	uint32 SizeZ;

	/** Num of mips of the texture */
	int32 NumMips;
	/** Format of the texture */
	EPixelFormat PixelFormat;
	/** Creation flags of the texture */
	ETextureCreateFlags CreationFlags;

	/** The filtering to use for this texture */
	ESamplerFilter SamplerFilter;

	/** A reference to the texture's RHI resource as a texture 3D. */
	FTexture3DRHIRef RenderTargetTextureRHI;

	FTextureReference* TextureReference;

	// UAVs for each mip level - ordered from the largest (mip 0 = full quality) to the lowest (mip NumMips - 1)
	TArray<FUnorderedAccessViewRHIRef> UnorderedAccessViewRHIs;
};
