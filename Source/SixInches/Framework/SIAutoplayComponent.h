// Six Inches - autoplay bot (-autoplay). Drives the plane through the same input path as a player: picks the most
// urgent bomb it can catch, dives from above and behind, matches speed, holds the wingtip in the band with a PD
// controller, hovers just outside the band while the impact preview reads houses, and tips over open ground.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SITypes.h"
#include "SIAutoplayComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnBotInput, const FSIFlightInput&, Input);

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIAutoplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIAutoplayComponent();

	/** The bot's stick each frame; the controller routes it exactly like a player's keys. */
	UPROPERTY(BlueprintAssignable, Category = "Autoplay")
	FSIOnBotInput OnBotInput;

	void StartAutoplay(const FString& InForcedDefeat);
	void StopAutoplay();
	bool IsRunning() const { return bRunning; }

	/** Approach geometry (m): dive in from this far behind and above the slot point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ApproachBehind = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ApproachAbove = 90.0f;

	/** Along-track distance (m) inside which the approach is flown at slot height (no height left to lose at slot entry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float RunInLevelDistance = 300.0f;

	/** Free-flight stick easing (1/s; 0 = raw). Smooth inputs read as a human pilot on video. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float InputSmoothing = 6.0f;

	/** Stick easing while holding the slot (1/s): faster than free flight, still no frame-to-frame flips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SlotInputSmoothing = 20.0f;

	/** Where the slot run-in starts (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FinalApproach = 420.0f;

	/** Target gap while waiting for safe ground (cm, outside the band). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HoverGap = 62.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MinHeightAboveGround = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float RetargetSeconds = 30.0f;

	/** Waiting for a bomb behind us: weave +-WeaveAngle across its track every WeavePeriod s once it is WeaveStart m back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float WeaveStart = 350.0f;

	/** Speed-matching profile: closing speed = sqrt(2 a d) (m/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ClosingAccel = 8.0f;

	/** Free flight holds the wanted speed on the engine (pace throttle); the airbrake only opens above this excess (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float BrakeExcessOn = 25.0f;

	/** Airbrake closes again once the excess falls below this (m/s); the gap stops on/off chatter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float BrakeExcessOff = 8.0f;

	/** Never ask for less than this while following (m/s; stall is 55). Below it the weave holds position instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FollowMinSpeed = 85.0f;

	/** Wake trim: lead on the balance roll rate (s) and the dead zone around level (meter units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TrimLead = 0.3f;

	/** Wide enough that the needle visibly works against the wake; 0.3 cost two of three rounds (slower holds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TrimDeadband = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float WeaveAngleDeep = 120.0f;

	/** How far into the flak belt (m) the bot will still plan to finish a catch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FlakMargin = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float WeaveAngle = 60.0f;

	/** Bomb further back than this + 400 m: fly a holding circle at FollowMinSpeed; roll out once it is within this (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float OrbitStart = 1200.0f;

	/** Roll stick in the holding circle (0.9 = 67 degree bank, about a 22 s circle at 85 m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float OrbitRoll = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float WeavePeriod = 3.5f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	AActor* PickTarget(const FVector& P, float Speed) const;
	FSIFlightInput FlyTo(const FVector& P, const FRotator& Rot, const FVector& Aim, float DesiredSpeed, float Speed, float Dt);
	FSIFlightInput HoldSlot(float Dt);

	bool bRunning = false;
	FString ForcedDefeat;
	TWeakObjectPtr<AActor> Target;
	float TargetTime = 0.0f;
	float LastGap = 0.0f;
	float LastOverlap = 0.0f;
	bool bHaveLast = false;
	bool bBotBraking = false;
	bool bOrbiting = false;

	/** Throttle for free flight: pace-assist toward DesiredSpeed, airbrake with hysteresis only for big excess. */
	void Throttle(FSIFlightInput& In, class USIFlightComponent* Flight, float DesiredSpeed, float Speed);
	int32 Tipped = 0;
	float LogTimer = 0.0f;
	float TargetSide = 1.0f;
	float WeaveTime = 0.0f;
	float DebugAlong = 0.0f;
	float DebugDesired = 0.0f;
	FVector DebugTarget = FVector::ZeroVector;
	FSIFlightInput Smoothed;
};
