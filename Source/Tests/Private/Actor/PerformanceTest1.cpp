// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "Actor/PerformanceTest1.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameUserSettings.h"
#include "VolumeTextureToolkit/Public/VolumeAsset/VolumeInfo.h"

#include <cstdlib>	  // For system function

void APerformanceTest1::Tick(float DeltaSeconds)
{
	if (!bRunning)
		return;

	PerformanceHelper->Tick(DeltaSeconds);

	CurrentTime += DeltaSeconds;

	constexpr float FirstRecomputeDuration = 1.0f;
	constexpr float WindowCenterMoveDuration = 2.0f;
	constexpr float SecondRecomputeDuration = 1.0f;
	constexpr float RotateCameraDuration = 4.0f;
	constexpr float RotateVolumeYawDuration = 4.0f;
	constexpr float RotateVolumeRollDuration = 4.0f;
		
	constexpr float InitializationEnd = 1.0f;
	constexpr float RecomputeTimeEnd = InitializationEnd + FirstRecomputeDuration;
	constexpr float WindowCenterMovingEnd = RecomputeTimeEnd + WindowCenterMoveDuration;
	constexpr float SecondRecomputeEnd = WindowCenterMovingEnd + SecondRecomputeDuration;
	constexpr float RotateCameraEnd = SecondRecomputeEnd + RotateCameraDuration;
	constexpr float RotateVolumeYawEnd = RotateCameraEnd + RotateVolumeYawDuration;
	constexpr float RotateVolumeRollEnd = RotateVolumeYawEnd + RotateVolumeRollDuration;

	constexpr float DefaultWindowCenter = 300.0f;
	constexpr float DefaultWindowWidth = 500.0f;
	constexpr float WindowCenterChangeSpeed = -200.0f;
	
	// Iterate the test. Each if case is played every frame in the TimeWindow.
	if (CurrentTime < InitializationEnd)
	{
		SetWindowCenter(DefaultWindowCenter);
		SetWindowWidth(DefaultWindowWidth);
	}
	else if (CurrentTime < RecomputeTimeEnd)
	{
		for (auto* ListenerVolume : ListenerVolumes)
		{
			ListenerVolume->bRequestedRecompute = true;
		}
	}
	else if (CurrentTime < WindowCenterMovingEnd)
	{
		SetWindowCenter(DefaultWindowCenter + (CurrentTime - RecomputeTimeEnd) * WindowCenterChangeSpeed);
	}
	else if (CurrentTime < SecondRecomputeEnd)
	{
		for (auto* ListenerVolume : ListenerVolumes)
		{
			ListenerVolume->bRequestedRecompute = true;
		}
	}
	else if (CurrentTime < RotateCameraEnd)
	{
		// Rotate camera around the volume.
		APawn* CurrentPlayerPawn = GetWorld()->GetFirstPlayerController()->GetPawn();
		AActor* Target = RotateAroundVolume; 

		// Check for valid pawn and target
		if (!CurrentPlayerPawn || !Target)
		{
			return;
		}

		float AngleToSetDegrees = (CurrentTime - RotateCameraEnd) * (360 / RotateCameraDuration);
		FVector OffsetVector = OriginalOffsetVector.RotateAngleAxis(AngleToSetDegrees, FVector::UpVector);
	
		FVector RotatedPosition = Target->GetActorLocation() - OffsetVector;
		CurrentPlayerPawn->SetActorLocation(RotatedPosition);

		// Make player look at the volume.
		FRotator Rotator = UKismetMathLibrary::FindLookAtRotation(RotatedPosition, Target->GetActorLocation());
		CurrentPlayerPawn->GetController()->SetControlRotation(Rotator);
	}
	else if(CurrentTime < RotateVolumeYawEnd)
	{
		// Change the rotation of the volume.
		AActor* Target = RotateAroundVolume;
		FRotator Rotator = Target->GetActorRotation();
		const float Angle = DeltaSeconds * (360 / RotateVolumeYawDuration);
		Rotator.Yaw = Rotator.Yaw + Angle;
		Target->SetActorRotation(Rotator);
	}
	else if(CurrentTime < RotateVolumeRollEnd)
	{
		// Change the rotation of the volume.
		AActor* Target = RotateAroundVolume;
		FRotator Rotator = Target->GetActorRotation();
		const float Angle = DeltaSeconds * (360 / RotateVolumeRollDuration);
		Rotator.Roll = Rotator.Roll + Angle;
		Target->SetActorRotation(Rotator);
	}
	else
	{
		// Report the numbers to the test output window in UE.
		if (auto* StatRecord = PerformanceHelper->GetCurrentRecord())
		{
			FString OutString = StatRecord->GetReportString();
			LogStep(ELogVerbosity::Log, OutString);
		}

		// Write the log file with the csv data to plot.
		PerformanceHelper->WriteLogFile("PerformanceTest1", ".csv");

		PerformanceHelper->EndRecording();
		PerformanceHelper->EndStatsFile();

		FinishTest(EFunctionalTestResult::Succeeded, "PerformanceTest1 passed.");
	}

	Super::Tick(DeltaSeconds);
}

bool APerformanceTest1::RunTest(const TArray<FString>& Params)
{
	bRunning = true;
	GEngine->AddOnScreenDebugMessage(20, 20, FColor::Purple, "Performance test 1 started.");

	PerformanceHelper = NewObject<UAutomationPerformaceHelper>(this);
	PerformanceHelper->BeginRecording(TEXT("PerformanceTest1"), 13.0f, 13.0f, 5.0f);

	// Save also the stats file. The stats file are saved in <Engine>/Saved/Profiling/UnrealStats
	PerformanceHelper->BeginStatsFile(TEXT("PerformanceTest1") + FDateTime::Now().ToString());

	// Change the resolution to 4K.
	UGameUserSettings* MyGameSettings = GEngine->GetGameUserSettings();
	MyGameSettings->SetScreenResolution(FIntPoint(3840, 2160));
	MyGameSettings->SetFrameRateLimit(10000.0f);
	MyGameSettings->ApplySettings(true);

	OriginalOffsetVector = RotateAroundVolume->GetActorLocation() - GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();

	
	return Super::RunTest(Params);
}

void APerformanceTest1::SetWindowCenter(float Value)
{
	for (auto* ListenerVolume : ListenerVolumes)
	{
		ListenerVolume->SetWindowCenter(ListenerVolume->VolumeAsset->ImageInfo.NormalizeValue(Value));
	}
}


void APerformanceTest1::SetWindowWidth(float Value)
{
	for (auto* ListenerVolume : ListenerVolumes)
	{
		ListenerVolume->SetWindowWidth(ListenerVolume->VolumeAsset->ImageInfo.NormalizeValue(Value));
	}
}
