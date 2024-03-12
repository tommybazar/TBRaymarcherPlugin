// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details.
// Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks
// (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Dummy.generated.h"

UCLASS()
class TESTS_API ADummy : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADummy();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
