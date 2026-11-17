// Six Inches - autoplay bot.

#include "Framework/SIAutoplayComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "Framework/SIMatchComponent.h"
#include "Components/SIFlightComponent.h"
#include "Components/SIWingtipComponent.h"
#include "Components/SIHealthComponent.h"
#include "Components/SITippableComponent.h"
#include "Interfaces/SIInterfaces.h"
#include "World/SIAirspaceSubsystem.h"
#include "SixInches.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

USIAutoplayComponent::USIAutoplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USIAutoplayComponent::StartAutoplay(const FString& InForcedDefeat)
{
	bRunning = true;
	ForcedDefeat = InForcedDefeat;
	UE_LOG(LogSixInches, Display, TEXT("[SixInches][Autoplay] Started%s"), ForcedDefeat.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (forced defeat: %s)"), *ForcedDefeat));
}

void USIAutoplayComponent::StopAutoplay()
{
	bRunning = false;
}

AActor* USIAutoplayComponent::PickTarget(const FVector& P, float Speed) const
{
	const USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
	if (!Air)
	{
		return nullptr;
	}
	// If letting one more through still wins, don't risk the plane in our own flak for it.
	const AGameModeBase* GM = GetWorld()->GetAuthGameMode();
	const USIMatchComponent* Match = GM ? GM->FindComponentByClass<USIMatchComponent>() : nullptr;
	int32 Untipped = 0;
	const TArray<AActor*> InAir = Air->GetBombs();
	for (AActor* B : InAir)
	{
		const ISITippable* T = Cast<ISITippable>(B);
		Untipped += T && T->IsTippable() ? 1 : 0;
	}
	const int32 Unlaunched = Match ? FMath::Max(0, Match->GetStats().Total - Match->GetResolved() - InAir.Num()) : 0;
	// Every bomb still to deal with could get through and we'd still win: no need to risk our own flak.
	const bool bCanAffordMiss = Match && Match->GetStats().Hits + Untipped + Unlaunched < Match->GetHitsToLose();
	const float Margin = bCanAffordMiss ? -300.0f : FlakMargin;
	AActor* Best = nullptr;
	AActor* Fallback = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	float FallbackToTown = -1.0f;
	const AController* Ctrl = Cast<AController>(GetOwner());
	const APawn* Me = Ctrl ? Ctrl->GetPawn() : nullptr;
	for (AActor* B : Air->GetBombs())
	{
		const ISITippable* T = Cast<ISITippable>(B);
		if (!T || !T->IsTippable())
		{
			continue;
		}
		// A squadron mate is already on it, or its lane is a hunter's.
		if (const AActor* Claimer = Air->GetClaimer(B))
		{
			if (Claimer != Me)
			{
				continue;
			}
		}
		else if (Air->IsLaneCovered(B->GetActorLocation().Y))
		{
			continue;
		}
		const FSIWingFrame W = T->GetWingFrame();
		const float ToTown = (Air->TownLineX - W.Location.X) / SI_CM / FMath::Max(W.Speed, 1.0f);
		if (W.Location.X > Air->FlakStartX - 40000.0f && W.Forward.X > 0.0f)  // a final-wave bomb heading out is fine
		{
			continue;  // into the flak belt: let it go rather than lose the plane
		}
		const float Rel = (W.Location.X - P.X) / SI_CM;
		// Ahead: we close at ~35 m/s with a dive; behind: we slow and let it come up to us. Crossing to its lane
		// happens at the same time, so the slower of the two sets the catch time.
		const float Along = Rel > 0.0f ? Rel / 32.0f : -Rel / 110.0f;
		const float Lateral = FMath::Abs(W.Location.Y - P.Y) / SI_CM / 110.0f;
		const float Catch = FMath::Max(Along, Lateral) + 4.0f;
		// Where we'd be holding it: never plan a catch inside our own flak belt.
		if (W.Location.X + W.Forward.X * W.Speed * SI_CM * (Catch + 2.0f) > Air->FlakStartX + Margin * SI_CM)
		{
			continue;
		}
		if (ToTown > FallbackToTown)
		{
			FallbackToTown = ToTown;
			Fallback = B;
		}
		if (Catch + 3.0f > ToTown)
		{
			continue;
		}
		// Most urgent first, weighted by how long it takes to get there.
		// Bombs launched for us (claimed at launch) first: the squadron is not coming for those.
		const float Score = ToTown + 0.8f * Catch - (Air->GetClaimer(B) == Me ? 30.0f : 0.0f);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = B;
		}
	}
	// Nothing catchable: head home rather than chase into the flak (a bomb through costs less than the plane).
	return Best;
}

FSIFlightInput USIAutoplayComponent::FlyTo(const FVector& P, const FRotator& Rot, const FVector& Aim, float DesiredSpeed, float Speed, float Dt)
{
	FSIFlightInput In;
	const FVector D = Aim - P;
	const float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	const float Horiz = FVector2D(D.X, D.Y).Size();
	const float DesiredPitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(D.Z, FMath::Max(Horiz, 1.0f))), -50.0f, 35.0f);
	const float YawErr = FMath::FindDeltaAngleDegrees(Rot.Yaw, DesiredYaw);
	const float Bank = FMath::Clamp(YawErr * 2.2f, -70.0f, 70.0f);
	In.Roll = FMath::Clamp(Bank / 75.0f, -1.0f, 1.0f);
	In.Pitch = FMath::Clamp((DesiredPitch - Rot.Pitch) / 9.0f, -1.0f, 1.0f);
	In.bBrake = Speed > DesiredSpeed + 6.0f;
	In.bBoost = Speed < DesiredSpeed - 12.0f;
	return In;
}

void USIAutoplayComponent::Throttle(FSIFlightInput& In, USIFlightComponent* Flight, float DesiredSpeed, float Speed)
{
	// User, 2026-11-14: "feels as if the plane is trying to brake and still trying to go forward". The engine always
	// pulls toward full speed, so brake-at-+2 m/s chattered on and off. Hold the speed on the engine instead.
	const float Excess = Speed - DesiredSpeed;
	bBotBraking = bBotBraking ? Excess > BrakeExcessOff : Excess > BrakeExcessOn;
	In.bBrake = bBotBraking;
	In.bBoost = !bBotBraking && Excess < -12.0f;
	if (Flight && !In.bBrake && !In.bBoost)
	{
		Flight->SetPaceAssist(DesiredSpeed);
	}
}

FSIFlightInput USIAutoplayComponent::HoldSlot(float Dt)
{
	FSIFlightInput In;
	const AController* Owner = Cast<AController>(GetOwner());
	const APawn* Pawn = Owner ? Owner->GetPawn() : nullptr;
	USIWingtipComponent* Wt = Pawn ? Pawn->FindComponentByClass<USIWingtipComponent>() : nullptr;
	const USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
	if (!Wt || !Air)
	{
		return In;
	}
	const AActor* Bomb = Wt->GetTarget();
	const float BombX = Bomb ? Bomb->GetActorLocation().X : 0.0f;
	// Tip over open ground; while the preview reads houses, sit just outside the band until it clears
	// (but don't follow it into the flak waiting for a field that won't come).
	const bool bUnsafe = Wt->HasPreview() && Wt->GetPreviewClass().bHit;
	const bool bTooDeep = BombX > Air->FlakStartX - 15000.0f;
	const float TargetGap = (bUnsafe && !bTooDeep) || !Wt->HasPreview() ? HoverGap : Wt->BandGapIdeal;
	// Never slide inboard while level with or above its wing: get below first, then in (no wing strikes).
	const float TargetOverlap = Wt->GetGapCm() < 1.0f && Wt->GetOverlap() < 0.2f ? -0.8f : Wt->BandOverlapIdeal;
	const float Gap = Wt->GetGapCm();
	const float Overlap = Wt->GetOverlap();
	const float GapRate = bHaveLast ? (Gap - LastGap) / FMath::Max(Dt, 0.001f) : 0.0f;
	const float OvRate = bHaveLast ? (Overlap - LastOverlap) / FMath::Max(Dt, 0.001f) : 0.0f;
	LastGap = Gap;
	LastOverlap = Overlap;
	bHaveLast = true;
	// Pitch +1 = ease up (shrinks the gap). Roll toward the target deepens the overlap.
	In.Pitch = FMath::Clamp((Gap - TargetGap) / 9.0f + GapRate * 0.06f, -1.0f, 1.0f);
	In.Roll = -Wt->GetSide() * FMath::Clamp((TargetOverlap - Overlap) / 0.35f - OvRate * 0.5f, -1.0f, 1.0f);
	return In;
}

void USIAutoplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AController* PC = Cast<AController>(GetOwner());
	if (!bRunning || !PC)
	{
		return;
	}
	// Take-over: a real key or mouse move hands the plane to the player until they go idle again.
	if (const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		if (!Flow->IsBotDriving())
		{
			return;
		}
	}
	APawn* Pawn = PC->GetPawn();
	USIFlightComponent* Flight = Pawn ? Pawn->FindComponentByClass<USIFlightComponent>() : nullptr;
	USIWingtipComponent* Wt = Pawn ? Pawn->FindComponentByClass<USIWingtipComponent>() : nullptr;
	if (!Flight || !Wt || Flight->IsLocked() || Flight->IsCrashed())
	{
		return;
	}
	const float Dt = FMath::Max(DeltaTime, 0.001f);
	const FVector P = Pawn->GetActorLocation();
	const FRotator Rot = Flight->GetFlightRotation();
	const float Speed = Flight->GetSpeed();
	FSIFlightInput In;

	// QA: deliberately lose so the Defeat screen can be checked.
	if (ForcedDefeat.Equals(TEXT("Ground"), ESearchCase::IgnoreCase))
	{
		In.Pitch = -1.0f;
		OnBotInput.Broadcast(In);
		return;
	}
	if (ForcedDefeat.Equals(TEXT("Town"), ESearchCase::IgnoreCase))
	{
		In.Pitch = FMath::Clamp((5.0f - Rot.Pitch) / 9.0f, -1.0f, 1.0f);
		OnBotInput.Broadcast(In);
		return;
	}

	if (Wt->IsInSlot())
	{
		bHaveLast = bHaveLast && Target.Get() == Wt->GetTarget();
		Target = Wt->GetTarget();
		In = HoldSlot(Dt);
		// Wake balance: tap Q / E like a pilot, leading the roll rate, with a small dead zone around level.
		const float Lead = Wt->GetBalance() + TrimLead * Wt->GetBalanceRate();
		In.Trim = FMath::Abs(Lead) > TrimDeadband ? -FMath::Sign(Lead) : 0.0f;
		bBotBraking = false;
		bOrbiting = false;
		const USIHealthComponent* H = Pawn->FindComponentByClass<USIHealthComponent>();
		if (H && H->GetHealth01() < 0.5f && P.X > GetWorld()->GetSubsystem<USIAirspaceSubsystem>()->FlakStartX - 10000.0f)
		{
			In.bBrake = true;  // break off before the flak finishes us
		}
	}
	else
	{
		bHaveLast = false;
		TargetTime += Dt;
		AActor* T = Target.Get();
		const ISITippable* Tip = Cast<ISITippable>(T);
		USIAirspaceSubsystem* AirClaims = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
		// Lost it to a squadron mate (they got there first): pick again.
		const bool bTaken = T && AirClaims && AirClaims->GetClaimer(T) && AirClaims->GetClaimer(T) != Pawn;
		if (!Tip || !Tip->IsTippable() || TargetTime > RetargetSeconds || bTaken)
		{
			AActor* NewTarget = PickTarget(P, Speed);
			if (AirClaims)
			{
				if (T && T != NewTarget) { AirClaims->ReleaseBomb(T, Pawn); }
				if (NewTarget) { AirClaims->ClaimBomb(NewTarget, Pawn); }
			}
			if (NewTarget != T)
			{
				TargetTime = 0.0f;
				if (NewTarget)
				{
					UE_LOG(LogSixInches, Display, TEXT("[SixInches][Autoplay] Target %s"), *NewTarget->GetName());
				}
			}
			if (NewTarget != Target.Get() && NewTarget)
			{
				// Pick a side once per target (the one we're already on) and keep it, or the slot flips as we cross.
				if (const ISITippable* NT = Cast<ISITippable>(NewTarget))
				{
					const FSIWingFrame NW = NT->GetWingFrame();
					const FVector NR = FVector::CrossProduct(FVector::UpVector, FRotator(0.0f, NW.Forward.Rotation().Yaw, 0.0f).Vector()).GetSafeNormal();
					TargetSide = FVector::DotProduct(P - NW.Location, NR) >= 0.0f ? 1.0f : -1.0f;
				}
			}
			Target = NewTarget;
			T = NewTarget;
			Tip = Cast<ISITippable>(T);
		}
		if (Tip)
		{
			const FSIWingFrame W = Tip->GetWingFrame();
			const FVector F = FRotator(0.0f, W.Forward.Rotation().Yaw, 0.0f).Vector();
			const FVector R = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
			const float Side = TargetSide;
			const FVector BombTip = Side > 0.0f ? W.TipRight : W.TipLeft;
			// Slot point: our root such that our inner tip sits a little outboard and below the bomb's tip.
			const FVector Slot = BombTip + R * Side * (Wt->TipRightLocal.Y + 300.0f) - FVector::UpVector * 150.0f;
				const float Along = FVector::DotProduct(Slot - P, F);
				const float AlongM = Along / SI_CM;
				const float LatErrM = FVector::DotProduct(Slot - P, R) / SI_CM;   // + = slot is to our right of its track
				const USIAirspaceSubsystem* AirHere = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
				const bool bTooDeep = AirHere && P.X > AirHere->FlakStartX - 60000.0f;
				float DesiredSpeed = W.Speed;
				{
					// Track following: fly the bomb's heading, bent toward the slot line by a capped sideways speed
					// (no overshoot), at a height that glides down onto the slot, at a speed set by the along-track gap.
					if (AlongM < 0.0f)
					{
						// It's behind: slow while it is far, back up to its speed as it closes so it arrives level.
						// Constant-deceleration profile: close fast, arrive matched.
						// User, 2026-11-14: "the plane weirdly slowing down while following" (it sat at 42 m/s, under stall).
						DesiredSpeed = FMath::Max(W.Speed - FMath::Min(FMath::Sqrt(2.0f * ClosingAccel * -AlongM) + 4.0f, 80.0f), FMath::Min(FollowMinSpeed, W.Speed));
					}
					else
					{
						DesiredSpeed = W.Speed + FMath::Min(FMath::Sqrt(2.0f * ClosingAccel * AlongM) + 4.0f, 70.0f);
					}
					const float LatSpeed = FMath::Clamp(LatErrM * 0.6f, -70.0f, 70.0f);
					if (FMath::Abs(AlongM) < 400.0f && FMath::Abs(LatErrM) < 300.0f)
					{
						TargetTime = 0.0f;  // closing in: don't give up on it
					}
					const float Offset = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(LatSpeed / FMath::Max(Speed, 40.0f), -0.6f, 0.6f)));
					// Far ahead of it: weave across its track (S-turns) so we hold position while it comes up to us.
					WeaveTime += Dt;
					const bool bWeave = AlongM < -WeaveStart;
					// Near our own guns, weave wider than 90 degrees so the net drift is back toward the coast.
					const float Angle = bTooDeep ? WeaveAngleDeep : WeaveAngle;
					const float Weave = bWeave ? Angle * (FMath::Fmod(WeaveTime, 2.0f * WeavePeriod) < WeavePeriod ? 1.0f : -1.0f) : 0.0f;
					if (!bWeave)
					{
						WeaveTime = 0.0f;
					}
					const float DesiredYaw = F.Rotation().Yaw + Offset + Weave;
					// Level with the slot well before entry: arriving metres above cost 3-4 s per bomb dropping into the band.
					const float Above = FMath::Clamp((FMath::Abs(AlongM) - RunInLevelDistance) * 0.25f, 0.0f, ApproachAbove + 60.0f);
					const float DesiredZ = Slot.Z / SI_CM + Above;
					const float DesiredPitch = FMath::Clamp((DesiredZ - P.Z / SI_CM) * 0.18f - Flight->GetVelocity().Z / SI_CM * 0.05f, -30.0f, 22.0f);
					const float YawErr = FMath::FindDeltaAngleDegrees(Rot.Yaw, DesiredYaw);
					In.Roll = FMath::Clamp(YawErr * 6.0f / 75.0f, -1.0f, 1.0f);
					In.Pitch = FMath::Clamp((DesiredPitch - Rot.Pitch) / 8.0f, -1.0f, 1.0f);
					// Bomb far behind: hold position in a steady circle (turning outboard of its track) instead of crawling
					// forward toward the guns; roll out with room for the normal run-in to line up.
					bOrbiting = bOrbiting ? AlongM < -OrbitStart : AlongM < -(OrbitStart + 400.0f);
					if (bOrbiting)
					{
						In.Roll = OrbitRoll * Side;
						WeaveTime = 0.0f;
					}
					Throttle(In, Flight, DesiredSpeed, Speed);
				}
			DebugAlong = Along / SI_CM;
			DebugDesired = DesiredSpeed;
			DebugTarget = W.Location;
		}
		else
		{
			// Nothing catchable: climb back toward the coast for height.
			// Deep over the town: slow right down and turn hard (small radius) to get out of the guns' reach fast.
			const USIAirspaceSubsystem* AirHome = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
			const bool bDeep = AirHome && P.X > AirHome->FlakStartX - 30000.0f;
			const FVector Aim(FMath::Max(P.X - 150000.0f, -100000.0f), P.Y * 0.5f, 60000.0f);
			const float HomeSpeed = bDeep && FMath::Abs(FMath::FindDeltaAngleDegrees(Rot.Yaw, 180.0f)) > 45.0f ? 85.0f : 165.0f;
			In = FlyTo(P, Rot, Aim, HomeSpeed, Speed, Dt);
			Throttle(In, Flight, HomeSpeed, Speed);
			DebugDesired = HomeSpeed;
		}

		// Stay clear of cables and the ground.
		if (const USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>())
		{
			const FVector Ahead = P + Rot.Vector() * 30000.0f;
			if (Air->DistanceToNearestCable(Ahead) < 6000.0f || Air->DistanceToNearestCable(P) < 8000.0f)
			{
				In.Roll = FMath::Clamp(In.Roll + 0.8f, -1.0f, 1.0f);
				In.Pitch = 1.0f;
			}
		}
		if (Flight->GetHeightAboveGround() < MinHeightAboveGround && Rot.Pitch < 8.0f)
		{
			In.Pitch = 1.0f;
			In.Roll *= 0.3f;
		}
	}

	LogTimer -= Dt;
	if (LogTimer <= 0.0f)
	{
		LogTimer = 1.0f;
		UE_LOG(LogSixInches, Display, TEXT("[SixInches][Autoplay] P=(%.0f,%.0f,%.0f) yaw=%.0f pit=%.0f spd=%.0f want=%.0f | T=(%.0f,%.0f,%.0f) along=%.0f | slot=%d gap=%.0f ov=%.2f prog=%.2f bal=%+.2f in=(%.2f,%.2f,%d,%d)"),
			P.X / SI_CM, P.Y / SI_CM, P.Z / SI_CM, Rot.Yaw, Rot.Pitch, Speed, DebugDesired, DebugTarget.X / SI_CM, DebugTarget.Y / SI_CM, DebugTarget.Z / SI_CM, DebugAlong,
			Wt->IsInSlot() ? 1 : 0, Wt->GetGapCm(), Wt->GetOverlap(), Wt->GetProgress(), Wt->GetBalance(), In.Pitch, In.Roll, In.bBoost ? 1 : 0, In.bBrake ? 1 : 0);
	}
	// Free flight: ease the stick like a pilot (no instant full-deflection flips); the slot keeps raw corrections.
	if (!Wt->IsInSlot() && InputSmoothing > 0.0f)
	{
		Smoothed.Pitch = FMath::FInterpTo(Smoothed.Pitch, In.Pitch, Dt, InputSmoothing);
		Smoothed.Roll = FMath::FInterpTo(Smoothed.Roll, In.Roll, Dt, InputSmoothing);
		In.Pitch = Smoothed.Pitch;
		In.Roll = Smoothed.Roll;
	}
	else
	{
		// In the slot the hold controller corrects constantly; ease it too so the plane doesn't twitch.
		Smoothed.Pitch = FMath::FInterpTo(Smoothed.Pitch, In.Pitch, Dt, SlotInputSmoothing);
		Smoothed.Roll = FMath::FInterpTo(Smoothed.Roll, In.Roll, Dt, SlotInputSmoothing);
		In.Pitch = Smoothed.Pitch;
		In.Roll = Smoothed.Roll;
	}
	OnBotInput.Broadcast(In);
}
