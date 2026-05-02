// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "Rendering/FractalVideoProcessor.h"

#include "Kismet/KismetRenderingLibrary.h"

const FName AFractalVideoProcessor::NormalMapMat_IntensityParam = "NormalMapIntensity";
const FName AFractalVideoProcessor::NormalMapMat_UVOffsetParam = "HeightMapUVOffset";
const FName AFractalVideoProcessor::NormalMapMat_HeightMapParam = "HeightMap";
const FName AFractalVideoProcessor::PassthroughMat_InputTextureParam = "VideoTexture";

// Sets default values
AFractalVideoProcessor::AFractalVideoProcessor()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AFractalVideoProcessor::BeginPlay()
{
    Super::BeginPlay();

    if (MediaPlayer)
    {
        MediaPlayer->OnMediaOpened.AddUniqueDynamic(this, &AFractalVideoProcessor::PlayFromStartDelegate);

        if (MediaSource)
        {
            MediaPlayer->OpenSource(MediaSource);
        }
    }
}

void AFractalVideoProcessor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Create dynamic material for video -> RT pass-through.
    if (PassthroughMaterialBase && MediaTexture)
    {
        PassthroughMaterialDynamic = UMaterialInstanceDynamic::Create(PassthroughMaterialBase, this);
        PassthroughMaterialDynamic->SetTextureParameterValue(PassthroughMat_InputTextureParam, MediaTexture);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Fractal Video Processor is missing base Passthrough material or Media Texture!"));
        return;
    }

    // Create dynamic material for NormalMap generation.
    if (NormalMapMaterialBase && HeightMapRT)
    {
        NormalMapMaterialDynamic = UMaterialInstanceDynamic::Create(NormalMapMaterialBase, this);
        NormalMapMaterialDynamic->SetScalarParameterValue(NormalMapMat_IntensityParam, NormalMapIntensity);
        NormalMapMaterialDynamic->SetScalarParameterValue(NormalMapMat_UVOffsetParam, NormalMapUVOffset);
        NormalMapMaterialDynamic->SetTextureParameterValue(NormalMapMat_HeightMapParam, HeightMapRT);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Fractal Video Processor is missing base NormalMap material or HeightMap RT!"));
        return;
    }
}

bool AFractalVideoProcessor::CheckMediaIsValid()
{
    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("Fractal Video Processor is missing media player!"));
        return false;
    }

    if (!MediaSource)
    {
        UE_LOG(LogTemp, Error, TEXT("Fractal Video Processor is missing media source!"));
        return false;
    }

    return true;
}

void AFractalVideoProcessor::SetAutoUpdateEachTick(bool bUpdateOnTick)
{
    bIsUpdatingOnTick = bUpdateOnTick;
}

void AFractalVideoProcessor::UpdateRenderTargets()
{
    if (!PassthroughMaterialDynamic)
    {
        UE_LOG(LogTemp, Error, TEXT("Fractal Video Processor is missing Passthrough material!"));
        return;
    }
    UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, HeightMapRT, PassthroughMaterialDynamic);

    if (!NormalMapMaterialDynamic)
    {
        UE_LOG(LogTemp, Error, TEXT("Fractal Video Processor is missing dynamic NormalMap material!"));
        return;
    }
    UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, NormalMapRT, NormalMapMaterialDynamic);
}

// Called every frame
void AFractalVideoProcessor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Each tick, update the render targets from the currently playing video.
    if (!bIsPlaying || !bIsUpdatingOnTick)
    {
        return;
    }

    UpdateRenderTargets();
}

void AFractalVideoProcessor::PlayFromStartDelegate(FString OpenedUrl)
{
    PlayFromStart();
}

void AFractalVideoProcessor::PlayFromStart()
{
    if (!CheckMediaIsValid())
    {
        return;
    }

    if (PlaybackRate < 0.0f)
    {
        PlaybackRate = -PlaybackRate;
    }

    bIsPlaying = true;
    MediaPlayer->Seek(FTimespan::Zero());
    MediaPlayer->SetRate(PlaybackRate);
}

void AFractalVideoProcessor::PlayFromEnd()
{
    if (!CheckMediaIsValid())
    {
        return;
    }

    if (PlaybackRate > 0.0f)
    {
        PlaybackRate = -PlaybackRate;
    }

    bIsPlaying = true;
    MediaPlayer->Seek(MediaPlayer->GetDuration());
    MediaPlayer->SetRate(PlaybackRate);
}

void AFractalVideoProcessor::TogglePause()
{
    if (!CheckMediaIsValid())
    {
        return;
    }

    if (bIsPlaying)
    {
        MediaPlayer->SetRate(0.0f);
    }
    else
    {
        MediaPlayer->SetRate(PlaybackRate);
    }

    bIsPlaying = !bIsPlaying;
}

void AFractalVideoProcessor::SetMediaSource(UMediaSource* NewSource)
{
    if (!NewSource)
    {
        UE_LOG(LogTemp, Error, TEXT("SetMediaSource cannot be called without a source."));
        return;
    }

    if (!MediaPlayer)
    {
        UE_LOG(LogTemp, Error, TEXT("SetMediaSource cannot be called without a valid player."));
        return;
    }

    MediaSource = NewSource;
    MediaPlayer->OpenSource(MediaSource);
    PlayFromStart();
}

void AFractalVideoProcessor::SetPlaybackRate(float NewPlaybackRate, bool bKeepDirection)
{
    if (!CheckMediaIsValid())
    {
        return;
    }

    if (NewPlaybackRate == 0.0f)
    {
        UE_LOG(LogTemp, Error, TEXT("SetPlaybackRate cannot be used for pausing! Use TogglePause instead."));
        return;
    }

    // Keep the direction (forward/back) - just change the magnitude.
    if (bKeepDirection)
    {
        if (NewPlaybackRate < 0.0f)
        {
            UE_LOG(LogTemp, Error, TEXT("SetPlaybackRate with bKeepDirection==true cannot take negative values as the parameter."));
            return;
        }
        float Signum = FMath::Sign(PlaybackRate);
        NewPlaybackRate = Signum * NewPlaybackRate;
    }

    if (MediaPlayer->SupportsRate(NewPlaybackRate, true))
    {
        MediaPlayer->SetRate(NewPlaybackRate);
        PlaybackRate = NewPlaybackRate;
        UE_LOG(LogTemp, Warning, TEXT("Setting rate : %.2f"), PlaybackRate);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Cannot set rate : %.2f on player. Rate not supported."), NewPlaybackRate);
    }
}

void AFractalVideoProcessor::SetNormalMapUVOffset(float NewUVOffset)
{
    NormalMapUVOffset = NewUVOffset;
    if (NormalMapMaterialDynamic)
    {
        NormalMapMaterialDynamic->SetScalarParameterValue(NormalMapMat_UVOffsetParam, NormalMapUVOffset);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SetNormalMapUVOffset called, but dynamic Normal Map material doesn't exist."));
    }
}

void AFractalVideoProcessor::SetNormalMapIntensity(float NewIntensity)
{
    NormalMapIntensity = NewIntensity;
    if (NormalMapMaterialDynamic)
    {
        NormalMapMaterialDynamic->SetScalarParameterValue(NormalMapMat_IntensityParam, NormalMapIntensity);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("SetNormalMapIntensity called, but dynamic Normal Map material doesn't exist."));
    }
}