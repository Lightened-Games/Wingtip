// Six Inches - lightweight flipbook FX (smoke puffs, flak bursts, flashes, sparks, vapour) on camera-facing billboards.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SIPuffActor.generated.h"

class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ESIPuffMaterial : uint8
{
	/** Lit translucent smoke (T_SmokeSheet). */
	Smoke,
	/** Additive glow (T_FlashSheet / T_SoftDot), emissive capped at 3.5 before exposure compensation. */
	Glow
};

USTRUCT(BlueprintType)
struct FSIPuffSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	ESIPuffMaterial Material = ESIPuffMaterial::Smoke;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	int32 Count = 1;

	/** Random spread of the sub-puffs (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float Spread = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float Life = 2.0f;

	/** Billboard size at birth and death (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float SizeStart = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float SizeEnd = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	FLinearColor Color = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float Opacity = 1.0f;

	/** Rise speed (cm/s) and an initial velocity (cm/s) that drags to a stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float Rise = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float Drag = 2.0f;

	/** Seconds before the puff appears (stagger for columns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	float Delay = 0.0f;

	/** Glow strength (<= 3.5); the material multiplies by its exposure compensation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff", meta = (ClampMax = "3.5"))
	float Glow = 1.0f;

	/** Glow: use the soft dot instead of the flash flipbook (sparks, vapour). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puff")
	bool bSoftDot = false;
};

UCLASS()
class SIXINCHES_API ASIPuffActor : public AActor
{
	GENERATED_BODY()

public:
	ASIPuffActor();

	static ASIPuffActor* Spawn(UWorld* World, const FSIPuffSpec& Spec, const FVector& Location, int32 Seed = 0);

	virtual void Tick(float DeltaSeconds) override;

	void Init(const FSIPuffSpec& InSpec, int32 Seed);
	/** Fade out over QuickFadeSeconds and go (the live cap retires the oldest puffs this way). */
	void FadeOutSoon();

	/** At most this many puffs alive; over it the oldest fade out quickly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	int32 MaxLivePuffs = 160;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float QuickFadeSeconds = 0.6f;

	/** Re-size the sprites only when the size moved this fraction (each resize rebuilds the render proxy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ResizeStep = 0.04f;

private:
	UPROPERTY()
	TArray<TObjectPtr<UMaterialBillboardComponent>> Sprites;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MID;

	FSIPuffSpec Spec;
	TArray<FVector> Offsets;
	TArray<float> SizeJitter;
	FVector Vel = FVector::ZeroVector;
	float Age = 0.0f;
	float PhaseOffset = 0.0f;
	float LastSize = -1.0f;
	bool bFadingOut = false;
	float FadeAge = 0.0f;
};
