// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "Actor/VR/VRMenu/VRMenuPanel.h"

AVRMenuPanel::AVRMenuPanel()
{
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>("Root");
    SetRootComponent(Root);

    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("Menu Panel Mesh");
    StaticMeshComponent->SetupAttachment(RootComponent);

    WidgetComponent = CreateDefaultSubobject<UWidgetComponent>("Menu Widget");
    WidgetComponent->SetupAttachment(RootComponent);
}

void AVRMenuPanel::EnsureWidgetIsSpawned()
{
}
