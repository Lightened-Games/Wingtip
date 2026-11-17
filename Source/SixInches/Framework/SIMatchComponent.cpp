// Six Inches - match state machine.

#include "Framework/SIMatchComponent.h"
#include "SixInches.h"

#define LOCTEXT_NAMESPACE "SixInches"

USIMatchComponent::USIMatchComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USIMatchComponent::Configure(float InRoundSeconds, int32 InCountdown, int32 InHitsToLose, int32 InTotalBombs)
{
	RoundSeconds = InRoundSeconds;
	CountdownSeconds = FMath::Max(0, InCountdown);
	HitsToLose = FMath::Max(1, InHitsToLose);
	Stats.Total = InTotalBombs;
}

void USIMatchComponent::StartCountdown()
{
	if (CountdownSeconds <= 0)
	{
		// Straight into play: Ready (listeners reset) then Active in the same frame.
		bStarted = true;
		SetState(ESIMatchState::Ready);
		SetState(ESIMatchState::Active);
		return;
	}
	// One extra second up front holds the objective banner before 3-2-1.
	ReadyRemaining = CountdownSeconds + 1.0f;
	LastCountdown = -1;
	bStarted = true;
	SetState(ESIMatchState::Ready);
}

void USIMatchComponent::SetState(ESIMatchState NewState)
{
	const ESIMatchState Old = State;
	State = NewState;
	OnStateChanged.Broadcast(NewState, Old);
}

void USIMatchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bStarted)
	{
		return;
	}
	if (State == ESIMatchState::Ready)
	{
		ReadyRemaining -= DeltaTime;
		const int32 Secs = FMath::CeilToInt(ReadyRemaining);
		if (Secs != LastCountdown && Secs <= CountdownSeconds)
		{
			LastCountdown = Secs;
			OnCountdownTick.Broadcast(FMath::Max(Secs, 0));
		}
		if (ReadyRemaining <= 0.0f)
		{
			SetState(ESIMatchState::Active);
		}
	}
	else if (State == ESIMatchState::Active)
	{
		Stats.Elapsed += DeltaTime;
	}
}

void USIMatchComponent::ResolveBomb(const FSIBombResult& Result)
{
	if (State != ESIMatchState::Active)
	{
		return;
	}
	++Resolved;
	if (Result.bTipped)
	{
		// Tipped onto buildings still stopped it reaching the town, but it counts against you.
		++Stats.Stopped;
		if (Result.Ground.bHit)
		{
			++Stats.Hits;
		}
		else if (Result.Ground.Kind == ESIZoneKind::Field)
		{
			++Stats.InFields;
		}
	}
	else
	{
		++Stats.Hits;
	}
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Bomb %d resolved: %s tipped=%d hit=%d  (stopped %d, hits %d/%d, resolved %d/%d)"),
		Result.BombIndex, *SIZones::Label(Result.Ground.Kind).ToString(), Result.bTipped ? 1 : 0, Result.Ground.bHit || !Result.bTipped ? 1 : 0,
		Stats.Stopped, Stats.Hits, HitsToLose, Resolved, Stats.Total);
	OnBombResolved.Broadcast(Result);

	if (Stats.Hits >= HitsToLose)
	{
		EndMatch(false, ESIDefeatReason::TownHit);
	}
	else if (Resolved >= Stats.Total)
	{
		EndMatch(true, ESIDefeatReason::None);
	}
}

void USIMatchComponent::EndMatch(bool bVictory, ESIDefeatReason Reason)
{
	if (State == ESIMatchState::Victory || State == ESIMatchState::Defeat)
	{
		return;
	}
	DefeatReason = bVictory ? ESIDefeatReason::None : Reason;
	ReasonText = bVictory ? LOCTEXT("WinReason", "The wave is down. The town sleeps tonight.")
		: (Reason == ESIDefeatReason::TownHit ? FText::Format(LOCTEXT("ReasonTownN", "{0} bombs came down on homes."), FText::AsNumber(HitsToLose)) : ReasonToText(Reason));
	SetState(bVictory ? ESIMatchState::Victory : ESIMatchState::Defeat);
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Match over: %s (%s) stopped %d fields %d hits %d time %.1f"),
		bVictory ? TEXT("VICTORY") : TEXT("DEFEAT"), *ReasonText.ToString(), Stats.Stopped, Stats.InFields, Stats.Hits, Stats.Elapsed);
	OnMatchEnded.Broadcast(bVictory, DefeatReason, ReasonText);
}

FText USIMatchComponent::ReasonToText(ESIDefeatReason Reason)
{
	switch (Reason)
	{
	case ESIDefeatReason::TownHit:  return LOCTEXT("ReasonTown", "Too many bombs came down on homes.");
	case ESIDefeatReason::Flak:     return LOCTEXT("ReasonFlak", "Shot down by our own flak.");
	case ESIDefeatReason::Cable:    return LOCTEXT("ReasonCable", "Caught a balloon cable.");
	case ESIDefeatReason::WingTorn: return LOCTEXT("ReasonWing", "The wingtip tore off.");
	case ESIDefeatReason::Ground:   return LOCTEXT("ReasonGround", "Flew into the ground.");
	case ESIDefeatReason::TimeUp:   return LOCTEXT("ReasonTime", "Time ran out with bombs still heading for the town.");
	default:                        return FText::GetEmpty();
	}
}

void USIMatchComponent::Prompt(FName Id, const FText& Text)
{
	if (State != ESIMatchState::Active || PromptsShown.Contains(Id))
	{
		return;
	}
	PromptsShown.Add(Id);
	OnPrompt.Broadcast(Id, Text);
}

#undef LOCTEXT_NAMESPACE
