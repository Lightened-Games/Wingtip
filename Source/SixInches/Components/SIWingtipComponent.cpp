// Six Inches - the wingtip trick.

#include "Components/SIWingtipComponent.h"
#include "Components/SIFlightComponent.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "Components/SIHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interfaces/SIInterfaces.h"
#include "World/SIAirspaceSubsystem.h"
#include "SixInches.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	/** Level frame along a target's heading: forward, right, up with no roll. */
	void LevelFrame(const FVector& Forward, FVector& F, FVector& R, FVector& U)
	{
		F = Forward.GetSafeNormal();
		R = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
		if (R.IsNearlyZero())
		{
			R = FVector::RightVector;
		}
		U = FVector::CrossProduct(F, R).GetSafeNormal();
	}
}

USIWingtipComponent::USIWingtipComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USIWingtipComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	Flight = Owner ? Owner->FindComponentByClass<USIFlightComponent>() : nullptr;
	Health = Owner ? Owner->FindComponentByClass<USIHealthComponent>() : nullptr;
	TipRightActual = TipRightLocal;
	TipLeftLocal = FVector(TipRightLocal.X, -TipRightLocal.Y, TipRightLocal.Z);
	if (Owner)
	{
		TArray<UStaticMeshComponent*> Meshes;
		Owner->GetComponents(Meshes);
		for (UStaticMeshComponent* M : Meshes)
		{
			if (M && M->ComponentHasTag(TEXT("SI.Body")) && M->DoesSocketExist(TEXT("TipR")) && M->DoesSocketExist(TEXT("TipL")))
			{
				// Actor space (for the root component, its "relative" transform is the world transform).
				TipRightActual = M->GetSocketTransform(TEXT("TipR"), RTS_Actor).GetLocation();
				TipLeftLocal = M->GetSocketTransform(TEXT("TipL"), RTS_Actor).GetLocation();
				UE_LOG(LogSixInches, Display, TEXT("[SixInches] Wingtips from sockets: R %s  L %s"), *TipRightActual.ToString(), *TipLeftLocal.ToString());
			}
		}
	}
}

USIAirspaceSubsystem* USIWingtipComponent::Airspace() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<USIAirspaceSubsystem>() : nullptr;
}

FVector USIWingtipComponent::GetInnerTipLocal(float InSide) const
{
	// Player on the target's right (Side +1): our LEFT tip is the inner one.
	return InSide > 0.0f ? TipLeftLocal : TipRightActual;
}

void USIWingtipComponent::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
	if (!bEnabled && State == ESISlotState::Slot)
	{
		ExitSlot(false);
	}
}

void USIWingtipComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	NoiseTime += Dt;
	SlotBlend = FMath::FInterpConstantTo(SlotBlend, State == ESISlotState::Slot ? 1.0f : 0.0f, Dt, 1.0f / 0.6f);
	ContactCool = FMath::Max(0.0f, ContactCool - Dt);
	Reentry = FMath::Max(0.0f, Reentry - Dt);
	if (!bEnabled || !Flight || Flight->IsCrashed())
	{
		return;
	}
	if (State == ESISlotState::Slot)
	{
		TickSlot(Dt);
	}
	else
	{
		TickFree(Dt);
	}
}

void USIWingtipComponent::TickFree(float Dt)
{
	if (Progress > 0.0f)
	{
		Progress = FMath::Max(0.0f, Progress - ProgressDecay * Dt);
	}
	USIAirspaceSubsystem* Air = Airspace();
	AActor* Owner = GetOwner();
	if (!Air || !Owner || Reentry > 0.0f || Flight->IsLocked())
	{
		return;
	}
	const FVector P = Owner->GetActorLocation();
	const FRotator Rot = Owner->GetActorRotation();
	for (AActor* Bomb : Air->GetBombs())
	{
		const ISITippable* T = Cast<ISITippable>(Bomb);
		if (!T || !T->IsTippable())
		{
			continue;
		}
		const FSIWingFrame W = T->GetWingFrame();
		FVector F, R, U;
		LevelFrame(W.Forward, F, R, U);
		const float SideHere = FVector::DotProduct(P - W.Location, R) >= 0.0f ? 1.0f : -1.0f;
		const FVector BombTip = SideHere > 0.0f ? W.TipRight : W.TipLeft;
		const FVector MyTip = P + Rot.RotateVector(GetInnerTipLocal(SideHere));
		const FVector D = MyTip - BombTip;
		const float Fwd = FVector::DotProduct(D, F) / SI_CM;
		const float Lat = FVector::DotProduct(D, R) * SideHere / SI_CM;
		const float Vert = FVector::DotProduct(D, U) / SI_CM;
		const float SpeedDelta = Flight->GetSpeed() - W.Speed;
		const bool bAligned = FVector::DotProduct(Rot.Vector(), F) > 0.8f;
		// No entry while our tip is already over and inside its wing (the first bomb catches up over us): it would have to
		// come down through the wing. Free flight keeps the planes apart until we are beside or below it.
		const bool bOverItsWing = false; // entry block (Lat < 0 && Vert > -0.1) lost a round; off
		if (bAligned && !bOverItsWing && FMath::Abs(Fwd) < SlotEnterForeAft && Lat > -3.0f && Lat < SlotEnterLateral && FMath::Abs(Vert) < SlotEnterVertical
			&& FMath::Abs(SpeedDelta) < SlotEnterSpeedDelta)
		{
			EnterSlot(Bomb, SideHere, SpeedDelta);
			return;
		}
	}
}

void USIWingtipComponent::EnterSlot(AActor* Bomb, float InSide, float SpeedDelta)
{
	const ISITippable* T = Cast<ISITippable>(Bomb);
	AActor* Owner = GetOwner();
	if (!T || !Owner)
	{
		return;
	}
	const FSIWingFrame W = T->GetWingFrame();
	FVector F, R, U;
	LevelFrame(W.Forward, F, R, U);
	Side = InSide;
	Target = Bomb;
	SITick::FollowMovers(PrimaryComponentTick, Bomb, true);
	const FVector BombTip = Side > 0.0f ? W.TipRight : W.TipLeft;
	const FVector Rel = Owner->GetActorLocation() - BombTip;
	Offset = FVector(FVector::DotProduct(Rel, F), FVector::DotProduct(Rel, R), FVector::DotProduct(Rel, U));
	// Carry the closing speed in so the speed match feels like a deceleration, not a snap.
	const FVector V = Flight->GetVelocity();
	OffsetVel = FVector(SpeedDelta * SI_CM, FVector::DotProduct(V, R) * 0.5f, FVector::DotProduct(V, U) * 0.5f);
	State = ESISlotState::Slot;
	// Every bomb's wake starts level and rolls its own way.
	Balance = 0.0f;
	BalanceRate = 0.0f;
	SlotRoll = 0.0f;
	GapCm = -1000.0f; // unknown side until the first slot tick: never treat an entry from above as "from below"
	WakeSeed += 37.3f;
	Flight->SetFormation(true);
	Flight->SetFormationTarget(Owner->GetActorLocation(), Owner->GetActorRotation(), W.Speed);
	PreviewTimer = 0.0f;
	bHasPreview = false;
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Slot entered on %s (side %+.0f, closing %.1f m/s)"), *Bomb->GetName(), Side, SpeedDelta);
	OnSlotChanged.Broadcast(true, Bomb);
}

void USIWingtipComponent::ExitSlot(bool bBreakAway)
{
	AActor* Old = Target.Get();
	SITick::FollowMovers(PrimaryComponentTick, Old, false);
	if (ISITippable* T = Cast<ISITippable>(Old))
	{
		if (T->IsTippable())
		{
			T->SetLiftProgress(0.0f, Side);
		}
	}
	State = ESISlotState::Free;
	Target.Reset();
	if (bInBand)
	{
		bInBand = false;
		OnBandChanged.Broadcast(false);
	}
	Reentry = ReentryCooldown;
	bHasPreview = false;
	if (Flight)
	{
		if (bBreakAway)
		{
			Flight->BreakAway(BreakAwayClimb, Side * BreakAwayBank);
		}
		else
		{
			Flight->SetFormation(false);
		}
	}
	OnSlotChanged.Broadcast(false, Old);
}

void USIWingtipComponent::TickSlot(float Dt)
{
	AActor* Owner = GetOwner();
	AActor* Bomb = Target.Get();
	ISITippable* T = Cast<ISITippable>(Bomb);
	if (!Owner || !T || !T->IsTippable())
	{
		ExitSlot(true);
		return;
	}
	if (Input.bBrake)
	{
		UE_LOG(LogSixInches, Display, TEXT("[SixInches] Broke off"));
		ExitSlot(true);
		return;
	}
	const FSIWingFrame W = T->GetWingFrame();
	FVector F, R, U;
	LevelFrame(W.Forward, F, R, U);
	const FVector BombTip = Side > 0.0f ? W.TipRight : W.TipLeft;
	const FVector InnerLocal = GetInnerTipLocal(Side);

	// Where the inner tip sits along track decides the fore/aft assist.
	const FRotator LevelRot = FRotationMatrix::MakeFromXZ(F, U).Rotator();
	const FVector TipFromRoot = LevelRot.RotateVector(InnerLocal);
	const float TipForeAft = (Offset.X + FVector::DotProduct(TipFromRoot, F));

	// Stick nudges (Pitch +1 = up, Roll +1 = right) and the fore/aft assist. Far from the tip the nudges are
	// quicker so closing in from the slot edge takes seconds, near the band they are fine.
	const float LatFar = FMath::Clamp(1.0f + (FMath::Abs(Overlap - BandOverlapIdeal) - 0.8f) * FarNudgeGain, 1.0f, 10.0f);
	const float VertFar = FMath::Clamp(1.0f + (FMath::Abs(GapCm - BandGapIdeal) / SI_CM - 0.5f) * FarNudgeGain * 2.0f, 1.0f, 16.0f);
	// Band hold assist (human pilots; user, 2026-11-13: "control is so hard"): with a key released, that axis drifts
	// the wingtip toward the six-inch band by itself; any key press overrides it. Never slides in while level with
	// or above the bomb's wing (it would strike it): drops below first.
	FSIFlightInput Stick = Input;
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	if (BandHoldAssist > 0.0f && !(Flow && Flow->IsBotDriving()))
	{
		const float AimOverlap = (GapCm < 1.0f && Overlap < 0.2f) ? -0.8f : BandOverlapIdeal;
		if (FMath::Abs(Stick.Roll) < 0.15f)
		{
			Stick.Roll = -Side * FMath::Clamp((AimOverlap - Overlap) / 0.5f, -1.0f, 1.0f) * BandHoldAssist;
		}
		if (FMath::Abs(Stick.Pitch) < 0.15f)
		{
			Stick.Pitch = FMath::Clamp((GapCm - BandGapIdeal) / 15.0f, -1.0f, 1.0f) * BandHoldAssist;
		}
	}
	const FVector DesiredVel(
		bSlotAssist ? FMath::Clamp(-TipForeAft * ForeAftAssist, -MaxAssistSpeed * SI_CM, MaxAssistSpeed * SI_CM) : OffsetVel.X,
		FMath::Clamp(Stick.Roll, -1.0f, 1.0f) * NudgeLateral * LatFar * SI_CM,
		FMath::Clamp(Stick.Pitch, -1.0f, 1.0f) * NudgeVertical * VertFar * SI_CM);
	const float K = FMath::Min(1.0f, NudgeResponse * Dt);
	OffsetVel.Y += (DesiredVel.Y - OffsetVel.Y) * K;
	OffsetVel.Z += (DesiredVel.Z - OffsetVel.Z) * K;
	OffsetVel.X += (DesiredVel.X - OffsetVel.X) * FMath::Min(1.0f, 2.0f * Dt);

	// Turbulence: smoke columns plus the ambient gust that grows with every bomb stopped.
	USIAirspaceSubsystem* Air = Airspace();
	const float Esc = Air ? Air->GetEscalation() : 1.0f;
	const float Hot = Air ? Air->SampleTurbulence(Owner->GetActorLocation()) * Esc : 0.0f;
	const float Gust = AmbientGust * FMath::Max(0.0f, Esc - 1.0f);
	// Fixed scale (was / SlotWobbleVertical, which made lowering that value strengthen the gust instead).
	const float Intensity = Hot + Gust / 0.2f;
	OffsetVel.Z += GustDrift * SI_CM * Intensity * FMath::PerlinNoise1D(NoiseTime * 0.6f + 41.0f) * Dt * 4.0f;
	OffsetVel.Y += GustDrift * SI_CM * 0.6f * Intensity * FMath::PerlinNoise1D(NoiseTime * 0.5f + 77.0f) * Dt * 4.0f;
	Offset += OffsetVel * Dt;

	const FVector Wobble = U * (SlotWobbleVertical * SI_CM * Intensity * FMath::PerlinNoise1D(NoiseTime * 1.7f + 5.0f))
		+ R * (SlotWobbleLateral * SI_CM * Intensity * FMath::PerlinNoise1D(NoiseTime * 1.3f + 9.0f));
	const float RollWobble = SlotWobbleRoll * Intensity * FMath::PerlinNoise1D(NoiseTime * 2.1f + 13.0f);
	const float BankFromSlide = -OffsetVel.Y / SI_CM * SlideBank;

	// Wake balance (user, 2026-11-16: "use a meter to balance plane while tailing someone"): the bomb's wingtip vortex
	// rolls the fighter one way then the other, and an off-level wing keeps rolling by itself. Q / E trim it back.
	// Off level, the lift stalls; at the end of the meter the wake flips the fighter out of the slot.
	if (bWakeBalance)
	{
		const float Push = WakePush * (bInBand ? WakeInBandScale : 1.0f) * (1.0f + WakeEscalation * FMath::Max(0.0f, Esc - 1.0f));
		const float Wake = Push * 1.6f * FMath::PerlinNoise1D(NoiseTime * WakeFrequency + WakeSeed);
		const float WantRate = Wake + WakeInstability * Balance + FMath::Clamp(Input.Trim, -1.0f, 1.0f) * TrimRate;
		BalanceRate += (WantRate - BalanceRate) * FMath::Min(1.0f, BalanceResponse * Dt);
		Balance += BalanceRate * Dt;
		if (FMath::Abs(Balance) >= 1.0f)
		{
			UE_LOG(LogSixInches, Display, TEXT("[SixInches] Wake flipped the fighter out (balance %+.2f)"), Balance);
			if (Health)
			{
				Health->ApplyDamage(WakeFlipDamage, ESIDamageSource::Contact);
			}
			OnContact.Broadcast((PlayerTipWorld + TargetTipWorld) * 0.5f);
			Balance = 0.0f;
			BalanceRate = 0.0f;
			ExitSlot(true);
			return;
		}
	}

	FVector Root = BombTip + F * Offset.X + R * Offset.Y + U * Offset.Z + Wobble;
	FRotator Rot = LevelRot;
	const float RollExtra = RollWobble + BankFromSlide + (bWakeBalance ? Balance * BalanceRollDeg : 0.0f);
	SlotRoll = SlotRollSmoothing > 0.0f ? FMath::FInterpTo(SlotRoll, RollExtra, Dt, SlotRollSmoothing) : RollExtra;
	Rot.Roll += SlotRoll;

	// Band metrics from the real tip positions.
	FVector MyTip = Root + Rot.RotateVector(InnerLocal);
	FVector D = MyTip - BombTip;
	const float PrevGap = GapCm;
	ForeAft = FVector::DotProduct(D, F) / SI_CM;
	const float LatCm = FVector::DotProduct(D, R) * Side;
	Overlap = -LatCm / SI_CM;
	GapCm = -FVector::DotProduct(D, U) - WingThickness;

	// Hard stop: the inner tip can't slide in past the end of the band (it would pass under the fuselage).
	const float MaxIn = BandOverlapMax + 0.8f;
	if (Overlap > MaxIn)
	{
		const float Fix = (Overlap - MaxIn) * SI_CM;
		Offset.Y += Side * Fix;
		Root += R * Side * Fix;
		MyTip += R * Side * Fix;
		Overlap = MaxIn;
		OffsetVel.Y = 0.0f;
	}
	// Coming in from above, the tip may not slide over and down through the bomb's wing (the clamp below used to snap it
	// down through it, user 2026-11-17: "i just saw overlapping"): hold it at the tip line until it is underneath.
	if (Overlap > 0.0f && GapCm < -2.0f && PrevGap < -2.0f && FMath::Abs(ForeAft) < 1.6f)
	{
		const float Fix = Overlap * SI_CM;
		Offset.Y += Side * Fix;
		Root += R * Side * Fix;
		MyTip += R * Side * Fix;
		Overlap = 0.0f;
		OffsetVel.Y = 0.0f;
	}
	// Never let the wings pass through each other (C2): clamp to a scrape and count it as contact.
	const bool bOverlapping = Overlap > 0.05f && FMath::Abs(ForeAft) < 1.6f;
	if (bOverlapping && GapCm < -2.0f)
	{
		const float Fix = -2.0f - GapCm;
		Offset.Z -= Fix;
		Root -= U * Fix;
		MyTip -= U * Fix;
		GapCm = -2.0f;
	}
	if (bOverlapping && GapCm < BandGapMin && ContactCool <= 0.0f)
	{
		ContactCool = ContactCooldown;
		OffsetVel.Z -= ContactPushDown * SI_CM;
		OffsetVel.Y += Side * ContactPushOut * SI_CM;
		T->Bump(U * 120.0f);
		if (Health)
		{
			Health->ApplyDamage(ContactDamage, ESIDamageSource::Contact);
		}
		UE_LOG(LogSixInches, Display, TEXT("[SixInches] Wing contact (gap %.1f cm)"), GapCm);
		OnContact.Broadcast((MyTip + BombTip) * 0.5f);
	}

	Flight->SetFormationTarget(Root, Rot, W.Speed);
	PlayerTipWorld = MyTip;
	TargetTipWorld = BombTip;

	// The band and the hold.
	const bool bNowInBand = Overlap >= BandOverlapMin && Overlap <= BandOverlapMax && FMath::Abs(ForeAft) <= BandForeAft
		&& GapCm >= BandGapMin && GapCm <= BandGapMax;
	if (bNowInBand != bInBand)
	{
		bInBand = bNowInBand;
		OnBandChanged.Broadcast(bInBand);
	}
	if (bInBand && IsBalanceSafe())
	{
		const float GapErr = GapCm < BandGapIdeal ? (BandGapIdeal - GapCm) / FMath::Max(BandGapIdeal - BandGapMin, 0.1f)
			: (GapCm - BandGapIdeal) / FMath::Max(BandGapMax - BandGapIdeal, 0.1f);
		const float OvErr = Overlap < BandOverlapIdeal ? (BandOverlapIdeal - Overlap) / FMath::Max(BandOverlapIdeal - BandOverlapMin, 0.01f)
			: (Overlap - BandOverlapIdeal) / FMath::Max(BandOverlapMax - BandOverlapIdeal, 0.01f);
		const float Err = FMath::Clamp(FMath::Max(GapErr, OvErr), 0.0f, 1.0f);
		Progress += Dt / FMath::Lerp(TipTimeBest, TipTimeEdge, Err);
	}
	else
	{
		Progress = FMath::Max(0.0f, Progress - ProgressDecay * Dt);
	}
	T->SetLiftProgress(FMath::Clamp(Progress, 0.0f, 1.0f), Side);

	// Where it would come down if it rolled now.
	PreviewTimer -= Dt;
	if (PreviewTimer <= 0.0f && Air)
	{
		PreviewTimer = PreviewInterval;
		PreviewLocation = T->PredictTipImpact(Side);
		const FSIGroundClass NewClass = Air->Classify(PreviewLocation);
		const bool bChanged = !bHasPreview || NewClass.Kind != PreviewClass.Kind;
		PreviewClass = NewClass;
		bHasPreview = true;
		if (bChanged)
		{
			OnPreviewChanged.Broadcast(PreviewClass);
		}
	}

	if (Progress >= 1.0f)
	{
		const FSIGroundClass Predicted = PreviewClass;
		UE_LOG(LogSixInches, Display, TEXT("[SixInches] Tipped %s, predicted %s"), *Bomb->GetName(), *SIZones::Label(Predicted.Kind).ToString());
		T->Tip(Side);
		Progress = 0.0f;
		OnBombTipped.Broadcast(Bomb, Predicted);
		ExitSlot(true);
		return;
	}

	// Drifted too far away: back to free flight.
	if (FMath::Abs(LatCm) > SlotExitDistance * SI_CM || FMath::Abs(ForeAft) > SlotExitDistance || FMath::Abs(GapCm) > SlotExitDistance * SI_CM)
	{
		ExitSlot(false);
	}
}
