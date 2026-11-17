// Six Inches - game mode.

#include "Framework/SIGameMode.h"
#include "Framework/SIMatchComponent.h"
#include "Framework/SIWaveDirectorComponent.h"
#include "Framework/SIPlayerController.h"
#include "Framework/SIAutoplayComponent.h"
#include "Player/SIPlanePawn.h"
#include "Components/SIFlightComponent.h"
#include "Components/SIWingtipComponent.h"
#include "Components/SIHealthComponent.h"
#include "Components/SITippableComponent.h"
#include "World/SIAirspaceSubsystem.h"
#include "Data/SIMatchConfig.h"
#include "Audio/SISynth.h"
#include "Feedback/SIPuffActor.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "FactoryKit/FactoryMenuWidget.h"
#include "World/SIWingman.h"
#include "SixInches.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "RenderCore.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Info.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Controller.h"
#include "Components/AudioComponent.h"
#include "RHI.h"
#include "UnrealEngine.h"

#define LOCTEXT_NAMESPACE "SixInches"

namespace SIModeStatics
{
	/** Process-lifetime counters (survive the level reload used for restarts). */
	int32 RunsCompleted = 0;
	bool bShotsScheduled = false;
	bool bAudioDumped = false;
}

ASIGameMode::ASIGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Match = CreateDefaultSubobject<USIMatchComponent>(TEXT("Match"));
	Wave = CreateDefaultSubobject<USIWaveDirectorComponent>(TEXT("Wave"));
	DefaultPawnClass = ASIPlanePawn::StaticClass();
	PlayerControllerClass = ASIPlayerController::StaticClass();
	Config = TSoftObjectPtr<USIMatchConfig>(FSoftObjectPath(TEXT("/Game/SixInches/Data/DA_SIMatchConfig.DA_SIMatchConfig")));
}

void ASIGameMode::BeginPlay()
{
	Super::BeginPlay();
	// One step limit for the whole world: the flight code caps its step at 0.05 s, so on a slow frame the bombs and
	// the clock must not run ahead of the planes (a 1 fps hitch let the wave escape: 2026-11-16).
	if (AWorldSettings* WS = GetWorldSettings())
	{
		WS->MaxUndilatedFrameTime = 0.05f;
	}
	const TCHAR* Cmd = FCommandLine::Get();

	FString DumpDir;
	if (!SIModeStatics::bAudioDumped && FParse::Value(Cmd, TEXT("SIDumpAudio="), DumpDir, false))
	{
		SIModeStatics::bAudioDumped = true;
		if (DumpDir.Equals(TEXT("lib"), ESearchCase::IgnoreCase))
		{
			DumpDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("asset-library/Audio/Generated/SixInches"));
		}
		USIAudioLibrary::DumpAll(DumpDir);
	}

	if (ScalabilityLevel >= 0 && GEngine && !FParse::Param(Cmd, TEXT("nullrhi")))
	{
		if (UGameUserSettings* Settings = GEngine->GetGameUserSettings())
		{
			Settings->SetOverallScalabilityLevel(ScalabilityLevel);
			Settings->SetResolutionScaleNormalized(1.0f);
			Settings->ApplySettings(false);
		}
	}
	GAreScreenMessagesEnabled = false;

	LoadedConfig = Config.LoadSynchronous();
	if (!LoadedConfig)
	{
		LoadedConfig = NewObject<USIMatchConfig>(this, TEXT("DefaultMatchConfig"));
		UE_LOG(LogSixInches, Warning, TEXT("[SixInches] DA_SIMatchConfig missing: code defaults."));
	}
	if (AWorldSettings* WS = GetWorldSettings())
	{
		WS->KillZ = KillZ;
		WS->bEnableWorldBoundsChecks = true;
	}
	if (USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>())
	{
		Air->TownLineX = LoadedConfig->TownLineX;
		Air->FlakStartX = LoadedConfig->FlakStartX;
		Air->SetEscalation(1.0f);
		Air->bHostilitiesActive = false;
	}

	TArray<FSIWaveRow> Rows;
	if (LoadedConfig->WaveTable)
	{
		TArray<FSIWaveRow*> Ptrs;
		LoadedConfig->WaveTable->GetAllRows<FSIWaveRow>(TEXT("SIWave"), Ptrs);
		for (const FSIWaveRow* R : Ptrs) { Rows.Add(*R); }
	}
	if (Rows.Num() == 0)
	{
		UE_LOG(LogSixInches, Error, TEXT("[SixInches] Wave table empty: using one fallback bomb."));
		Rows.Add(FSIWaveRow());
	}
	Wave->SetRows(Rows);
	Wave->SetTownLineX(LoadedConfig->TownLineX);
	Wave->OnBombLanded.AddDynamic(this, &ASIGameMode::HandleBombLanded);

	Match->Configure(LoadedConfig->RoundSeconds, LoadedConfig->CountdownSeconds, LoadedConfig->HitsToLose, Wave->GetTotal());
	Match->OnStateChanged.AddDynamic(this, &ASIGameMode::HandleState);
	Match->OnCountdownTick.AddDynamic(this, &ASIGameMode::HandleCountdown);
	Match->OnMatchEnded.AddDynamic(this, &ASIGameMode::HandleMatchEnded);

	FParse::Value(Cmd, TEXT("SIDefeat="), ForcedDefeat);
	FParse::Value(Cmd, TEXT("SIShotTag="), ShotTag);
	bEventShots = FParse::Param(Cmd, TEXT("SIEventShots"));
	bPerfLog = FParse::Param(Cmd, TEXT("SIPerfLog"));

	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		WirePawn(PC->GetPawn());
	}
	if (FParse::Param(Cmd, TEXT("SIFxTest")))
	{
		// QA: one of each puff kind in front of the start view.
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				const FVector Base = Pawn->GetActorLocation() + FVector(9000.0f, 0.0f, -1500.0f);
				FSIPuffSpec Smoke; Smoke.Life = 30.0f; Smoke.SizeStart = 1500.0f; Smoke.SizeEnd = 2500.0f; Smoke.Color = FLinearColor(0.05f, 0.05f, 0.05f); Smoke.Opacity = 1.0f;
				ASIPuffActor::Spawn(GetWorld(), Smoke, Base + FVector(0.0f, -2500.0f, 0.0f));
				FSIPuffSpec White = Smoke; White.Color = FLinearColor(0.8f, 0.8f, 0.8f);
				ASIPuffActor::Spawn(GetWorld(), White, Base + FVector(0.0f, 0.0f, 0.0f));
				FSIPuffSpec Glow; Glow.Material = ESIPuffMaterial::Glow; Glow.Life = 30.0f; Glow.SizeStart = 1500.0f; Glow.SizeEnd = 2500.0f; Glow.Color = FLinearColor(1.0f, 0.6f, 0.3f); Glow.Glow = 2.0f;
				ASIPuffActor::Spawn(GetWorld(), Glow, Base + FVector(0.0f, 2500.0f, 0.0f));
			}
		}
	}
	// Standing patrols are up from the first frame (the start screen sky has traffic); hunters join after GO.
	for (const FSIWingmanSpec& S : LoadedConfig->Squadron)
	{
		if (S.Role == ESIWingmanRole::Patrol)
		{
			const FVector C = S.PatrolCenter * SI_CM;
			SpawnWingman(S, C + FVector(S.PatrolRadius * SI_CM, 0.0f, 0.0f), FRotator(0.0f, S.Seed % 2 == 0 ? 90.0f : -90.0f, 0.0f), S.CruiseSpeed);
		}
	}
	ScheduleQAScreenshots();
	CheckCommandLineAutoplay();
	// Menu sounds come from the runtime synth (no sound assets).
	UFactoryMenuWidget::UISoundHook = [](const UObject* Context, bool bSelect)
	{
		USIAudioLibrary::PlaySynth2D(Context, bSelect ? USIAudioLibrary::Go() : USIAudioLibrary::Prompt(), bSelect ? 0.6f : 0.35f);
	};
	// The countdown starts when the start screen and intro hand over (FactoryKit OnStartGame), see Tick.
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Match configured: %.0f s, %d bombs, %d hits to lose, autoplay %d"),
		LoadedConfig->RoundSeconds, Rows.Num(), LoadedConfig->HitsToLose, bAutoplayActive ? 1 : 0);
}

void ASIGameMode::CheckCommandLineAutoplay()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("autoplay")))
	{
		UE_LOG(LogSixInches, Warning, TEXT("Autoplay mode requested via command line."));
		bAutoplayActive = true;
		SetupAutoplayController();
	}
}

void ASIGameMode::SetupAutoplayController()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (USIAutoplayComponent* Bot = PC ? PC->FindComponentByClass<USIAutoplayComponent>() : nullptr)
	{
		Bot->StartAutoplay(ForcedDefeat);
	}
	else
	{
		UE_LOG(LogSixInches, Error, TEXT("[SixInches] Autoplay requested but the controller has no autoplay component."));
	}
}

void ASIGameMode::ScheduleQAScreenshots()
{
	FString ShotList;
	if (SIModeStatics::bShotsScheduled || !FParse::Value(FCommandLine::Get(), TEXT("SIShots="), ShotList, false))
	{
		return;
	}
	SIModeStatics::bShotsScheduled = true;
	TArray<FString> Times;
	ShotList.ParseIntoArray(Times, TEXT(","), true);
	for (int32 i = 0; i < Times.Num(); ++i)
	{
		const float Delay = FCString::Atof(*Times[i]);
		if (Delay <= 0.0f)
		{
			continue;
		}
		const FString Name = FString::Printf(TEXT("SIShot%s_%02d_%03ds"), *ShotTag, i, FMath::RoundToInt(Delay));
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(Name, true, false);
			UE_LOG(LogSixInches, Display, TEXT("[SixInches] Screenshot %s"), *Name);
		}), Delay, false);
	}
}

void ASIGameMode::EventShot(const TCHAR* Tag)
{
	if (!bEventShots || EventShotCount > 40)
	{
		return;
	}
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (Now - LastEventShot < 1.2f)
	{
		return;
	}
	LastEventShot = Now;
	const FString Name = FString::Printf(TEXT("SIEvent%s_%02d_%s_%03ds"), *ShotTag, EventShotCount++, Tag, FMath::RoundToInt(Match->GetStats().Elapsed));
	// Let the event's visuals land on screen first.
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [Name]()
	{
		FScreenshotRequest::RequestScreenshot(Name, true, false);
		UE_LOG(LogSixInches, Display, TEXT("[SixInches] Event screenshot %s"), *Name);
	}), 0.25f, false);
}

void ASIGameMode::WirePawn(APawn* Pawn)
{
	if (!Pawn || WiredPawn.Get() == Pawn)
	{
		return;
	}
	WiredPawn = Pawn;
	if (USIFlightComponent* F = Pawn->FindComponentByClass<USIFlightComponent>())
	{
		F->Initialise(Pawn->GetActorLocation(), FRotator(0.0f, Pawn->GetActorRotation().Yaw, 0.0f), StartSpeed);
		F->SetLocked(true);
		F->OnCrashed.AddDynamic(this, &ASIGameMode::HandleCrashed);
	}
	if (USIHealthComponent* H = Pawn->FindComponentByClass<USIHealthComponent>())
	{
		H->OnDepleted.AddDynamic(this, &ASIGameMode::HandleDepleted);
		H->OnDamaged.AddDynamic(this, &ASIGameMode::HandleDamaged);
	}
	if (USIWingtipComponent* W = Pawn->FindComponentByClass<USIWingtipComponent>())
	{
		W->OnSlotChanged.AddDynamic(this, &ASIGameMode::HandleSlot);
		W->OnPreviewChanged.AddDynamic(this, &ASIGameMode::HandlePreview);
		W->OnBombTipped.AddDynamic(this, &ASIGameMode::HandleTipped);
		W->OnBandChanged.AddDynamic(this, &ASIGameMode::HandleBand);
	}
}

void ASIGameMode::SetPlaneActive(bool bActive, bool bEnded)
{
	APawn* Pawn = WiredPawn.Get();
	if (!Pawn)
	{
		return;
	}
	if (USIFlightComponent* F = Pawn->FindComponentByClass<USIFlightComponent>())
	{
		F->SetLocked(!bActive && !bEnded);
		F->SetAutoLevel(bEnded);
	}
	if (USIHealthComponent* H = Pawn->FindComponentByClass<USIHealthComponent>()) { H->SetDamageEnabled(bActive); }
	if (USIWingtipComponent* W = Pawn->FindComponentByClass<USIWingtipComponent>()) { W->SetEnabled(bActive); }
}

void ASIGameMode::HandleState(ESIMatchState NewState, ESIMatchState OldState)
{
	USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
	switch (NewState)
	{
	case ESIMatchState::Ready:
		SetPlaneActive(false, false);
		break;
	case ESIMatchState::Active:
		SetPlaneActive(true, false);
		Wave->SetRunning(true);
		for (const FSIWingmanSpec& S : LoadedConfig->Squadron)
		{
			if (S.Role != ESIWingmanRole::Hunter)
			{
				continue;
			}
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, S]()
			{
				const APawn* Pawn = WiredPawn.Get();
				if (!Pawn || Match->GetState() != ESIMatchState::Active)
				{
					return;
				}
				const FRotator PY(0.0f, Pawn->GetActorRotation().Yaw, 0.0f);
				SpawnWingman(S, Pawn->GetActorLocation() + PY.RotateVector(S.EntryOffset * SI_CM), PY, Pawn->GetVelocity().Size() / SI_CM + WingmanJoinOvertake);
				Match->Prompt(TEXT("squadron"), FText::Format(LOCTEXT("PromptJoin", "{0} joining. The squadron takes the far lanes"), FText::FromName(S.Callsign)));
			}), FMath::Max(S.JoinDelay, 0.1f), false);
		}
		if (Air) { Air->bHostilitiesActive = true; }
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::Go());
		break;
	case ESIMatchState::Victory:
	case ESIMatchState::Defeat:
		SetPlaneActive(false, true);
		if (Air) { Air->bHostilitiesActive = false; }
		break;
	}
}

void ASIGameMode::HandleCountdown(int32 SecondsLeft)
{
	if (SecondsLeft > 0)
	{
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::CountdownBeep());
	}
}

void ASIGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPerfLog)
	{
		// QA (-SIPerfLog): real frame times; spikes logged with what was happening, a summary every 10 s.
		const double Now = FPlatformTime::Seconds();
		if (PerfLast > 0.0)
		{
			const float Ms = static_cast<float>((Now - PerfLast) * 1000.0);
			++PerfFrames;
			PerfSum += Ms;
			PerfMax = FMath::Max(PerfMax, Ms);
			if (Ms > 33.0f) { ++PerfOver33; }
			if (Ms > 50.0f)
			{
				const APawn* PP = WiredPawn.Get();
				const USIWingtipComponent* WT = PP ? PP->FindComponentByClass<USIWingtipComponent>() : nullptr;
				UE_LOG(LogSixInches, Display, TEXT("[SixInches][Perf] spike %.0f ms at t=%.1f slot=%d actors=%d | game %.0f render %.0f gpu %.0f ms"), Ms, Match->GetStats().Elapsed,
					WT && WT->IsInSlot() ? 1 : 0, GetWorld()->GetActorCount(), FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
					FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
			}
			if (Now - PerfWindowStart > 10.0)
			{
				UE_LOG(LogSixInches, Display, TEXT("[SixInches][Perf] avg %.1f ms (%.0f fps) max %.0f ms, %d of %d frames over 33 ms | dilation %.3f game dt %.4f t=%.1f"), PerfSum / PerfFrames, 1000.0f / (PerfSum / PerfFrames), PerfMax, PerfOver33, PerfFrames,
					GetWorldSettings()->TimeDilation, GetWorld()->GetDeltaSeconds(), Match->GetStats().Elapsed);
				TMap<FName, int32> ByClass;
				for (TActorIterator<AActor> It(GetWorld()); It; ++It) { ByClass.FindOrAdd(It->GetClass()->GetFName())++; }
				ByClass.ValueSort([](int32 A, int32 B) { return A > B; });
				FString Top;
				int32 N = 0;
				for (const TPair<FName, int32>& C : ByClass) { if (N++ >= 4) { break; } Top += FString::Printf(TEXT("%s=%d "), *C.Key.ToString(), C.Value); }
				UE_LOG(LogSixInches, Display, TEXT("[SixInches][Perf] actors: %s"), *Top);
				PerfWindowStart = Now; PerfFrames = 0; PerfSum = 0.0f; PerfMax = 0.0f; PerfOver33 = 0;
			}
		}
		else
		{
			PerfWindowStart = Now;
		}
		PerfLast = Now;
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		WirePawn(PC->GetPawn());
	}
	// First tick with the pawn: start screen -> intro fly-in -> countdown (or straight to the countdown after [R]).
	if (!bFlowBegun && WiredPawn.IsValid())
	{
		bFlowBegun = true;
		if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
		{
			Flow->OnStartGame.AddDynamic(this, &ASIGameMode::HandleFlowStart);
			Flow->Begin(bAutoplayActive);
		}
		else
		{
			HandleFlowStart();
		}
	}
	if (Match->GetState() != ESIMatchState::Active)
	{
		return;
	}
	APawn* Pawn = WiredPawn.Get();
	USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
	if (Pawn && Air)
	{
		const FVector P = Pawn->GetActorLocation();
		if (P.X > Air->FlakStartX - 25000.0f)
		{
			Match->Prompt(TEXT("flak"), LOCTEXT("PromptFlak", "Our own guns fire at anything up here"));
		}
		if (const USIFlightComponent* F = Pawn->FindComponentByClass<USIFlightComponent>())
		{
			if (F->GetTurbulence() > 0.25f)
			{
				Match->Prompt(TEXT("smoke"), LOCTEXT("PromptSmoke", "Hot air off the blast. Expect bumps"));
			}
		}
		if (Air->DistanceToNearestCable(P) < 90000.0f)
		{
			Match->Prompt(TEXT("cable"), LOCTEXT("PromptCable", "Balloon cables. Fly around them"));
		}
	}
	if (const USIHealthComponent* H = Pawn ? Pawn->FindComponentByClass<USIHealthComponent>() : nullptr)
	{
		Match->SetDamageTaken(H->GetDamageTaken());
	}

	// Time up: whatever is still cruising is over the town.
	if (Match->GetTimeRemaining() <= 0.0f)
	{
		int32 StillFlying = 0;
		if (Air)
		{
			for (AActor* B : Air->GetBombs())
			{
				const USITippableComponent* T = B ? B->FindComponentByClass<USITippableComponent>() : nullptr;
				if (T && T->GetState() != ESIAutopilotState::Done && T->GetState() != ESIAutopilotState::Tipped)
				{
					++StillFlying;
				}
			}
		}
		const int32 Unlaunched = FMath::Max(0, Wave->GetTotal() - Match->GetResolved() - (Air ? Air->GetBombs().Num() : 0));
		const bool bLose = Match->GetStats().Hits + StillFlying + Unlaunched >= Match->GetHitsToLose();
		Match->EndMatch(!bLose, Match->GetStats().Hits >= Match->GetHitsToLose() ? ESIDefeatReason::TownHit : ESIDefeatReason::TimeUp);
	}
}

ASIWingman* ASIGameMode::SpawnWingman(const FSIWingmanSpec& Spec, const FVector& Location, const FRotator& Rotation, float SpeedMS)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASIWingman* W = GetWorld()->SpawnActor<ASIWingman>(ASIWingman::StaticClass(), Location, Rotation, Params);
	if (W)
	{
		W->Configure(Spec, SpeedMS);
		W->OnTipped.AddDynamic(this, &ASIGameMode::HandleWingmanTipped);
	}
	return W;
}

void ASIGameMode::HandleWingmanTipped(FName Callsign, AActor* Bomb, FSIGroundClass Predicted)
{
	Match->Prompt(FName(*FString::Printf(TEXT("wing_%s"), Bomb ? *Bomb->GetName() : TEXT("x"))),
		FText::Format(LOCTEXT("PromptWingTip", "{0} tipped one over the fields"), FText::FromName(Callsign)));
	EventShot(TEXT("WingTip"));
}

void ASIGameMode::HandleFlowStart()
{
	if (Match->GetState() == ESIMatchState::Ready && !bCountdownStarted)
	{
		bCountdownStarted = true;
		Match->StartCountdown();
	}
}

void ASIGameMode::HandleBombLanded(const FSIBombResult& Result)
{
	Match->ResolveBomb(Result);
	if (USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>())
	{
		Air->SetEscalation(FMath::Min(1.0f + EscalationPerStop * Match->GetStats().Stopped, FMath::Max(MaxEscalation, 1.0f)));
	}
	if (Result.Ground.bHit || !Result.bTipped)
	{
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::TownHit());
	}
	EventShot(Result.Ground.bHit ? TEXT("ImpactHit") : TEXT("ImpactSafe"));
}

void ASIGameMode::HandleSlot(bool bInSlot, AActor* Target)
{
	if (bInSlot)
	{
		Match->Prompt(TEXT("slot"), LOCTEXT("PromptSlot", "Ease your wingtip under its wing"));
		EventShot(TEXT("Slot"));
	}
}

void ASIGameMode::HandleBand(bool bInBand)
{
	if (bInBand)
	{
		EventShot(TEXT("Band"));
	}
}

void ASIGameMode::HandlePreview(FSIGroundClass Predicted)
{
	if (Predicted.bHit)
	{
		Match->Prompt(TEXT("unsafe"), LOCTEXT("PromptUnsafe", "Not over the houses. Hold it a little longer"));
		EventShot(TEXT("PreviewHit"));
	}
}

void ASIGameMode::HandleTipped(AActor* Bomb, FSIGroundClass Predicted)
{
	EventShot(TEXT("Tip"));
}

void ASIGameMode::HandleDamaged(float Amount, ESIDamageSource Source, float HealthAfter)
{
	EventShot(Source == ESIDamageSource::Flak ? TEXT("Flak") : TEXT("Contact"));
}

void ASIGameMode::HandleDepleted(ESIDamageSource Source)
{
	LosePlane(Source);
}

void ASIGameMode::HandleCrashed(ESIDamageSource Source)
{
	if (APawn* Pawn = WiredPawn.Get())
	{
		if (USIHealthComponent* H = Pawn->FindComponentByClass<USIHealthComponent>())
		{
			H->SetDamageEnabled(true);
			H->Kill(Source);
		}
	}
	LosePlane(Source);
}

void ASIGameMode::LosePlane(ESIDamageSource Source)
{
	if (bPlaneLost || Match->GetState() != ESIMatchState::Active)
	{
		return;
	}
	bPlaneLost = true;
	APawn* Pawn = WiredPawn.Get();
	if (Pawn)
	{
		USIFlightComponent* F = Pawn->FindComponentByClass<USIFlightComponent>();
		if (F && !F->IsCrashed())
		{
			// Shot down / wing torn: the airframe goes; the game feel component plays the crash on OnCrashed only,
			// so broadcast it here for the visuals.
			F->OnCrashed.Broadcast(Source);
		}
		Pawn->SetActorHiddenInGame(true);
	}
	ESIDefeatReason Reason = ESIDefeatReason::Ground;
	switch (Source)
	{
	case ESIDamageSource::Flak:    Reason = ESIDefeatReason::Flak; break;
	case ESIDamageSource::Cable:   Reason = ESIDefeatReason::Cable; break;
	case ESIDamageSource::Contact: Reason = ESIDefeatReason::WingTorn; break;
	default:                       Reason = ESIDefeatReason::Ground; break;
	}
	Match->EndMatch(false, Reason);
}

void ASIGameMode::HandleMatchEnded(bool bVictory, ESIDefeatReason Reason, const FText& ReasonText)
{
	Wave->SetRunning(false);
	USIAudioLibrary::PlaySynth2D(this, bVictory ? USIAudioLibrary::Fanfare() : USIAudioLibrary::Defeat());
	EventShot(bVictory ? TEXT("Victory") : TEXT("Defeat"));
	WriteTelemetry(bVictory);
	// Ending camera shot on the plane (or where it went down).
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		Flow->PlayEnding(bVictory, WiredPawn.Get());
	}
	FreezeWorldForEnding();
	if (bAutoplayActive)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (USIAutoplayComponent* Bot = PC->FindComponentByClass<USIAutoplayComponent>())
			{
				Bot->StopAutoplay();
			}
		}
		GetWorldTimerManager().SetTimer(AutoplayEndTimer, this, &ASIGameMode::AutoplayRunOver, FMath::Max(AutoplayRestartDelay, 0.1f), false);
	}
}

void ASIGameMode::FreezeWorldForEnding()
{
	if (!bFreezeOnEnd)
	{
		return;
	}
	// Everything in the scene stops (actor time dilation 0); the controller, camera manager, ending camera, game mode and
	// world timers keep running, so the end screen, [R] and the autoplay restart still work. Engine and jet loops fade out.
	int32 Frozen = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == this || A->IsA<AController>() || A->IsA<APlayerCameraManager>() || A->IsA<ACameraActor>() || A->IsA<AInfo>() || A->IsA<AHUD>())
		{
			continue;
		}
		A->CustomTimeDilation = 0.0f;
		TArray<UAudioComponent*> Sounds;
		A->GetComponents(Sounds);
		for (UAudioComponent* S : Sounds)
		{
			if (S && S->IsPlaying()) { S->FadeOut(0.8f, 0.0f); }
		}
		++Frozen;
	}
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Ending: froze %d actors"), Frozen);
}

void ASIGameMode::AutoplayRunOver()
{
	++SIModeStatics::RunsCompleted;
	int32 MaxRuns = 0;
	const bool bHasRuns = FParse::Value(FCommandLine::Get(), TEXT("SIRuns="), MaxRuns);
	if (FParse::Param(FCommandLine::Get(), TEXT("SIExit")) || FParse::Param(FCommandLine::Get(), TEXT("autoplayexit")) || (bHasRuns && SIModeStatics::RunsCompleted >= MaxRuns))
	{
		UE_LOG(LogSixInches, Display, TEXT("[SixInches][Autoplay] %d run(s) complete - quitting."), SIModeStatics::RunsCompleted);
		UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
		return;
	}
	RequestRestart();
}

void ASIGameMode::RequestRestart()
{
	// [R] restarts skip the start screen and the intro (straight to the countdown).
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		Flow->MarkRestart();
	}
	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void ASIGameMode::WriteTelemetry(bool bVictory) const
{
	const FSIMatchStats& S = Match->GetStats();
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("result"), bVictory ? TEXT("VICTORY") : TEXT("DEFEAT"));
	J->SetStringField(TEXT("reason"), Match->GetReasonText().ToString());
	J->SetBoolField(TEXT("autoplay"), bAutoplayActive);
	J->SetNumberField(TEXT("total"), S.Total);
	J->SetNumberField(TEXT("stopped"), S.Stopped);
	J->SetNumberField(TEXT("in_fields"), S.InFields);
	J->SetNumberField(TEXT("hits"), S.Hits);
	J->SetNumberField(TEXT("elapsed"), S.Elapsed);
	J->SetNumberField(TEXT("damage_taken"), S.DamageTaken);
	J->SetNumberField(TEXT("run"), SIModeStatics::RunsCompleted);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(J, Writer);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Telemetry") / FString::Printf(TEXT("match_%s_r%d.json"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), SIModeStatics::RunsCompleted);
	FFileHelper::SaveStringToFile(Out, *Path);
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Telemetry: %s"), *Out);
}

#undef LOCTEXT_NAMESPACE
