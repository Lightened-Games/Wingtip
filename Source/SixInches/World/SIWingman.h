// Six Inches - an AI fighter of the squadron. Hunters join the player, then catch and tip flying bombs in the lanes the
// player is not working (through ISITippable, exactly like the player's wingtip); patrols fly a standing circuit.
// Kinematic flight: turn-rate-limited heading, banked into turns, speed eased; a smooth slide into the slot.
// Never enters the friendly flak belt; yields a bomb the player is lifting. Tuning provisional until confirmed (C8).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/SITypes.h"
#include "SIWingman.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UAudioComponent;
class USILoopSynth;
class USIAirspaceSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSIOnWingmanTipped, FName, Callsign, AActor*, Bomb, FSIGroundClass, Predicted);

UCLASS()
class SIXINCHES_API ASIWingman : public AActor
{
	GENERATED_BODY()

public:
	ASIWingman();

	void Configure(const FSIWingmanSpec& InSpec, float StartSpeedMS);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FVector GetVelocity() const override;

	const FSIWingmanSpec& GetSpec() const { return Spec; }
	bool IsHolding() const { return Mode == EMode::Hold; }
	FText GetCallsignText() const;

	UPROPERTY(BlueprintAssignable, Category = "Wingman")
	FSIOnWingmanTipped OnTipped;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wingman")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wingman")
	TObjectPtr<UStaticMeshComponent> PropDisc;

	// ---- tuning (m, s, deg)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MaxTurnRate = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float BankPerTurnRate = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MaxBank = 62.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float Accel = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MinSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MaxSpeed = 195.0f;

	/** Seconds of steady lift to tip a bomb (the player's best is TipTimeBest on the wingtip component). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HoldSeconds = 2.4f;

	/** Within this along/lateral distance of the slot the run-in becomes a slide into position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SlotCaptureDistance = 45.0f;

	/** Slide-in rate onto the slot (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SlideRate = 1.3f;

	/** Lift only counts once this close to the slot point (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float InSlotTolerance = 1.2f;

	/** Wingtip under the bomb's: overlap (m) and gap (m), same band as the player's ideal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SlotOverlap = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SlotGap = 0.30f;

	/** Stay this far short of the flak belt (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FlakKeepOut = 350.0f;

	/** Don't start a catch that cannot finish before the belt: needs this much runway (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MinRunway = 900.0f;

	/** Leave bombs this close to the player to the player (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float PlayerYieldDistance = 650.0f;

	/** Keep this far from the player's plane while manoeuvring (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float PlayerSeparation = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float BreakSeconds = 4.0f;

	/** After joining, fly formation on the player this long before hunting (the join reads on camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float JoinFormationSeconds = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SearchInterval = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float PropSpinDegPerSec = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float EngineVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float EngineFalloff = 700.0f;

private:
	enum class EMode : uint8 { Escort, Hunt, Hold, Break, Patrol };

	void FlyToward(const FVector& Aim, float DesiredSpeed, float Dt);
	void TickEscort(float Dt);
	void TickHunt(float Dt);
	void TickHold(float Dt);
	void TickBreak(float Dt);
	void TickPatrol(float Dt);
	AActor* FindBomb() const;
	FVector SlotPoint(const FSIWingFrame& W) const;
	void Release(bool bBreak);
	APawn* Player() const;
	USIAirspaceSubsystem* Air() const;
	FVector KeepClearOfPlayer(const FVector& Aim) const;

	FSIWingmanSpec Spec;
	EMode Mode = EMode::Escort;
	TWeakObjectPtr<AActor> Target;
	float Side = 1.0f;
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	float Roll = 0.0f;
	float Speed = 140.0f;
	float HoldTime = 0.0f;
	float ModeTime = 0.0f;
	float SearchTimer = 0.0f;
	float PatrolAngle = 0.0f;
	float Age = 0.0f;
	FVector HalfSpanTip = FVector(47.0f, 625.0f, 25.7f);

	UPROPERTY() TObjectPtr<UAudioComponent> EngineAudio;
	UPROPERTY() TObjectPtr<USILoopSynth> EngineSynth;
	TSoftObjectPtr<UStaticMesh> BodyMesh;
	TSoftObjectPtr<UStaticMesh> PropMesh;
};
