// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "Raymarcher/Public/Actor/RaymarchVolume.h"

#include "PerformanceTest1.generated.h"

/** PerformanceTest1
 * Test is used over the test interface ("Tools" -> "Test Automation"). Then search for 'PerformanceTest' keyword.
 * The performance test actor added to the scene should be filtered. Check it and press Start Tests button to play it.
 *
 * The Stats are saved to <Engine>/Saved/Profiling/PerformanceTest1.
 * There is also a WIP tool that will show all tests results plotted. To use it, open CMD (or any other command line),
 * CD to the <Engine>/Saved/ folder and call 'python3 plotResults.py PerformanceTest1' (the mathplotlib library must be
 * installed within python3).
 */
UCLASS()
class TESTS_API APerformanceTest1 : public AActor
{
    GENERATED_BODY()

    virtual void Tick(float DeltaSeconds) override;

    virtual void BeginPlay() override;

    // Called when the test is started.
    void RunTest(const TArray<FString>& Params = TArray<FString>());

    // Set the window center to each volume added to ListenerVolumes.
    void SetWindowCenter(float Value);

    // Set the window width to each volume added to ListenerVolumes.
    void SetWindowWidth(float Value);

    // Return true if the bookmark was not yet added to the trace.
    bool IsBookmarkNew(FString Name);

    // Define if the test was started by calling 'RunTest'
    bool bRunning = false;

    // Define the current execution time from the start of the test.
    float CurrentTime = 0.0f;

    // The volumes this test is affecting.
    UPROPERTY(EditAnywhere)
    TArray<ARaymarchVolume*> ListenerVolumes;

    // Volume the camera rotate around.
    UPROPERTY(EditAnywhere)
    ARaymarchVolume* RotateAroundVolume;

    // Plane to rotate.
    UPROPERTY(EditAnywhere)
    ARaymarchClipPlane* PlaneToRotate;

    FVector OriginalOffsetVector{};

    // List of all applied bookmarks in current test run.
    TSet<FString> BookmarksApplied;
};
