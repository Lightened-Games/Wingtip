// FactoryKit flow (design guide 2.5): Start screen -> Intro shots -> (game countdown + play) -> Ending shots.
// Also: take-over autoplay (real input hands control to the player; the bot resumes after idle), options storage,
// the bottom-left legend, and QA screenshots (-FactoryKitShots=1.5,6,12; UI included).
//
// Game glue (all of it):
//   UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
//   Flow->OnStartGame.AddDynamic(this, &AMyGameMode::StartCountdown);   // fired after the intro (or at once on restart)
//   Flow->Begin(bAutoplay);                                               // first Tick of the game mode (pawn exists)
//   Flow->PlayEnding(bVictory, FocusActor);                               // when the match ends
//   Flow->MarkRestart(); <reload level>                                   // [R]: the next load skips menu + intro
//   Flow->ReturnToMenu();                                                 // [Esc] in play
//   if (Flow->IsBotDriving()) { <bot acts> }                              // take-over: the bot only acts when this is true
//   Flow->PulseLegend(TEXT("Jump"));                                      // from the action handler (player and bot)
//   UFactoryFlowSubsystem::GetMouseSensitivity() / GetInvertY()           // in the look input handler

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FactoryKitSettings.h"
#include "FactoryFlowSubsystem.generated.h"

class UFactoryMenuWidget;
class UFactoryLegendWidget;
class AFactoryCinematicCamera;
class FFactoryInputWatcher;

UENUM(BlueprintType)
enum class EFactoryFlowState : uint8 { None, Menu, Intro, Playing, Ending };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFactoryStartGameSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFactoryControlChangedSignature, bool, bBotDriving);

/** Player options, saved to GameUserSettings.ini [FactoryKit.Options]. */
struct FFactoryOptions
{
	float Master = 1.0f, Music = 0.8f, Sfx = 1.0f, Sensitivity = 1.0f;
	bool bInvertY = false, bFullscreen = true;
	int32 Quality = 3; // 0 low .. 3 epic
	void Load();
	void Save() const;
};

UCLASS()
class UFactoryFlowSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UFactoryFlowSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void Begin(bool bInAutoplay);
	void PlayEnding(bool bVictory, AActor* Focus);
	/** In-place restart (no level reload): gameplay camera + legend back, state Playing. */
	void ResumePlay(float BlendSeconds = 0.5f);
	void MarkRestart();
	void ReturnToMenu();
	void PulseLegend(FName ActionId);

	bool IsAutoplay() const { return bAutoplay; }
	/** True while the bot should drive: autoplay on and no real input for AutoplayResumeIdleSeconds. */
	bool IsBotDriving() const { return bAutoplay && bBotDriving; }
	EFactoryFlowState GetState() const { return State; }

	static float GetMouseSensitivity();
	static bool GetInvertY();

	UPROPERTY(BlueprintAssignable) FFactoryStartGameSignature OnStartGame;
	UPROPERTY(BlueprintAssignable) FFactoryControlChangedSignature OnControlChanged;

	// Used by the menu widget.
	void MenuPlay();
	void MenuQuit();
	FFactoryOptions& Options() { return Opts; }
	void ApplyOptions(bool bVideoToo);
	bool HasMixClasses() const;
	/** Called by the input watcher on any real key, mouse button or mouse move past the dead zone. */
	void NotifyHumanInput(const FKey* Key);

private:
	void StartIntro();
	void FinishIntro();
	void ShowLegend();
	/** Applies the legend settings for the current state (list on the menu and/or in play, key flash in play). */
	void UpdateLegendMode();
	void SetMenuInput(bool bMenu);
	AFactoryCinematicCamera* Camera();
	void TakeQAShots(float DeltaTime);

	UPROPERTY() TObjectPtr<UFactoryMenuWidget> Menu;
	UPROPERTY() TObjectPtr<UFactoryLegendWidget> Legend;
	UPROPERTY() TObjectPtr<AFactoryCinematicCamera> Cam;

	TSharedPtr<FFactoryInputWatcher> Watcher;
	FFactoryOptions Opts;
	EFactoryFlowState State = EFactoryFlowState::None;
	bool bAutoplay = false;
	bool bBotDriving = true;
	double LastHumanInput = -1.0e9;
	bool FakeInputPending = false;  // QA -FactoryKitFakeInputAt: counts as a take-over without a key
	float StateTime = 0.0f;
	bool bBotPressed = false;
	float Elapsed = 0.0f;
	TArray<float> ShotTimes;
	int32 ShotIndex = 0;
	float FakeInputAt = -1.0f;
};
