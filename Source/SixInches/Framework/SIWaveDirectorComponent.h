// Six Inches - launches the wave from DT_SIWave rows and reports where each bomb came down.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SITypes.h"
#include "SIWaveDirectorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSIOnBombLaunched, AActor*, Bomb, int32, Index);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnWaveBomb, const FSIBombResult&, Result);

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIWaveDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIWaveDirectorComponent();

	/** The actor class launched for each row (must carry a USITippableComponent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TSubclassOf<AActor> BombClass;

	// ---- Near-player launches (user, 2026-11-16: "the game pacing is actually slow and most planes are uncatchable").
	// Rows in the player's lanes (|SpawnY| <= PlayerLaneHalfWidth) launch a short way behind the player, run past and settle
	// just ahead; the next one goes as soon as the last is dealt with. Far-lane rows (the squadron's) keep their table timing.
	/** Off restores the fixed table (every bomb from its SpawnX at its SpawnTime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	bool bLaunchNearPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float PlayerLaneHalfWidth = 700.0f;

	/** Only this many player-lane rows launch near the player; later ones keep the table (each bomb stopped carries the
	 *  player ~1.3 km toward the guns, so a 5th launched beside them starts inside the flak belt). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	int32 MaxNearBombs = 4;

	/** Final wave (user, 2026-11-17: "when the last 20-30 seconds are left the plane just turns and keep going ... wasted
	 *  gameplay"): this many extra bombs launch near the player from FinalWaveStart (s), one after another. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	int32 FinalBombs = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float FinalWaveStart = 38.0f;

	/** Final-wave bombs wait for the last one to be dealt with, at most this long (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float FinalMaxWait = 24.0f;

	/** Final-wave bombs fly the player's heading and dive after this many metres if not tipped (counts as a home hit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float FinalDiveAfter = 2600.0f;

	/** Final-wave launch distance ahead of the player (m) and cruise speed (m/s, below the fighter's so it is caught quickly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float FinalAhead = 120.0f; // was 250: the bot spent ~15 s catching bomb 11

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float FinalSpeed = 100.0f; // was 110

	/** Table launch times of the other rows are scaled by this (quicker round: the last far bomb used to land at ~87 s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float TableTimeScale = 0.7f;

	/** Launch distance behind the player (m) and sideways offset (m, alternating sides). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float LaunchBehind = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float LaunchSide = 70.0f;

	/** Catch-up: this much faster than the player (m/s) until CatchUpAhead m ahead, at most CatchUpSeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float CatchUpExtra = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float CatchUpAhead = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float CatchUpSeconds = 16.0f;

	/** Launch height: the player's height minus this (m), within the band below. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float LaunchBelowPlayer = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	FVector2D LaunchAltitudeRange = FVector2D(300.0f, 460.0f);

	/** Never launch nearer the flak belt than this (m), so there is time to tip it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float MinRunwayToFlak = 3200.0f;

	/** Near-player bombs: at least this long apart (s), and the next waits at most this long for the last to be dealt with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float NearMinGap = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|NearPlayer")
	float NearMaxWait = 9.0f;

	UPROPERTY(BlueprintAssignable, Category = "Wave") FSIOnBombLaunched OnBombLaunched;
	UPROPERTY(BlueprintAssignable, Category = "Wave") FSIOnWaveBomb OnBombLanded;

	/** Takes the table rows and appends the final wave (FinalBombs near-player bombs). GetTotal counts both. */
	void SetRows(const TArray<FSIWaveRow>& InRows);
	int32 GetTotal() const { return Rows.Num(); }
	void SetRunning(bool bInRunning) { bRunning = bInRunning; }
	float GetElapsed() const { return Elapsed; }
	void SetTownLineX(float X) { TownLineX = X; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Launch(int32 Index);
	bool IsNearRow(int32 Index) const;
	bool NearBombBusy() const;
	UFUNCTION() void HandleImpact(FVector Location, bool bTipped);

	TArray<FSIWaveRow> Rows;
	TMap<TWeakObjectPtr<AActor>, int32> Live;
	int32 Next = 0;
	TArray<bool> Launched;
	TWeakObjectPtr<AActor> LastNear;
	float LastNearTime = -100.0f;
	int32 NearCount = 0;
	int32 BaseRowCount = 0;
	float Elapsed = 0.0f;
	bool bRunning = false;
	float TownLineX = 645000.0f;
};
