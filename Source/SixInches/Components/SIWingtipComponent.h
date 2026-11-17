// Six Inches - the wingtip trick. Near a tippable target the plane drops into slot mode (speed matched, fine nudges);
// holding the inner wingtip a few inches under the target's wingtip lifts its wing until its autopilot gives up.
// Every band and timing value is Tuning and provisional until confirmed.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SITypes.h"
#include "SIWingtipComponent.generated.h"

class USIFlightComponent;
class USIHealthComponent;
class USIAirspaceSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSIOnSlotChanged, bool, bInSlot, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnBandChanged, bool, bInBand);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnWingContact, FVector, Where);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSIOnBombTipped, AActor*, Bomb, FSIGroundClass, Predicted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnPreviewChanged, FSIGroundClass, Predicted);

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIWingtipComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIWingtipComponent();

	// ---- slot entry / exit (metres, m/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	bool bSlotAssist = true;

	/** Human pilots: a released key lets that axis drift the wingtip toward the band (0 = off, 1 = full stick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float BandHoldAssist = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float SlotEnterLateral = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float SlotEnterForeAft = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float SlotEnterVertical = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float SlotEnterSpeedDelta = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float SlotExitDistance = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float ReentryCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float NudgeVertical = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float NudgeLateral = 1.2f;

	/** Nudges speed up with distance from the band so closing in from the slot edge takes about a second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float FarNudgeGain = 1.2f;

	/** How quickly the nudge velocity follows the stick (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float NudgeResponse = 3.5f;

	/** Fore/aft assist: closes the along-track error at this rate (1/s), capped at MaxAssistSpeed (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float ForeAftAssist = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float MaxAssistSpeed = 15.0f;

	// ---- the band (cm for the gap, metres for overlap / fore-aft)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float BandGapMin = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float BandGapIdeal = 30.0f; // was 15: from the chase camera 15 cm read as touching (user 2026-11-17: "planes getting too close and touching")

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float BandGapMax = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float BandOverlapMin = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float BandOverlapIdeal = 0.6f; // was 0.8

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float BandOverlapMax = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float BandForeAft = 2.0f;

	/** Combined half-thickness of both wingtips (cm), subtracted from the tip-to-tip height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float WingThickness = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float TipTimeBest = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float TipTimeEdge = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Band")
	float ProgressDecay = 0.15f;

	// ---- contact
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Contact")
	float ContactDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Contact")
	float ContactPushDown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Contact")
	float ContactPushOut = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Contact")
	float ContactCooldown = 0.7f;

	// ---- turbulence in the slot (metres, degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float SlotWobbleVertical = 0.08f; // was 0.2 (user 2026-11-17: "our jet is wobbling")

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float SlotWobbleLateral = 0.06f; // was 0.15

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float SlotWobbleRoll = 0.35f; // was 1.2

	/** Ambient gust per escalation step beyond the first (m of wobble). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float AmbientGust = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float GustDrift = 0.25f;

	// ---- wake balance (meter units: 0 = level, +-1 = flipped out). Provisional (C8), see Docs/TUNING.md.
	/** Off turns the wake balance off entirely (the slot then plays as before). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	bool bWakeBalance = true;

	/** The wake's push on the meter (units/s at full noise). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float WakePush = 0.55f;

	/** Extra push while in the hold band (the vortex is strongest right under the bomb's tip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float WakeInBandScale = 1.5f;

	/** Extra push per escalation step beyond the first (later bombs, rougher air). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float WakeEscalation = 0.15f;

	/** How fast the wake's direction changes (Hz-ish). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float WakeFrequency = 0.45f;

	/** Unstable roll: an off-level wing rolls further by itself (1/s). Left alone, the meter runs away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float WakeInstability = 0.7f;

	/** Q / E trim authority (units/s at full key). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float TrimRate = 1.6f;

	/** How quickly the roll rate follows trim and wake (1/s): some inertia, so keys must be fed early. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float BalanceResponse = 4.0f;

	/** Beyond this the lift stalls (the hold doesn't grow and slowly drains). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float BalanceSafe = 0.55f;

	/** Visual and physical roll at the end of the meter (deg): tilts the inner tip up or down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float BalanceRollDeg = 1.2f; // was 3
	/** Seconds-scale smoothing of the slot's extra roll (wobble + slide bank + wake), so it eases instead of shaking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float SlotRollSmoothing = 0.0f; // 0 = off (left off; the lost test rounds turned out to be machine load, not this)

	/** Damage when the wake flips the fighter out of the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Wake")
	float WakeFlipDamage = 6.0f;

	// ---- break away
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float BreakAwayClimb = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float BreakAwayBank = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float PreviewInterval = 0.15f;

	/** Visual bank while sliding sideways (deg per m/s). Small: it moves the inner tip up and down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Slot")
	float SlideBank = 0.5f;

	/** Fallback wingtip offsets (cm, actor space) if the body mesh has no TipL / TipR sockets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Geometry")
	FVector TipRightLocal = FVector(47.0f, 625.0f, 25.7f);

	UPROPERTY(BlueprintAssignable, Category = "Wingtip")
	FSIOnSlotChanged OnSlotChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wingtip")
	FSIOnBandChanged OnBandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Wingtip")
	FSIOnWingContact OnContact;

	UPROPERTY(BlueprintAssignable, Category = "Wingtip")
	FSIOnBombTipped OnBombTipped;

	UPROPERTY(BlueprintAssignable, Category = "Wingtip")
	FSIOnPreviewChanged OnPreviewChanged;

	void SetEnabled(bool bInEnabled);
	void SetInput(const FSIFlightInput& In) { Input = In; }

	ESISlotState GetState() const { return State; }
	bool IsInSlot() const { return State == ESISlotState::Slot; }
	AActor* GetTarget() const { return Target.Get(); }
	float GetGapCm() const { return GapCm; }
	float GetOverlap() const { return Overlap; }
	float GetForeAft() const { return ForeAft; }
	bool IsInBand() const { return bInBand; }
	float GetProgress() const { return Progress; }
	/** Wake balance meter: 0 level, +-1 flipped out (+ = rolling right). */
	float GetBalance() const { return Balance; }
	float GetBalanceRate() const { return BalanceRate; }
	bool IsBalanceSafe() const { return !bWakeBalance || FMath::Abs(Balance) <= BalanceSafe; }
	float GetSide() const { return Side; }
	float GetSlotBlend() const { return SlotBlend; }
	FVector GetPlayerTip() const { return PlayerTipWorld; }
	FVector GetTargetTip() const { return TargetTipWorld; }
	FVector GetPreviewLocation() const { return PreviewLocation; }
	FSIGroundClass GetPreviewClass() const { return PreviewClass; }
	bool HasPreview() const { return bHasPreview; }
	/** Inner wingtip (the one next to the target) in actor space. */
	FVector GetInnerTipLocal(float InSide) const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void TickFree(float Dt);
	void TickSlot(float Dt);
	void EnterSlot(AActor* Bomb, float InSide, float SpeedDelta);
	void ExitSlot(bool bBreakAway);
	USIAirspaceSubsystem* Airspace() const;

	UPROPERTY()
	TObjectPtr<USIFlightComponent> Flight;

	UPROPERTY()
	TObjectPtr<USIHealthComponent> Health;

	TWeakObjectPtr<AActor> Target;
	FSIFlightInput Input;
	ESISlotState State = ESISlotState::Free;
	bool bEnabled = false;
	FVector TipLeftLocal = FVector(47.0f, -625.0f, 25.7f);
	FVector TipRightActual = FVector(47.0f, 625.0f, 25.7f);
	float SlotRoll = 0.0f;
	FVector Offset = FVector::ZeroVector;     // player root relative to target, target frame (cm)
	FVector OffsetVel = FVector::ZeroVector;  // cm/s
	float Side = 1.0f;
	float GapCm = 100.0f;
	float Overlap = 0.0f;
	float ForeAft = 0.0f;
	bool bInBand = false;
	float Progress = 0.0f;
	float Balance = 0.0f;
	float BalanceRate = 0.0f;
	float WakeSeed = 0.0f;
	float SlotBlend = 0.0f;
	float ContactCool = 0.0f;
	float Reentry = 0.0f;
	float PreviewTimer = 0.0f;
	float NoiseTime = 0.0f;
	bool bHasPreview = false;
	FVector PreviewLocation = FVector::ZeroVector;
	FSIGroundClass PreviewClass;
	FVector PlayerTipWorld = FVector::ZeroVector;
	FVector TargetTipWorld = FVector::ZeroVector;
};
