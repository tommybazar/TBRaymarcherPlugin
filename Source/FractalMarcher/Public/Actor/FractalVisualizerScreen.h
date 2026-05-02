// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "FractalVisualizerScreen.generated.h"

UCLASS()
class FRACTALMARCHER_API AFractalVisualizerScreen : public AActor
{
    GENERATED_BODY()

public:
    static const FName VisMat_NormalMapParam;
    // Sets default values for this actor's properties
    AFractalVisualizerScreen();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    static const FName VisMat_ScreenCountParam;
    static const FName VisMat_ScreenIndexParam;
    static const FName VisMat_BlendColorLowParam;
    static const FName VisMat_BlendColorMidParam;
    static const FName VisMat_BlendColorHighParam;
    static const FName VisMat_MaxHeightParam;
    static const FName VisMat_MetallicParam;
    static const FName VisMat_RoughnessParam;
    static const FName VisMat_SpecularParam;
    static const FName VisMat_HeightMapParam;

    void OnConstruction(const FTransform& Transform) override;

    void BeginDestroy() override;

    UPROPERTY(VisibleAnywhere)
    TArray<UStaticMeshComponent*> MeshComponents;

    void SetScreenSpecificParameters(UMaterialInstanceDynamic* Material, FVector2D ScreenCount, FVector2D ScreenIndex);
    void SetSharedMaterialParameters(UMaterialInstanceDynamic* Material);

public:
    UPROPERTY(EditAnywhere)
    float MaxHeight = 5.0f;

    UPROPERTY(EditAnywhere)
    UTextureRenderTarget2D* HeightMap;

    UPROPERTY(EditAnywhere)
    UTextureRenderTarget2D* NormalMap;

    UPROPERTY(EditAnywhere)
    FLinearColor BlendColorLow = FLinearColor(1.0, 0, 0, 1);

    UPROPERTY(EditAnywhere)
    FLinearColor BlendColorMid = FLinearColor(0, 1.0, 0, 1);

    UPROPERTY(EditAnywhere)
    FLinearColor BlendColorHigh = FLinearColor(0, 0, 1.0, 1);

    UPROPERTY(EditAnywhere)
    float Metallic = 1.0;

    UPROPERTY(EditAnywhere)
    float Roughness = 0.4;

    UPROPERTY(EditAnywhere)
    float Specular = 0.2;

    UPROPERTY(EditAnywhere)
    UMaterialInterface* VisualizeMaterialBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
    TArray<UMaterialInstanceDynamic*> VisualizeMaterialsDynamic;

    UPROPERTY(EditAnywhere)
    int XYScreenCount = 2;

    UPROPERTY(EditAnywhere)
    FVector2D ScreenSize = FVector2D(160, 90);

    UPROPERTY(EditAnywhere)
    UStaticMesh* ScreenMesh;

    UFUNCTION(BlueprintCallable)
    void SetBlendColorLow(FLinearColor& Color);

    UFUNCTION(BlueprintCallable)
    void SetBlendColorMid(FLinearColor& Color);

    UFUNCTION(BlueprintCallable)
    void SetBlendColorHigh(FLinearColor& Color);

    UFUNCTION(BlueprintCallable)
    void SetMaxHeight(float InMaxHeight);

    UFUNCTION(BlueprintCallable)
    void SetMetallic(float InMetallic);

    UFUNCTION(BlueprintCallable)
    void SetRoughness(float InRoughness);

    UFUNCTION(BlueprintCallable)
    void SetSpecular(float InSpecular);
};
