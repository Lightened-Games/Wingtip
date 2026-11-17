// Six Inches - HUD style (DA_SIHUDStyle). Every colour, size, margin and timing the HUD uses lives here.
// Military / tactical theme row: Barlow Condensed (OFL), amber accent, one danger red.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Layout/Margin.h"
#include "SIHUDStyle.generated.h"

class UFont;

UCLASS(BlueprintType)
class SIXINCHES_API USIHUDStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Composite font with "Light" (numerals, body) and "Medium" (labels, keys) typefaces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	TSoftObjectPtr<UFont> Font;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	int32 LabelSize = 21;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	int32 BodySize = 21;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	int32 HeadlineSize = 46;

	/** Big numbers on the corner plates (stopped count, clock). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	int32 PlateNumberSize = 34;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	int32 LabelTracking = 380;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FLinearColor Text = FLinearColor(0.88f, 0.84f, 0.76f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FLinearColor DimText = FLinearColor(0.88f, 0.84f, 0.76f, 0.72f);

	/** The one accent: objective, progress, bombs, the hold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FLinearColor Accent = FLinearColor(0.80f, 0.37f, 0.046f, 1.0f);

	/** Only for threats and defeat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FLinearColor Danger = FLinearColor(0.67f, 0.06f, 0.045f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FLinearColor Track = FLinearColor(1.0f, 1.0f, 1.0f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FLinearColor Scrim = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	/** Soft text shadow (2026-11-09 the HUD was boxless; since 2026-11-16 the corner clusters sit on plates, bPlates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.72f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	FVector2D ShadowOffset = FVector2D(1.5f, 2.0f);

	/** Dark gradient bands at the top/bottom edges and behind banners (off: text shadow only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bEdgeScrims = false;

	/** Show "AUTOPLAY" in the corner while the bot flies (off: a clean showcase run reads as a pilot playing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	bool bShowAutoplayTag = false;

	/** Width of the key column in the controls legend (fits "SPACE" at LabelSize). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float LegendKeyWidth = 105.0f;

	/** End screens dim the view this much (the ending camera shot stays visible). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour")
	float EndDim = 0.3f;

	/** Home-hit pips built (only the match's HitsToLose of them show). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	int32 MaxHomePips = 4;

	/** Squadron callsign labels show within this range (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float WingTagRange = 2500.0f;

	/** Off-screen bomb marker: distance kept from the screen edge (px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float EdgeMarkerMargin = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float EdgeMargin = 20.0f; // HUD tucked into the corners (user 2026-11-17: "move all HUD towards corners")

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float TopScrimHeight = 170.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float BottomScrimHeight = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	float ProgressWidth = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float BannerSeconds = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float EventSeconds = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float FadeSeconds = 0.2f;

	/** The legend dims to this after the first seconds; a row brightens for KeyLitSeconds on input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float LegendIdleOpacity = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float LegendDimAfter = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float KeyLitSeconds = 0.3f;

	// ---- Instrument plates (RAF 1944 cockpit dials and ops-room plotting boards; reviewer 2026-11-16: "ui looks too simple")

	/** Smallest text on the plates and world tags: 21 pt is about 28 px at 1080p (Slate renders at 96 DPI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Type")
	int32 SmallSize = 21;

	/** The objective prompt line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Type")
	int32 PromptSize = 22;

	/** Instrument values (speed, altitude). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Type")
	int32 ValueSize = 26;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Type")
	int32 PlateLabelTracking = 160;

	/** Off: back to bare text on the scene (plates draw nothing, ticks hidden). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	bool bPlates = true;

	/** Dark olive-black backing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	FLinearColor PlateFill = FLinearColor(0.010f, 0.012f, 0.008f, 0.45f);

	/** Thin amber outline round each plate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	FLinearColor PlateLine = FLinearColor(0.80f, 0.37f, 0.046f, 0.35f);

	/** Corner brackets and inner rules. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	FLinearColor PlateTick = FLinearColor(0.80f, 0.37f, 0.046f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float PlateLineWidth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float PlateRadius = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	FMargin PlatePadding = FMargin(12.0f, 6.0f, 12.0f, 8.0f);

	/** Gap between stacked plates (px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float PlateGap = 5.0f;

	/** Top-left and top-right plates share this minimum width so the corners mirror. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float PlateMinWidth = 215.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float TickLength = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float TickThickness = 2.0f;

	/** Amber edge on the objective prompt plate (px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float EdgeAccentWidth = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate")
	float RuleThickness = 1.0f;

	// ---- Wave pips (one per bomb in the wave)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Pips")
	int32 MaxWavePips = 14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Pips")
	FVector2D PipSize = FVector2D(14.0f, 7.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Pips")
	float PipGap = 4.0f;

	/** Bomb still to come: outline only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Pips")
	FLinearColor PipEmpty = FLinearColor(0.88f, 0.84f, 0.76f, 0.35f);

	/** Bomb a squadron mate stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Pips")
	FLinearColor PipSquadron = FLinearColor(0.50f, 0.48f, 0.40f, 0.60f);

	/** Home-hit markers (house silhouettes), px. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Pips")
	float HouseSize = 14.0f;

	// ---- Timer

	/** Under this many seconds the timer turns red and pulses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Timer")
	float LowTimeSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Timer")
	float LowTimePulseHz = 1.0f;

	// ---- Hull and boost gauges

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	int32 BarSegments = 10;

	/** One segment (px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	FVector2D BarSegmentSize = FVector2D(10.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	float BarSegmentGap = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	FLinearColor SegmentOff = FLinearColor(1.0f, 1.0f, 1.0f, 0.09f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	FLinearColor HullFill = FLinearColor(0.88f, 0.84f, 0.76f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	FLinearColor BoostFill = FLinearColor(0.52f, 0.62f, 0.68f, 0.90f);

	/** Hull goes amber below this fraction... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	float HullWarnAt = 0.6f;

	/** ...and red (pulsing) below this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gauges")
	float HullDangerAt = 0.35f;

	// ---- Slot gap meter

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gap")
	float GapMeterWidth = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gap")
	float GapMeterHeight = 8.0f;

	/** The meter spans 0 to this many inches of gap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gap")
	float GapMeterRangeIn = 30.0f;

	/** The hold band drawn on the meter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gap")
	FLinearColor BandZone = FLinearColor(0.80f, 0.37f, 0.046f, 0.40f);

	/** Bracket marks either side of the gap number (px). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Gap")
	FVector2D BracketSize = FVector2D(9.0f, 42.0f);

	// ---- Wake balance meter (under the hold line in the slot)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Wake")
	float BalanceMeterWidth = 190.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plate|Wake")
	float BalanceMeterHeight = 9.0f;
};
