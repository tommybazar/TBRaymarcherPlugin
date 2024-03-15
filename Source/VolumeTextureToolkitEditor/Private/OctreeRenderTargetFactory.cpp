// Copyright 2021 Tomas Bartipan and Technical University of Munich .Licensed under MIT license - See License.txt for details. Special credits go to : Temaran (compute shader tutorial), TheHugeManatee (original concept, supervision) and Ryan Brucks (original raymarching code).


#include "OctreeRenderTargetFactory.h"

#include "RenderTargetVolumeMipped.h"

UOctreeRenderTargetFactory::UOctreeRenderTargetFactory()
{
	SupportedClass = URenderTargetVolumeMipped::StaticClass();
	bCreateNew = true;
}
UObject* UOctreeRenderTargetFactory::FactoryCreateNew(
	UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	int SizeX = 512;
	int SizeY = 512;
	int SizeZ = 256;
	URenderTargetVolumeMipped* OctreeVolumeRenderTarget = NewObject<URenderTargetVolumeMipped>(InParent, "Render Target Volume");
	OctreeVolumeRenderTarget->bCanCreateUAV = true;
	OctreeVolumeRenderTarget->bHDR = false;
	OctreeVolumeRenderTarget->Init(FMath::RoundUpToPowerOfTwo(SizeX),
												FMath::RoundUpToPowerOfTwo(SizeY),
												FMath::RoundUpToPowerOfTwo(SizeZ),
												2, PF_G16);

	return OctreeVolumeRenderTarget;
}