// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MediaTexture.h"
#include "Runtime/MediaAssets/Public/MediaPlayer.h"

#include "FractalVideoProcessor.generated.h"

/// This class takes a video source and creates 2 render targets from it.\
/// One is just mono video data (used as a heightmap in later visualization step) and the second one is for a normal map generated from the video.
///
UCLASS()
class FRACTALMARCHER_API AFractalVideoProcessor : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AFractalVideoProcessor();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    virtual void OnConstruction(const FTransform& Transform) override;

    bool CheckMediaIsValid();

    static const FName NormalMapMat_IntensityParam;
    static const FName NormalMapMat_UVOffsetParam;
    static const FName NormalMapMat_HeightMapParam;
    static const FName PassthroughMat_InputTextureParam;

public:
    /// If true, tick will automatically update render targets.
    /// Set to false if you need to set material parameters each tick and only want to update RT AFTER the material param set.
    UFUNCTION(BlueprintCallable)
    void SetAutoUpdateEachTick(bool bUpdateOnTick);

    UFUNCTION(BlueprintCallable)
    void UpdateRenderTargets();

    // Called every frame
    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void PlayFromStartDelegate(FString OpenedUrl);

    UFUNCTION(BlueprintCallable)
    void PlayFromStart();

    UFUNCTION(BlueprintCallable)
    void PlayFromEnd();

    UFUNCTION(BlueprintCallable)
    void TogglePause();

    UFUNCTION(BlueprintCallable)
    void SetMediaSource(UMediaSource* NewSource);

    UFUNCTION(BlueprintCallable)
    void SetPlaybackRate(float PlaybackRate, bool KeepDirection = false);

    UFUNCTION(BlueprintCallable)
    void SetNormalMapUVOffset(float NewUVOffset);

    UFUNCTION(BlueprintCallable)
    void SetNormalMapIntensity(float NewIntensity);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsUpdatingOnTick = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    bool bIsPlaying = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float PlaybackRate = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UTextureRenderTarget2D* HeightMapRT;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UTextureRenderTarget2D* NormalMapRT;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMaterialInterface* PassthroughMaterialBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UMaterialInstanceDynamic* PassthroughMaterialDynamic;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMaterialInterface* NormalMapMaterialBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UMaterialInstanceDynamic* NormalMapMaterialDynamic;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMediaPlayer* MediaPlayer;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMediaTexture* MediaTexture;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    UMediaSource* MediaSource;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float NormalMapIntensity = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float NormalMapUVOffset = 0.005f;
};
