// Six Inches - the 4-state match: Ready (3-2-1-GO, input locked) -> Active (timer, stopped/hits) -> Victory | Defeat.
// Lives on the game mode; the HUD and audio only listen to its delegates.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SITypes.h"
#include "SIMatchComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSIOnMatchState, ESIMatchState, NewState, ESIMatchState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnCountdown, int32, SecondsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSIOnBombResolved, const FSIBombResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSIOnMatchEnded, bool, bVictory, ESIDefeatReason, Reason, const FText&, ReasonText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSIOnPrompt, FName, Id, const FText&, Text);

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIMatchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIMatchComponent();

	UPROPERTY(BlueprintAssignable, Category = "Match") FSIOnMatchState OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Match") FSIOnCountdown OnCountdownTick;
	UPROPERTY(BlueprintAssignable, Category = "Match") FSIOnBombResolved OnBombResolved;
	UPROPERTY(BlueprintAssignable, Category = "Match") FSIOnMatchEnded OnMatchEnded;
	UPROPERTY(BlueprintAssignable, Category = "Match") FSIOnPrompt OnPrompt;

	void Configure(float InRoundSeconds, int32 InCountdown, int32 InHitsToLose, int32 InTotalBombs);
	void StartCountdown();
	void ResolveBomb(const FSIBombResult& Result);
	void EndMatch(bool bVictory, ESIDefeatReason Reason);
	/** Show a first-time prompt once per match. */
	void Prompt(FName Id, const FText& Text);
	void SetDamageTaken(float Value) { Stats.DamageTaken = Value; }

	ESIMatchState GetState() const { return State; }
	float GetReadyRemaining() const { return ReadyRemaining; }
	int32 GetCountdownSeconds() const { return CountdownSeconds; }
	float GetTimeRemaining() const { return FMath::Max(0.0f, RoundSeconds - Stats.Elapsed); }
	float GetRoundSeconds() const { return RoundSeconds; }
	int32 GetHitsToLose() const { return HitsToLose; }
	const FSIMatchStats& GetStats() const { return Stats; }
	int32 GetResolved() const { return Resolved; }
	ESIDefeatReason GetDefeatReason() const { return DefeatReason; }
	FText GetReasonText() const { return ReasonText; }
	static FText ReasonToText(ESIDefeatReason Reason);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetState(ESIMatchState NewState);

	ESIMatchState State = ESIMatchState::Ready;
	float RoundSeconds = 90.0f;
	int32 CountdownSeconds = 3;
	int32 HitsToLose = 2;
	float ReadyRemaining = 4.0f;
	int32 LastCountdown = -1;
	int32 Resolved = 0;
	FSIMatchStats Stats;
	ESIDefeatReason DefeatReason = ESIDefeatReason::None;
	FText ReasonText;
	TSet<FName> PromptsShown;
	bool bStarted = false;
};
