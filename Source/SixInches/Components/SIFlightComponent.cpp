// Six Inches - kinematic arcade flight.

#include "Components/SIFlightComponent.h"
#include "World/SIAirspaceSubsystem.h"
#include "SixInches.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"

USIFlightComponent::USIFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USIFlightComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Rot = Owner->GetActorRotation();
	}
}

USIAirspaceSubsystem* USIFlightComponent::Airspace() const
{
	return GetWorld() ? GetWorld()->GetSubsystem<USIAirspaceSubsystem>() : nullptr;
}

void USIFlightComponent::Initialise(const FVector& Location, const FRotator& Rotation, float SpeedMS)
{
	Rot = Rotation;
	Speed = SpeedMS;
	bCrashed = false;
	bFormation = false;
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocationAndRotation(Location, Rot);
	}
	Velocity = Rot.Vector() * Speed * SI_CM;
}

void USIFlightComponent::SetFormation(bool bOn)
{
	bFormation = bOn;
}

void USIFlightComponent::SetFormationTarget(const FVector& Location, const FRotator& Rotation, float SpeedMS)
{
	FormationLoc = Location;
	FormationRot = Rotation;
	FormationSpeed = SpeedMS;
}

void USIFlightComponent::BreakAway(float ClimbDegrees, float BankDegrees)
{
	bFormation = false;
	Rot.Pitch = FMath::Clamp(Rot.Pitch + ClimbDegrees, -MaxPitch, MaxPitch);
	Rot.Roll = BankDegrees;
}

float USIFlightComponent::GetBoostAvailable01() const
{
	if (BoostTime > 0.0f)
	{
		return BoostTime / FMath::Max(BoostSeconds, 0.01f);
	}
	return 1.0f - BoostCool / FMath::Max(BoostCooldown, 0.01f);
}

void USIFlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!Owner || bCrashed || DeltaTime <= 0.0f)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	NoiseTime += Dt;
	const FVector OldLoc = Owner->GetActorLocation();
	USIAirspaceSubsystem* Air = Airspace();

	// Ground height under the plane (HUD altitude + low-flying checks), refreshed 10x a second.
	GroundTimer -= Dt;
	if (GroundTimer <= 0.0f && Air)
	{
		GroundTimer = 0.1f;
		GroundZ = Air->GetGroundHeight(OldLoc);
	}
	Altitude = OldLoc.Z / SI_CM;
	HeightAboveGround = (OldLoc.Z - GroundZ) / SI_CM;

	if (bLocked)
	{
		Velocity = FVector::ZeroVector;
		Owner->SetActorRotation(Rot);
		return;
	}

	// Turbulence intensity: smoke/hot-air columns plus an ambient gust that grows with escalation.
	TurbIntensity = Air ? Air->SampleTurbulence(OldLoc) * Air->GetEscalation() : 0.0f;
	BuffetCooldown -= Dt;
	if (TurbIntensity > 0.05f && BuffetCooldown <= 0.0f)
	{
		const float Kick = FMath::Abs(FMath::PerlinNoise1D(NoiseTime * 2.3f + 11.0f)) * TurbIntensity;
		if (Kick > BuffetThreshold * 0.5f)
		{
			OnBuffet.Broadcast(FMath::Clamp(Kick * 2.0f, 0.2f, 1.5f));
			BuffetCooldown = 0.35f;
		}
	}

	FVector NewLoc = OldLoc;
	if (bFormation)
	{
		TickFormation(Dt, NewLoc);
	}
	else
	{
		TickFree(Dt, NewLoc);
		KeepClearOfBombs(NewLoc, Dt);
	}

	ESIDamageSource Source = ESIDamageSource::Ground;
	if (SweepFor(OldLoc, NewLoc, Source))
	{
		bCrashed = true;
		Owner->SetActorLocation(NewLoc);
		Velocity = FVector::ZeroVector;
		UE_LOG(LogSixInches, Display, TEXT("[SixInches] Plane crashed (%d) at %s"), static_cast<int32>(Source), *NewLoc.ToString());
		OnCrashed.Broadcast(Source);
		return;
	}
	if (Air && Air->DistanceToNearestCable(NewLoc) < CableRadius * SI_CM)
	{
		bCrashed = true;
		Owner->SetActorLocation(NewLoc);
		Velocity = FVector::ZeroVector;
		OnCrashed.Broadcast(ESIDamageSource::Cable);
		return;
	}

	Velocity = (NewLoc - OldLoc) / Dt;
	Owner->SetActorLocationAndRotation(NewLoc, Rot);
}

namespace
{
	struct FClearSample { FVector P; float R; };

	/** Sample points (bomb space, cm) with radii covering the flying bomb: fuselage, wings, pulse-jet duct, tailplane. */
	const TArray<FClearSample>& BombSamples()
	{
		static TArray<FClearSample> S;
		if (S.Num() == 0)
		{
			for (float X = -400.0f; X <= 430.0f; X += 50.0f) { S.Add({ FVector(X, 0.0f, 0.0f), 42.0f }); }
			for (float Y = 50.0f; Y <= 266.0f; Y += 36.0f)
			{
				for (const float X : { 66.0f, 20.0f, -30.0f })
				{
					S.Add({ FVector(X, Y, -6.0f), 12.0f });
					S.Add({ FVector(X, -Y, -6.0f), 12.0f });
				}
			}
			for (float X = -475.0f; X <= -45.0f; X += 45.0f) { S.Add({ FVector(X, 0.0f, 78.0f), 33.0f }); }
			for (float Y = 20.0f; Y <= 108.0f; Y += 30.0f) { S.Add({ FVector(-360.0f, Y, 9.0f), 10.0f }); S.Add({ FVector(-360.0f, -Y, 9.0f), 10.0f }); }
			S.Add({ FVector(-350.0f, 0.0f, 45.0f), 30.0f }); // fin
		}
		return S;
	}

	/** Same for another fighter (squadron): fuselage axis and the dihedral wing. */
	const TArray<FClearSample>& FighterSamples()
	{
		static TArray<FClearSample> S;
		if (S.Num() == 0)
		{
			for (float X = -470.0f; X <= 540.0f; X += 50.0f) { S.Add({ FVector(X, 0.0f, 10.0f), 55.0f }); }
			for (float Y = 60.0f; Y <= 620.0f; Y += 40.0f)
			{
				const float Z = -40.0f + Y * 0.105f;
				for (const float X : { 100.0f, 10.0f, -80.0f })
				{
					S.Add({ FVector(X, Y, Z), 16.0f });
					S.Add({ FVector(X, -Y, Z), 16.0f });
				}
			}
			for (float Y = 20.0f; Y <= 220.0f; Y += 40.0f) { S.Add({ FVector(-420.0f, Y, 30.0f), 12.0f }); S.Add({ FVector(-420.0f, -Y, 30.0f), 12.0f }); }
		}
		return S;
	}

	/** Push (our space) needed to clear one sphere from our fuselage and wing; zero if clear. */
	FVector ClearPush(const FVector& L, float R)
	{
		FVector Best = FVector::ZeroVector;
		float BestLen = 0.0f;
		auto Keep = [&](const FVector& V) { if (V.SizeSquared() > BestLen * BestLen) { Best = V; BestLen = V.Size(); } };
		// Fuselage: a box round the body (half width 54, z -58..78, nose to tail).
		if (L.X > -490.0f - R && L.X < 546.0f + R)
		{
			const float PenY = 54.0f + R - FMath::Abs(L.Y);
			const float ZC = 10.0f;
			const float PenZ = 68.0f + R - FMath::Abs(L.Z - ZC);
			if (PenY > 0.0f && PenZ > 0.0f)
			{
				// Move us the short way: sideways or up/down.
				Keep(PenY < PenZ ? FVector(0.0f, -FMath::Sign(L.Y) * PenY, 0.0f) : FVector(0.0f, 0.0f, -FMath::Sign(L.Z - ZC) * PenZ));
			}
		}
		// Wing: elliptical planform, 6 degree dihedral from z -40 at the root, thickness 14.5 % at the root.
		const float AY = FMath::Abs(L.Y);
		if (AY < 625.0f + R)
		{
			const float YC = FMath::Min(AY, 624.0f);
			const float Chord = 232.0f * FMath::Sqrt(FMath::Max(1.0f - FMath::Square(YC / 640.0f), 0.0f));
			const float LE = 62.0f + 0.25f * Chord;
			if (L.X < LE + R && L.X > LE - Chord - R)
			{
				const float ZW = -40.0f + YC * 0.105f;
				const float Half = 0.5f * Chord * (0.145f - 0.055f * YC / 625.0f) + 4.0f;
				const float Pen = Half + R - FMath::Abs(L.Z - ZW);
				if (Pen > 0.0f)
				{
					Keep(FVector(0.0f, 0.0f, -FMath::Sign(L.Z - ZW) * Pen));
				}
			}
		}
		return Best;
	}
}

void USIFlightComponent::KeepClearOfBombs(FVector& NewLoc, float Dt)
{
	ClearLogCool -= Dt;
	USIAirspaceSubsystem* Air = GetWorld() ? GetWorld()->GetSubsystem<USIAirspaceSubsystem>() : nullptr;
	if (!bKeepClearOfBombs || !Air)
	{
		return;
	}
	const FQuat Q = Rot.Quaternion();
	TArray<TPair<AActor*, const TArray<FClearSample>*>> Others;
	for (AActor* B : Air->GetBombs()) { Others.Add({ B, &BombSamples() }); }
	for (AActor* A : Air->GetAircraft()) { if (A != GetOwner()) { Others.Add({ A, &FighterSamples() }); } }
	for (const TPair<AActor*, const TArray<FClearSample>*>& O : Others)
	{
		AActor* B = O.Key;
		if (!B || B->IsHidden() || FVector::DistSquared(B->GetActorLocation(), NewLoc) > FMath::Square(1500.0f))
		{
			continue;
		}
		const FTransform BT = B->GetActorTransform();
		// A few passes: the deepest point first, then whatever is left.
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			FVector Push = FVector::ZeroVector;
			for (const FClearSample& S : *O.Value)
			{
				const FVector L = Q.UnrotateVector(BT.TransformPosition(S.P) - NewLoc);
				const FVector P = ClearPush(L, S.R + ClearanceMargin);
				if (P.SizeSquared() > Push.SizeSquared()) { Push = P; }
			}
			if (Push.IsNearlyZero(0.5f))
			{
				break;
			}
			NewLoc += Q.RotateVector(Push);
			if (ClearLogCool <= 0.0f)
			{
				ClearLogCool = 1.0f;
				UE_LOG(LogSixInches, Display, TEXT("[SixInches] Kept clear of %s by %.0f cm"), *B->GetName(), Push.Size());
			}
		}
	}
}

void USIFlightComponent::TickFree(float Dt, FVector& NewLoc)
{
	if (bAutoLevel)
	{
		Input = FSIFlightInput();
		Input.Pitch = FMath::Clamp((4.0f - Rot.Pitch) / 10.0f, -1.0f, 1.0f);
	}
	// Boost and airbrake edges.
	if (Input.bBoost && !bBoostHeld && BoostTime <= 0.0f && BoostCool <= 0.0f)
	{
		BoostTime = BoostSeconds;
		OnBoost.Broadcast(true);
	}
	bBoostHeld = Input.bBoost;
	if (BoostTime > 0.0f)
	{
		BoostTime -= Dt;
		if (BoostTime <= 0.0f || !Input.bBoost)
		{
			BoostTime = 0.0f;
			BoostCool = BoostCooldown;
			OnBoost.Broadcast(false);
		}
	}
	else
	{
		BoostCool = FMath::Max(0.0f, BoostCool - Dt);
	}
	if (Input.bBrake != bBrakeHeld)
	{
		bBrakeHeld = Input.bBrake;
		OnBrake.Broadcast(bBrakeHeld);
	}

	// Roll toward the commanded bank; level out when the stick is centred.
	const float TargetRoll = FMath::Clamp(Input.Roll, -1.0f, 1.0f) * MaxBank;
	const float RollStep = RollRate * (FMath::Abs(Input.Roll) > 0.05f ? 1.0f : AutoLevelFraction) * Dt;
	Rot.Roll = FMath::Clamp(TargetRoll, Rot.Roll - RollStep, Rot.Roll + RollStep);

	// Pitch is held where you put it; banked hard, the nose follows the turn a little.
	Rot.Pitch = FMath::Clamp(Rot.Pitch + FMath::Clamp(Input.Pitch, -1.0f, 1.0f) * PitchRate * Dt, -MaxPitch, MaxPitch);
	if (Speed < MinSpeed)
	{
		Rot.Pitch = FMath::Max(-MaxPitch, Rot.Pitch - StallNoseDrop * Dt);
	}

	// Coordinated turn: yaw rate = g tan(bank) / v.
	const float Bank = FMath::DegreesToRadians(FMath::Clamp(Rot.Roll, -80.0f, 80.0f));
	const float YawRate = TurnFactor * Gravity * FMath::Tan(Bank) / FMath::Max(Speed, 30.0f);
	Rot.Yaw += FMath::RadiansToDegrees(YawRate) * Dt;

	// Soft bounds: steer back toward the playable box before the BlockingVolumes.
	AActor* Owner = GetOwner();
	const FVector P = Owner->GetActorLocation();
	FVector Push = FVector::ZeroVector;
	if (P.Y > BoundsMax.Y - SoftBoundsMargin) { Push.Y = -1.0f; }
	if (P.Y < BoundsMin.Y + SoftBoundsMargin) { Push.Y = 1.0f; }
	if (P.X > BoundsMax.X - SoftBoundsMargin) { Push.X = -1.0f; }
	if (P.X < BoundsMin.X + SoftBoundsMargin) { Push.X = 1.0f; }
	if (!Push.IsNearlyZero())
	{
		const float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(Push.Y, Push.X));
		const float Err = FMath::FindDeltaAngleDegrees(Rot.Yaw, DesiredYaw);
		Rot.Yaw += FMath::Clamp(Err, -30.0f * Dt, 30.0f * Dt);
	}
	if (P.Z > BoundsMax.Z - SoftBoundsMargin * 0.5f)
	{
		Rot.Pitch = FMath::Min(Rot.Pitch, Rot.Pitch - 30.0f * Dt);
	}

	// Energy: thrust pulls toward the level maximum, gravity trades height for speed.
	float Target = LevelMaxSpeed + (BoostTime > 0.0f ? BoostAdd : 0.0f);
	const float SinPitch = FMath::Sin(FMath::DegreesToRadians(Rot.Pitch));
	float Accel = ThrustResponse * (Target - Speed) - Gravity * SinPitch;
	// Pace assist (user, 2026-11-13: "control is so hard"): near a bomb with no throttle key held, the engine
	// holds the closing speed for you, so you only steer.
	if (PaceAssistSpeed > 0.0f && !Input.bBrake && !Input.bBoost && BoostTime <= 0.0f)
	{
		Target = PaceAssistSpeed;
		Accel = FMath::Clamp((Target - Speed) * PaceAssistResponse, -PaceAssistMaxDecel, PaceAssistMaxAccel) - Gravity * SinPitch * 0.3f;
	}
	PaceAssistSpeed = 0.0f;  // set again each frame by whoever owns the assist
	if (Input.bBrake)
	{
		Accel -= AirbrakeDecel;
	}
	Speed = FMath::Clamp(Speed + Accel * Dt, MinSpeed * 0.75f, DiveMaxSpeed);

	// Turbulence: roll/pitch knocks and a vertical bump.
	FVector Bump = FVector::ZeroVector;
	if (TurbIntensity > 0.01f)
	{
		Rot.Roll += TurbulenceRoll * TurbIntensity * FMath::PerlinNoise1D(NoiseTime * 1.9f + 3.0f) * Dt * 6.0f;
		Rot.Pitch += TurbulencePitch * TurbIntensity * FMath::PerlinNoise1D(NoiseTime * 1.4f + 7.0f) * Dt * 6.0f;
		Bump.Z = TurbulenceLift * TurbIntensity * FMath::PerlinNoise1D(NoiseTime * 1.1f + 19.0f) * SI_CM * Dt;
	}

	const FRotator Heading(Rot.Pitch, Rot.Yaw, 0.0f);
	NewLoc = P + Heading.Vector() * Speed * SI_CM * Dt + Bump;
	NewLoc.X = FMath::Clamp(NewLoc.X, BoundsMin.X, BoundsMax.X);
	NewLoc.Y = FMath::Clamp(NewLoc.Y, BoundsMin.Y, BoundsMax.Y);
	NewLoc.Z = FMath::Min(NewLoc.Z, BoundsMax.Z);
}

void USIFlightComponent::TickFormation(float Dt, FVector& NewLoc)
{
	if (bBoostHeld || BoostTime > 0.0f)
	{
		BoostTime = 0.0f;
		bBoostHeld = false;
		BoostCool = FMath::Max(BoostCool, 0.0f);
		OnBoost.Broadcast(false);
	}
	Speed = FMath::FInterpTo(Speed, FormationSpeed, Dt, 2.5f);
	NewLoc = FormationLoc;
	Rot = FormationRot;
}

bool USIFlightComponent::SweepFor(const FVector& From, FVector& To, ESIDamageSource& OutSource)
{
	UWorld* World = GetWorld();
	if (!World || From.Equals(To, 0.01f))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SIFlightSweep), false, GetOwner());
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(Hits, From, To, FQuat::Identity, Objects, FCollisionShape::MakeSphere(SweepRadius * SI_CM), Params);
	for (const FHitResult& Hit : Hits)
	{
		const AActor* A = Hit.GetActor();
		if (!Hit.bBlockingHit && !Hit.bStartPenetrating)
		{
			continue;
		}
		if (A && A->ActorHasTag(TEXT("NoCollide")))
		{
			continue;
		}
		if (A && A->ActorHasTag(TEXT("SI.Bounds")))
		{
			// Hard edge: stop at the volume, turn around.
			To = Hit.Location;
			Rot.Yaw += 180.0f;
			return false;
		}
		To = Hit.Location;
		OutSource = (A && A->ActorHasTag(TEXT("SI.Balloon"))) ? ESIDamageSource::Cable : ESIDamageSource::Ground;
		return true;
	}
	return false;
}
