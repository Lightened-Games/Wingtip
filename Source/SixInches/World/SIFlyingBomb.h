// Six Inches - a flying bomb: mesh, pulse-jet sound, exhaust flame, world marker chevron, and an autopilot component
// that makes it tippable. On impact it explodes, leaves a turbulence column and smoke.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/SIInterfaces.h"
#include "Components/SITippableComponent.h"
#include "SIFlyingBomb.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UAudioComponent;
class USILoopSynth;
class UMaterialBillboardComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UCLASS()
class SIXINCHES_API ASIFlyingBomb : public AActor, public ISITippable
{
	GENERATED_BODY()

public:
	ASIFlyingBomb();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ISITippable
	virtual FSIWingFrame GetWingFrame() const override;
	virtual bool IsTippable() const override;
	virtual void SetLiftProgress(float Progress01, float LiftedSide) override;
	virtual void Tip(float LiftedSide) override;
	virtual FVector PredictTipImpact(float LiftedSide) const override;
	virtual void Bump(const FVector& Velocity) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomb")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomb")
	TObjectPtr<UStaticMeshComponent> Chevron;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomb")
	TObjectPtr<UMaterialBillboardComponent> Flame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bomb")
	TObjectPtr<USITippableComponent> Autopilot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> ChevronMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UMaterialInterface> ChevronMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UMaterialInterface> FlameMaterial;

	/** Pulse-jet audible range (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float JetFalloff = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float JetVolume = 0.9f;

	/** Chevron: hidden inside this distance (cm), scaled with distance beyond it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ChevronHideDistance = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ChevronScalePerKm = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ChevronHeight = 600.0f;

	// Explosion / aftermath
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TurbulenceRadius = 16000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TurbulenceHeight = 60000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float TurbulenceLife = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ShakeInnerRadius = 30000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float ShakeOuterRadius = 160000.0f;

	/** Speed of sound for the delayed boom (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SoundSpeed = 343.0f;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION() void HandleImpact(FVector Location, bool bTipped);
	UFUNCTION() void HandleState(ESIAutopilotState NewState);

	UPROPERTY() TObjectPtr<UAudioComponent> JetAudio;
	UPROPERTY() TObjectPtr<USILoopSynth> JetSynth;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlameMID;
	float FlameUpdateTimer = 0.0f;

public:
	/** Exhaust core size (cm) and brightness (<= 3.5 emissive cap applies in the material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FlameSize = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FlameGlow = 0.65f;

	/** Flicker updates per second (each one rebuilds the billboard's render proxy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float FlameUpdateRate = 20.0f;

private:

	FVector TipLeftLocal = FVector(-4.0f, -266.0f, -6.0f);
	FVector TipRightLocal = FVector(-4.0f, 266.0f, -6.0f);
	FVector JetLocal = FVector(-470.0f, 0.0f, 78.0f);
	float FlickerTime = 0.0f;
	float EngineCutTimer = -1.0f;
};
