// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).


#include "Actor/FractalVisualizerScreen.h"

#include "Engine/TextureRenderTarget2D.h"

const FName AFractalVisualizerScreen::VisMat_ScreenCountParam = "ScreenCount";
const FName AFractalVisualizerScreen::VisMat_ScreenIndexParam = "ScreenIndex";

const FName AFractalVisualizerScreen::VisMat_BlendColorLowParam = "BlendColorLow";
const FName AFractalVisualizerScreen::VisMat_BlendColorMidParam = "BlendColorMid";
const FName AFractalVisualizerScreen::VisMat_BlendColorHighParam = "BlendColorHigh";

const FName AFractalVisualizerScreen::VisMat_MaxHeightParam = "MaxHeight";
const FName AFractalVisualizerScreen::VisMat_MetallicParam = "Metallic";
const FName AFractalVisualizerScreen::VisMat_RoughnessParam = "Roughness";
const FName AFractalVisualizerScreen::VisMat_SpecularParam = "Specular";

const FName AFractalVisualizerScreen::VisMat_HeightMapParam = "HeightMap";
const FName AFractalVisualizerScreen::VisMat_NormalMapParam = "NormalMap";

#pragma optimize("", off)

// Sets default values
AFractalVisualizerScreen::AFractalVisualizerScreen()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

// Called when the game starts or when spawned
void AFractalVisualizerScreen::BeginPlay()
{
	Super::BeginPlay();
}

void AFractalVisualizerScreen::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (MeshComponents.Num())
	{
		for (UStaticMeshComponent* Component : MeshComponents)
		{
			Component->DestroyComponent();
		}
		MeshComponents.Empty();
	}

	if (!ScreenMesh || !VisualizeMaterialBase)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot construct Fractal Visualizer Screen - no static mesh or base material is set!"));
		return;
	}

	FVector2D TotalSize = XYScreenCount * ScreenSize;
	FVector2D TopLeft = (TotalSize / 2) * (-1);
	FVector ScreenSize3D = FVector(ScreenSize, 1);
	FVector PanelScaling = ScreenSize3D / (ScreenMesh->GetBounds().BoxExtent * 2);
	PanelScaling.Z = 1;
	
	FVector2D ScreenCountVec(XYScreenCount, XYScreenCount);
	
	for (int x = 0; x < XYScreenCount; x++)
	{
		for (int y = 0; y < XYScreenCount; y++)
		{
			FVector2D ScreenIndexVec((float)x, (float)y);
			FVector2D PanelPosition = TopLeft + FVector2D(x * ScreenSize.X, y * ScreenSize.Y);
			FTransform ScreenTransform = FTransform::Identity;
			ScreenTransform.SetLocation(FVector(PanelPosition, 0));
			ScreenTransform.SetScale3D(PanelScaling);

			UStaticMeshComponent* NewMeshComponent = NewObject<UStaticMeshComponent>(this);
			NewMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			NewMeshComponent->SetRelativeTransform(ScreenTransform);
			NewMeshComponent->RegisterComponent();

			NewMeshComponent->SetStaticMesh(ScreenMesh);
			MeshComponents.Add(NewMeshComponent);
			
			if (VisualizeMaterialBase)
			{
				UMaterialInstanceDynamic* ScreenMaterial = UMaterialInstanceDynamic::Create(VisualizeMaterialBase, this);
				SetSharedMaterialParameters(ScreenMaterial);
				SetScreenSpecificParameters(ScreenMaterial, ScreenCountVec, ScreenIndexVec);
				VisualizeMaterialsDynamic.Add(ScreenMaterial);
				NewMeshComponent->SetMaterial(0, ScreenMaterial);
			}
			
			UE_LOG(LogTemp, Display, TEXT("Constructed plane index %d:%d of %dx%d total"), x, y, XYScreenCount, XYScreenCount);
		}
	}
}

void AFractalVisualizerScreen::BeginDestroy()
{
	Super::BeginDestroy();
	
	if (MeshComponents.Num())
	{
		for (UStaticMeshComponent* Component : MeshComponents)
		{
			Component->UnregisterComponent();
		}
		MeshComponents.Empty();
	}
}

void AFractalVisualizerScreen::SetScreenSpecificParameters(UMaterialInstanceDynamic* Material, FVector2D ScreenCount,
	FVector2D ScreenIndex)
{
	FLinearColor ScreenCountVector = FLinearColor(ScreenCount.X, ScreenCount.Y, 0, 0);
	FLinearColor ScreenIndexVector = FLinearColor(ScreenIndex.X, ScreenIndex.Y, 0, 0);
	Material->SetVectorParameterValue(VisMat_ScreenCountParam, ScreenCountVector);
	Material->SetVectorParameterValue(VisMat_ScreenIndexParam, ScreenIndexVector);
}

void AFractalVisualizerScreen::SetSharedMaterialParameters(UMaterialInstanceDynamic* Material)
{
	Material->SetScalarParameterValue(VisMat_MetallicParam, Metallic);
	Material->SetScalarParameterValue(VisMat_RoughnessParam, Roughness);
	Material->SetScalarParameterValue(VisMat_SpecularParam, Specular);
	Material->SetScalarParameterValue(VisMat_MaxHeightParam, MaxHeight);

	Material->SetTextureParameterValue(VisMat_HeightMapParam, HeightMap);
	Material->SetTextureParameterValue(VisMat_NormalMapParam, NormalMap);

	Material->SetVectorParameterValue(VisMat_BlendColorLowParam, BlendColorLow);
	Material->SetVectorParameterValue(VisMat_BlendColorMidParam, BlendColorMid);
	Material->SetVectorParameterValue(VisMat_BlendColorHighParam, BlendColorHigh);
}

void AFractalVisualizerScreen::SetMaxHeight(float InMaxHeight)
{
	MaxHeight = InMaxHeight;
	for (UMaterialInstanceDynamic* Material : VisualizeMaterialsDynamic)
	{
		Material->SetScalarParameterValue(VisMat_MaxHeightParam, MaxHeight);
	}
}

void AFractalVisualizerScreen::SetMetallic(float InMetallic)
{
	Metallic = InMetallic;
	for (UMaterialInstanceDynamic* Material : VisualizeMaterialsDynamic)
	{
		Material->SetScalarParameterValue(VisMat_MetallicParam, Metallic);
	}
}

void AFractalVisualizerScreen::SetRoughness(float InRoughness)
{
	Roughness = InRoughness;
	for (UMaterialInstanceDynamic* Material : VisualizeMaterialsDynamic)
	{
		Material->SetScalarParameterValue(VisMat_RoughnessParam, Roughness);
	}
}

void AFractalVisualizerScreen::SetSpecular(float InSpecular)
{
	Specular = InSpecular;
	for (UMaterialInstanceDynamic* Material : VisualizeMaterialsDynamic)
	{
		Material->SetScalarParameterValue(VisMat_SpecularParam, Specular);
	}
}

void AFractalVisualizerScreen::SetBlendColorLow(FLinearColor& Color)
{
	BlendColorLow = Color;
	for (UMaterialInstanceDynamic* Material : VisualizeMaterialsDynamic)
	{
		Material->SetVectorParameterValue(VisMat_BlendColorLowParam, BlendColorLow);
	}
}

void AFractalVisualizerScreen::SetBlendColorMid(FLinearColor& Color)
{
	BlendColorMid = Color;
	for (UMaterialInstanceDynamic* Material : VisualizeMaterialsDynamic)
	{
		Material->SetVectorParameterValue(VisMat_BlendColorMidParam, BlendColorMid);
	}
}

void AFractalVisualizerScreen::SetBlendColorHigh(FLinearColor& Color)
{
	BlendColorHigh = Color;
	for (UMaterialInstanceDynamic* Material : VisualizeMaterialsDynamic)
	{
		Material->SetVectorParameterValue(VisMat_BlendColorHighParam, BlendColorHigh);
	}
}

#pragma optimize("", on)