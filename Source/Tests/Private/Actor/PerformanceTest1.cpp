// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#include "Actor/PerformanceTest1.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "VolumeTextureToolkit/Public/VolumeAsset/VolumeInfo.h"

#include <cstdlib>	  // For system function

void APerformanceTest1::Tick(float DeltaSeconds)
{
	CurrentTime += DeltaSeconds;
	// Start the test on the first iteration.
	if (!bRunning)
	{
		if (CurrentTime > 3.0f)
		{
			RunTest();
		}
		return;
	}
	
	static constexpr float FirstRecomputeDuration = 1.0f;
	static constexpr float WindowCenterMoveDuration = 2.0f;
	static constexpr float SecondRecomputeDuration = 1.0f;
	static constexpr float RotateCameraDuration = 4.0f;
	static constexpr float RotateVolumeYawDuration = 4.0f;
	static constexpr float RotateVolumeRollDuration = 4.0f;
	static constexpr float RotatePlaneRollDuration = 4.0f;
	static constexpr float RotatePlaneYawDuration = 4.0f;

	static constexpr float InitializationEnd = 5.0f;
	static constexpr float RecomputeTimeEnd = InitializationEnd + FirstRecomputeDuration;
	static constexpr float WindowCenterMovingEnd = RecomputeTimeEnd + WindowCenterMoveDuration;
	static constexpr float SecondRecomputeEnd = WindowCenterMovingEnd + SecondRecomputeDuration;
	static constexpr float RotateCameraEnd = SecondRecomputeEnd + RotateCameraDuration;
	static constexpr float RotateVolumeYawEnd = RotateCameraEnd + RotateVolumeYawDuration;
	static constexpr float RotateVolumeRollEnd = RotateVolumeYawEnd + RotateVolumeRollDuration;
	static constexpr float RotatePlaneRollEnd = RotateVolumeRollEnd + RotatePlaneRollDuration;
	static constexpr float RotatePlaneYawEnd = RotatePlaneRollEnd + RotatePlaneYawDuration;

	static constexpr float DefaultWindowCenter = 300.0f;
	static constexpr float DefaultWindowWidth = 500.0f;
	static constexpr float WindowCenterChangeSpeed = -200.0f;

	// Iterate the test. Each if case is played every frame in the TimeWindow.
	if (CurrentTime < InitializationEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 SetWindowCenter1");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		SetWindowCenter(DefaultWindowCenter);
		SetWindowWidth(DefaultWindowWidth);
	}
	else if (CurrentTime < RecomputeTimeEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 RecomputeLights1");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		for (auto* ListenerVolume : ListenerVolumes)
		{
			ListenerVolume->bRequestedRecompute = true;
		}
	}
	else if (CurrentTime < WindowCenterMovingEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 SetWindowCenter2");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		SetWindowCenter(DefaultWindowCenter + (CurrentTime - RecomputeTimeEnd) * WindowCenterChangeSpeed);
	}
	else if (CurrentTime < SecondRecomputeEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 RecomputeLights2");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		for (auto* ListenerVolume : ListenerVolumes)
		{
			ListenerVolume->bRequestedRecompute = true;
		}
	}
	else if (CurrentTime < RotateCameraEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 RotateCameraAroundVolume");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

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
	else if (CurrentTime < RotateVolumeYawEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 RotateVolumeYaw");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		// Change the rotation of the volume.
		AActor* Target = RotateAroundVolume;
		if (!Target)
			return;

		FRotator Rotator = Target->GetActorRotation();
		const float Angle = DeltaSeconds * (360 / RotateVolumeYawDuration);
		Rotator.Yaw = Rotator.Yaw + Angle;
		Target->SetActorRotation(Rotator);
	}
	else if (CurrentTime < RotateVolumeRollEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 RotateVolumeRoll");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		// Change the rotation of the volume.
		AActor* Target = RotateAroundVolume;
		if (!Target)
			return;

		FRotator Rotator = Target->GetActorRotation();
		const float Angle = DeltaSeconds * (360 / RotateVolumeRollDuration);
		Rotator.Roll = Rotator.Roll + Angle;
		Target->SetActorRotation(Rotator);
	}
	else if (CurrentTime < RotatePlaneRollEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 RotatePlaneRoll");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		// Change the rotation of the plane.
		AActor* Plane = PlaneToRotate;
		if (!Plane)
			return;

		FRotator Rotator = Plane->GetActorRotation();
		const float Angle = DeltaSeconds * (360 / RotatePlaneRollDuration / 4);
		Rotator.Roll = Rotator.Roll - Angle;
		Plane->SetActorRotation(Rotator);
	}
	else if (CurrentTime < RotatePlaneYawEnd)
	{
		const FString CurrentTestName = TEXT("PerformanceTest1 RotatePlaneYaw");
		if (IsBookmarkNew(CurrentTestName))
		{
			TRACE_BOOKMARK(*CurrentTestName);
		}

		// Change the rotation of the plane.
		AActor* Plane = PlaneToRotate;
		if (!Plane)
			return;

		FRotator Rotator = Plane->GetActorRotation();
		const float Angle = DeltaSeconds * (360 / RotatePlaneYawDuration / 2);
		Rotator.Yaw = Rotator.Yaw - Angle;
		Plane->SetActorRotation(Rotator);
	}
	else
	{
		TRACE_BOOKMARK(TEXT("PerformanceTest1 End"));

		if (UWorld* World = GetWorld())
		{
			GEngine->Exec(World, TEXT("Trace.Stop"));
		}

		// FinishTest(EFunctionalTestResult::Succeeded, "PerformanceTest1 passed.");
	}

	Super::Tick(DeltaSeconds);
}
void APerformanceTest1::BeginPlay()
{
	RunTest();
	
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	Super::BeginPlay();
}

void APerformanceTest1::RunTest(const TArray<FString>& Params)
{
	bRunning = true;
	GEngine->AddOnScreenDebugMessage(20, 20, FColor::Purple, "Performance test 1 started.");

	// Change the resolution to 4K.
	UGameUserSettings* MyGameSettings = GEngine->GetGameUserSettings();
	MyGameSettings->SetScreenResolution(FIntPoint(3840, 2160));
	MyGameSettings->SetFrameRateLimit(10000.0f);
	MyGameSettings->ApplySettings(true);

	OriginalOffsetVector = RotateAroundVolume->GetActorLocation() - GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();

	if (UWorld* World = GetWorld())
	{
		GEngine->Exec(World, TEXT("Trace.Start"));
		TRACE_BOOKMARK(TEXT("PerformanceTest1 Start."))
	}

	// Clear the bookmarks to log them properly this test run.
	BookmarksApplied.Empty();
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

bool APerformanceTest1::IsBookmarkNew(FString Name)
{
	if(!BookmarksApplied.Find(Name))
	{
		BookmarksApplied.Add(Name);
		return true;
	}
	return false;
}
