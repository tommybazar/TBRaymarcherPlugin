// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "Actor/VR/VRMenu/TFMenuPanel.h"

#include "Actor/RaymarchVolume.h"
#include "Widget/TransferFuncMenu.h"

void ATFMenuPanel::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsureWidgetIsSpawned();
}

void ATFMenuPanel::BeginPlay()
{
    Super::BeginPlay();
    EnsureWidgetIsSpawned();

    if (!TransferFuncMenu)
    {
        return;
    }

    // Set volumes to the underlying menu.
    TransferFuncMenu->ListenerVolumes.Empty();
    TransferFuncMenu->SetRangeProviderVolume(ProviderVolume);
    for (ARaymarchVolume* Volume : ListenerVolumes)
    {
        TransferFuncMenu->AddListenerVolume(Volume);
    }
}

void ATFMenuPanel::EnsureWidgetIsSpawned()
{
    if (TransferFuncMenuClass && WidgetComponent)
    {
        // Force initialization of widget on WidgetComponent.
        WidgetComponent->SetWidgetClass(TransferFuncMenuClass);
        TransferFuncMenu = Cast<UTransferFuncMenu>(WidgetComponent->GetUserWidgetObject());
        if (!TransferFuncMenu)
        {
            WidgetComponent->InitWidget();
            TransferFuncMenu = Cast<UTransferFuncMenu>(WidgetComponent->GetUserWidgetObject());
        }
    }
}