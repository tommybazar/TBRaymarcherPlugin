// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#include "RayUtils.h"

#define LOCTEXT_NAMESPACE "FRayUtilsModule"

void FRayUtilsModule::StartupModule()
{
}

void FRayUtilsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRayUtilsModule, RayUtils)