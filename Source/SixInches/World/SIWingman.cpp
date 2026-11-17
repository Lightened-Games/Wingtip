// Six Inches - AI squadron fighter.

#include "World/SIWingman.h"
#include "World/SIAirspaceSubsystem.h"
#include "Interfaces/SIInterfaces.h"
#include "Components/SIWingtipComponent.h"
#include "Audio/SISynth.h"
#include "SixInches.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

ASIWingman::ASIWingman()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	Body->ComponentTags.Add(TEXT("NoCollide"));
	RootComponent = Body;

	PropDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropDisc"));
	PropDisc->SetupAttachment(Body);
	PropDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PropDisc->CastShadow = false;
	PropDisc->ComponentTags.Add(TEXT("NoCollide"));

	BodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_Fighter.SM_Fighter")));
	PropMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_PropDisc.SM_PropDisc")));
	Tags.Add(TEXT("SI.Wingman"));
}

FText ASIWingman::GetCallsignText() const
{
	return FText::FromName(Spec.Callsign);
}

void ASIWingman::Configure(const FSIWingmanSpec& InSpec, float StartSpeedMS)
{
	Spec = InSpec;
	Speed = StartSpeedMS;
	const FRotator R = GetActorRotation();
	Yaw = R.Yaw;
	Pitch = 0.0f;
	Mode = Spec.Role == ESIWingmanRole::Patrol ? EMode::Patrol : EMode::Escort;
	SearchTimer = Spec.Seed * 0.05f;  // stagger the searches between hunters

	if (UStaticMesh* M = BodyMesh.LoadSynchronous())
	{
		Body->SetStaticMesh(M);
		if (const UStaticMeshSocket* S = M->FindSocket(TEXT("Prop"))) { PropDisc->SetRelativeLocation(S->RelativeLocation); }
		if (const UStaticMeshSocket* S = M->FindSocket(TEXT("TipR"))) { HalfSpanTip = S->RelativeLocation; }
	}
	if (UStaticMesh* M = PropMesh.LoadSynchronous())
	{
		PropDisc->SetStaticMesh(M);
	}
	if (Spec.Role == ESIWingmanRole::Hunter)
	{
		if (USIAirspaceSubsystem* A = Air())
		{
			A->CoverLane(this, Spec.LaneMinY * SI_CM, Spec.LaneMaxY * SI_CM);
			A->RegisterAircraft(this);
		}
		Tags.Add(Spec.Callsign);
	}
	USILoopSynth* Synth = nullptr;
	EngineAudio = USIAudioLibrary::CreateLoop(this, Body, ESILoopKind::Engine, 200 + Spec.Seed, true, EngineFalloff, Synth);
	EngineSynth = Synth;
	if (EngineSynth) { EngineSynth->SetLoopVolume(EngineVolume); }
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] %s airborne (%s)"), *Spec.Callsign.ToString(), Spec.Role == ESIWingmanRole::Patrol ? TEXT("patrol") : TEXT("hunter"));
}

void ASIWingman::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USIAirspaceSubsystem* A = Air()) { A->ReleaseAllClaims(this); }
	Super::EndPlay(EndPlayReason);
}

FVector ASIWingman::GetVelocity() const
{
	return FRotator(Pitch, Yaw, 0.0f).Vector() * Speed * SI_CM;
}

APawn* ASIWingman::Player() const
{
	APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	return P && !P->IsHidden() ? P : nullptr;
}

USIAirspaceSubsystem* ASIWingman::Air() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<USIAirspaceSubsystem>() : nullptr;
}

void ASIWingman::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);
	ModeTime += Dt;
	Age += Dt;
	PropDisc->AddLocalRotation(FRotator(0.0f, 0.0f, PropSpinDegPerSec * Dt));
	switch (Mode)
	{
	case EMode::Escort: TickEscort(Dt); break;
	case EMode::Hunt:   TickHunt(Dt); break;
	case EMode::Hold:   TickHold(Dt); break;
	case EMode::Break:  TickBreak(Dt); break;
	case EMode::Patrol: TickPatrol(Dt); break;
	}
}

FVector ASIWingman::KeepClearOfPlayer(const FVector& Aim) const
{
	const APawn* P = Player();
	if (!P)
	{
		return Aim;
	}
	const FVector Away = GetActorLocation() - P->GetActorLocation();
	const float D = Away.Size() / SI_CM;
	if (D < PlayerSeparation * 2.0f)
	{
		return Aim + Away.GetSafeNormal() * PlayerSeparation * SI_CM * 4.0f * (1.0f - D / (PlayerSeparation * 2.0f));
	}
	return Aim;
}

void ASIWingman::FlyToward(const FVector& Aim, float DesiredSpeed, float Dt)
{
	FVector P = GetActorLocation();
	const FVector D = Aim - P;
	const float DesYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	const float Horiz = FMath::Max(FVector2D(D.X, D.Y).Size(), 1.0f);
	float DesPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(D.Z, Horiz)), -18.0f, 18.0f);
	if (P.Z < 15000.0f) { DesPitch = FMath::Max(DesPitch, 8.0f); }  // never low over the fields
	const float YawErr = FMath::FindDeltaAngleDegrees(Yaw, DesYaw);
	const float Turn = FMath::Clamp(YawErr * 0.9f, -MaxTurnRate, MaxTurnRate);
	Yaw = FRotator::NormalizeAxis(Yaw + Turn * Dt);
	Pitch = FMath::FInterpTo(Pitch, DesPitch, Dt, 1.2f);
	Roll = FMath::FInterpTo(Roll, FMath::Clamp(Turn * BankPerTurnRate, -MaxBank, MaxBank), Dt, 2.5f);
	Speed = FMath::FInterpConstantTo(Speed, FMath::Clamp(DesiredSpeed, MinSpeed, MaxSpeed), Dt, Accel);
	P += FRotator(Pitch, Yaw, 0.0f).Vector() * Speed * SI_CM * Dt;
	SetActorLocationAndRotation(P, FRotator(Pitch, Yaw, Roll));
}

void ASIWingman::TickEscort(float Dt)
{
	const APawn* P = Player();
	if (!P)
	{
		// No one to fly with: hold a loose circle where we are.
		FlyToward(GetActorLocation() + FRotator(0.0f, Yaw + 25.0f, 0.0f).Vector() * 100000.0f, Spec.CruiseSpeed, Dt);
		return;
	}
	SearchTimer -= Dt;
	if (SearchTimer <= 0.0f && Age >= JoinFormationSeconds)
	{
		SearchTimer = SearchInterval;
		if (AActor* B = FindBomb())
		{
			SITick::FollowMovers(PrimaryActorTick, Target.Get(), false);
			Target = B;
			SITick::FollowMovers(PrimaryActorTick, B, true);
			const ISITippable* T = Cast<ISITippable>(B);
			const FSIWingFrame W = T->GetWingFrame();
			Side = FVector::DotProduct(GetActorLocation() - W.Location, W.Right) >= 0.0f ? 1.0f : -1.0f;
			if (USIAirspaceSubsystem* A = Air()) { A->ClaimBomb(B, this); }
			Mode = EMode::Hunt;
			ModeTime = 0.0f;
			UE_LOG(LogSixInches, Display, TEXT("[SixInches] %s going for %s (side %+.0f)"), *Spec.Callsign.ToString(), *B->GetName(), Side);
			return;
		}
	}
	const FRotator PY(0.0f, P->GetActorRotation().Yaw, 0.0f);
	const FVector Fwd = PY.Vector();
	const FVector Station = P->GetActorLocation() + PY.RotateVector(Spec.FormationOffset * SI_CM);
	const float Along = FVector::DotProduct(Station - GetActorLocation(), Fwd) / SI_CM;
	const float PSpeed = P->GetVelocity().Size() / SI_CM;
	FlyToward(KeepClearOfPlayer(Station + Fwd * 40000.0f), PSpeed + FMath::Clamp(Along * 0.3f, -50.0f, 60.0f), Dt);
}

AActor* ASIWingman::FindBomb() const
{
	USIAirspaceSubsystem* A = Air();
	const APawn* P = Player();
	if (!A || !P)
	{
		return nullptr;
	}
	const USIWingtipComponent* PlayerTip = P->FindComponentByClass<USIWingtipComponent>();
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AActor* B : A->GetBombs())
	{
		const ISITippable* T = Cast<ISITippable>(B);
		if (!T || !T->IsTippable())
		{
			continue;
		}
		const AActor* Claimer = A->GetClaimer(B);
		if ((Claimer && Claimer != this) || (PlayerTip && PlayerTip->GetTarget() == B))
		{
			continue;
		}
		const FVector L = B->GetActorLocation();
		if (L.Y < Spec.LaneMinY * SI_CM || L.Y > Spec.LaneMaxY * SI_CM)
		{
			continue;  // not our lane
		}
		if (L.X > A->FlakStartX - (FlakKeepOut + MinRunway) * SI_CM)
		{
			continue;  // can't finish before our own guns
		}
		const float ToPlayer = FVector::Dist(L, P->GetActorLocation()) / SI_CM;
		if (ToPlayer < PlayerYieldDistance)
		{
			continue;  // the player's
		}
		const float ToMe = FVector::Dist(L, GetActorLocation()) / SI_CM;
		const float Score = ToMe - 0.5f * ToPlayer;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = B;
		}
	}
	return Best;
}

FVector ASIWingman::SlotPoint(const FSIWingFrame& W) const
{
	// Our inner wingtip SlotOverlap inboard of (under) the bomb's tip and SlotGap below it.
	const FVector BombTip = Side > 0.0f ? W.TipRight : W.TipLeft;
	const FVector InnerTip = BombTip - W.Right * Side * SlotOverlap * SI_CM - W.Up * SlotGap * SI_CM;
	const FVector InnerLocal(HalfSpanTip.X, -Side * FMath::Abs(HalfSpanTip.Y), HalfSpanTip.Z);
	return InnerTip - (W.Forward * InnerLocal.X + W.Right * InnerLocal.Y + W.Up * InnerLocal.Z);
}

void ASIWingman::Release(bool bBreak)
{
	if (AActor* B = Target.Get())
	{
		if (ISITippable* T = Cast<ISITippable>(B))
		{
			if (T->IsTippable() && Mode == EMode::Hold) { T->SetLiftProgress(0.0f, Side); }
		}
		if (USIAirspaceSubsystem* A = Air()) { A->ReleaseBomb(B, this); }
		SITick::FollowMovers(PrimaryActorTick, B, false);
	}
	Target.Reset();
	HoldTime = 0.0f;
	Mode = bBreak ? EMode::Break : EMode::Escort;
	ModeTime = 0.0f;
}

void ASIWingman::TickHunt(float Dt)
{
	AActor* B = Target.Get();
	ISITippable* T = Cast<ISITippable>(B);
	USIAirspaceSubsystem* A = Air();
	if (!T || !T->IsTippable() || !A || B->GetActorLocation().X > A->FlakStartX - FlakKeepOut * SI_CM || ModeTime > 40.0f)
	{
		Release(false);
		return;
	}
	if (const APawn* P = Player())
	{
		const USIWingtipComponent* PlayerTip = P->FindComponentByClass<USIWingtipComponent>();
		if (PlayerTip && PlayerTip->GetTarget() == B)
		{
			Release(true);
			return;
		}
	}
	const FSIWingFrame W = T->GetWingFrame();
	const FVector Slot = SlotPoint(W);
	const FVector F = FRotator(0.0f, W.Forward.Rotation().Yaw, 0.0f).Vector();
	const FVector R = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
	const float Along = FVector::DotProduct(Slot - GetActorLocation(), F) / SI_CM;
	const float Lat = FVector::DotProduct(Slot - GetActorLocation(), R) / SI_CM;
	if (FMath::Abs(Along) < SlotCaptureDistance && FMath::Abs(Lat) < SlotCaptureDistance * 0.35f)
	{
		Mode = EMode::Hold;
		ModeTime = 0.0f;
		HoldTime = 0.0f;
		return;
	}
	// Run in along the slot line, a few metres below it (slide up under the wing at the end), speed set by the gap.
	const float SpeedAdd = FMath::Sign(Along) * FMath::Min(FMath::Sqrt(2.0f * Accel * FMath::Abs(Along)) + 3.0f, 70.0f);
	const float Below = FMath::Abs(Along) > SlotCaptureDistance ? 300.0f : 0.0f;
	const FVector Aim = Slot + F * FMath::Clamp(Along, 120.0f, 400.0f) * SI_CM - FVector::UpVector * Below;
	FlyToward(KeepClearOfPlayer(Aim), W.Speed + SpeedAdd, Dt);
}

void ASIWingman::TickHold(float Dt)
{
	AActor* B = Target.Get();
	ISITippable* T = Cast<ISITippable>(B);
	USIAirspaceSubsystem* A = Air();
	if (!T || !T->IsTippable() || !A)
	{
		Release(true);
		return;
	}
	if (const APawn* P = Player())
	{
		const USIWingtipComponent* PlayerTip = P->FindComponentByClass<USIWingtipComponent>();
		if (PlayerTip && PlayerTip->GetTarget() == B)
		{
			Release(true);
			return;
		}
	}
	const FSIWingFrame W = T->GetWingFrame();
	if (W.Location.X > A->FlakStartX - FlakKeepOut * SI_CM)
	{
		UE_LOG(LogSixInches, Display, TEXT("[SixInches] %s breaks off %s at the flak belt"), *Spec.Callsign.ToString(), *B->GetName());
		Release(true);
		return;
	}
	// Ride along with the bomb and ease onto the slot (exponential slide: no snap).
	const FVector Slot = SlotPoint(W);
	FVector P = GetActorLocation() + W.Forward * W.Speed * SI_CM * Dt;
	P = FMath::VInterpTo(P, Slot, Dt, SlideRate);
	const FRotator Want = W.Forward.Rotation();
	Yaw = FMath::FInterpTo(Yaw, Yaw + FMath::FindDeltaAngleDegrees(Yaw, Want.Yaw), Dt, 3.0f);
	Pitch = FMath::FInterpTo(Pitch, Want.Pitch, Dt, 3.0f);
	Roll = FMath::FInterpTo(Roll, 0.0f, Dt, 2.0f);
	Speed = W.Speed;
	SetActorLocationAndRotation(P, FRotator(Pitch, Yaw, Roll));

	if (FVector::Dist(P, Slot) < InSlotTolerance * SI_CM)
	{
		HoldTime += Dt;
	}
	float Lift = FMath::Clamp(HoldTime / FMath::Max(HoldSeconds, 0.1f), 0.0f, 1.0f);
	if (Lift >= 0.85f)
	{
		const FSIGroundClass Pred = A->Classify(T->PredictTipImpact(Side));
		if (Pred.bHit)
		{
			// Houses under the dive: keep the wing up a little and wait for open ground.
			HoldTime = 0.85f * HoldSeconds;
			Lift = 0.85f;
		}
		else if (Lift >= 1.0f)
		{
			UE_LOG(LogSixInches, Display, TEXT("[SixInches] %s tipped %s"), *Spec.Callsign.ToString(), *B->GetName());
			T->Tip(Side);
			OnTipped.Broadcast(Spec.Callsign, B, Pred);
			if (A) { A->ReleaseBomb(B, this); }
			SITick::FollowMovers(PrimaryActorTick, B, false);
			Target.Reset();
			HoldTime = 0.0f;
			Mode = EMode::Break;
			ModeTime = 0.0f;
			return;
		}
	}
	T->SetLiftProgress(Lift, Side);
}

void ASIWingman::TickBreak(float Dt)
{
	// Peel away outboard and up, then rejoin.
	const FVector Aim = GetActorLocation() + FRotator(0.0f, Yaw + Side * 40.0f, 0.0f).Vector() * 100000.0f + FVector::UpVector * 12000.0f;
	FlyToward(KeepClearOfPlayer(Aim), Spec.CruiseSpeed + 20.0f, Dt);
	if (ModeTime > BreakSeconds)
	{
		Mode = EMode::Escort;
		ModeTime = 0.0f;
	}
}

void ASIWingman::TickPatrol(float Dt)
{
	const FVector C = Spec.PatrolCenter * SI_CM;
	const FVector Off = GetActorLocation() - C;
	PatrolAngle = FMath::Atan2(Off.Y, Off.X);
	const float Ahead = PatrolAngle + 0.35f * (Spec.Seed % 2 == 0 ? 1.0f : -1.0f);
	const FVector Aim(C.X + FMath::Cos(Ahead) * Spec.PatrolRadius * SI_CM, C.Y + FMath::Sin(Ahead) * Spec.PatrolRadius * SI_CM, C.Z);
	FlyToward(Aim, Spec.CruiseSpeed, Dt);
}
