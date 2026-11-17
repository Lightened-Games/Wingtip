// Six Inches - wave director.

#include "Framework/SIWaveDirectorComponent.h"
#include "Components/SITippableComponent.h"
#include "World/SIAirspaceSubsystem.h"
#include "World/SIFlyingBomb.h"  // default BombClass only
#include "SixInches.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

USIWaveDirectorComponent::USIWaveDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	BombClass = ASIFlyingBomb::StaticClass();
}

void USIWaveDirectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRunning)
	{
		return;
	}
	Elapsed += DeltaTime;
	if (Launched.Num() != Rows.Num())
	{
		Launched.Init(false, Rows.Num());
	}
	bool bNearTried = false;
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		if (Launched[i])
		{
			continue;
		}
		if (!IsNearRow(i))
		{
			if (Rows[i].SpawnTime * (bLaunchNearPlayer ? TableTimeScale : 1.0f) <= Elapsed)
			{
				Launched[i] = true;
				Launch(i);
			}
			continue;
		}
		// Near-player rows go one after another, paced by the player: the next leaves once the last is tipped or down
		// (at least NearMinGap apart), or after NearMaxWait regardless. The first one keeps its table time.
		if (bNearTried)
		{
			continue;
		}
		bNearTried = true;
		const bool bFirst = LastNearTime < -50.0f;
		const float Since = Elapsed - LastNearTime;
		const bool bFinalWait = i >= BaseRowCount && Elapsed < FinalWaveStart;
		const bool bGo = !bFinalWait && (bFirst ? Rows[i].SpawnTime <= Elapsed : (Since >= NearMinGap && (!NearBombBusy() || Since >= (i >= BaseRowCount ? FinalMaxWait : NearMaxWait))));
		if (bGo)
		{
			Launched[i] = true;
			LastNearTime = Elapsed;
			Launch(i);
		}
	}
	Next = Launched.FilterByPredicate([](bool b) { return b; }).Num();
}

void USIWaveDirectorComponent::SetRows(const TArray<FSIWaveRow>& InRows)
{
	Rows = InRows;
	BaseRowCount = Rows.Num();
	if (bLaunchNearPlayer)
	{
		for (int32 k = 0; k < FinalBombs; ++k)
		{
			FSIWaveRow R;
			R.SpawnTime = FinalWaveStart;
			R.SpawnX = -650.0f;
			R.SpawnY = 0.0f;
			R.Altitude = 360.0f;
			R.TargetY = 0.0f;
			R.Speed = FinalSpeed;
			Rows.Add(R);
		}
	}
	Launched.Reset();
}

bool USIWaveDirectorComponent::IsNearRow(int32 Index) const
{
	if (bLaunchNearPlayer && Index >= BaseRowCount && Rows.IsValidIndex(Index))
	{
		return true;  // final wave
	}
	if (!bLaunchNearPlayer || !Rows.IsValidIndex(Index) || FMath::Abs(Rows[Index].SpawnY) > PlayerLaneHalfWidth)
	{
		return false;
	}
	int32 Before = 0;  // player-lane rows ahead of this one in the table
	for (int32 i = 0; i < Index; ++i) { if (FMath::Abs(Rows[i].SpawnY) <= PlayerLaneHalfWidth) { ++Before; } }
	return Before < MaxNearBombs;
}

bool USIWaveDirectorComponent::NearBombBusy() const
{
	const AActor* A = LastNear.Get();
	const USITippableComponent* T = A ? A->FindComponentByClass<USITippableComponent>() : nullptr;
	return T && T->GetState() == ESIAutopilotState::Cruise;
}

void USIWaveDirectorComponent::Launch(int32 Index)
{
	UWorld* World = GetWorld();
	if (!World || !BombClass)
	{
		return;
	}
	const FSIWaveRow& R = Rows[Index];
	FVector Start(R.SpawnX * SI_CM, R.SpawnY * SI_CM, R.Altitude * SI_CM);
	float AimY = R.TargetY * SI_CM;
	APawn* Player = IsNearRow(Index) ? UGameplayStatics::GetPlayerPawn(this, 0) : nullptr;
	if (Player)
	{
		// A short way behind the player, alternating sides, at about their height; never too close to the flak belt.
		const USIAirspaceSubsystem* Air = World->GetSubsystem<USIAirspaceSubsystem>();
		const float FlakX = Air ? Air->FlakStartX : 500000.0f;
		const FVector P = Player->GetActorLocation();
		const float Side = (NearCount++ % 2 == 0) ? 1.0f : -1.0f;
		Start.X = FMath::Min(FMath::Max(P.X - LaunchBehind * SI_CM, R.SpawnX * SI_CM), FlakX - MinRunwayToFlak * SI_CM);
		Start.Y = P.Y + Side * LaunchSide * SI_CM;
		Start.Z = FMath::Clamp(P.Z / SI_CM - LaunchBelowPlayer, LaunchAltitudeRange.X, LaunchAltitudeRange.Y) * SI_CM;
		AimY = Start.Y * 0.6f + AimY * 0.4f;
	}
	FVector Aim(TownLineX + 60000.0f, AimY, Start.Z);
	if (Player && Index >= BaseRowCount)
	{
		// Final wave: it flies the player's way (usually back toward the coast, away from the town), launched just behind
		// them so it overtakes, and comes down after FinalDiveAfter metres if nobody tips it.
		const FVector P = Player->GetActorLocation();
		FVector Fwd = Player->GetVelocity().GetSafeNormal2D();
		if (Fwd.IsNearlyZero()) { Fwd = Player->GetActorForwardVector().GetSafeNormal2D(); }
		// Never toward the guns: an eastward heading loses its east part, so it crosses ahead of the player instead.
		if (Fwd.X > 0.0f)
		{
			Fwd.X = 0.0f;
			Fwd = Fwd.GetSafeNormal();
			if (Fwd.IsNearlyZero()) { Fwd = FVector(0.0f, P.Y > 0.0f ? -1.0f : 1.0f, 0.0f); }
		}
		// Out toward the map edge it would leave the playable area (and the player turns away from the edge): cross inward.
		// Turning it straight back made the player U-turn and chase (about 22 s, and the round often ran out): head it west
		// and a little inward instead, away from the guns, a 90 degree turn for the player.
		if (FMath::Abs(P.Y) > 120000.0f && Fwd.Y * P.Y > 0.0f)
		{
			Fwd = FVector(-1.0f, P.Y > 0.0f ? -0.35f : 0.35f, 0.0f).GetSafeNormal();
		}
		const FVector Right(-Fwd.Y, Fwd.X, 0.0f);
		const float Side = (NearCount % 2 == 0) ? 1.0f : -1.0f;
		// Just ahead of the player and slower than them (no catch-up): a few seconds to draw level, whatever they were doing.
		Start = P + Fwd * FinalAhead * SI_CM + Right * Side * LaunchSide * SI_CM;
		Start.Z = FMath::Clamp(P.Z / SI_CM - LaunchBelowPlayer, LaunchAltitudeRange.X, LaunchAltitudeRange.Y) * SI_CM;
		Aim = Start + Fwd * 1.0e6f;
		Aim.Z = Start.Z;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AActor* Bomb = World->SpawnActor<AActor>(BombClass, FTransform((Aim - Start).Rotation(), Start), Params);
	if (!Bomb)
	{
		return;
	}
	if (USITippableComponent* T = Bomb->FindComponentByClass<USITippableComponent>())
	{
		T->Index = Index + 1;
	}
	Bomb->FinishSpawning(FTransform((Aim - Start).Rotation(), Start));
	if (USITippableComponent* T = Bomb->FindComponentByClass<USITippableComponent>())
	{
		T->Launch(Aim, R.Speed, 1944 + Index * 17);
		if (Player)
		{
			if (Index >= BaseRowCount) { T->SetDiveAfter(FinalDiveAfter); }
			else { T->SetCatchUp(Player, CatchUpExtra, CatchUpAhead, CatchUpSeconds); }
			LastNear = Bomb;
			// It is the player's: the squadron's hunters leave it alone (they skipped none of it before, but the
			// player's own pick treated a hunter-covered centre lane as taken).
			if (USIAirspaceSubsystem* Air = World->GetSubsystem<USIAirspaceSubsystem>())
			{
				Air->ClaimBomb(Bomb, Player);
			}
		}
		T->OnImpact.AddDynamic(this, &USIWaveDirectorComponent::HandleImpact);
	}
	Live.Add(Bomb, Index + 1);
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Bomb %d launched at t=%.1f from %s"), Index + 1, Elapsed, *Start.ToString());
	if (Player) { UE_LOG(LogSixInches, Display, TEXT("[SixInches] Bomb %d launched near the player (catch-up)"), Index + 1); }
	if (Player) { UE_LOG(LogSixInches, Display, TEXT("[SixInches] Bomb %d launched near the player (catch-up)"), Index + 1); }
	OnBombLaunched.Broadcast(Bomb, Index + 1);
}

void USIWaveDirectorComponent::HandleImpact(FVector Location, bool bTipped)
{
	// Find which bomb it was (the one whose autopilot just finished).
	int32 Index = 0;
	for (auto It = Live.CreateIterator(); It; ++It)
	{
		const AActor* A = It.Key().Get();
		const USITippableComponent* T = A ? A->FindComponentByClass<USITippableComponent>() : nullptr;
		if (!A || (T && T->GetState() == ESIAutopilotState::Done))
		{
			Index = It.Value();
			It.RemoveCurrent();
			break;
		}
	}
	FSIBombResult Result;
	Result.Location = Location;
	Result.bTipped = bTipped;
	Result.BombIndex = Index;
	if (const USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>())
	{
		Result.Ground = Air->Classify(Location);
	}
	if (!bTipped)
	{
		Result.Ground.Kind = ESIZoneKind::Town;
		Result.Ground.bHit = true;
	}
	OnBombLanded.Broadcast(Result);
}
