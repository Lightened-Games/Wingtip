// Six Inches - the player's fighter: a thin shell around components (flight, wingtip, health, game feel) plus the
// chase camera that swings to a low outboard "slot cam" framing both wingtips.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/SIInterfaces.h"
#include "SIPlanePawn.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UCameraComponent;
class USIFlightComponent;
class USIWingtipComponent;
class USIHealthComponent;
class USIGameFeelComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class SIXINCHES_API ASIPlanePawn : public APawn, public ISIFlightControllable
{
	GENERATED_BODY()

public:
	ASIPlanePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override;

	// ISIFlightControllable
	virtual void SetFlightInput(const FSIFlightInput& Input) override;
	virtual FSIFlightInput GetFlightInput() const override { return CurrentInput; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<UStaticMeshComponent> PropDisc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<UCameraComponent> Camera;

	/** World marker under where the held bomb would come down (P43 preview). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<UStaticMeshComponent> PreviewRing;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<USIFlightComponent> Flight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<USIWingtipComponent> Wingtip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<USIHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Plane")
	TObjectPtr<USIGameFeelComponent> GameFeel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> PropMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> RingMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UMaterialInterface> RingMaterial;

	/** Preview ring radius (m) and its safe / unsafe tints (linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float PreviewRingRadius = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FLinearColor RingSafeColor = FLinearColor(0.88f, 0.84f, 0.76f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	FLinearColor RingDangerColor = FLinearColor(0.68f, 0.06f, 0.04f);

	// ---- camera (cm, degrees). Provisional until confirmed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float ChaseDistance = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float ChaseHeight = 520.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float ChaseLookAhead = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float ChaseLag = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float ChaseFOV = 74.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float SpeedFOVKick = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float CameraRollFollow = 0.3f;

	/** Slot cam: behind the two wingtips, out toward the player's plane, at wing height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float SlotBack = 1700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float SlotOut = -150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float SlotUp = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float SlotLookAhead = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float SlotFOV = 62.0f;

	/** Slot cam pitch-down (deg) so the impact preview ahead is in view while both wingtips stay framed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Camera")
	float SlotPitchDown = 5.0f;

	/** Slot camera: how fast the anchor follows the wingtip midpoint relative to the bomb (1/s; lower = steadier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float SlotAnchorSmoothing = 2.5f;

	/** Slot camera rotation follow rate (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float SlotRotationRate = 6.0f;

	/** Pace assist for human pilots: near a bomb (same heading, within range) the engine holds a closing speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assist")
	bool bPaceAssist = true;

	/** Range (m along track; 40% of it laterally) and the most heading difference (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assist")
	float PaceAssistRange = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assist")
	float PaceAssistMaxAngle = 35.0f;

	/** Closing speed per metre of gap (1/s) and its cap (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assist")
	float PaceAssistClosing = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assist")
	float PaceAssistMaxClosing = 40.0f;

private:
	void UpdatePaceAssist();
	FVector SlotAnchorLocal = FVector::ZeroVector;
	bool bSlotAnchorInit = false;
	FVector JitterLastRel = FVector::ZeroVector;
	FVector JitterLastStep = FVector::ZeroVector;
	float JitterMax = 0.0f;
	float JitterTimer = 0.0f;
	int32 JitterFrames = 0;
	float SlotYaw = 0.0f;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float PropSpinDegPerSec = 1300.0f;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateCamera(float Dt);
	void UpdatePreviewRing(float Dt);

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMID;
	float RingAlpha = 0.0f;

	FSIFlightInput CurrentInput;
	FVector CamPos = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	bool bCamInit = false;
};
