// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "FractalVisualizerScreen.h"
#include "GameFramework/Actor.h"

#include "FractalAudioVisualizeDriver.generated.h"

UCLASS()
class FRACTALMARCHER_API AFractalAudioVisualizeDriver : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AFractalAudioVisualizeDriver();

    // The screen that will be modified by this audio visualize driver.
    UPROPERTY(EditAnywhere)
    AFractalVisualizerScreen* Screen;

    UPROPERTY(EditAnywhere)
    USoundBase* TrackToPlay;

    UPROPERTY(EditAnywhere, Category = "AFractal|ColorLow")
    FLinearColor LowFreqLowColor = FLinearColor(0.3, 0, 0, 1);
    UPROPERTY(EditAnywhere, Category = "AFractal|ColorLow")
    FLinearColor LowFreqHighColor = FLinearColor(1, 0, 0, 1);

    UPROPERTY(EditAnywhere, Category = "AFractal|ColorMid")
    FLinearColor MidFreqLowColor = FLinearColor(0, 0.3, 0, 1);
    UPROPERTY(EditAnywhere, Category = "AFractal|ColorMid")
    FLinearColor MidFreqHighColor = FLinearColor(0, 1, 0, 1);

    UPROPERTY(EditAnywhere, Category = "AFractal|ColorHigh")
    FLinearColor HighFreqLowColor = FLinearColor(0, 0, 1.0, 1);
    UPROPERTY(EditAnywhere, Category = "AFractal|ColorHigh")
    FLinearColor HighFreqHighColor = FLinearColor(0, 0, 0.3, 1);

    UPROPERTY(EditAnywhere, Category = "AFractal|Frequencies")
    float FrequencyIntensity = 100;
    UPROPERTY(EditAnywhere, Category = "AFractal|Frequencies")
    float FrequencyLow = 500;
    UPROPERTY(EditAnywhere, Category = "AFractal|Frequencies")
    float FrequencyMid = 1000;
    UPROPERTY(EditAnywhere, Category = "AFractal|Frequencies")
    float FrequencyHigh = 5000;

protected:
    UPROPERTY()
    UAudioComponent* PlayingSound;

    TArray<float> FrequenciesToGet;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    virtual void OnConstruction(const FTransform& Transform) override;

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;
};
