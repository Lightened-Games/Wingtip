// Six Inches - game feel for the player aircraft: camera shakes, 50 ms hit-stop, vapour, sparks, engine/wind/hold
// audio loops and one-shots. It only listens to the other components' delegates.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SITypes.h"
#include "SIGameFeelComponent.generated.h"

class UAudioComponent;
class USILoopSynth;
class USIFlightComponent;
class USIWingtipComponent;
class USIHealthComponent;
class UCameraShakeBase;

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIGameFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIGameFeelComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HitStopSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HitStopDilation = 0.05f;

	/** Minimum real time between hit-stops (s): back-to-back freezes read as lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HitStopMinInterval = 1.5f;

	/** Damage below this never hit-stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HitStopMinDamage = 8.0f;

	/** Wingtip scrapes in the slot hit-stop too (off: shake, sparks and sound only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	bool bHitStopOnContact = false;

	/** Turbulence camera shake scale while in the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SlotBuffetShakeScale = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float EngineVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float WindVolume = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float HoldVolume = 0.32f;

	/** Vapour puffs off the wingtips in hard pulls and in the slot (seconds between puffs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float VapourInterval = 0.05f;

	void PlayShake(TSubclassOf<UCameraShakeBase> Shake, float Scale);
	void HitStop();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION() void HandleDamaged(float Amount, ESIDamageSource Source, float HealthAfter);
	UFUNCTION() void HandleSlot(bool bInSlot, AActor* Target);
	UFUNCTION() void HandleContact(FVector Where);
	UFUNCTION() void HandleTipped(AActor* Bomb, FSIGroundClass Predicted);
	UFUNCTION() void HandleBuffet(float Strength);
	UFUNCTION() void HandleBoost(bool bOn);
	UFUNCTION() void HandleBrake(bool bOn);
	UFUNCTION() void HandleCrashed(ESIDamageSource Source);
	void Vapour(const FVector& Where, float Scale);

	UPROPERTY() TObjectPtr<USIFlightComponent> Flight;
	UPROPERTY() TObjectPtr<USIWingtipComponent> Wingtip;
	UPROPERTY() TObjectPtr<USIHealthComponent> Health;
	UPROPERTY() TObjectPtr<UAudioComponent> EngineAudio;
	UPROPERTY() TObjectPtr<UAudioComponent> WindAudio;
	UPROPERTY() TObjectPtr<UAudioComponent> HoldAudio;
	UPROPERTY() TObjectPtr<USILoopSynth> EngineSynth;
	UPROPERTY() TObjectPtr<USILoopSynth> WindSynth;
	UPROPERTY() TObjectPtr<USILoopSynth> HoldSynth;

	float LastHitStop = -10.0f;
	float VapourTimer = 0.0f;
	bool bHitStopActive = false;
};
