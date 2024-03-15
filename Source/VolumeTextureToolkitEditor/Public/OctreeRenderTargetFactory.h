// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "OctreeRenderTargetFactory.generated.h"

/**
 * 
 */
UCLASS()
class VOLUMETEXTURETOOLKITEDITOR_API UOctreeRenderTargetFactory : public UFactory
{
	GENERATED_BODY()
public:
public:
	UOctreeRenderTargetFactory();
	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);
};
