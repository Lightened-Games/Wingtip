// Six Inches - player controller.

#include "Framework/SIPlayerController.h"
#include "Framework/SIAutoplayComponent.h"
#include "Framework/SIInputEchoComponent.h"
#include "Framework/SIMatchComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/SIInterfaces.h"
#include "UI/SIHUDWidget.h"
#include "SixInches.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "InputAction.h"
#include "InputMappingContext.h"

const FName ASIPlayerController::KeyId_Pitch(TEXT("Pitch"));
const FName ASIPlayerController::KeyId_Bank(TEXT("Bank"));
const FName ASIPlayerController::KeyId_Boost(TEXT("Boost"));
const FName ASIPlayerController::KeyId_Brake(TEXT("Brake"));
const FName ASIPlayerController::KeyId_Trim(TEXT("Trim"));
const FName ASIPlayerController::KeyId_Restart(TEXT("Restart"));

ASIPlayerController::ASIPlayerController()
{
	bShowMouseCursor = false;
	Autoplay = CreateDefaultSubobject<USIAutoplayComponent>(TEXT("Autoplay"));
	InputEcho = CreateDefaultSubobject<USIInputEchoComponent>(TEXT("InputEcho"));
	HUDWidgetClass = USIHUDWidget::StaticClass();
	UpKeys = { EKeys::S, EKeys::Down };
	DownKeys = { EKeys::W, EKeys::Up };
	LeftKeys = { EKeys::A, EKeys::Left };
	RightKeys = { EKeys::D, EKeys::Right };
	TrimLeftKeys = { EKeys::Q };
	TrimRightKeys = { EKeys::E };
	BoostKeys = { EKeys::LeftShift, EKeys::RightShift };
	BrakeKeys = { EKeys::SpaceBar };
	RestartKeys = { EKeys::R };
	MenuKeys = { EKeys::Escape };
}

void ASIPlayerController::OnMenuPressed()
{
	// Esc during the round or on an end screen: back to the start screen.
	UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	if (Flow && (Flow->GetState() == EFactoryFlowState::Playing || Flow->GetState() == EFactoryFlowState::Ending))
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		Flow->ReturnToMenu();
	}
}

void ASIPlayerController::BeginPlay()
{
	Super::BeginPlay();
	Autoplay->OnBotInput.AddDynamic(this, &ASIPlayerController::ApplySimulatedInput);
	if (IsLocalController())
	{
		SetupHUD();
		SetInputMode(FInputModeGameOnly());
	}
}

void ASIPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (IsLocalController())
	{
		SetupHUD();
	}
}

void ASIPlayerController::SetupHUD()
{
	if (HUDWidget || !HUDWidgetClass)
	{
		return;
	}
	HUDWidget = CreateWidget<USIHUDWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
	}
	else
	{
		UE_LOG(LogSixInches, Error, TEXT("[SixInches] Failed to create the HUD widget."));
	}
}

void ASIPlayerController::BuildInputAssets()
{
	if (Mapping)
	{
		return;
	}
	auto Make = [this](const TCHAR* Name) { return NewObject<UInputAction>(this, Name); };
	UpAction = Make(TEXT("IA_SI_Up"));
	DownAction = Make(TEXT("IA_SI_Down"));
	LeftAction = Make(TEXT("IA_SI_Left"));
	RightAction = Make(TEXT("IA_SI_Right"));
	TrimLeftAction = Make(TEXT("IA_SI_TrimLeft"));
	TrimRightAction = Make(TEXT("IA_SI_TrimRight"));
	BoostAction = Make(TEXT("IA_SI_Boost"));
	BrakeAction = Make(TEXT("IA_SI_Brake"));
	RestartAction = Make(TEXT("IA_SI_Restart"));
	MenuAction = Make(TEXT("IA_SI_Menu"));
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_SixInches"));
	for (const FKey& K : UpKeys) { Mapping->MapKey(UpAction, K); }
	for (const FKey& K : DownKeys) { Mapping->MapKey(DownAction, K); }
	for (const FKey& K : LeftKeys) { Mapping->MapKey(LeftAction, K); }
	for (const FKey& K : RightKeys) { Mapping->MapKey(RightAction, K); }
	for (const FKey& K : TrimLeftKeys) { Mapping->MapKey(TrimLeftAction, K); }
	for (const FKey& K : TrimRightKeys) { Mapping->MapKey(TrimRightAction, K); }
	for (const FKey& K : BoostKeys) { Mapping->MapKey(BoostAction, K); }
	for (const FKey& K : BrakeKeys) { Mapping->MapKey(BrakeAction, K); }
	for (const FKey& K : RestartKeys) { Mapping->MapKey(RestartAction, K); }
	for (const FKey& K : MenuKeys) { Mapping->MapKey(MenuAction, K); }
}

void ASIPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInputAssets();
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Sub->AddMappingContext(Mapping, 0);
		}
	}
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogSixInches, Error, TEXT("[SixInches] InputComponent is not an EnhancedInputComponent."));
		return;
	}
	EIC->BindAction(UpAction, ETriggerEvent::Started, this, &ASIPlayerController::OnUpPressed);
	EIC->BindAction(UpAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnUpReleased);
	EIC->BindAction(DownAction, ETriggerEvent::Started, this, &ASIPlayerController::OnDownPressed);
	EIC->BindAction(DownAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnDownReleased);
	EIC->BindAction(LeftAction, ETriggerEvent::Started, this, &ASIPlayerController::OnLeftPressed);
	EIC->BindAction(LeftAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnLeftReleased);
	EIC->BindAction(RightAction, ETriggerEvent::Started, this, &ASIPlayerController::OnRightPressed);
	EIC->BindAction(RightAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnRightReleased);
	EIC->BindAction(TrimLeftAction, ETriggerEvent::Started, this, &ASIPlayerController::OnTrimLeftPressed);
	EIC->BindAction(TrimLeftAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnTrimLeftReleased);
	EIC->BindAction(TrimRightAction, ETriggerEvent::Started, this, &ASIPlayerController::OnTrimRightPressed);
	EIC->BindAction(TrimRightAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnTrimRightReleased);
	EIC->BindAction(BoostAction, ETriggerEvent::Started, this, &ASIPlayerController::OnBoostPressed);
	EIC->BindAction(BoostAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnBoostReleased);
	EIC->BindAction(BrakeAction, ETriggerEvent::Started, this, &ASIPlayerController::OnBrakePressed);
	EIC->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ASIPlayerController::OnBrakeReleased);
	EIC->BindAction(RestartAction, ETriggerEvent::Started, this, &ASIPlayerController::OnRestartPressed);
	EIC->BindAction(MenuAction, ETriggerEvent::Started, this, &ASIPlayerController::OnMenuPressed);
}

void ASIPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// Take-over autoplay: the bot drives until a real key or mouse move; it resumes after the idle time.
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	if (Autoplay && Autoplay->IsRunning() && (!Flow || Flow->IsBotDriving()))
	{
		return;
	}
	FSIFlightInput In;
	const float PitchSign = bInvertPitch ? -1.0f : 1.0f;
	In.Pitch = PitchSign * ((bUp ? 1.0f : 0.0f) - (bDown ? 1.0f : 0.0f));
	In.Roll = (bRight ? 1.0f : 0.0f) - (bLeft ? 1.0f : 0.0f);
	In.Trim = (bTrimRight ? 1.0f : 0.0f) - (bTrimLeft ? 1.0f : 0.0f);
	In.bBoost = bBoost;
	In.bBrake = bBrake;
	PushInput(In, false);
}

void ASIPlayerController::ApplySimulatedInput(const FSIFlightInput& Input)
{
	// Light the legend on the bot's "key presses" (edges), like a human's.
	if (FMath::Abs(Input.Pitch) > 0.5f && FMath::Abs(LastSimulated.Pitch) <= 0.5f) { InputEcho->Echo(KeyId_Pitch, true); }
	if (FMath::Abs(Input.Roll) > 0.5f && FMath::Abs(LastSimulated.Roll) <= 0.5f) { InputEcho->Echo(KeyId_Bank, true); }
	if (FMath::Abs(Input.Trim) > 0.5f && FMath::Abs(LastSimulated.Trim) <= 0.5f) { InputEcho->Echo(KeyId_Trim, true); }
	if (Input.bBoost && !LastSimulated.bBoost) { InputEcho->Echo(KeyId_Boost, true); }
	if (Input.bBrake && !LastSimulated.bBrake) { InputEcho->Echo(KeyId_Brake, true); }
	LastSimulated = Input;
	PushInput(Input, true);
}

void ASIPlayerController::PushInput(const FSIFlightInput& Input, bool bSimulated)
{
	if (ISIFlightControllable* Ctrl = Cast<ISIFlightControllable>(GetPawn()))
	{
		Ctrl->SetFlightInput(Input);
	}
}

void ASIPlayerController::Visualize(FName Id)
{
	InputEcho->Echo(Id, false);
}

void ASIPlayerController::OnRestartPressed()
{
	AGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	const USIMatchComponent* Match = GM ? GM->FindComponentByClass<USIMatchComponent>() : nullptr;
	if (Match && (Match->GetState() == ESIMatchState::Victory || Match->GetState() == ESIMatchState::Defeat))
	{
		InputEcho->Echo(KeyId_Restart, false);
		if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this)) { Flow->MarkRestart(); }
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
}
