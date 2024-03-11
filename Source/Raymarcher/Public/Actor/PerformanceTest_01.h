// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "RaymarchVolume.h"
#include "PerformanceTest_01.generated.h"

/**
 * 
 */
UCLASS()
class RAYMARCHER_API APerformanceTest_01 : public AFunctionalTest
{
	GENERATED_BODY()

	virtual void Tick(float DeltaSeconds) override;
	
	virtual bool RunTest(const TArray<FString>& Params = TArray<FString>()) override;

	void SetWindowCenter(float Value);
	
	// Define if the test was started by calling 'RunTest'
	bool bRunning = false;

	float CurrentTime = 0.0f;

	UPROPERTY(EditAnywhere)
	bool bPlotResult = true;

	/// The volumes this test is affecting.
	/// #TODO do not touch the volume directly and expose delegates instead?
	UPROPERTY(EditAnywhere)
	TArray<ARaymarchVolume*> ListenerVolumes;
	
	UAutomationPerformaceHelper* PerformanceHelper;
};
