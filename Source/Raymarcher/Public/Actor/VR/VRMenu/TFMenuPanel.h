// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "Actor/VR/VRMenu/VRMenuPanel.h"

#include "TFMenuPanel.generated.h"

class UTransferFuncMenu;
class ARaymarchVolume;

/**
 * Class for a Transfer Function VR menu. Exposes the widget properties on the actor.
 */
UCLASS(Abstract)
class ATFMenuPanel : public AVRMenuPanel
{
    GENERATED_BODY()
public:
    virtual void OnConstruction(const FTransform& Transform) override;

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere)
    TSubclassOf<UTransferFuncMenu> TransferFuncMenuClass;

    UTransferFuncMenu* TransferFuncMenu;

    UPROPERTY(EditAnywhere)
    ARaymarchVolume* ProviderVolume;

    UPROPERTY(EditAnywhere)
    TArray<ARaymarchVolume*> ListenerVolumes;

protected:
    virtual void EnsureWidgetIsSpawned();
};
