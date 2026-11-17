// Six Inches - camera shake presets.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "SICameraShakes.generated.h"

/** Wingtip contact / small knock. */
UCLASS()
class SIXINCHES_API USIShakeContact : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	USIShakeContact(const FObjectInitializer& ObjectInitializer);
};

/** Flak burst nearby. */
UCLASS()
class SIXINCHES_API USIShakeFlak : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	USIShakeFlak(const FObjectInitializer& ObjectInitializer);
};

/** A bomb going off on the ground (scaled by distance). */
UCLASS()
class SIXINCHES_API USIShakeExplosion : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	USIShakeExplosion(const FObjectInitializer& ObjectInitializer);
};

/** One buffet from hot air and smoke. */
UCLASS()
class SIXINCHES_API USIShakeBuffet : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	USIShakeBuffet(const FObjectInitializer& ObjectInitializer);
};
