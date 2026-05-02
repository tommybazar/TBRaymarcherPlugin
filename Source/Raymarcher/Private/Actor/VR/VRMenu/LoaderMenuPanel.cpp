// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "Actor/VR/VRMenu/LoaderMenuPanel.h"

#include "Actor/RaymarchVolume.h"
#include "Widget/VolumeLoadMenu.h"

void ALoaderMenuPanel::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    EnsureWidgetIsSpawned();
}

void ALoaderMenuPanel::BeginPlay()
{
    Super::BeginPlay();
    EnsureWidgetIsSpawned();

    if (!LoaderMenu)
    {
        return;
    }

    LoaderMenu->ListenerVolumes.Empty();
    for (ARaymarchVolume* Volume : ListenerVolumes)
    {
        LoaderMenu->AddListenerVolume(Volume);
    }
}

void ALoaderMenuPanel::EnsureWidgetIsSpawned()
{
    if (LoaderMenuClass && WidgetComponent)
    {
        WidgetComponent->SetWidgetClass(LoaderMenuClass);
        LoaderMenu = Cast<UVolumeLoadMenu>(WidgetComponent->GetUserWidgetObject());
        if (!LoaderMenu)
        {
            WidgetComponent->InitWidget();
            LoaderMenu = Cast<UVolumeLoadMenu>(WidgetComponent->GetUserWidgetObject());
        }
    }
}
