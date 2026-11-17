// Six Inches - airframe integrity (attach to anything that can be damaged).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SITypes.h"
#include "SIHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSIOnDamaged, float, Amount, ESIDamageSource, Source, float, HealthAfter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnDepleted, ESIDamageSource, Source);

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning", meta = (ClampMin = "1"))
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FSIOnDamaged OnDamaged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FSIOnDepleted OnDepleted;

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Amount, ESIDamageSource Source);

	/** Instantly destroys the airframe (cable strike, ground). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Kill(ESIDamageSource Source);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetDamageEnabled(bool bEnabled) { bDamageEnabled = bEnabled; }

	float GetHealth() const { return Health; }
	float GetHealth01() const { return Health / FMath::Max(MaxHealth, 1.0f); }
	bool IsDepleted() const { return Health <= 0.0f; }
	float GetDamageTaken() const { return DamageTaken; }

protected:
	virtual void BeginPlay() override;

private:
	float Health = 100.0f;
	float DamageTaken = 0.0f;
	bool bDamageEnabled = false;
};
