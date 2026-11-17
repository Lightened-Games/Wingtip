// Six Inches - the shared airspace: ground zones, live bombs, turbulence columns, balloon cables.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/SITypes.h"
#include "SIAirspaceSubsystem.generated.h"

USTRUCT()
struct FSITurbulence
{
	GENERATED_BODY()

	FVector Center = FVector::ZeroVector;
	float Radius = 15000.0f;
	float Height = 60000.0f;
	float BornTime = 0.0f;
	float Life = 25.0f;
	float Strength = 1.0f;
};

USTRUCT()
struct FSICable
{
	GENERATED_BODY()

	FVector Top = FVector::ZeroVector;
	FVector Bottom = FVector::ZeroVector;
};

UCLASS()
class SIXINCHES_API USIAirspaceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---- ground zones
	void SetZones(const TArray<FSIZoneRow>& InZones) { Zones = InZones; }
	FSIGroundClass Classify(const FVector& Location) const;
	/** Ground height (cm) under an XY point by a downward trace against the world (terrain, buildings). */
	float GetGroundHeight(const FVector& Location, bool bTerrainOnly = false) const;

	// ---- bombs
	void RegisterBomb(AActor* Bomb);
	void UnregisterBomb(AActor* Bomb);
	TArray<AActor*> GetBombs() const;

	/** Who is going for which bomb (the bot, a wingman), so two aircraft never chase or lift the same one. */
	void ClaimBomb(AActor* Bomb, AActor* By);
	void ReleaseBomb(AActor* Bomb, AActor* By);
	void ReleaseAllClaims(AActor* By);
	AActor* GetClaimer(const AActor* Bomb) const;
	/** Friendly aircraft the HUD may label (squadron hunters). Callsign = the actor's second tag. */
	void RegisterAircraft(AActor* A) { Aircraft.AddUnique(A); }
	TArray<AActor*> GetAircraft() const
	{
		TArray<AActor*> Out;
		for (const TWeakObjectPtr<AActor>& A : Aircraft) { if (A.IsValid()) { Out.Add(A.Get()); } }
		return Out;
	}

	/** A squadron hunter covers bombs whose Y lies in [MinY, MaxY] (cm); the player leaves those to it. */
	void CoverLane(AActor* By, float MinY, float MaxY) { Lanes.Add(By, FVector2D(MinY, MaxY)); }
	bool IsLaneCovered(float Y, const AActor* Except = nullptr) const
	{
		for (const TPair<TWeakObjectPtr<AActor>, FVector2D>& L : Lanes)
		{
			if (L.Key.IsValid() && L.Key.Get() != Except && Y >= L.Value.X && Y <= L.Value.Y) { return true; }
		}
		return false;
	}

	// ---- turbulence
	void AddTurbulence(const FVector& Center, float Radius, float Height, float Life, float Strength);
	/** 0..~1 intensity of hot air / smoke turbulence at a point (fades over each column's life). */
	float SampleTurbulence(const FVector& Location) const;
	/** Escalation multiplier for every bump (grows with each bomb stopped). */
	void SetEscalation(float Value) { Escalation = Value; }
	float GetEscalation() const { return Escalation; }

	// ---- cables
	void RegisterCable(const FVector& Top, const FVector& Bottom);
	float DistanceToNearestCable(const FVector& Location) const;

	// ---- landmarks (cm), set by the game mode from the match config
	float TownLineX = 645000.0f;
	float FlakStartX = 500000.0f;

	/** Guns only fire during active play. */
	bool bHostilitiesActive = false;

private:
	TArray<FSIZoneRow> Zones;
	TArray<TWeakObjectPtr<AActor>> Bombs;
	TMap<TWeakObjectPtr<const AActor>, TWeakObjectPtr<AActor>> Claims;
	TMap<TWeakObjectPtr<AActor>, FVector2D> Lanes;
	TArray<TWeakObjectPtr<AActor>> Aircraft;
	TArray<FSITurbulence> Turbulence;
	TArray<FSICable> Cables;
	float Escalation = 1.0f;
};
