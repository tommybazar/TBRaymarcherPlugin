// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "Factories/Factory.h"
#include "UObject/ObjectMacros.h"

#include "VolumeAssetFactory.generated.h"

/**
 * Implements a factory for creating volume texture assets by drag'n'dropping .mhd and .dcm files into the content browser.
 */
UCLASS(hidecategories = Object)
class UVolumeAssetFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

public:
    //~ UFactory Interface
    virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
        const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};
