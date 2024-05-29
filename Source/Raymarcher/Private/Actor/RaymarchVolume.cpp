// Copyright 2021 Tomas Bartipan and Technical University of Munich.
// Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "Actor/RaymarchVolume.h"

#include "GenericPlatform/GenericPlatformTime.h"
#include "RenderTargetVolumeMipped.h"
#include "Rendering/RaymarchMaterialParameters.h"
#include "TextureUtilities.h"
#include "UObject/SavePackage.h"
#include "Util/RaymarchUtils.h"
#include "VolumeAsset/Loaders/MHDLoader.h"
#include "VolumeAsset/VolumeAsset.h"

#include <Curves/CurveLinearColor.h>
#include <Engine/TextureRenderTargetVolume.h>

#include <bitset>

DEFINE_LOG_CATEGORY(LogRaymarchVolume)

#if !UE_BUILD_SHIPPING
#pragma optimize("", off)
#endif

// Sets default values
ARaymarchVolume::ARaymarchVolume() : AActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SetActorEnableCollision(true);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Default Scene Root"));
	RootComponent->SetWorldScale3D(FVector(1.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> UnitCubeInsideOut(
		TEXT("/TBRaymarcherPlugin/Meshes/Unit_Cube_Inside_Out"));

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Raymarch Cube Static Mesh"));
	/// Set basic unit cube properties.
	if (UnitCubeInsideOut.Succeeded())
	{
		StaticMeshComponent->SetStaticMesh(UnitCubeInsideOut.Object);
		StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
		StaticMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
		StaticMeshComponent->SetRelativeScale3D(FVector(100.0f));
		StaticMeshComponent->SetupAttachment(RootComponent);
	}

	// Create CubeBorderMeshComponent and find and assign cube border mesh (that's a cube with only edges visible).
	CubeBorderMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Raymarch Volume Cube Border"));
	CubeBorderMeshComponent->SetupAttachment(StaticMeshComponent);
	CubeBorderMeshComponent->SetRelativeScale3D(FVector(1.01));
	CubeBorderMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeBorder(TEXT("/TBRaymarcherPlugin/Meshes/Unit_Cube"));

	if (CubeBorder.Succeeded())
	{
		// Find and assign cube material.
		CubeBorderMeshComponent->SetStaticMesh(CubeBorder.Object);
		static ConstructorHelpers::FObjectFinder<UMaterial> BorderMaterial(TEXT("/TBRaymarcherPlugin/Materials/M_CubeBorder"));
		if (BorderMaterial.Succeeded())
		{
			CubeBorderMeshComponent->SetMaterial(0, BorderMaterial.Object);
		}
	}

	// Find and assign default raymarch materials.
	static ConstructorHelpers::FObjectFinder<UMaterial> LitMaterial(TEXT("/TBRaymarcherPlugin/Materials/M_Raymarch"));
	static ConstructorHelpers::FObjectFinder<UMaterial> IntensityMaterial(
		TEXT("/TBRaymarcherPlugin/Materials/M_Intensity_Raymarch"));
	static ConstructorHelpers::FObjectFinder<UMaterial> OctreeMaterial(TEXT("/TBRaymarcherPlugin/Materials/M_Octree_Raymarch"));

	if (LitMaterial.Succeeded())
	{
		LitRaymarchMaterialBase = LitMaterial.Object;
	}

	if (IntensityMaterial.Succeeded())
	{
		IntensityRaymarchMaterialBase = IntensityMaterial.Object;
	}

	if (OctreeMaterial.Succeeded())
	{
		OctreeRaymarchMaterialBase = OctreeMaterial.Object;
	}

	// Set default values for steps and half-res.
	RaymarchingSteps = 150;
	RaymarchResources.LightVolumeHalfResolution = false;
}

// Called after registering all components. This is the last action performed before editor window is spawned and before BeginPlay.
void ARaymarchVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		// Do not perform this on default class objects or archetype objects
		return;
	}

	if (RaymarchResources.bIsInitialized)
	{
		// Do not perform this if this object already is initialized
		// PostRegisterAllComponents also gets called in every OnPropertyChanged call, so
		// we want to ignore every call to this except the first one.
		return;
	}

	if (LitRaymarchMaterialBase)
	{
		LitRaymarchMaterial = UMaterialInstanceDynamic::Create(LitRaymarchMaterialBase, this, "Lit Raymarch Mat Dynamic Inst");
		// Set default values for the lit and intensity raymarchers.
		LitRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
	}

	if (IntensityRaymarchMaterialBase)
	{
		IntensityRaymarchMaterial =
			UMaterialInstanceDynamic::Create(IntensityRaymarchMaterialBase, this, "Intensity Raymarch Mat Dynamic Inst");

		IntensityRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
	}

	if (OctreeRaymarchMaterialBase)
	{
		OctreeRaymarchMaterial =
			UMaterialInstanceDynamic::Create(OctreeRaymarchMaterialBase, this, "Octree Raymarch Mat Dynamic Inst");
		// Set default valuees for the octree raymarch material.
		OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
		OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::OctreeMip, OctreeVolumeMip);
		OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::OctreeStartingMip, OctreeStartingMip);
		OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::OctreeStartingMip, OctreeStartingMip);
	}

	if (StaticMeshComponent)
	{
		if (LitRaymarchMaterial && SelectRaymarchMaterial == ERaymarchMaterial::Lit)
		{
			StaticMeshComponent->SetMaterial(0, LitRaymarchMaterial);
		}
		else if (IntensityRaymarchMaterial && SelectRaymarchMaterial == ERaymarchMaterial::Intensity)
		{
			StaticMeshComponent->SetMaterial(0, IntensityRaymarchMaterial);
		}
		else if (OctreeRaymarchMaterial && SelectRaymarchMaterial == ERaymarchMaterial::Octree)
		{
			StaticMeshComponent->SetMaterial(0, OctreeRaymarchMaterial);
		}
	}

	if (VolumeAsset)
	{
		SetVolumeAsset(VolumeAsset);
	}
}

void ARaymarchVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	LightParametersMap.Empty();
	for (ARaymarchLight* Light : LightsArray)
	{
		if (Light && Light->LightIntensity > 0.0f)
		{
			LightParametersMap.Add(Light, Light->GetCurrentParameters());
		}
	}
}

#if WITH_EDITOR

void ARaymarchVolume::OnVolumeAssetChangedTF(UCurveLinearColor* Curve)
{
	// Remove OnUpdateGradient delegate from old curve (if it exists)
	bool bChangedCurve = false;
	if (Curve != CurrentTFCurve)
	{
		bChangedCurve = true;
		CurrentTFCurve->OnUpdateCurve.Remove(CurveGradientUpdateDelegateHandle);
	}

	SetTFCurve(Curve);

	// Add gradient update delegate to new curve.
	if (bChangedCurve)
	{
		CurrentTFCurve->OnUpdateCurve.AddUObject(this, &ARaymarchVolume::OnTFColorCurveUpdated);
	}
}

void ARaymarchVolume::OnTFColorCurveUpdated(UCurveBase* Curve, EPropertyChangeType::Type ChangeType)
{
	SetTFCurve(Cast<UCurveLinearColor>(Curve));
}

void ARaymarchVolume::OnImageInfoChangedInEditor()
{
	// Just update parameters from default VolumeAsset values, that's the only thing that can change that's interesting in the Image
	// Info we're initialized.
	RaymarchResources.WindowingParameters = VolumeAsset->ImageInfo.DefaultWindowingParameters;
	SetMaterialWindowingParameters();

	static double LastTimeReset = 0.0f;
	if (SelectRaymarchMaterial == ERaymarchMaterial::Lit)
	{
		// Don't wait for recompute for next frame.

		// Editor-viewport sometimes doesn't tick when it doesn't have focus, so
		// if we're dragging Window Center/Width from within the VolumeAsset window,
		// lighting wouldn't get recomputed until we let go of the slider.

		// #TODO figure out how to force the editor to tick no matter what and this can be changed to :
		// bRequestedRecompute = true;

		// Until then, make sure that this only causes lights recalculating 60 times a second, otherwise sliders can
		// send the event like a 1000 times per second, causing noticeable slowdown.
		double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - 0.016 > LastTimeReset)
		{
			ResetAllLights();
			LastTimeReset = CurrentTime;
		}
	}
}

void ARaymarchVolume::SetOctreeLightParamters()
{
    FVector LightDirection =  OctreeLight->GetCurrentParameters().LightDirection;

    GEngine->AddOnScreenDebugMessage(23,10,FColor::Orange, FString("Light Direction: ") + LightDirection.ToString());

    float LightIntensity = OctreeLight->GetCurrentParameters().LightIntensity;

    OctreeRaymarchMaterial->SetVectorParameterValue(RaymarchParams::LightParams, FLinearColor(
        static_cast<float>(LightDirection.X),
        static_cast<float>(LightDirection.Y),
        static_cast<float>(LightDirection.Z),
        LightIntensity));
}


void ARaymarchVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	//	RaymarchResources.PostEditChangeProperty(PropertyChangedEvent);
	FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, VolumeAsset))
	{
		SetVolumeAsset(VolumeAsset);
		return;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, LightsArray))
	{
		if (SelectRaymarchMaterial == ERaymarchMaterial::Lit)
		{
			bRequestedRecompute = true;
		}
		return;
	}

    //if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, OctreeLight))
	// {
 //        if(OctreeRaymarchMaterial)
 //        {
 //            SetOctreeLightParamters();
 //        }
	// 	return;
	// }

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, ClippingPlane))
	{
		if (SelectRaymarchMaterial == ERaymarchMaterial::Lit)
		{
			bRequestedRecompute = true;
		}
		return;
	}

	// Only writable property in rendering resources is windowing parameters -> update those
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FWindowingParameters, Center) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FWindowingParameters, Width) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FWindowingParameters, HighCutoff) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FWindowingParameters, LowCutoff))
	{
		if (SelectRaymarchMaterial == ERaymarchMaterial::Lit)
		{
			bRequestedRecompute = true;
		}
		SetMaterialWindowingParameters();
		return;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FBasicRaymarchRenderingResources, LightVolumeHalfResolution) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, bLightVolume32Bit))
	{
		InitializeRaymarchResources(RaymarchResources.DataVolumeTextureRef);
		SetMaterialVolumeParameters();
		if (SelectRaymarchMaterial == ERaymarchMaterial::Lit)
		{
			bRequestedRecompute = true;
		}
		return;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, RaymarchingSteps))
	{
		if (RaymarchResources.bIsInitialized)
		{
			LitRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
			IntensityRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
			OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
		}
		return;
	}

	if (PropertyName == GET_ENUMERATOR_NAME_CHECKED(ARaymarchVolume, SelectRaymarchMaterial))
	{
		SwitchRenderer(SelectRaymarchMaterial);
		if (SelectRaymarchMaterial == ERaymarchMaterial::Lit)
		{
			bRequestedRecompute = true;
		}
		if (SelectRaymarchMaterial == ERaymarchMaterial::Octree)
		{
			bRequestedOctreeRebuild = true;
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, OctreeVolumeMip))
	{
		if (RaymarchResources.bIsInitialized)
		{
			OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::OctreeMip, OctreeVolumeMip);
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, OctreeStartingMip))
	{
		if (RaymarchResources.bIsInitialized)
		{
			OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::OctreeStartingMip, OctreeStartingMip);
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARaymarchVolume, WindowMaskEdgeBitsCount))
	{
		if (RaymarchResources.bIsInitialized)
		{
			SetMaterialWindowingParameters();
		}
	}
}

bool ARaymarchVolume::ShouldTickIfViewportsOnly() const
{
	return true;
}

#endif	  // #if WITH_EDITOR

// Called every frame
void ARaymarchVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Uncomment to see logs of potentially weird ticking behavior in-editor when dragging sliders in VolumeInfo.
	//
	// 	static int TickFrame = 0;
	//
	// 	FString Log = "Tick :\nInitialized = " + FString::FromInt(RaymarchResources.bIsInitialized) +
	// 				  "\nRecompute = " + FString::FromInt(bRequestedRecompute) + "\nDeltaTime = " +
	// FString::SanitizeFloat(DeltaTime)
	// +
	// 				  "\nTick number = " + FString::FromInt(TickFrame++);
	//
	// 	GEngine->AddOnScreenDebugMessage(0, 0, FColor::Yellow, Log);

	if (!RaymarchResources.bIsInitialized || !RootComponent->IsVisible())
	{
		// If not initialized, return.
		// #TODO? we could also stop ticks altogether when not initialized.
		return;
	}

	// Volume transform changed or clipping plane moved -> need full recompute.
	if (WorldParameters != GetWorldParameters())
	{
		bRequestedRecompute = true;
		UpdateWorldParameters();
		SetMaterialClippingParameters();
	}

	if (bRequestedOctreeRebuild && SelectRaymarchMaterial == ERaymarchMaterial::Octree)
	{
		URaymarchUtils::GenerateOctree(RaymarchResources);
		// We rebuild the octree. Set to false to prevent additional unwanted rebuild.
		bRequestedOctreeRebuild = false;
	}
    if(OctreeLight && SelectRaymarchMaterial == ERaymarchMaterial::Octree)
    {
        SetOctreeLightParamters();
    }

	// Only check if we need to update lights if we're using Lit raymarch material.
	// (No point in recalculating a light volume that's not currently being used anyways).
	if (SelectRaymarchMaterial == ERaymarchMaterial::Lit)
	{
		// For testing light calculation shader speed - comment out when not testing! (otherwise lights get recalculated every tick
		// for no reason).
		// 		ResetAllLights();
		// 		return;

		if (bRequestedRecompute)
		{
			// If we're requesting recompute or parameters changed,
			ResetAllLights();
		}
		else
		{
			// Check each individual light if it needs an update.
			TArray<ARaymarchLight*> LightsToUpdate;
			for (ARaymarchLight* Light : LightsArray)
			{
				if (!Light)
				{
					continue;
				}
				if (!LightParametersMap.Contains(Light))
				{
					LightParametersMap.Add(Light, Light->GetCurrentParameters());
					LightsToUpdate.Add(Light);
				}
				else if (Light->GetCurrentParameters() != LightParametersMap[Light])
				{
					LightsToUpdate.Add(Light);
				}
			}

			// More than half lights need update -> full reset is quicker
			if ((LightsToUpdate.Num() > 1) && LightsToUpdate.Num() >= (LightsArray.Num() / 2))
			{
				ResetAllLights();
			}
			else
			{
				// Only update the lights that need it.
				for (ARaymarchLight* UpdatedLight : LightsToUpdate)
				{
					UpdateSingleLight(UpdatedLight);
					LightParametersMap[UpdatedLight] = UpdatedLight->GetCurrentParameters();
				}
			}
		}
	}
}

void ARaymarchVolume::ResetAllLights()
{
	if (!RaymarchResources.bIsInitialized)
	{
		return;
	}

	// Clear Light volume to zero.
	UVolumeTextureToolkit::ClearVolumeTexture(RaymarchResources.LightVolumeRenderTarget, 0);

	// Add all lights.
	bool bResetWasSuccessful = true;
	for (ARaymarchLight* Light : LightsArray)
	{
		if (!Light)
		{
			continue;
		}
		bool bLightAddWasSuccessful = false;

		URaymarchUtils::AddDirLightToSingleVolume(
			RaymarchResources, Light->GetCurrentParameters(), true, WorldParameters, bResetWasSuccessful, bFastShader);

		if (!bResetWasSuccessful)
		{
			FString log = "Error. Could not add/remove light " + Light->GetName() + " in volume " + GetName() + " .";
			UE_LOG(LogRaymarchVolume, Error, TEXT("%s"), *log, 3);
			return;
		}
	}

	// False-out request recompute flag when we succeeded in resetting lights.
	bRequestedRecompute = false;
}

void ARaymarchVolume::UpdateSingleLight(ARaymarchLight* UpdatedLight)
{
	bool bLightAddWasSuccessful = false;

	URaymarchUtils::ChangeDirLightInSingleVolume(RaymarchResources, LightParametersMap[UpdatedLight],
		UpdatedLight->GetCurrentParameters(), WorldParameters, bLightAddWasSuccessful);

	if (!bLightAddWasSuccessful)
	{
		FString log = "Error. Could not change light " + UpdatedLight->GetName() + " in volume " + GetName() + " .";
		UE_LOG(LogRaymarchVolume, Error, TEXT("%s"), *log, 3);
	}
}

bool ARaymarchVolume::SetVolumeAsset(UVolumeAsset* InVolumeAsset)
{
	if (!InVolumeAsset)
	{
		return false;
	}

#if WITH_EDITOR
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		if (InVolumeAsset != OldVolumeAsset)
		{
			// If we're in editor and we already have an asset loaded before, unbind the delegate
			// from it's color curve change broadcast and also the OnCurve and OnVolumeInfo changed broadcasts.
			if (OldVolumeAsset)
			{
				OldVolumeAsset->TransferFuncCurve->OnUpdateCurve.Remove(CurveGradientUpdateDelegateHandle);
				OldVolumeAsset->OnCurveChanged.Remove(CurveChangedInVolumeDelegateHandle);
				OldVolumeAsset->OnImageInfoChanged.Remove(VolumeAssetUpdatedDelegateHandle);
			}
			if (InVolumeAsset)
			{
				CurveChangedInVolumeDelegateHandle =
					InVolumeAsset->OnCurveChanged.AddUObject(this, &ARaymarchVolume::OnVolumeAssetChangedTF);
				VolumeAssetUpdatedDelegateHandle =
					InVolumeAsset->OnImageInfoChanged.AddUObject(this, &ARaymarchVolume::OnImageInfoChangedInEditor);
			}
		}
	}
#endif

	// Create the transfer function BEFORE calling initialize resources and assign TF texture AFTER initializing!
	// Initialize resources calls FlushRenderingCommands(), so it ensures the TF is useable by the time we bind it.

	// Generate texture for transfer function from curve or make default (if none provided).
	if (InVolumeAsset->TransferFuncCurve)
	{
		CurrentTFCurve = InVolumeAsset->TransferFuncCurve;
		URaymarchUtils::ColorCurveToTexture(CurrentTFCurve, RaymarchResources.TFTextureRef);

#if WITH_EDITOR
		// Bind a listener to the delegate notifying about color curve changes
		if ((!GetWorld() || !GetWorld()->IsGameWorld()) && InVolumeAsset != OldVolumeAsset)
		{
			CurveGradientUpdateDelegateHandle =
				CurrentTFCurve->OnUpdateCurve.AddUObject(this, &ARaymarchVolume::OnTFColorCurveUpdated);
		}
#endif
	}
	else
	{
		// Create default black-to-white texture if the VolumeAsset doesn't have one.
		URaymarchUtils::MakeDefaultTFTexture(RaymarchResources.TFTextureRef);
	}

	VolumeAsset = InVolumeAsset;
	OldVolumeAsset = InVolumeAsset;

	InitializeRaymarchResources(VolumeAsset->DataTexture);

	if (!RaymarchResources.bIsInitialized)
	{
		UE_LOG(LogRaymarchVolume, Warning, TEXT("Could not initialize raymarching resources!"), 3);
		return false;
	}

	// Set TF Texture in the lit material (after resource init, so FlushRenderingCommands has been called).
	if (LitRaymarchMaterial)
	{
		LitRaymarchMaterial->SetTextureParameterValue(RaymarchParams::TransferFunction, RaymarchResources.TFTextureRef);
	}

	if (OctreeRaymarchMaterial)
	{
		OctreeRaymarchMaterial->SetTextureParameterValue(RaymarchParams::TransferFunction, RaymarchResources.TFTextureRef);
	}

	RaymarchResources.WindowingParameters = VolumeAsset->ImageInfo.DefaultWindowingParameters;

	// Unreal units are in cm, MHD and Dicoms both have sizes in mm -> divide by 10.
	StaticMeshComponent->SetRelativeScale3D(InVolumeAsset->ImageInfo.WorldDimensions / 10);

	// Update world, set all parameters and request recompute.
	UpdateWorldParameters();
	SetAllMaterialParameters();
	bRequestedRecompute = true;
	// Update the octree.
	bRequestedOctreeRebuild = true;

	// Notify listeners that we've loaded a new volume.
	OnVolumeLoaded.ExecuteIfBound();

	return true;
}

void ARaymarchVolume::SetTFCurve(UCurveLinearColor* InTFCurve)
{
	if (InTFCurve)
	{
		CurrentTFCurve = InTFCurve;
		URaymarchUtils::ColorCurveToTexture(CurrentTFCurve, RaymarchResources.TFTextureRef);
		// #TODO flushing rendering commands can lead to hitches, maybe figure out a better way to make sure TF is created in time
		// for the texture parameter to be set.
		// e.g. render-thread promise and game-thread future?
		FlushRenderingCommands();
		// Set TF Texture to the lit and octree material.
		LitRaymarchMaterial->SetTextureParameterValue(RaymarchParams::TransferFunction, RaymarchResources.TFTextureRef);
		OctreeRaymarchMaterial->SetTextureParameterValue(RaymarchParams::TransferFunction, RaymarchResources.TFTextureRef);
		SetMaterialWindowingParameters();
		bRequestedRecompute = true;
	}
}

void ARaymarchVolume::SaveCurrentParamsToVolumeAsset()
{
	if (VolumeAsset)
	{
		VolumeAsset->TransferFuncCurve = CurrentTFCurve;
		VolumeAsset->ImageInfo.DefaultWindowingParameters = RaymarchResources.WindowingParameters;

		UPackage* Package = VolumeAsset->GetOutermost();

		// #TODO This fails in UE5 because of trying to re-save the VolumeTexture bulk data and failing.
		FSavePackageArgs PackageArgs;
		PackageArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
		PackageArgs.SaveFlags = SAVE_NoError;
		UPackage::SavePackage(Package, VolumeAsset, *(Package->GetLoadedPath().GetPackageName()), PackageArgs);
	}
}

bool ARaymarchVolume::LoadMHDFileIntoVolumeTransientR32F(FString FileName)
{
	UVolumeAsset* NewVolumeAsset;

	UMHDLoader* Loader = UMHDLoader::Get();
	NewVolumeAsset = Loader->CreateVolumeFromFile(FileName, false, true);

	if (NewVolumeAsset)
	{
		return SetVolumeAsset(NewVolumeAsset);
	}
	else
	{
		return false;
	}
}

bool ARaymarchVolume::LoadMHDFileIntoVolumeNormalized(FString FileName, bool bPersistent, FString OutFolder)
{
	UVolumeAsset* NewVolumeAsset;

	UMHDLoader* Loader = UMHDLoader::Get();
	NewVolumeAsset = Loader->CreateVolumeFromFile(FileName, true, false);

	if (NewVolumeAsset)
	{
		return SetVolumeAsset(NewVolumeAsset);
	}
	else
	{
		return false;
	}
}

FRaymarchWorldParameters ARaymarchVolume::GetWorldParameters()
{
	FRaymarchWorldParameters retVal;
	if (ClippingPlane)
	{
		retVal.ClippingPlaneParameters = ClippingPlane->GetCurrentParameters();
	}
	else
	{
		// Set clipping plane parameters to ridiculously far and facing away, so that the volume doesn't get clipped at all
		retVal.ClippingPlaneParameters.Center = FVector(0, 0, 100000);
		retVal.ClippingPlaneParameters.Direction = FVector(0, 0, -1);
	}

	retVal.VolumeTransform = StaticMeshComponent->GetComponentTransform();
	return retVal;
}

void ARaymarchVolume::UpdateWorldParameters()
{
	if (ClippingPlane)
	{
		WorldParameters.ClippingPlaneParameters = ClippingPlane->GetCurrentParameters();
	}
	else
	{
		// Set clipping plane parameters to ridiculously far and facing away, so that the volume doesn't get clipped at all
		WorldParameters.ClippingPlaneParameters.Center = FVector(0, 0, 100000);
		WorldParameters.ClippingPlaneParameters.Direction = FVector(0, 0, -1);
	}

	WorldParameters.VolumeTransform = StaticMeshComponent->GetComponentTransform();
}

void ARaymarchVolume::SetAllMaterialParameters()
{
	SetMaterialVolumeParameters();
	SetMaterialWindowingParameters();
	SetMaterialClippingParameters();
}

void ARaymarchVolume::SetMaterialVolumeParameters()
{
	if (IntensityRaymarchMaterial)
	{
		IntensityRaymarchMaterial->SetTextureParameterValue(RaymarchParams::DataVolume, RaymarchResources.DataVolumeTextureRef);
	}
	if (LitRaymarchMaterial)
	{
		LitRaymarchMaterial->SetTextureParameterValue(RaymarchParams::DataVolume, RaymarchResources.DataVolumeTextureRef);
		LitRaymarchMaterial->SetTextureParameterValue(RaymarchParams::LightVolume, RaymarchResources.LightVolumeRenderTarget);
	}
	if (OctreeRaymarchMaterial)
	{
		OctreeRaymarchMaterial->SetTextureParameterValue(RaymarchParams::DataVolume, RaymarchResources.DataVolumeTextureRef);
		OctreeRaymarchMaterial->SetTextureParameterValue(RaymarchParams::OctreeVolume, RaymarchResources.OctreeVolumeRenderTarget);
	}
}

void ARaymarchVolume::SetMaterialWindowingParameters()
{
	if (LitRaymarchMaterial)
	{
		LitRaymarchMaterial->SetVectorParameterValue(
			RaymarchParams::WindowingParams, RaymarchResources.WindowingParameters.ToLinearColor());
	}
	if (IntensityRaymarchMaterial)
	{
		IntensityRaymarchMaterial->SetVectorParameterValue(
			RaymarchParams::WindowingParams, RaymarchResources.WindowingParameters.ToLinearColor());
	}
	if (OctreeRaymarchMaterial)
	{
		OctreeRaymarchMaterial->SetVectorParameterValue(
			RaymarchParams::WindowingParams, RaymarchResources.WindowingParameters.ToLinearColor());

		FVector4 WindowMask = URaymarchUtils::GetWindowingParamsBitMask(RaymarchResources.WindowingParameters, WindowMaskEdgeBitsCount, RaymarchResources.TFTextureRef);
		FLinearColor LinearColor(WindowMask.X, WindowMask.Y, WindowMask.Z, WindowMask.W);
		OctreeRaymarchMaterial->SetVectorParameterValue(RaymarchParams::WindowMask, LinearColor);

		// Uncomment to debug window bit mask.
		// GEngine->AddOnScreenDebugMessage(324, 100, FColor::Orange, std::bitset<32>(LinearColor.R).to_string().c_str());
	}
}

void ARaymarchVolume::SetMaterialClippingParameters()
{
	// Get the Clipping Plane parameters and transform them to local space.
	FClippingPlaneParameters LocalClippingparameters = GetLocalClippingParameters(WorldParameters);
	if (LitRaymarchMaterial)
	{
		LitRaymarchMaterial->SetVectorParameterValue(RaymarchParams::ClippingCenter, LocalClippingparameters.Center);
		LitRaymarchMaterial->SetVectorParameterValue(RaymarchParams::ClippingDirection, LocalClippingparameters.Direction);
	}

	if (IntensityRaymarchMaterial)
	{
		IntensityRaymarchMaterial->SetVectorParameterValue(RaymarchParams::ClippingCenter, LocalClippingparameters.Center);
		IntensityRaymarchMaterial->SetVectorParameterValue(RaymarchParams::ClippingDirection, LocalClippingparameters.Direction);
	}
	if (OctreeRaymarchMaterial)
	{
		OctreeRaymarchMaterial->SetVectorParameterValue(RaymarchParams::ClippingCenter, LocalClippingparameters.Center);
		OctreeRaymarchMaterial->SetVectorParameterValue(RaymarchParams::ClippingDirection, LocalClippingparameters.Direction);
	}
}

void ARaymarchVolume::GetMinMaxValues(float& Min, float& Max)
{
	Min = VolumeAsset->ImageInfo.MinValue;
	Max = VolumeAsset->ImageInfo.MaxValue;
}

float ARaymarchVolume::GetWindowCenter()
{
	return RaymarchResources.WindowingParameters.Center;
}

float ARaymarchVolume::GetWindowWidth()
{
	return RaymarchResources.WindowingParameters.Width;
}

void ARaymarchVolume::SetWindowCenter(const float& Center)
{
	if (Center == RaymarchResources.WindowingParameters.Center)
		return;

	RaymarchResources.WindowingParameters.Center = Center;
	SetMaterialWindowingParameters();
	bRequestedRecompute = true;
}

void ARaymarchVolume::SetWindowWidth(const float& Width)
{
	if (Width == RaymarchResources.WindowingParameters.Width)
		return;

	RaymarchResources.WindowingParameters.Width = Width;
	SetMaterialWindowingParameters();
	bRequestedRecompute = true;
}

void ARaymarchVolume::SetLowCutoff(const bool& LowCutoff)
{
	if (LowCutoff == RaymarchResources.WindowingParameters.LowCutoff)
		return;

	RaymarchResources.WindowingParameters.LowCutoff = LowCutoff;
	SetMaterialWindowingParameters();
	bRequestedRecompute = true;
}

void ARaymarchVolume::SetHighCutoff(const bool& HighCutoff)
{
	if (HighCutoff == RaymarchResources.WindowingParameters.HighCutoff)
		return;

	RaymarchResources.WindowingParameters.HighCutoff = HighCutoff;
	SetMaterialWindowingParameters();
	bRequestedRecompute = true;
}

void ARaymarchVolume::SwitchRenderer(ERaymarchMaterial InSelectRaymarchMaterial)
{
	switch (InSelectRaymarchMaterial)
	{
		case ERaymarchMaterial::Lit:
			StaticMeshComponent->SetMaterial(0, LitRaymarchMaterial);
			break;
		case ERaymarchMaterial::Intensity:
			StaticMeshComponent->SetMaterial(0, IntensityRaymarchMaterial);
			break;
		case ERaymarchMaterial::Octree:
			StaticMeshComponent->SetMaterial(0, OctreeRaymarchMaterial);
			break;
	}
}

void ARaymarchVolume::SetRaymarchSteps(float InRaymarchingSteps)
{
	RaymarchingSteps = InRaymarchingSteps;
	if (LitRaymarchMaterial)
	{
		LitRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
	}

	if (IntensityRaymarchMaterial)
	{
		IntensityRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
	}

	if (OctreeRaymarchMaterial)
	{
		OctreeRaymarchMaterial->SetScalarParameterValue(RaymarchParams::Steps, RaymarchingSteps);
	}
}

void ARaymarchVolume::InitializeRaymarchResources(UVolumeTexture* Volume)
{
	if (RaymarchResources.bIsInitialized)
	{
		FreeRaymarchResources();
	}

	if (!Volume)
	{
		UE_LOG(LogRaymarchVolume, Error, TEXT("Tried to initialize Raymarch resources with no data volume!"));
		return;
	}
	else if (!Volume->GetPlatformData() || Volume->GetSizeX() == 0 || Volume->GetSizeY() == 0 || Volume->GetSizeZ() == 0)
	{
		// Happens in cooking stage where per-platform data isn't initalized. Return.
		UE_LOG(LogRaymarchVolume, Warning,
			TEXT("Following is safe to ignore during cooking :\nTried to initialize Raymarch resources with an unitialized data "
				 "volume with size 0!\nRaymarch volume name = %s, VolumeTexture name = %s"),
			*(GetName()), *(Volume->GetName()));
		return;
	};

	RaymarchResources.DataVolumeTextureRef = Volume;

	int X = Volume->GetSizeX();
	int Y = Volume->GetSizeY();
	int Z = Volume->GetSizeZ();

	// If using half res, divide by two.
	if (RaymarchResources.LightVolumeHalfResolution)
	{
		X = FMath::DivideAndRoundUp(X, 2);
		Y = FMath::DivideAndRoundUp(Y, 2);
		Z = FMath::DivideAndRoundUp(Z, 2);
	}

	EPixelFormat PixelFormat = PF_G8;
	if (bLightVolume32Bit)
	{
		PixelFormat = PF_R32_FLOAT;
	}

	// Call the actual rendering code on RenderThread.
	FIntPoint XBufferSize = FIntPoint(Y, Z);
	FIntPoint YBufferSize = FIntPoint(X, Z);
	FIntPoint ZBufferSize = FIntPoint(X, Y);

	RaymarchResources.LightVolumeRenderTarget = NewObject<UTextureRenderTargetVolume>(this, "Light Volume Render Target");
	RaymarchResources.LightVolumeRenderTarget->bCanCreateUAV = true;
	RaymarchResources.LightVolumeRenderTarget->bHDR = bLightVolume32Bit;
	RaymarchResources.LightVolumeRenderTarget->Init(X, Y, Z, PixelFormat);

	RaymarchResources.OctreeVolumeRenderTarget = NewObject<URenderTargetVolumeMipped>(this, "Octree Render Target");
	RaymarchResources.OctreeVolumeRenderTarget->bCanCreateUAV = true;
	RaymarchResources.OctreeVolumeRenderTarget->bHDR = false;
	RaymarchResources.OctreeVolumeRenderTarget->Init(FMath::RoundUpToPowerOfTwo(Volume->GetSizeX()),
		FMath::RoundUpToPowerOfTwo(Volume->GetSizeY()), FMath::RoundUpToPowerOfTwo(Volume->GetSizeZ()), 4, PF_R32_UINT);

	// Flush rendering commands so that all textures are definitely initialized with resources and we can create a UAV ref.
	FlushRenderingCommands();

	ENQUEUE_RENDER_COMMAND(CaptureCommand)
	(
		[&](FRHICommandListImmediate& RHICmdList)
		{
			RaymarchResources.DataVolumeTextureRef = Volume;

			// Make buffers fully colored if we need to support colored lights.
			URaymarchUtils::CreateBufferTextures(XBufferSize, PixelFormat, RaymarchResources.XYZReadWriteBuffers[0]);
			URaymarchUtils::CreateBufferTextures(YBufferSize, PixelFormat, RaymarchResources.XYZReadWriteBuffers[1]);
			URaymarchUtils::CreateBufferTextures(ZBufferSize, PixelFormat, RaymarchResources.XYZReadWriteBuffers[2]);

			if (!RaymarchResources.LightVolumeRenderTarget || !RaymarchResources.LightVolumeRenderTarget->GetResource() ||
				!RaymarchResources.LightVolumeRenderTarget->GetResource()->TextureRHI)
			{
				// Return if anything was not initialized.
				return;
			}

			RaymarchResources.LightVolumeUAVRef =
				RHICreateUnorderedAccessView(RaymarchResources.LightVolumeRenderTarget->GetResource()->TextureRHI);

			if (!RaymarchResources.OctreeVolumeRenderTarget || !RaymarchResources.OctreeVolumeRenderTarget->GetResource() ||
				!RaymarchResources.OctreeVolumeRenderTarget->GetResource()->TextureRHI)
			{
				// Return if anything was not initialized.
				return;
			}

			RaymarchResources.OctreeUAVRef =
				RHICreateUnorderedAccessView(RaymarchResources.OctreeVolumeRenderTarget->GetResource()->TextureRHI);

			RaymarchResources.bIsInitialized = true;
		});
	FlushRenderingCommands();
	if (RaymarchResources.bIsInitialized)
	{
		SetMaterialVolumeParameters();
	}
}

void ARaymarchVolume::FreeRaymarchResources()
{
	ENQUEUE_RENDER_COMMAND(CaptureCommand)
	(
		[&](FRHICommandListImmediate& RHICmdList)
		{
			RaymarchResources.DataVolumeTextureRef = nullptr;
			if (RaymarchResources.LightVolumeRenderTarget)
			{
				RaymarchResources.LightVolumeRenderTarget->MarkAsGarbage();
				RaymarchResources.LightVolumeRenderTarget = nullptr;
			}

			if (RaymarchResources.OctreeVolumeRenderTarget)
			{
				RaymarchResources.OctreeVolumeRenderTarget->MarkAsGarbage();
				RaymarchResources.OctreeVolumeRenderTarget = nullptr;
			}

			for (OneAxisReadWriteBufferResources& Buffer : RaymarchResources.XYZReadWriteBuffers)
			{
				URaymarchUtils::ReleaseOneAxisReadWriteBufferResources(Buffer);
			}

			RaymarchResources.bIsInitialized = false;
		});
	FlushRenderingCommands();
}

#if !UE_BUILD_SHIPPING
#pragma optimize("", on)
#endif
