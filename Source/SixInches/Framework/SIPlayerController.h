// Six Inches - player controller: Enhanced Input created at runtime (no template Input assets), HUD wiring,
// and the autoplay bot. Human or bot, input reaches the pawn through ISIFlightControllable.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/SITypes.h"
#include "SIPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class USIHUDWidget;
class USIAutoplayComponent;
class USIInputEchoComponent;

UCLASS()
class SIXINCHES_API ASIPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASIPlayerController();

	static const FName KeyId_Pitch;
	static const FName KeyId_Bank;
	static const FName KeyId_Boost;
	static const FName KeyId_Brake;
	static const FName KeyId_Trim;
	static const FName KeyId_Restart;

	/** Legend rows light up when their input fires (human or bot). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<USIInputEchoComponent> InputEcho;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Autoplay")
	TObjectPtr<USIAutoplayComponent> Autoplay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<USIHUDWidget> HUDWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> UpKeys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> DownKeys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> LeftKeys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> RightKeys;

	/** Wake trim (slot mode): roll left / right against the bomb's wake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> TrimLeftKeys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> TrimRightKeys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> BoostKeys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> BrakeKeys;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> RestartKeys;

	/** Back to the start screen (in play or on an end screen). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TArray<FKey> MenuKeys;

	/** Invert W/S (W = climb) if a player prefers it. Default is stick convention: W pushes the nose down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bInvertPitch = false;

	/** The bot drives through here so the legend lights exactly like a human's keys. */
	UFUNCTION()
	void ApplySimulatedInput(const FSIFlightInput& Input);

	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

private:
	void BuildInputAssets();
	void SetupHUD();
	void PushInput(const FSIFlightInput& Input, bool bSimulated);

	void OnUpPressed() { bUp = true; Visualize(KeyId_Pitch); }
	void OnUpReleased() { bUp = false; }
	void OnDownPressed() { bDown = true; Visualize(KeyId_Pitch); }
	void OnDownReleased() { bDown = false; }
	void OnLeftPressed() { bLeft = true; Visualize(KeyId_Bank); }
	void OnLeftReleased() { bLeft = false; }
	void OnRightPressed() { bRight = true; Visualize(KeyId_Bank); }
	void OnRightReleased() { bRight = false; }
	void OnTrimLeftPressed() { bTrimLeft = true; Visualize(KeyId_Trim); }
	void OnTrimLeftReleased() { bTrimLeft = false; }
	void OnTrimRightPressed() { bTrimRight = true; Visualize(KeyId_Trim); }
	void OnTrimRightReleased() { bTrimRight = false; }
	void OnBoostPressed() { bBoost = true; Visualize(KeyId_Boost); }
	void OnBoostReleased() { bBoost = false; }
	void OnBrakePressed() { bBrake = true; Visualize(KeyId_Brake); }
	void OnBrakeReleased() { bBrake = false; }
	void OnRestartPressed();
	void OnMenuPressed();
	void Visualize(FName Id);

	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<UInputAction> UpAction;
	UPROPERTY() TObjectPtr<UInputAction> DownAction;
	UPROPERTY() TObjectPtr<UInputAction> LeftAction;
	UPROPERTY() TObjectPtr<UInputAction> RightAction;
	UPROPERTY() TObjectPtr<UInputAction> TrimLeftAction;
	UPROPERTY() TObjectPtr<UInputAction> TrimRightAction;
	UPROPERTY() TObjectPtr<UInputAction> BoostAction;
	UPROPERTY() TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY() TObjectPtr<UInputAction> RestartAction;
	UPROPERTY() TObjectPtr<UInputAction> MenuAction;
	UPROPERTY() TObjectPtr<USIHUDWidget> HUDWidget;

	bool bUp = false, bDown = false, bLeft = false, bRight = false, bTrimLeft = false, bTrimRight = false, bBoost = false, bBrake = false;
	FSIFlightInput LastSimulated;
};
