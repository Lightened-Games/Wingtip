// Six Inches - a friendly anti-aircraft gun near the town. It fires at anything in the sky inside its belt:
// bursts creep onto a target that holds its course (accuracy improves shot by shot). Tuning provisional.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SIFlakBattery.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

UCLASS()
class SIXINCHES_API ASIFlakBattery : public AActor
{
	GENERATED_BODY()

public:
	ASIFlakBattery();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flak")
	TObjectPtr<UStaticMeshComponent> Pit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> PitMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FireInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FireJitter = 0.6f;

	/** Shell flight time before the burst (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ShellTime = 1.1f;

	/** Engagement range from the gun (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float Range = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float BurstRadius = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MaxDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ShakeRadius = 160.0f;

	/** Aim error (m): first shot, decay per shot at the same target, floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float BaseError = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ErrorDecayPerShot = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float MinError = 22.0f;

protected:
	virtual void BeginPlay() override;

private:
	void Fire(AActor* Target, bool bPlayer);
	void Burst(const FVector& At);

	float Cooldown = 0.0f;
	TWeakObjectPtr<AActor> LastTarget;
	int32 ShotsAtTarget = 0;
	FRandomStream Rng;
};
