#include "FactoryFlowSubsystem.h"
#include "FactoryCinematicCamera.h"
#include "FactoryLegendWidget.h"
#include "FactoryMenuWidget.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "FactoryKitSettings.h"

namespace FactoryFlowStatics
{
	static bool bSkipMenuNextLoad = false;
	static bool bOptionsLoaded = false;
	static FFactoryOptions Cached;
	static const TCHAR* Section = TEXT("FactoryKit.Options");
}

// ---------------- options ----------------
void FFactoryOptions::Load()
{
	GConfig->GetFloat(FactoryFlowStatics::Section, TEXT("Master"), Master, GGameUserSettingsIni);
	GConfig->GetFloat(FactoryFlowStatics::Section, TEXT("Music"), Music, GGameUserSettingsIni);
	GConfig->GetFloat(FactoryFlowStatics::Section, TEXT("Sfx"), Sfx, GGameUserSettingsIni);
	GConfig->GetFloat(FactoryFlowStatics::Section, TEXT("Sensitivity"), Sensitivity, GGameUserSettingsIni);
	GConfig->GetBool(FactoryFlowStatics::Section, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->GetBool(FactoryFlowStatics::Section, TEXT("Fullscreen"), bFullscreen, GGameUserSettingsIni);
	GConfig->GetInt(FactoryFlowStatics::Section, TEXT("Quality"), Quality, GGameUserSettingsIni);
	FactoryFlowStatics::Cached = *this;
	FactoryFlowStatics::bOptionsLoaded = true;
}

void FFactoryOptions::Save() const
{
	GConfig->SetFloat(FactoryFlowStatics::Section, TEXT("Master"), Master, GGameUserSettingsIni);
	GConfig->SetFloat(FactoryFlowStatics::Section, TEXT("Music"), Music, GGameUserSettingsIni);
	GConfig->SetFloat(FactoryFlowStatics::Section, TEXT("Sfx"), Sfx, GGameUserSettingsIni);
	GConfig->SetFloat(FactoryFlowStatics::Section, TEXT("Sensitivity"), Sensitivity, GGameUserSettingsIni);
	GConfig->SetBool(FactoryFlowStatics::Section, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->SetBool(FactoryFlowStatics::Section, TEXT("Fullscreen"), bFullscreen, GGameUserSettingsIni);
	GConfig->SetInt(FactoryFlowStatics::Section, TEXT("Quality"), Quality, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	FactoryFlowStatics::Cached = *this;
}

float UFactoryFlowSubsystem::GetMouseSensitivity()
{
	if (!FactoryFlowStatics::bOptionsLoaded) { FFactoryOptions O; O.Load(); }
	return FactoryFlowStatics::Cached.Sensitivity;
}

bool UFactoryFlowSubsystem::GetInvertY()
{
	if (!FactoryFlowStatics::bOptionsLoaded) { FFactoryOptions O; O.Load(); }
	return FactoryFlowStatics::Cached.bInvertY;
}

// ---------------- take-over input watcher ----------------
class FFactoryInputWatcher : public IInputProcessor
{
public:
	explicit FFactoryInputWatcher(UFactoryFlowSubsystem* InFlow) : Flow(InFlow) {}
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (!InKeyEvent.IsRepeat()) { const FKey K = InKeyEvent.GetKey(); Notify(&K); }
		return false;
	}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		const FKey K = MouseEvent.GetEffectingButton();
		Notify(&K);
		return false;
	}
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetCursorDelta().Size() > UFactoryKitSettings::Get()->TakeoverMouseDeadZone) { Notify(nullptr); }
		return false;
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("FactoryInputWatcher"); }

private:
	void Notify(const FKey* Key)
	{
		if (UFactoryFlowSubsystem* F = Flow.Get()) { F->NotifyHumanInput(Key); }
	}
	TWeakObjectPtr<UFactoryFlowSubsystem> Flow;
};

// ---------------- subsystem ----------------
UFactoryFlowSubsystem* UFactoryFlowSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* W = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return W ? W->GetSubsystem<UFactoryFlowSubsystem>() : nullptr;
}

bool UFactoryFlowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* W = Cast<UWorld>(Outer);
	return W && (W->WorldType == EWorldType::Game || W->WorldType == EWorldType::PIE);
}

void UFactoryFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString List;
	if (FParse::Value(FCommandLine::Get(), TEXT("FactoryKitShots="), List, false))
	{
		TArray<FString> Parts;
		List.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& P : Parts) { ShotTimes.Add(FCString::Atof(*P)); }
	}
	FParse::Value(FCommandLine::Get(), TEXT("FactoryKitFakeInputAt="), FakeInputAt);
	if (FSlateApplication::IsInitialized())
	{
		Watcher = MakeShared<FFactoryInputWatcher>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Watcher);
	}
}

void UFactoryFlowSubsystem::Deinitialize()
{
	if (Watcher.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Watcher);
	}
	Watcher.Reset();
	Super::Deinitialize();
}

TStatId UFactoryFlowSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFactoryFlowSubsystem, STATGROUP_Tickables);
}

AFactoryCinematicCamera* UFactoryFlowSubsystem::Camera()
{
	if (!Cam)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Cam = GetWorld()->SpawnActor<AFactoryCinematicCamera>(AFactoryCinematicCamera::StaticClass(), FTransform::Identity, P);
	}
	return Cam;
}

void UFactoryFlowSubsystem::Begin(bool bInAutoplay)
{
	bAutoplay = bInAutoplay;
	bBotDriving = true;
	Opts.Load();
	ApplyOptions(false);
	const bool bSkip = FactoryFlowStatics::bSkipMenuNextLoad || FParse::Param(FCommandLine::Get(), TEXT("FactoryNoMenu"));
	FactoryFlowStatics::bSkipMenuNextLoad = false;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	UE_LOG(LogTemp, Display, TEXT("[FactoryKit] Begin: autoplay=%d menu=%d"), bAutoplay ? 1 : 0, bSkip ? 0 : 1);

	if (bSkip || !PC)
	{
		State = EFactoryFlowState::Playing;
		ShowLegend();
		OnStartGame.Broadcast();
		return;
	}
	State = EFactoryFlowState::Menu;
	StateTime = 0.0f;
	bBotPressed = false;
	// Keep the hero opposite the menu text: looking at a point beside or below the hero moves the hero across the frame.
	TArray<FFactoryShot> MenuShots = UFactoryKitSettings::Get()->MenuShots;
	const float Shift = UFactoryKitSettings::Get()->MenuFrameShift;
	for (FFactoryShot& Shot : MenuShots)
	{
		switch (UFactoryKitSettings::Get()->MenuLayout)
		{
		case EFactoryMenuLayout::RightColumn:  Shot.LookAtOffset.Y -= Shift; break;           // hero to screen left
		default:                               Shot.LookAtOffset.Y += Shift; break;           // hero to screen right
		}
	}
	Camera()->Play(MenuShots, true, PC->GetPawn());
	PC->SetViewTarget(Camera());
	Menu = CreateWidget<UFactoryMenuWidget>(PC, UFactoryMenuWidget::StaticClass());
	Menu->Init(this);
	Menu->AddToViewport(20);
	if (FParse::Param(FCommandLine::Get(), TEXT("FactoryKitShowOptions"))) { Menu->ShowOptionsPage(); }
	ShowLegend();
	SetMenuInput(true);
}

void UFactoryFlowSubsystem::ShowLegend()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (Legend || !PC)
	{
		return;
	}
	Legend = CreateWidget<UFactoryLegendWidget>(PC, UFactoryLegendWidget::StaticClass());
	Legend->SetRows(UFactoryKitSettings::Get()->LegendRows);
	Legend->AddToViewport(5);
	UpdateLegendMode();
	Legend->SetAutoplayHint(bAutoplay && State == EFactoryFlowState::Playing && bBotDriving);
}

void UFactoryFlowSubsystem::UpdateLegendMode()
{
	if (!Legend)
	{
		return;
	}
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	const bool bMenu = State == EFactoryFlowState::Menu;
	const bool bPlay = State == EFactoryFlowState::Playing;
	// With no in-play legend it keeps its start-screen corner and fades out there: switching corners on PLAY flashed the
	// controls list bottom-left, where the HUD legend sits (user, 2026-11-17: "the main game hud flashes for a second").
	Legend->SetMenuPlacement(!S->bLegendInPlay);
	Legend->SetLegendVisible(bMenu ? S->bLegendOnMenu : (bPlay && S->bLegendInPlay));
	Legend->SetFlashEnabled(bPlay && S->bKeyFlashInPlay);
}

void UFactoryFlowSubsystem::SetMenuInput(bool bMenu)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	if (bMenu)
	{
		FInputModeUIOnly Mode;
		if (Menu) { Mode.SetWidgetToFocus(Menu->TakeWidget()); }
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void UFactoryFlowSubsystem::MenuPlay()
{
	if (State != EFactoryFlowState::Menu)
	{
		return;
	}
	if (Menu) { Menu->FadeOut(); }
	if (UFactoryKitSettings::Get()->bSkipIntro)
	{
		// Let the menu fade (about MotionSeconds), then hand over at once.
		State = EFactoryFlowState::Intro;
		StateTime = 0.0f;
		SetMenuInput(false);
		UpdateLegendMode();
		TWeakObjectPtr<UFactoryFlowSubsystem> WeakThis(this);
		FTimerHandle H;
		GetWorld()->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			if (UFactoryFlowSubsystem* F = WeakThis.Get()) { F->FinishIntro(); }
		}), FMath::Max(UFactoryKitSettings::Get()->MotionSeconds * 1.5f, 0.05f), false);
		return;
	}
	StartIntro();
}

void UFactoryFlowSubsystem::MenuQuit()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}

void UFactoryFlowSubsystem::StartIntro()
{
	State = EFactoryFlowState::Intro;
	StateTime = 0.0f;
	SetMenuInput(false); // keyboard to the game (skip works); movement stays ignored until the intro ends
	UpdateLegendMode();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	TWeakObjectPtr<UFactoryFlowSubsystem> WeakThis(this);
	Camera()->Play(UFactoryKitSettings::Get()->IntroShots, false, PC ? PC->GetPawn() : nullptr, [WeakThis]()
	{
		if (UFactoryFlowSubsystem* F = WeakThis.Get()) { F->FinishIntro(); }
	});
	UE_LOG(LogTemp, Display, TEXT("[FactoryKit] Intro (%d shots)"), UFactoryKitSettings::Get()->IntroShots.Num());
}

void UFactoryFlowSubsystem::FinishIntro()
{
	if (State != EFactoryFlowState::Intro)
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, UFactoryKitSettings::Get()->IntroBlendSeconds, VTBlend_EaseInOut, 2.0f);
		}
		PC->ResetIgnoreMoveInput();
		PC->ResetIgnoreLookInput();
	}
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	State = EFactoryFlowState::Playing;
	StateTime = 0.0f;
	if (Legend)
	{
		UpdateLegendMode();
		Legend->SetAutoplayHint(bAutoplay && bBotDriving);
	}
	UE_LOG(LogTemp, Display, TEXT("[FactoryKit] StartGame"));
	OnStartGame.Broadcast();
}

namespace
{
	/** A view blend moves the camera in a straight line; if that line passes through a wall, cut instead (no clipping). */
	float ClearBlend(UWorld* World, const FVector& From, const FVector& To, float Seconds)
	{
		if (!World || Seconds <= 0.0f)
		{
			return 0.0f;
		}
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FactoryBlendPath), false);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->IsA<APawn>() || It->FindComponentByClass<USkeletalMeshComponent>()) { Q.AddIgnoredActor(*It); }
		}
		FHitResult Hit;
		return World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(UFactoryKitSettings::Get()->CameraClipRadius), Q) ? 0.0f : Seconds;
	}
}

void UFactoryFlowSubsystem::PlayEnding(bool bVictory, AActor* Focus)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	State = EFactoryFlowState::Ending;
	StateTime = 0.0f;
	if (Legend)
	{
		UpdateLegendMode();
		Legend->SetAutoplayHint(false);
	}
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	Camera()->Play(bVictory ? S->VictoryShots : S->DefeatShots, false, Focus ? Focus : (PC ? PC->GetPawn() : nullptr));
	if (PC)
	{
		const FVector From = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : Camera()->GetActorLocation();
		PC->SetViewTargetWithBlend(Camera(), ClearBlend(GetWorld(), From, Camera()->GetActorLocation(), 0.6f), VTBlend_EaseInOut, 2.0f);
	}
	UE_LOG(LogTemp, Display, TEXT("[FactoryKit] Ending: %s"), bVictory ? TEXT("victory") : TEXT("defeat"));
}

void UFactoryFlowSubsystem::ResumePlay(float BlendSeconds)
{
	// For games that restart a match in place (no level reload): back to the gameplay camera and the legend.
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->GetPawn())
	{
		const FVector From = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : PC->GetPawn()->GetActorLocation();
		PC->SetViewTargetWithBlend(PC->GetPawn(), ClearBlend(GetWorld(), From, PC->GetPawn()->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f), BlendSeconds), VTBlend_EaseInOut, 2.0f);
	}
	if (Cam)
	{
		Cam->Stop();
	}
	State = EFactoryFlowState::Playing;
	StateTime = 0.0f;
	if (Legend)
	{
		UpdateLegendMode();
		Legend->SetAutoplayHint(bAutoplay && bBotDriving);
	}
	UE_LOG(LogTemp, Display, TEXT("[FactoryKit] ResumePlay"));
}

void UFactoryFlowSubsystem::MarkRestart()
{
	FactoryFlowStatics::bSkipMenuNextLoad = true;
}

void UFactoryFlowSubsystem::ReturnToMenu()
{
	FactoryFlowStatics::bSkipMenuNextLoad = false;
	UGameplayStatics::OpenLevel(GetWorld(), FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true)));
}

void UFactoryFlowSubsystem::PulseLegend(FName ActionId)
{
	if (Legend) { Legend->Pulse(ActionId); }
}

bool UFactoryFlowSubsystem::HasMixClasses() const
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	return !S->VolumeMix.IsNull() && !S->MusicClass.IsNull() && !S->SfxClass.IsNull();
}

void UFactoryFlowSubsystem::ApplyOptions(bool bVideoToo)
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	UWorld* World = GetWorld();
	USoundMix* Mix = S->VolumeMix.LoadSynchronous();
	USoundClass* MasterC = S->MasterClass.LoadSynchronous();
	if (Mix && MasterC)
	{
		UGameplayStatics::SetSoundMixClassOverride(World, Mix, MasterC, Opts.Master, 1.0f, 0.0f, true);
		if (USoundClass* C = S->MusicClass.LoadSynchronous()) { UGameplayStatics::SetSoundMixClassOverride(World, Mix, C, Opts.Music, 1.0f, 0.0f, true); }
		if (USoundClass* C = S->SfxClass.LoadSynchronous()) { UGameplayStatics::SetSoundMixClassOverride(World, Mix, C, Opts.Sfx, 1.0f, 0.0f, true); }
		UGameplayStatics::PushSoundMixModifier(World, Mix);
	}
	else if (FAudioDeviceHandle Device = World->GetAudioDevice())
	{
		Device->SetTransientPrimaryVolume(Opts.Master);
	}
	if (bVideoToo)
	{
		if (UGameUserSettings* G = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			G->SetOverallScalabilityLevel(FMath::Clamp(Opts.Quality, 0, 3));
			G->SetFullscreenMode(Opts.bFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
			G->ApplySettings(false);
			G->SaveSettings();
		}
	}
}

void UFactoryFlowSubsystem::NotifyHumanInput(const FKey* Key)
{
	// Only a real game key takes the controls from the bot. The game never uses the mouse, and recorder hotkeys
	// (Alt+F9, Win+Alt+R, PrintScreen) or a click to focus the window used to hand the plane to nobody mid-recording
	// (user, 2026-11-16: "the plane just goes somewhere without finishing the game").
	if (bAutoplay && !FakeInputPending)
	{
		if (!Key || Key->IsMouseButton() || Key->IsModifierKey() || Key->IsTouch() || !Key->IsDigital())
		{
			return;
		}
		static const FName Ignored[] = { TEXT("F1"), TEXT("F2"), TEXT("F3"), TEXT("F4"), TEXT("F5"), TEXT("F6"), TEXT("F7"), TEXT("F8"),
			TEXT("F9"), TEXT("F10"), TEXT("F11"), TEXT("F12"), TEXT("LeftCommand"), TEXT("RightCommand"), TEXT("Pause"), TEXT("ScrollLock"),
			TEXT("CapsLock"), TEXT("NumLock"), TEXT("Tab") };
		for (const FName& N : Ignored) { if (Key->GetFName() == N) { return; } }
	}
	FakeInputPending = false;
	LastHumanInput = FPlatformTime::Seconds();
	if (Legend && Key) { Legend->PulseKey(*Key); }
	if (State == EFactoryFlowState::Intro && Key && Cam)
	{
		Cam->SkipToEnd(); // any key skips the fly-in
	}
	if (bAutoplay && bBotDriving)
	{
		bBotDriving = false;
		if (Legend) { Legend->SetAutoplayHint(false); }
		UE_LOG(LogTemp, Display, TEXT("[FactoryKit] Take-over: player has control"));
		OnControlChanged.Broadcast(false);
	}
}

void UFactoryFlowSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Elapsed += DeltaTime;
	StateTime += DeltaTime;
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();

	if (FakeInputAt >= 0.0f && Elapsed >= FakeInputAt)
	{
		FakeInputAt = -1.0f;
		UE_LOG(LogTemp, Display, TEXT("[FactoryKit] QA: simulated human input"));
		FakeInputPending = true;
		NotifyHumanInput(nullptr);
	}
	if (bAutoplay && !bBotDriving && FPlatformTime::Seconds() - LastHumanInput >= S->AutoplayResumeIdleSeconds)
	{
		bBotDriving = true;
		if (Legend) { Legend->SetAutoplayHint(State == EFactoryFlowState::Playing); }
		UE_LOG(LogTemp, Display, TEXT("[FactoryKit] Take-over: bot resumed after %.1f s idle"), S->AutoplayResumeIdleSeconds);
		OnControlChanged.Broadcast(true);
	}
	if (State == EFactoryFlowState::Menu && IsBotDriving())
	{
		const float Idle = static_cast<float>(FPlatformTime::Seconds() - LastHumanInput);
		if (!bBotPressed && StateTime >= S->MenuHoldSeconds && Idle >= S->MenuHoldSeconds && Menu)
		{
			bBotPressed = true;
			StateTime = S->MenuHoldSeconds;
			Menu->PressPlayVisibly();
			UE_LOG(LogTemp, Display, TEXT("[FactoryKit] Autoplay presses PLAY"));
		}
		else if (bBotPressed && StateTime >= S->MenuHoldSeconds + S->MenuPressSeconds)
		{
			MenuPlay();
		}
	}
	TakeQAShots(DeltaTime);
}

void UFactoryFlowSubsystem::TakeQAShots(float DeltaTime)
{
	if (ShotIndex < ShotTimes.Num() && Elapsed >= ShotTimes[ShotIndex])
	{
		const FString Name = FString::Printf(TEXT("FactoryKit_%02d_%s.png"), ShotIndex, *UEnum::GetValueAsString(State).RightChop(19));
		FScreenshotRequest::RequestScreenshot(Name, true, false);
		UE_LOG(LogTemp, Display, TEXT("[FactoryKit] QA screenshot %s at %.1f s"), *Name, Elapsed);
		++ShotIndex;
	}
}
