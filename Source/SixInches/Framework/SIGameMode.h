// Six Inches - game mode: owns the match and the wave, wires the plane's components to them, and handles
// -autoplay plus the QA switches (-SIShots=, -SIEventShots, -SIExit, -SIRuns=, -SIDefeat=, -SIDumpAudio=, -SISeed=).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/SITypes.h"
#include "SIGameMode.generated.h"

class USIMatchComponent;
class USIWaveDirectorComponent;
class USIMatchConfig;

UCLASS()
class SIXINCHES_API ASIGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASIGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	TObjectPtr<USIMatchComponent> Match;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	TObjectPtr<USIWaveDirectorComponent> Wave;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TSoftObjectPtr<USIMatchConfig> Config;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay")
	bool bAutoplayActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float KillZ = -2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float StartSpeed = 160.0f;

	/** Each bomb stopped makes the air rougher: escalation = 1 + this x stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float EscalationPerStop = 0.12f; // was 0.35: by the last bombs the air was ~4x rougher and the end read as stutter (user 2026-11-17)
	/** Ceiling on the escalation multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MaxEscalation = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float AutoplayRestartDelay = 4.0f;

	/** Hunters join from behind this much faster than the player (m/s), so they fly past in view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float WingmanJoinOvertake = 45.0f;

	/** Epic scalability on launch (-1 keeps the user's settings). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	int32 ScalabilityLevel = 3;

	UFUNCTION(BlueprintCallable, Category = "Match")
	void RequestRestart();

	bool IsAutoplay() const { return bAutoplayActive; }
	FString GetForcedDefeat() const { return ForcedDefeat; }

private:
	void CheckCommandLineAutoplay();
	void SetupAutoplayController();
	void ScheduleQAScreenshots();
	void EventShot(const TCHAR* Tag);
	void WirePawn(APawn* Pawn);
	void SetPlaneActive(bool bActive, bool bEnded);
	void WriteTelemetry(bool bVictory) const;

	UFUNCTION() void HandleState(ESIMatchState NewState, ESIMatchState OldState);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandleBombLanded(const FSIBombResult& Result);
	UFUNCTION() void HandleMatchEnded(bool bVictory, ESIDefeatReason Reason, const FText& ReasonText);
	/** End screen: freeze the planes, bombs, smoke and guns where they are (user 2026-11-17: "pause the plane and everything on winning screen"). */
	void FreezeWorldForEnding();
	UPROPERTY(EditAnywhere, Category = "Tuning")
	bool bFreezeOnEnd = true;
	UFUNCTION() void HandleDepleted(ESIDamageSource Source);
	UFUNCTION() void HandleCrashed(ESIDamageSource Source);
	UFUNCTION() void HandleSlot(bool bInSlot, AActor* Target);
	UFUNCTION() void HandlePreview(FSIGroundClass Predicted);
	UFUNCTION() void HandleTipped(AActor* Bomb, FSIGroundClass Predicted);
	UFUNCTION() void HandleDamaged(float Amount, ESIDamageSource Source, float HealthAfter);
	UFUNCTION() void HandleBand(bool bInBand);
	/** FactoryKit hands over after the start screen and intro: start the 3-2-1 countdown. */
	UFUNCTION() void HandleFlowStart();
	UFUNCTION() void HandleWingmanTipped(FName Callsign, AActor* Bomb, FSIGroundClass Predicted);
	class ASIWingman* SpawnWingman(const FSIWingmanSpec& Spec, const FVector& Location, const FRotator& Rotation, float SpeedMS);

	void LosePlane(ESIDamageSource Source);
	void AutoplayRunOver();

	UPROPERTY() TObjectPtr<USIMatchConfig> LoadedConfig;
	TWeakObjectPtr<APawn> WiredPawn;
	FString ForcedDefeat;
	FString ShotTag;
	bool bEventShots = false;
	float LastEventShot = -10.0f;
	int32 EventShotCount = 0;
	bool bPlaneLost = false;
	bool bFlowBegun = false;
	bool bPerfLog = false;
	double PerfLast = 0.0;
	double PerfWindowStart = 0.0;
	int32 PerfFrames = 0;
	int32 PerfOver33 = 0;
	float PerfSum = 0.0f;
	float PerfMax = 0.0f;
	bool bCountdownStarted = false;
	FTimerHandle AutoplayEndTimer;
};
