#include "FactoryCinematicCamera.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"

AFactoryCinematicCamera::AFactoryCinematicCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	GetCameraComponent()->bConstrainAspectRatio = false;
	SetActorEnableCollision(false);
}

void AFactoryCinematicCamera::Play(const TArray<FFactoryShot>& InShots, bool bInLoop, AActor* InFallbackAnchor, TFunction<void()> InOnFinished)
{
	Shots = InShots;
	bLoop = bInLoop;
	FallbackAnchor = InFallbackAnchor;
	OnFinished = MoveTemp(InOnFinished);
	ShotIndex = 0;
	ShotTime = 0.0f;
	bPlaying = Shots.Num() > 0;
	if (bPlaying)
	{
		Pose(Shots[0], 0.0f);
	}
	else if (OnFinished)
	{
		TFunction<void()> Done = MoveTemp(OnFinished);
		Done();
	}
}

void AFactoryCinematicCamera::SkipToEnd()
{
	if (!bPlaying || Shots.Num() == 0)
	{
		return;
	}
	ShotIndex = Shots.Num() - 1;
	ShotTime = Shots[ShotIndex].Duration;
}

AActor* AFactoryCinematicCamera::ResolveAnchor(const FFactoryShot& Shot) const
{
	if (!Shot.AnchorTag.IsNone())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(Shot.AnchorTag)) { return *It; }
		}
	}
	return FallbackAnchor.Get();
}

void AFactoryCinematicCamera::Pose(const FFactoryShot& Shot, float Alpha)
{
	const AActor* Anchor = ResolveAnchor(Shot);
	const FVector Origin = Anchor ? Anchor->GetActorLocation() : FVector::ZeroVector;
	const FRotator Frame = (Anchor && Shot.bUseAnchorYaw) ? FRotator(0.0f, Anchor->GetActorRotation().Yaw, 0.0f) : FRotator::ZeroRotator;
	const float A = FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f), FMath::Max(Shot.EaseExponent, 1.0f));
	const FVector LookAt = Origin + Frame.RotateVector(Shot.LookAtOffset);
	FVector Pos = Origin + Frame.RotateVector(FMath::Lerp(Shot.StartOffset, Shot.EndOffset, A));

	// Never inside geometry: sweep from the subject to the camera and stop short of the first blocking hit.
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FactoryCineClip), false, this);
	if (Anchor) { Q.AddIgnoredActor(Anchor); }
	// People never block the shot: pawns and any actor with a skeletal mesh (NPCs are often plain actors).
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<APawn>() || It->FindComponentByClass<USkeletalMeshComponent>()) { Q.AddIgnoredActor(*It); }
	}
	FHitResult Hit;
	// Sweep from the subject (not the look-at point: a look-at on the floor starts the sweep inside it and drops the
	// camera to the ground; found in Pocket Sand, 2026-11-09).
	const FCollisionShape Ball = FCollisionShape::MakeSphere(UFactoryKitSettings::Get()->CameraClipRadius);
	if (GetWorld()->SweepSingleByChannel(Hit, Origin, Pos, FQuat::Identity, ECC_Camera, Ball, Q))
	{
		// A subject whose origin sits inside geometry (feet on a platform slab) starts the sweep penetrating; then sweep back
		// from the camera toward the look-at instead, and only accept a hit that does not start inside something.
		if (Hit.bStartPenetrating)
		{
			FHitResult Back;
			if (GetWorld()->SweepSingleByChannel(Back, LookAt, Pos, FQuat::Identity, ECC_Camera, Ball, Q) && !Back.bStartPenetrating)
			{
				Pos = Back.Location;
			}
		}
		else
		{
			Pos = Hit.Location;
		}
	}
	SetActorLocationAndRotation(Pos, (LookAt - Pos).Rotation());
	GetCameraComponent()->SetFieldOfView(FMath::Lerp(Shot.StartFOV, Shot.EndFOV, A));
}

void AFactoryCinematicCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaying || Shots.Num() == 0)
	{
		return;
	}
	ShotTime += DeltaSeconds;
	const FFactoryShot& Shot = Shots[ShotIndex];
	const float Dur = FMath::Max(Shot.Duration, 0.05f);
	Pose(Shot, ShotTime / Dur);
	if (ShotTime >= Dur)
	{
		if (ShotIndex + 1 < Shots.Num())
		{
			++ShotIndex;
			ShotTime = 0.0f;
		}
		else if (bLoop)
		{
			ShotIndex = 0;
			ShotTime = 0.0f;
		}
		else
		{
			bPlaying = false; // hold the last frame
			if (OnFinished)
			{
				TFunction<void()> Done = MoveTemp(OnFinished);
				OnFinished = nullptr;
				Done();
			}
		}
	}
}
