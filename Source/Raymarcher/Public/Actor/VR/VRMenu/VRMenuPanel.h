// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineMinimal.h"

#include "VRMenuPanel.generated.h"

/**
 * Base class for VR Menus. Has a mesh and a widget.
 */
UCLASS(Abstract)
class AVRMenuPanel : public AActor
{
    GENERATED_BODY()
public:
    // Sets default values for this actor's properties
    AVRMenuPanel();

    /// Mesh serving as a menu backgrounds.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UStaticMeshComponent* StaticMeshComponent;

    /// Actual menu.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UWidgetComponent* WidgetComponent;

protected:
    virtual void EnsureWidgetIsSpawned();
};
