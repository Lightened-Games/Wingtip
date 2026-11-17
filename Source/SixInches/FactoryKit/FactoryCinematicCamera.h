// FactoryKit cinematic camera: plays FFactoryShot lists (menu loop, intro, victory/defeat) relative to an anchor,
// eases each move, and sphere-traces so it never ends up inside a wall.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "FactoryKitSettings.h"
#include "FactoryCinematicCamera.generated.h"

UCLASS(NotPlaceable)
class AFactoryCinematicCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	AFactoryCinematicCamera();
	virtual void Tick(float DeltaSeconds) override;

	/** Play Shots once (or loop). FallbackAnchor is used for shots with no AnchorTag (normally the player's pawn). */
	void Play(const TArray<FFactoryShot>& InShots, bool bInLoop, AActor* InFallbackAnchor, TFunction<void()> InOnFinished = nullptr);
	/** Jump to the end of the current list (skip). */
	void SkipToEnd();
	bool IsPlaying() const { return bPlaying; }
	/** Stop moving (no finish callback). */
	void Stop() { bPlaying = false; OnFinished = nullptr; }

private:
	void Pose(const FFactoryShot& Shot, float Alpha);
	AActor* ResolveAnchor(const FFactoryShot& Shot) const;

	TArray<FFactoryShot> Shots;
	TWeakObjectPtr<AActor> FallbackAnchor;
	TFunction<void()> OnFinished;
	int32 ShotIndex = 0;
	float ShotTime = 0.0f;
	bool bLoop = false;
	bool bPlaying = false;
};
