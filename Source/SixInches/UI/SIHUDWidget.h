// Six Inches - native C++ HUD. Theme: RAF 1944 cockpit instruments and ops-room plotting boards. Corner clusters sit on
// dark translucent plates with thin amber rules and corner brackets (code-only brushes, all from USIHUDStyle).
// Normal play: stopped count + wave pips and the objective prompt (TL), time and homes hit (TR), instruments with hull
// and boost gauges (BR), legend (BL), and the bracketed gap readout with its band meter at the wingtip.
// It only reads components and listens to delegates.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/SITypes.h"
#include "SIHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UImage;
class UVerticalBox;
class UHorizontalBox;
class UBorder;
class UWidget;
class USIHUDStyle;
class USIMatchComponent;
class UFont;

UENUM()
enum class ESIWeight : uint8
{
	Light,
	Medium
};

UCLASS()
class SIXINCHES_API USIHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	TSoftObjectPtr<USIHUDStyle> StyleAsset;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FLine
	{
		TObjectPtr<UImage> Fill;
		TObjectPtr<UImage> Rest;
		float Width = 100.0f;
		float Thickness = 2.0f;
	};
	/** A dark instrument plate: backing + outline (Back), four corner brackets (Ticks), optional amber left edge. */
	struct FPlate
	{
		TObjectPtr<UBorder> Back;
		TArray<TObjectPtr<UImage>> Ticks;
		TObjectPtr<UImage> Edge;
		bool bAlert = false;
	};
	/** A framed segmented gauge (hull, boost). */
	struct FBar
	{
		TArray<TObjectPtr<UImage>> Segs;
	};
	enum class EPip : uint8 { Empty, Stopped, Squadron, Homes };
	struct FLegendRow
	{
		FName Id;
		TObjectPtr<UTextBlock> Key;
		TObjectPtr<UTextBlock> Label;
		FText FreeLabel;
		FText SlotLabel;
		float Lit = 0.0f;
	};
	struct FBombTag
	{
		TWeakObjectPtr<AActor> Bomb;
		TObjectPtr<UVerticalBox> Root;
		TObjectPtr<UTextBlock> Text;
	};
	struct FPopup
	{
		TObjectPtr<UTextBlock> Text;
		FVector World = FVector::ZeroVector;
		float Age = 0.0f;
	};

	// ---- build
	void BuildTree();
	void BuildTopLeft();
	void BuildTopRight();
	void BuildBottomRight();
	void BuildLegend();
	void BuildBanner();
	void BuildWorldTags();
	void BuildEndScreen();
	void AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY = 0.5f);
	UTextBlock* Txt(const FText& Text, int32 Size, const FLinearColor& Colour, ESIWeight Weight, int32 Tracking = 0);
	UImage* Blk(const FLinearColor& Colour, const FVector2D& Size);
	UHorizontalBox* MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour);
	void SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const;
	FSlateBrush PlateBrush(const FLinearColor& Fill, const FLinearColor& Line, float LineWidth, float Radius) const;
	UWidget* MakePlate(UWidget* Content, FPlate& Out, bool bTicks, bool bLeftEdge, float MinWidth);
	void SetPlateAlert(FPlate& Plate, bool bAlert);
	UWidget* MakeBar(FBar& Out);
	void SetBar(const FBar& Bar, float Value, const FLinearColor& Colour) const;
	UWidget* MakeBracket(bool bLeft);
	UWidget* MakeHouse(TArray<TObjectPtr<UImage>>& OutParts);
	UImage* Rule(float Width);
	UCanvasPanelSlot* Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position);
	FText Caps(const FText& In) const { return In.ToUpper(); }

	// ---- runtime
	void Bind();
	USIMatchComponent* GetMatch() const;
	void UpdateReadouts(float Dt);
	void UpdateBanner(float Dt);
	void UpdateFeedback(float Dt);
	void UpdateWorld(float Dt);
	void UpdateEnd(float Dt);
	void ShowEvent(const FText& Main, const FText& Sub, const FLinearColor& Colour, float Seconds, const FText& Kicker = FText::GetEmpty());
	bool Project(const FVector& World, FVector2D& OutScreen) const;

	UFUNCTION() void HandleState(ESIMatchState NewState, ESIMatchState OldState);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandleResolved(const FSIBombResult& Result);
	UFUNCTION() void HandlePrompt(FName Id, const FText& Text);
	UFUNCTION() void HandleInput(FName KeyId, bool bSimulated);
	UFUNCTION() void HandleDamaged(float Amount, ESIDamageSource Source, float HealthAfter);
	UFUNCTION() void HandleTipped(AActor* Bomb, FSIGroundClass Predicted);
	UFUNCTION() void HandleContact(FVector Where);

	UPROPERTY() TObjectPtr<USIHUDStyle> Style;
	UPROPERTY() TObjectPtr<UFont> FontAsset;
	UPROPERTY() TObjectPtr<UCanvasPanel> Root;

	// top-left
	UPROPERTY() TObjectPtr<UTextBlock> StoppedBig;
	UPROPERTY() TObjectPtr<UTextBlock> StoppedTotal;
	UPROPERTY() TObjectPtr<UTextBlock> ObjectiveLabel;
	UPROPERTY() TObjectPtr<UTextBlock> StepText;
	UPROPERTY() TObjectPtr<UWidget> StepPlateWidget;
	FText LastStep;
	float StepFlash = 0.0f;
	/** Start screen / intro hide the HUD; it fades in for the countdown. */
	float FlowAlpha = 0.0f;
	FPlate StoppedPlate;
	FPlate StepPlate;
	/** One pip per wave bomb, filled in the order bombs resolve. */
	UPROPERTY() TArray<TObjectPtr<UImage>> WavePips;
	TArray<EPip> PipShown;
	TArray<EPip> Outcomes;
	/** Bombs the player's own wingtip tipped, so a resolved field result can be told apart from a squadron mate's. */
	TArray<TWeakObjectPtr<AActor>> PlayerTipped;
	FSlateBrush PipBrushes[4];
	// top-right
	UPROPERTY() TObjectPtr<UTextBlock> TimerText;
	UPROPERTY() TObjectPtr<UTextBlock> TimerLabel;
	FPlate TimerPlate;
	FPlate HomesPlate;
	/** Per house: its parts (roof + body), all tinted together. */
	TArray<TArray<TObjectPtr<UImage>>> Houses;
	UPROPERTY() TArray<TObjectPtr<UWidget>> HouseRoots;
	// bottom-right
	UPROPERTY() TObjectPtr<UTextBlock> SpeedText;
	UPROPERTY() TObjectPtr<UTextBlock> AltText;
	UPROPERTY() TObjectPtr<UTextBlock> AutoplayText;
	FPlate InstrumentPlate;
	FPlate LegendPlate;
	FBar HullBar;
	FBar BoostBar;
	// legend
	UPROPERTY() TObjectPtr<UVerticalBox> LegendBox;
	TArray<FLegendRow> Legend;
	// banner
	UPROPERTY() TObjectPtr<UVerticalBox> BannerBox;
	UPROPERTY() TObjectPtr<UTextBlock> BannerText;
	UPROPERTY() TObjectPtr<UImage> BannerRule;
	UPROPERTY() TObjectPtr<UTextBlock> BannerKicker;
	UPROPERTY() TObjectPtr<UImage> BannerEdge;
	UPROPERTY() TObjectPtr<UTextBlock> SubText;
	UPROPERTY() TArray<TObjectPtr<UImage>> BannerBand;
	UPROPERTY() TObjectPtr<UVerticalBox> PromptBox;
	UPROPERTY() TObjectPtr<UTextBlock> PromptText;
	// world
	TArray<FBombTag> BombTags;
	TArray<FBombTag> WingTags;
	UPROPERTY() TObjectPtr<UVerticalBox> SlotTag;
	UPROPERTY() TObjectPtr<UTextBlock> GapText;
	UPROPERTY() TObjectPtr<UTextBlock> GapUnit;
	FLine HoldLine;
	UPROPERTY() TArray<TObjectPtr<UImage>> BracketParts;
	UPROPERTY() TObjectPtr<UImage> MeterBand;
	UPROPERTY() TObjectPtr<UImage> MeterIdeal;
	UPROPERTY() TObjectPtr<UImage> MeterNeedle;
	UPROPERTY() TObjectPtr<UWidget> BalanceRow;
	UPROPERTY() TObjectPtr<UImage> BalanceSafeZone;
	UPROPERTY() TObjectPtr<UImage> BalanceNeedle;
	UPROPERTY() TObjectPtr<UTextBlock> BalanceKeyL;
	UPROPERTY() TObjectPtr<UTextBlock> BalanceKeyR;
	float DisplayedBalance = 0.0f;
	UPROPERTY() TObjectPtr<UWidget> GroundTag;
	UPROPERTY() TObjectPtr<UTextBlock> GroundText;
	FPlate GroundPlate;
	TArray<FPopup> Popups;
	// feedback + end
	UPROPERTY() TArray<TObjectPtr<UImage>> Vignette;
	UPROPERTY() TObjectPtr<UImage> Dim;
	// End screen (user 2026-11-17: "make end screen better"): a headline + story line bottom-left, a combat report card right.
	/** Everything but the end screen: hidden at the end so the report stands alone. */
	UPROPERTY() TObjectPtr<UCanvasPanel> PlayLayer;
	UPROPERTY() TObjectPtr<UWidget> EndStory;
	UPROPERTY() TObjectPtr<UWidget> EndPanel;
	UPROPERTY() TObjectPtr<UTextBlock> EndTitle;
	UPROPERTY() TObjectPtr<UTextBlock> EndReason;
	UPROPERTY() TObjectPtr<UTextBlock> EndSortie;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> EndStatValues;
	UPROPERTY() TObjectPtr<UTextBlock> EndBestTag;
	/** Three RAF roundels (outer, ring, centre per roundel) lit by the rating. */
	UPROPERTY() TArray<TObjectPtr<UImage>> EndRoundels;
	UPROPERTY() TArray<TObjectPtr<UWidget>> EndRoundelBoxes;
	UPROPERTY() TObjectPtr<UTextBlock> EndRank;
	UPROPERTY() TArray<TObjectPtr<UImage>> EndWavePips;
	UPROPERTY() TObjectPtr<UTextBlock> EndAgainText;
	bool bEndLatched = false;
	bool bEndNewBest = false;
	int32 EndSortieNo = 0;
	UImage* Circle(float Size, const FLinearColor& Colour);
	UBorder* Hair(const FLinearColor& Colour);
	UWidget* KeyCap(const FText& Key);
	UWidget* Card(UWidget* Content, const FMargin& CardPad);

	bool bBound = false;
	float StateAge = 0.0f;
	float MatchAge = 0.0f;
	float EventLeft = 0.0f;
	float EventAge = 0.0f;
	float PromptLeft = 0.0f;
	float PromptAge = 0.0f;
	float VignetteAlpha = 0.0f;
	FLinearColor VignetteColour = FLinearColor::Red;
	float StoppedPop = 0.0f;
	float TownPop = 0.0f;
	float DisplayedHold = 0.0f;
	float SlotTagAlpha = 0.0f;
	float ObjectiveShrink = 0.0f;
	ESIMatchState LastState = ESIMatchState::Ready;
};
