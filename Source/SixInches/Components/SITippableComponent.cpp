// Six Inches - flying bomb autopilot.

#include "Components/SITippableComponent.h"
#include "World/SIAirspaceSubsystem.h"
#include "SixInches.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

USITippableComponent::USITippableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

USIAirspaceSubsystem* USITippableComponent::Airspace() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<USIAirspaceSubsystem>() : nullptr;
}

void USITippableComponent::Launch(const FVector& Target, float SpeedMS, int32 Seed)
{
	AActor* Owner = GetOwner();
	TargetPoint = Target;
	Speed = SpeedMS;
	CruiseSpeed = SpeedMS;
	BaseAltitude = Owner ? Owner->GetActorLocation().Z : Target.Z;
	LaunchPoint = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	const FVector Dir = (Target - (Owner ? Owner->GetActorLocation() : FVector::ZeroVector)).GetSafeNormal2D();
	Rot = FRotator(0.0f, Dir.Rotation().Yaw, 0.0f);
	FRandomStream R(Seed);
	PhaseA = R.FRandRange(0.0f, 6.28f);
	PhaseB = R.FRandRange(0.0f, 6.28f);
	bLaunched = true;
	State = ESIAutopilotState::Cruise;
	if (Owner)
	{
		Owner->SetActorRotation(Rot);
	}
}

void USITippableComponent::SetCatchUp(AActor* InPlayer, float ExtraMS, float AheadM, float MaxSeconds)
{
	CatchUpPlayer = InPlayer;
	CatchUpExtra = ExtraMS;
	CatchUpAhead = AheadM;
	CatchUpLeft = InPlayer ? MaxSeconds : 0.0f;
	if (InPlayer)
	{
		Speed = FMath::Clamp(InPlayer->GetVelocity().Size() / SI_CM + ExtraMS, CruiseSpeed, CruiseSpeed + 70.0f);
	}
}

void USITippableComponent::SetState(ESIAutopilotState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnStateChanged.Broadcast(NewState);
	}
}

void USITippableComponent::SetLiftProgress(float Progress01, float LiftedSide)
{
	if (State == ESIAutopilotState::Cruise)
	{
		Lift = FMath::Clamp(Progress01, 0.0f, 1.0f);
		LiftSide = LiftedSide;
	}
}

void USITippableComponent::Tip(float LiftedSide)
{
	if (State != ESIAutopilotState::Cruise)
	{
		return;
	}
	TipSide = LiftedSide;
	SetState(ESIAutopilotState::Tipped);
}

FVector USITippableComponent::PredictTipImpact(float LiftedSide) const
{
	const AActor* Owner = GetOwner();
	USIAirspaceSubsystem* Air = Airspace();
	if (!Owner || !Air)
	{
		return FVector::ZeroVector;
	}
	// Same kinematics as the Tipped branch below, stepped ahead.
	FVector P = Owner->GetActorLocation();
	FRotator R = Rot;
	const float Dt = 0.08f;
	float RollDone = FMath::Abs(R.Roll) / FMath::Max(TipRollTarget, 1.0f);
	for (int32 i = 0; i < 160; ++i)
	{
		const float TargetRoll = -LiftedSide * TipRollTarget;
		R.Roll = FMath::FixedTurn(R.Roll, TargetRoll, TipRollRate * Dt);
		RollDone = FMath::Abs(R.Roll) / FMath::Max(TipRollTarget, 1.0f);
		R.Pitch = FMath::Max(-TipMaxDive, R.Pitch - TipPitchRate * Dt * FMath::Clamp(RollDone * 1.6f, 0.2f, 1.0f));
		R.Yaw += -LiftedSide * TipYawRate * Dt;
		P += FRotator(R.Pitch, R.Yaw, 0.0f).Vector() * Speed * SI_CM * Dt;
		if (i % 3 == 0 || P.Z < 40000.0f)
		{
			const float G = Air->GetGroundHeight(P);
			if (P.Z <= G + 100.0f)
			{
				P.Z = G;
				return P;
			}
		}
	}
	P.Z = Air->GetGroundHeight(P);
	return P;
}

void USITippableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!Owner || !bLaunched || State == ESIAutopilotState::Done)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	Age += Dt;
	FVector P = Owner->GetActorLocation();
	USIAirspaceSubsystem* Air = Airspace();

	// Catch-up launch (user, 2026-11-16: "most planes are uncatchable"): run past the player, then settle to cruise just ahead.
	if (State == ESIAutopilotState::Cruise && CatchUpLeft > 0.0f)
	{
		CatchUpLeft -= Dt;
		const AActor* Pl = CatchUpPlayer.Get();
		const FVector Fwd = FRotator(0.0f, Rot.Yaw, 0.0f).Vector();
		const FVector Rel = Pl ? (P - Pl->GetActorLocation()) / SI_CM : FVector(1.0e6f);
		const float Along = FVector::DotProduct(Rel, Fwd);
		const float Lateral = FVector::VectorPlaneProject(Rel, Fwd).Size();
		// Done once well ahead, once level and close (someone is about to slot on it: chasing their speed would run away),
		// or as soon as anyone starts lifting it.
		if (!Pl || Along > CatchUpAhead || (Along > -30.0f && Lateral < 60.0f) || Lift > 0.0f)
		{
			CatchUpLeft = 0.0f;
		}
		else
		{
			// At least cruise + 50 so it reaches a player who turned back for it, at most cruise + 70.
			const float Want = FMath::Clamp(FMath::Max(Pl->GetVelocity().Size() / SI_CM + CatchUpExtra, CruiseSpeed + 50.0f), CruiseSpeed, CruiseSpeed + 70.0f);
			Speed = FMath::FInterpConstantTo(Speed, Want, Dt, 40.0f);
		}
	}
	else if (State == ESIAutopilotState::Cruise && Speed != CruiseSpeed)
	{
		Speed = FMath::FInterpConstantTo(Speed, CruiseSpeed, Dt, 25.0f);
	}

	switch (State)
	{
	case ESIAutopilotState::Cruise:
	{
		// Hold course toward the aim point with a slow hunting wander; hold height.
		const FVector ToTarget = (TargetPoint - P).GetSafeNormal2D();
		const float DesiredYaw = ToTarget.Rotation().Yaw + HuntYaw * FMath::Sin(Age * UE_TWO_PI / HuntPeriod + PhaseA);
		Rot.Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(Rot.Yaw, DesiredYaw) * CourseGain, -3.0f, 3.0f) * Dt;
		const float DesiredZ = BaseAltitude + HuntAltitude * SI_CM * FMath::Sin(Age * UE_TWO_PI / (HuntPeriod * 1.3f) + PhaseB);
		Rot.Pitch = FMath::Clamp((DesiredZ - P.Z) / SI_CM * 0.8f, -4.0f, 4.0f);
		const float TargetRoll = -LiftSide * Lift * MaxLiftRoll;
		Rot.Roll = FMath::FInterpTo(Rot.Roll, TargetRoll, Dt, 4.0f);
		if (DiveAfter > 0.0f && FVector::Dist2D(P, LaunchPoint) > DiveAfter * SI_CM)
		{
			UE_LOG(LogSixInches, Display, TEXT("[SixInches] %s ran its course: diving"), *Owner->GetName());
			SetState(ESIAutopilotState::TownDive);
		}
		else if (Air && P.X >= Air->TownLineX)
		{
			UE_LOG(LogSixInches, Display, TEXT("[SixInches] %s crossed the town line: diving"), *Owner->GetName());
			SetState(ESIAutopilotState::TownDive);
		}
		break;
	}
	case ESIAutopilotState::Tipped:
	{
		const float TargetRoll = -TipSide * TipRollTarget;
		Rot.Roll = FMath::FixedTurn(Rot.Roll, TargetRoll, TipRollRate * Dt);
		const float RollDone = FMath::Abs(Rot.Roll) / FMath::Max(TipRollTarget, 1.0f);
		Rot.Pitch = FMath::Max(-TipMaxDive, Rot.Pitch - TipPitchRate * Dt * FMath::Clamp(RollDone * 1.6f, 0.2f, 1.0f));
		Rot.Yaw += -TipSide * TipYawRate * Dt;
		break;
	}
	case ESIAutopilotState::TownDive:
		Rot.Pitch = FMath::Max(-TipMaxDive, Rot.Pitch - TownDivePitchRate * Dt);
		Rot.Roll = FMath::FInterpTo(Rot.Roll, 0.0f, Dt, 2.0f);
		break;
	default:
		break;
	}

	BumpVel *= FMath::Exp(-3.0f * Dt);
	P += FRotator(Rot.Pitch, Rot.Yaw, 0.0f).Vector() * Speed * SI_CM * Dt + BumpVel * Dt;
	Owner->SetActorLocationAndRotation(P, Rot);

	if (State != ESIAutopilotState::Cruise && Air)
	{
		GroundTimer -= Dt;
		if (GroundTimer <= 0.0f || P.Z < GroundZ + 3000.0f)
		{
			GroundTimer = 0.05f;
			GroundZ = Air->GetGroundHeight(P);
		}
		if (P.Z <= GroundZ + 100.0f)
		{
			const bool bTipped = State == ESIAutopilotState::Tipped;
			SetState(ESIAutopilotState::Done);
			OnImpact.Broadcast(FVector(P.X, P.Y, GroundZ), bTipped);
		}
	}
}
