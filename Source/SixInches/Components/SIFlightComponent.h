// Six Inches - kinematic arcade flight with an energy model: height buys speed, climbing spends it.
// Owns the owner's transform. Tuning is in metres / seconds / degrees; all values provisional until confirmed.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SITypes.h"
#include "SIFlightComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnCrashed, ESIDamageSource, Source);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnFlightToggle, bool, bOn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnBuffet, float, Strength);

class USIAirspaceSubsystem;

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIFlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIFlightComponent();

	// ---- speed / energy (m/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float LevelMaxSpeed = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float MinSpeed = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float DiveMaxSpeed = 240.0f;

	/** How fast thrust/drag pull speed back toward the level maximum (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float ThrustResponse = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float Gravity = 9.81f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float AirbrakeDecel = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float BoostAdd = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float BoostSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Speed")
	float BoostCooldown = 6.0f;

	// ---- handling (deg, deg/s)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Handling")
	float PitchRate = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Handling")
	float RollRate = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Handling")
	float MaxBank = 75.0f;

	/** Roll returns to level at this fraction of RollRate when the stick is centred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Handling")
	float AutoLevelFraction = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Handling")
	float TurnFactor = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Handling")
	float MaxPitch = 80.0f;

	/** Nose drop when slower than MinSpeed (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Handling")
	float StallNoseDrop = 25.0f;

	// ---- turbulence (applied in free flight; slot mode adds its own on the wingtip)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float TurbulenceRoll = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float TurbulencePitch = 4.0f;

	/** Vertical bump speed at full intensity (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float TurbulenceLift = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Turbulence")
	float BuffetThreshold = 0.35f;

	/** Free flight: never let our fuselage or wing pass through a flying bomb (user 2026-11-17: "make sure our plane doesn't touch enemy planes"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clearance")
	bool bKeepClearOfBombs = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clearance")
	float ClearanceMargin = 15.0f;

	// ---- safety (m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Safety")
	float SweepRadius = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Safety")
	float CableRadius = 5.0f;

	/** Playable box (cm). The edges turn the plane back before the BlockingVolumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Safety")
	FVector BoundsMin = FVector(-150000.0f, -250000.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Safety")
	FVector BoundsMax = FVector(800000.0f, 250000.0f, 200000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Safety")
	float SoftBoundsMargin = 30000.0f;

	UPROPERTY(BlueprintAssignable, Category = "Flight")
	FSIOnCrashed OnCrashed;

	UPROPERTY(BlueprintAssignable, Category = "Flight")
	FSIOnFlightToggle OnBoost;

	UPROPERTY(BlueprintAssignable, Category = "Flight")
	FSIOnFlightToggle OnBrake;

	UPROPERTY(BlueprintAssignable, Category = "Flight")
	FSIOnBuffet OnBuffet;

	void SetInput(const FSIFlightInput& In) { Input = In; }
	const FSIFlightInput& GetInput() const { return Input; }
	void SetLocked(bool bInLocked) { bLocked = bInLocked; }
	/** Ignore the stick and fly level with a gentle climb (end screens). */
	void SetAutoLevel(bool bOn) { bAutoLevel = bOn; }
	bool IsLocked() const { return bLocked; }
	void Initialise(const FVector& Location, const FRotator& Rotation, float SpeedMS);

	/** Formation (slot mode): the wingtip component supplies the transform; speed follows the target. */
	void SetFormation(bool bOn);
	void SetFormationTarget(const FVector& Location, const FRotator& Rotation, float SpeedMS);
	bool IsInFormation() const { return bFormation; }
	/** Leaving formation: pitch up and bank away, keep the target's speed. */
	void BreakAway(float ClimbDegrees, float BankDegrees);

	float GetSpeed() const { return Speed; }
	float GetSpeed01() const { return FMath::Clamp((Speed - MinSpeed) / FMath::Max(DiveMaxSpeed - MinSpeed, 1.0f), 0.0f, 1.0f); }
	FVector GetVelocity() const { return Velocity; }
	FRotator GetFlightRotation() const { return Rot; }
	float GetAltitude() const { return Altitude; }
	float GetHeightAboveGround() const { return HeightAboveGround; }
	bool IsBoosting() const { return BoostTime > 0.0f; }
	float GetBoostAvailable01() const;
	bool IsCrashed() const { return bCrashed; }
	float GetTurbulence() const { return TurbIntensity; }

	/** Pace assist: hold this speed (m/s) this frame unless the pilot boosts or brakes. Call every frame. */
	void SetPaceAssist(float SpeedMS) { PaceAssistSpeed = SpeedMS; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Assist")
	float PaceAssistResponse = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Assist")
	float PaceAssistMaxAccel = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Assist")
	float PaceAssistMaxDecel = 30.0f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void TickFree(float Dt, FVector& NewLoc);
	void TickFormation(float Dt, FVector& NewLoc);
	bool SweepFor(const FVector& From, FVector& To, ESIDamageSource& OutSource);
	USIAirspaceSubsystem* Airspace() const;

	FSIFlightInput Input;
	FRotator Rot = FRotator::ZeroRotator;
	float Speed = 150.0f;
	FVector Velocity = FVector::ZeroVector;
	float BoostTime = 0.0f;
	float BoostCool = 0.0f;
	bool bBoostHeld = false;
	bool bBrakeHeld = false;
	bool bLocked = true;
	bool bAutoLevel = false;
	bool bCrashed = false;
	bool bFormation = false;
	FVector FormationLoc = FVector::ZeroVector;
	FRotator FormationRot = FRotator::ZeroRotator;
	float FormationSpeed = 140.0f;
	float Altitude = 0.0f;
	float HeightAboveGround = 0.0f;
	float GroundTimer = 0.0f;
	float GroundZ = 0.0f;
	float TurbIntensity = 0.0f;
	float BuffetCooldown = 0.0f;
	float ClearLogCool = 0.0f;
	void KeepClearOfBombs(FVector& NewLoc, float Dt);
	float NoiseTime = 0.0f;
	float PaceAssistSpeed = 0.0f;
};
