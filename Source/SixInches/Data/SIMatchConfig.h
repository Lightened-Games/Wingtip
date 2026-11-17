// Six Inches - match configuration data asset (DA_SIMatchConfig). All values provisional until confirmed.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/SITypes.h"
#include "SIMatchConfig.generated.h"

class UDataTable;

UCLASS(BlueprintType)
class SIXINCHES_API USIMatchConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "10"))
	float RoundSeconds = 95.0f; // was 90: autoplay finished at 85-90 s with no margin under load (user 2026-11-17: "fix the autoplay completely")

	/** 0 = no 3-2-1: play starts the moment the menu hands over (user 2026-11-17: "start game right after main menu dont wait for countdown"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "0"))
	int32 CountdownSeconds = 0;

	/** Bombs landing on buildings or reaching the town before the round is lost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "1"))
	int32 HitsToLose = 2;

	/** A bomb that crosses this X (cm) untipped dives into the town. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TownLineX = 645000.0f;

	/** Friendly anti-aircraft fire starts here (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FlakStartX = 500000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	int32 Seed = 1944;

	/** Rows of FSIWaveRow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TObjectPtr<UDataTable> WaveTable;

	/** The rest of the squadron: hunters that take the far lanes, patrols that fill the sky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TArray<FSIWingmanSpec> Squadron;

	USIMatchConfig()
	{
		FSIWingmanSpec Blue2;
		Blue2.Callsign = TEXT("Blue 2");
		Blue2.JoinDelay = 4.0f;
		Blue2.EntryOffset = FVector(-160.0f, 110.0f, 20.0f);
		Blue2.FormationOffset = FVector(300.0f, 85.0f, 22.0f);
		Blue2.Seed = 21;
		FSIWingmanSpec Blue3 = Blue2;
		Blue3.Callsign = TEXT("Blue 3");
		Blue3.JoinDelay = 12.0f;
		Blue3.EntryOffset = FVector(-170.0f, -120.0f, 28.0f);
		Blue3.FormationOffset = FVector(330.0f, -95.0f, 30.0f);
		Blue3.LaneMinY = -4000.0f;
		Blue3.LaneMaxY = -450.0f;  // the left pair too (2026-11-13: the player's 4th catch landed at the town edge)
		Blue3.Seed = 33;
		FSIWingmanSpec Blue4 = Blue2;
		Blue4.Callsign = TEXT("Blue 4");
		Blue4.JoinDelay = 10.0f;  // 20 s joined too late for the 10-bomb wave (2026-11-16: broke off bomb 7 at the flak)
		Blue4.EntryOffset = FVector(-180.0f, 130.0f, 35.0f);
		Blue4.FormationOffset = FVector(280.0f, 120.0f, 40.0f);
		Blue4.Seed = 39;
		FSIWingmanSpec Blue5 = Blue3;
		Blue5.Callsign = TEXT("Blue 5");
		Blue5.JoinDelay = 8.0f;
		Blue5.EntryOffset = FVector(-190.0f, -140.0f, 40.0f);
		Blue5.FormationOffset = FVector(260.0f, -130.0f, 45.0f);
		Blue5.Seed = 51;
		// Blue 6: joins late and works the centre lanes, taking only bombs the player hasn't claimed (busier wave).
		FSIWingmanSpec Blue6 = Blue2;
		Blue6.Callsign = TEXT("Blue 6");
		// 30 s left bomb 9 to a frame-timing race with the flak belt (lost some first rounds); 25 s gives it slack.
		Blue6.JoinDelay = 20.0f;
		Blue6.EntryOffset = FVector(-200.0f, 150.0f, 50.0f);
		Blue6.FormationOffset = FVector(240.0f, 150.0f, 55.0f);
		Blue6.LaneMinY = -450.0f;
		Blue6.LaneMaxY = 800.0f;
		Blue6.Seed = 63;
		FSIWingmanSpec Red1;
		Red1.Callsign = TEXT("Red 1");
		Red1.Role = ESIWingmanRole::Patrol;
		Red1.PatrolCenter = FVector(1500.0f, -900.0f, 620.0f);
		Red1.PatrolRadius = 900.0f;
		Red1.Seed = 45;
		FSIWingmanSpec Red2 = Red1;
		Red2.Callsign = TEXT("Red 2");
		Red2.PatrolCenter = FVector(3600.0f, 900.0f, 700.0f);
		Red2.PatrolRadius = 1100.0f;
		Red2.CruiseSpeed = 150.0f;
		Red2.Seed = 57;
		Squadron = { Blue2, Blue5, Blue3, Blue4, Blue6, Red1, Red2 };
	}
};
