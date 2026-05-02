// Copyright 2024 - Tomas Bartipan
// Licensed under MIT license - See License.txt for details.
// Special credits go to :
// Temaran (compute shader tutorial), TheHugeManatee (original concept) and Ryan Brucks(original raymarching code).

#pragma once

#include "CoreMinimal.h"

// This file just stores all FNames used in Materials so that we don't use raw strings in code.
// Helps consistency.

namespace RaymarchParams
{
const static FName DataVolume = "Volume";
const static FName LightVolume = "ALightVolume";
const static FName WindowingParams = "WindowingParameters";
const static FName ClippingCenter = "ClippingCenter";
const static FName ClippingDirection = "ClippingDirection";
const static FName TransferFunction = "TransferFunction";
const static FName Steps = "Steps";
const static FName OctreeVolume = "OctreeVolume";
const static FName OctreeMip = "OctreeMip";
const static FName OctreeStartingMip = "StartingMip";
const static FName WindowMask = "WindowMask";
const static FName LightParams = "LightParams";

}    // namespace RaymarchParams
