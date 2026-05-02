// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "Actor/FractalAudioVisualizeDriver.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AFractalAudioVisualizeDriver::AFractalAudioVisualizeDriver()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AFractalAudioVisualizeDriver::BeginPlay()
{
    Super::BeginPlay();

    if (!TrackToPlay)
    {
        GEngine->AddOnScreenDebugMessage(444, 100, FColor::Red, "No track set to be played! Set TrackToPlay.");
        return;
    }

    PlayingSound = UGameplayStatics::SpawnSound2D(this, TrackToPlay);
    FrequenciesToGet = {FrequencyIntensity, FrequencyLow, FrequencyMid, FrequencyHigh};
}

void AFractalAudioVisualizeDriver::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
}

// Called every frame
void AFractalAudioVisualizeDriver::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!Screen)
    {
        GEngine->AddOnScreenDebugMessage(445, 1, FColor::Red, "No screen set in FractalAudioVisualizeDriver.");
        return;
    }

    TArray<FSoundWaveSpectralData> SpectralData;

    if (PlayingSound->GetCookedFFTData(FrequenciesToGet, SpectralData))
    {
        float IntensityMag = SpectralData[0].Magnitude * 100;
        float LowMag = SpectralData[1].Magnitude * 100;
        float MidMag = SpectralData[2].Magnitude * 100;
        float HighMag = SpectralData[3].Magnitude * 100;

        float Sum = (IntensityMag + LowMag + MidMag + HighMag);

        FString Log = FString::Printf(TEXT("%.6f, %.6f, %.6f, %.6f, %.6f"), IntensityMag, LowMag, MidMag, HighMag, Sum);
        GEngine->AddOnScreenDebugMessage(445, 0, FColor::Red, Log, true, FVector2D(3));

        FLinearColor ColorLow = FLinearColor::LerpUsingHSV(LowFreqLowColor, LowFreqHighColor, LowMag);
        FLinearColor ColorMid = FLinearColor::LerpUsingHSV(MidFreqLowColor, MidFreqHighColor, MidMag);
        FLinearColor ColorHigh = FLinearColor::LerpUsingHSV(HighFreqLowColor, HighFreqHighColor, HighMag);

        Screen->SetBlendColorLow(ColorLow);
        Screen->SetBlendColorMid(ColorMid);
        Screen->SetBlendColorHigh(ColorHigh);
        Screen->SetMaxHeight((0.5 + IntensityMag / 2) * 20);
    }
}
