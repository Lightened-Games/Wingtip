// Six Inches - shared enums, data-table rows and event payloads.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SITypes.generated.h"

UENUM(BlueprintType)
enum class ESIMatchState : uint8
{
	Ready,
	Active,
	Victory,
	Defeat
};

UENUM(BlueprintType)
enum class ESIDefeatReason : uint8
{
	None,
	TownHit,
	Flak,
	Cable,
	WingTorn,
	Ground,
	TimeUp
};

UENUM(BlueprintType)
enum class ESIZoneKind : uint8
{
	Field,
	River,
	Woods,
	Sea,
	Farm,
	Village,
	Church,
	Town
};

UENUM(BlueprintType)
enum class ESIDamageSource : uint8
{
	Contact,
	Flak,
	Cable,
	Ground
};

UENUM(BlueprintType)
enum class ESISlotState : uint8
{
	Free,
	Slot
};

/** One flying bomb of the wave (metres, seconds, metres per second). */
/** What an AI fighter in the squadron does. */
UENUM(BlueprintType)
enum class ESIWingmanRole : uint8
{
	/** Joins the player, then catches and tips bombs in the lanes the player is not working. */
	Hunter,
	/** Flies a standing patrol circuit (sky life; never touches bombs). */
	Patrol
};

/** One AI fighter (DA_SIMatchConfig Squadron). Metres and seconds; provisional until confirmed. */
USTRUCT(BlueprintType)
struct FSIWingmanSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	FName Callsign = TEXT("Blue 2");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	ESIWingmanRole Role = ESIWingmanRole::Hunter;

	/** Hunters: seconds after GO before joining. Patrols are airborne from the start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	float JoinDelay = 6.0f;

	/** Hunters: where it appears relative to the player when joining (m; X ahead, Y right, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	FVector EntryOffset = FVector(-600.0f, -500.0f, 60.0f);

	/** Hunters: station kept off the player's wing while no bomb is free (m, player frame). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	FVector FormationOffset = FVector(220.0f, -320.0f, 40.0f);

	/** Hunters: only bombs whose current Y lies in this band (m, world) are theirs; the middle lanes are the player's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	float LaneMinY = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	float LaneMaxY = 4000.0f;

	/** Patrols: circuit centre (m, world) and radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	FVector PatrolCenter = FVector(2500.0f, -2500.0f, 750.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	float PatrolRadius = 1400.0f;

	/** Cruise speed when not chasing (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	float CruiseSpeed = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wingman")
	int32 Seed = 7;
};

USTRUCT(BlueprintType)
struct FSIWaveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float SpawnTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float SpawnX = -900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float SpawnY = 0.0f;

	/** Cruise height above sea level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float Altitude = 350.0f;

	/** Lateral aim point at the town line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float TargetY = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	float Speed = 140.0f;
};

/** Ground classification shape (centimetres): a circle when Radius > 0, otherwise an oriented rectangle. */
USTRUCT(BlueprintType)
struct FSIZoneRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	ESIZoneKind Kind = ESIZoneKind::Field;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	float X = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	float Y = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	float HalfX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	float HalfY = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	float Radius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	float Yaw = 0.0f;
};

/** One placed prop (centimetres, degrees). Mesh is a key resolved by the world builder (SM_<Mesh>). */
USTRUCT(BlueprintType)
struct FSIPropRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	FName Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	float X = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	float Y = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	float Z = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	float Yaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	float ScaleX = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	float ScaleY = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop")
	float ScaleZ = 1.0f;
};

/** What lies under a point on the ground, and whether a bomb landing there counts against the player. */
USTRUCT(BlueprintType)
struct FSIGroundClass
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ground")
	ESIZoneKind Kind = ESIZoneKind::Field;

	UPROPERTY(BlueprintReadOnly, Category = "Ground")
	bool bHit = false;
};

USTRUCT(BlueprintType)
struct FSIBombResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Bomb")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Bomb")
	FSIGroundClass Ground;

	/** True when the player tipped it; false when it reached the town on its own. */
	UPROPERTY(BlueprintReadOnly, Category = "Bomb")
	bool bTipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Bomb")
	int32 BombIndex = 0;
};

USTRUCT(BlueprintType)
struct FSIFlightInput
{
	GENERATED_BODY()

	/** +1 = nose up (S), -1 = nose down (W). In slot mode: ease up / down. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float Pitch = 0.0f;

	/** +1 = bank right (D), -1 = bank left (A). In slot mode: slide right / left. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float Roll = 0.0f;

	/** Wake trim in slot mode: +1 = roll right (E), -1 = roll left (Q). Counters the bomb's wake on the balance meter. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	float Trim = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bBoost = false;

	/** Airbrake in free flight, break off in slot mode. */
	UPROPERTY(BlueprintReadWrite, Category = "Input")
	bool bBrake = false;
};

/** World-space wing geometry of a tippable target. */
USTRUCT(BlueprintType)
struct FSIWingFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wing")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wing")
	FVector Forward = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wing")
	FVector Right = FVector::RightVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wing")
	FVector Up = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wing")
	FVector TipLeft = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Wing")
	FVector TipRight = FVector::ZeroVector;

	/** Metres per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Wing")
	float Speed = 0.0f;
};

USTRUCT(BlueprintType)
struct FSIMatchStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Total = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Stopped = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 InFields = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 Hits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float Elapsed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	float DamageTaken = 0.0f;
};

namespace SIZones
{
	inline bool CountsAsHit(ESIZoneKind Kind)
	{
		return Kind == ESIZoneKind::Farm || Kind == ESIZoneKind::Village || Kind == ESIZoneKind::Church || Kind == ESIZoneKind::Town;
	}

	inline FText Label(ESIZoneKind Kind)
	{
		switch (Kind)
		{
		case ESIZoneKind::River:   return NSLOCTEXT("SixInches", "ZoneRiver", "RIVER");
		case ESIZoneKind::Woods:   return NSLOCTEXT("SixInches", "ZoneWoods", "WOODS");
		case ESIZoneKind::Sea:     return NSLOCTEXT("SixInches", "ZoneSea", "SEA");
		case ESIZoneKind::Farm:    return NSLOCTEXT("SixInches", "ZoneFarm", "FARM");
		case ESIZoneKind::Village: return NSLOCTEXT("SixInches", "ZoneVillage", "VILLAGE");
		case ESIZoneKind::Church:  return NSLOCTEXT("SixInches", "ZoneChurch", "CHURCH");
		case ESIZoneKind::Town:    return NSLOCTEXT("SixInches", "ZoneTown", "TOWN");
		default:                   return NSLOCTEXT("SixInches", "ZoneField", "FIELD");
		}
	}
}
