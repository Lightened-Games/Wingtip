// Six Inches - a simple autopilot that holds course and height until its wing is lifted long enough, then rolls over
// and dives. Attach to any flying actor to make it a tippable target. Tuning provisional until confirmed.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SITippableComponent.generated.h"

class USIAirspaceSubsystem;

UENUM(BlueprintType)
enum class ESIAutopilotState : uint8
{
	Cruise,
	Tipped,
	TownDive,
	Done
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSIOnTargetImpact, FVector, Location, bool, bTipped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnAutopilotState, ESIAutopilotState, NewState);

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USITippableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USITippableComponent();

	/** Visual roll at full lift (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MaxLiftRoll = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TipRollTarget = 125.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TipRollRate = 75.0f;

	/** Nose-down rate after the roll (deg/s) and the steepest dive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TipPitchRate = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TipMaxDive = 55.0f;

	/** Turn toward the low wing while diving (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TipYawRate = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TownDivePitchRate = 30.0f;

	/** Autopilot hunting: small height and heading wander (m, deg, s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HuntAltitude = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HuntYaw = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HuntPeriod = 6.0f;

	/** Heading correction toward the target (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float CourseGain = 0.25f;

	UPROPERTY(BlueprintAssignable, Category = "Autopilot")
	FSIOnTargetImpact OnImpact;

	/** Position in the wave (1-based), set by whoever launches it. */
	UPROPERTY(BlueprintReadOnly, Category = "Autopilot")
	int32 Index = 0;

	UPROPERTY(BlueprintAssignable, Category = "Autopilot")
	FSIOnAutopilotState OnStateChanged;

	/** Start flying from the owner's location toward Target (cm) at SpeedMS. */
	void Launch(const FVector& Target, float SpeedMS, int32 Seed);
	void SetLiftProgress(float Progress01, float LiftedSide);
	void Tip(float LiftedSide);
	void Bump(const FVector& Velocity) { BumpVel += Velocity; }
	FVector PredictTipImpact(float LiftedSide) const;

	ESIAutopilotState GetState() const { return State; }
	float GetSpeed() const { return Speed; }

	/** Launched behind the player: fly ExtraMS faster than them until AheadM metres ahead (or MaxSeconds), then ease to cruise. */
	void SetCatchUp(AActor* InPlayer, float ExtraMS, float AheadM, float MaxSeconds);

	/** Final-wave bombs fly the player's way, not at the town: after this many metres they dive (a home hit if not tipped). */
	void SetDiveAfter(float Meters) { DiveAfter = Meters; }
	float GetLift() const { return Lift; }
	FRotator GetFlightRotation() const { return Rot; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetState(ESIAutopilotState NewState);
	USIAirspaceSubsystem* Airspace() const;

	ESIAutopilotState State = ESIAutopilotState::Cruise;
	FVector TargetPoint = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	float Speed = 140.0f;
	float CruiseSpeed = 140.0f;
	TWeakObjectPtr<AActor> CatchUpPlayer;
	float CatchUpExtra = 0.0f;
	float CatchUpAhead = 0.0f;
	float CatchUpLeft = 0.0f;
	float DiveAfter = 0.0f;
	FVector LaunchPoint = FVector::ZeroVector;
	float BaseAltitude = 35000.0f;
	float Lift = 0.0f;
	float LiftSide = 1.0f;
	float TipSide = 1.0f;
	float Age = 0.0f;
	float PhaseA = 0.0f;
	float PhaseB = 0.0f;
	FVector BumpVel = FVector::ZeroVector;
	float GroundTimer = 0.0f;
	float GroundZ = 0.0f;
	bool bLaunched = false;
};
