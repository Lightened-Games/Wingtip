// Six Inches - native C++ HUD.

#include "UI/SIHUDWidget.h"
#include "UI/SIHUDStyle.h"
#include "Framework/SIMatchComponent.h"
#include "Framework/SIInputEchoComponent.h"
#include "Framework/SIAutoplayComponent.h"
#include "Components/SIFlightComponent.h"
#include "Components/SIWingtipComponent.h"
#include "Components/SIHealthComponent.h"
#include "Components/SITippableComponent.h"
#include "World/SIAirspaceSubsystem.h"
#include "SixInches.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "SixInches"

namespace
{
	void SetImageSize(UImage* Img, const FVector2D& Size)
	{
		if (!Img) { return; }
		FSlateBrush Brush = Img->GetBrush();
		Brush.ImageSize = Size;
		Img->SetBrush(Brush);
	}

	template <typename SlotT>
	SlotT* Pad(SlotT* Slot, const FMargin& Margin)
	{
		if (Slot) { Slot->SetPadding(Margin); }
		return Slot;
	}

	FLinearColor WithAlpha(const FLinearColor& C, float A) { return FLinearColor(C.R, C.G, C.B, A); }

	FString Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}

	float EaseOut(float T) { T = FMath::Clamp(T, 0.0f, 1.0f); return 1.0f - (1.0f - T) * (1.0f - T); }
}

// ---------------------------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------------------------

void USIHUDWidget::AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY)
{
	// Nested slices: slice i spans edge -> (i+1)/N of Extent; per-slice alpha solved so the edge reaches Colour.A.
	const int32 N = FMath::Max(1, Slices);
	const float SliceAlpha = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Colour.A, 0.0f, 0.99f), 1.0f / N);
	for (int32 i = 0; i < N; ++i)
	{
		UImage* Slice = Blk(FLinearColor(Colour.R, Colour.G, Colour.B, SliceAlpha), FVector2D(4.0f));
		Slice->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Slice);
		if (!S) { continue; }
		const float Depth = Extent * (i + 1) / N;
		switch (Edge)
		{
		case 0: S->SetAnchors(FAnchors(0, 0, 1, 0)); S->SetOffsets(FMargin(0, 0, 0, Depth)); break;
		case 1: S->SetAnchors(FAnchors(0, 1, 1, 1)); S->SetOffsets(FMargin(0, -Depth, 0, Depth)); break;
		case 2: S->SetAnchors(FAnchors(0, 0, 0, 1)); S->SetOffsets(FMargin(0, 0, Depth, 0)); break;
		case 3: S->SetAnchors(FAnchors(1, 0, 1, 1)); S->SetOffsets(FMargin(-Depth, 0, Depth, 0)); break;
		default: S->SetAnchors(FAnchors(0, BandAnchorY, 1, BandAnchorY)); S->SetOffsets(FMargin(0, -Depth * 0.5f, 0, Depth)); break;
		}
		if (Out) { Out->Add(Slice); }
	}
}

UTextBlock* USIHUDWidget::Txt(const FText& Text, int32 Size, const FLinearColor& Colour, ESIWeight Weight, int32 Tracking)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Font = Block->GetFont();
	const FName Face = Weight == ESIWeight::Light ? FName(TEXT("Light")) : FName(TEXT("Medium"));
	if (FontAsset)
	{
		Font = FSlateFontInfo(FontAsset, Size, Face);
	}
	else
	{
		Font.TypefaceFontName = Weight == ESIWeight::Light ? FName(TEXT("Light")) : FName(TEXT("Bold"));
	}
	Font.Size = Size;
	Font.LetterSpacing = Tracking;
	Font.OutlineSettings.OutlineSize = 0;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Colour));
	Block->SetShadowOffset(Style->ShadowOffset);
	Block->SetShadowColorAndOpacity(Style->ShadowColor);
	return Block;
}

UImage* USIHUDWidget::Blk(const FLinearColor& Colour, const FVector2D& Size)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.ImageSize = Size;
	Img->SetBrush(Brush);
	Img->SetColorAndOpacity(Colour);
	return Img;
}

UHorizontalBox* USIHUDWidget::MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Out.Width = Width;
	Out.Thickness = Thickness;
	Out.Fill = Blk(FillColour, FVector2D(0.0f, Thickness));
	Out.Rest = Blk(Style->Track, FVector2D(Width, Thickness));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Fill)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Rest)) { S->SetVerticalAlignment(VAlign_Center); }
	return Row;
}

void USIHUDWidget::SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const
{
	const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	SetImageSize(Line.Fill, FVector2D(Line.Width * A, Line.Thickness));
	SetImageSize(Line.Rest, FVector2D(Line.Width * (1.0f - A), Line.Thickness));
	if (Line.Fill) { Line.Fill->SetColorAndOpacity(FillColour); }
}

UCanvasPanelSlot* USIHUDWidget::Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
{
	UCanvasPanelSlot* S = Root->AddChildToCanvas(Widget);
	if (S)
	{
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
	}
	return S;
}

FSlateBrush USIHUDWidget::PlateBrush(const FLinearColor& Fill, const FLinearColor& Line, float LineWidth, float Radius) const
{
	FSlateBrush B;
	B.DrawAs = ESlateBrushDrawType::RoundedBox;
	B.TintColor = FSlateColor(Fill);
	B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
	B.OutlineSettings.Color = FSlateColor(Line);
	B.OutlineSettings.Width = LineWidth;
	B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	return B;
}

UImage* USIHUDWidget::Rule(float Width)
{
	return Blk(Style->PlateTick, FVector2D(Width, Style->RuleThickness));
}

UWidget* USIHUDWidget::MakePlate(UWidget* Content, FPlate& Out, bool bTicks, bool bLeftEdge, float MinWidth)
{
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Out.Back = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Out.Back->SetBrush(Style->bPlates ? PlateBrush(Style->PlateFill, Style->PlateLine, Style->PlateLineWidth, Style->PlateRadius)
		: PlateBrush(FLinearColor::Transparent, FLinearColor::Transparent, 0.0f, 0.0f));
	FMargin PlatePad = Style->PlatePadding;
	if (bLeftEdge) { PlatePad.Left += Style->EdgeAccentWidth; }
	Out.Back->SetPadding(PlatePad);
	if (MinWidth > 0.0f)
	{
		USizeBox* Min = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Min->SetMinDesiredWidth(MinWidth);
		Min->SetContent(Content);
		Out.Back->SetContent(Min);
	}
	else
	{
		Out.Back->SetContent(Content);
	}
	if (UOverlaySlot* S = O->AddChildToOverlay(Out.Back)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
	if (bLeftEdge)
	{
		Out.Edge = Blk(Style->Accent, FVector2D(Style->EdgeAccentWidth, 2.0f));
		if (UOverlaySlot* S = O->AddChildToOverlay(Out.Edge)) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Fill); }
	}
	if (bTicks && Style->bPlates)
	{
		const EHorizontalAlignment H[] = { HAlign_Left, HAlign_Right, HAlign_Left, HAlign_Right };
		const EVerticalAlignment V[] = { VAlign_Top, VAlign_Top, VAlign_Bottom, VAlign_Bottom };
		for (int32 c = 0; c < 4; ++c)
		{
			UImage* Across = Blk(Style->PlateTick, FVector2D(Style->TickLength, Style->TickThickness));
			UImage* Down = Blk(Style->PlateTick, FVector2D(Style->TickThickness, Style->TickLength));
			for (UImage* T : { Across, Down })
			{
				if (UOverlaySlot* S = O->AddChildToOverlay(T)) { S->SetHorizontalAlignment(H[c]); S->SetVerticalAlignment(V[c]); }
				Out.Ticks.Add(T);
			}
		}
	}
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return O;
}

void USIHUDWidget::SetPlateAlert(FPlate& Plate, bool bAlert)
{
	if (Plate.bAlert == bAlert || !Style->bPlates)
	{
		return;
	}
	Plate.bAlert = bAlert;
	if (Plate.Back)
	{
		Plate.Back->SetBrush(PlateBrush(Style->PlateFill, bAlert ? WithAlpha(Style->Danger, 0.8f) : Style->PlateLine, Style->PlateLineWidth, Style->PlateRadius));
	}
	for (UImage* T : Plate.Ticks) { if (T) { T->SetColorAndOpacity(bAlert ? Style->Danger : Style->PlateTick); } }
	if (Plate.Edge) { Plate.Edge->SetColorAndOpacity(bAlert ? Style->Danger : Style->Accent); }
}

UWidget* USIHUDWidget::MakeBar(FBar& Out)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const int32 N = FMath::Clamp(Style->BarSegments, 1, 40);
	for (int32 i = 0; i < N; ++i)
	{
		UImage* Seg = Blk(Style->SegmentOff, Style->BarSegmentSize);
		Pad(Row->AddChildToHorizontalBox(Seg), FMargin(i == 0 ? 0.0f : Style->BarSegmentGap, 0.0f, 0.0f, 0.0f));
		Out.Segs.Add(Seg);
	}
	// Thin frame round the segments, like the bezel of a cockpit gauge.
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrush(PlateBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), WithAlpha(Style->Text, 0.35f), 1.0f, 1.0f));
	Frame->SetPadding(FMargin(3.0f));
	Frame->SetContent(Row);
	return Frame;
}

void USIHUDWidget::SetBar(const FBar& Bar, float Value, const FLinearColor& Colour) const
{
	const int32 N = Bar.Segs.Num();
	const float Lit = FMath::Clamp(Value, 0.0f, 1.0f) * N;
	for (int32 i = 0; i < N; ++i)
	{
		const float Part = FMath::Clamp(Lit - i, 0.0f, 1.0f);
		if (Bar.Segs[i])
		{
			Bar.Segs[i]->SetColorAndOpacity(Part <= 0.0f ? Style->SegmentOff : FMath::Lerp(Style->SegmentOff, Colour, 0.35f + 0.65f * Part));
		}
	}
}

UWidget* USIHUDWidget::MakeBracket(bool bLeft)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Style->BracketSize.X);
	Box->SetHeightOverride(Style->BracketSize.Y);
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	const EHorizontalAlignment Side = bLeft ? HAlign_Left : HAlign_Right;
	UImage* Spine = Blk(Style->Text, FVector2D(2.0f, 2.0f));
	UImage* Top = Blk(Style->Text, FVector2D(2.0f, 2.0f));
	UImage* Bottom = Blk(Style->Text, FVector2D(2.0f, 2.0f));
	if (UOverlaySlot* S = O->AddChildToOverlay(Spine)) { S->SetHorizontalAlignment(Side); S->SetVerticalAlignment(VAlign_Fill); }
	if (UOverlaySlot* S = O->AddChildToOverlay(Top)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Top); }
	if (UOverlaySlot* S = O->AddChildToOverlay(Bottom)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Bottom); }
	BracketParts.Append({ Spine, Top, Bottom });
	Box->SetContent(O);
	return Box;
}

UWidget* USIHUDWidget::MakeHouse(TArray<TObjectPtr<UImage>>& OutParts)
{
	// A cottage silhouette from two boxes: a roof diamond half-hidden behind the walls.
	const float H = Style->HouseSize;
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(H);
	Box->SetHeightOverride(H);
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	const float RoofSide = H * 0.62f;
	UImage* Roof = Blk(Style->PipEmpty, FVector2D(RoofSide, RoofSide));
	Roof->SetRenderTransformAngle(45.0f);
	UImage* Walls = Blk(Style->PipEmpty, FVector2D(H * 0.78f, H * 0.52f));
	if (UOverlaySlot* S = O->AddChildToOverlay(Roof)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Top); S->SetPadding(FMargin(0.0f, H * 0.08f, 0.0f, 0.0f)); }
	if (UOverlaySlot* S = O->AddChildToOverlay(Walls)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Bottom); }
	OutParts = { Roof, Walls };
	Box->SetContent(O);
	return Box;
}

// ---------------------------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------------------------

TSharedRef<SWidget> USIHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void USIHUDWidget::BuildTree()
{
	if (StyleAsset.IsNull())
	{
		StyleAsset = TSoftObjectPtr<USIHUDStyle>(FSoftObjectPath(TEXT("/Game/SixInches/UI/DA_SIHUDStyle.DA_SIHUDStyle")));
	}
	Style = StyleAsset.LoadSynchronous();
	if (!Style)
	{
		Style = NewObject<USIHUDStyle>(this);
	}
	if (Style->Font.IsNull())
	{
		Style->Font = TSoftObjectPtr<UFont>(FSoftObjectPath(TEXT("/Game/SixInches/UI/F_BarlowCondensed.F_BarlowCondensed")));
	}
	FontAsset = Style->Font.LoadSynchronous();

	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	Root->SetRenderOpacity(0.0f);
	Root->SetVisibility(ESlateVisibility::Hidden);
	FlowAlpha = 0.0f;

	// Gameplay pieces build into their own layer (Root points at it while they build), the end screen on top.
	UCanvasPanel* Top = Root;
	PlayLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayLayer"));
	if (UCanvasPanelSlot* S = Top->AddChildToCanvas(PlayLayer)) { S->SetAnchors(FAnchors(0, 0, 1, 1)); S->SetOffsets(FMargin(0.0f)); }
	Root = PlayLayer;

	if (Style->bEdgeScrims)
	{
		AddFade(0, Style->TopScrimHeight, Style->Scrim, 28, nullptr);
		AddFade(1, Style->BottomScrimHeight, Style->Scrim, 28, nullptr);
	}
	const FLinearColor Red = WithAlpha(Style->Danger, 0.75f);
	AddFade(2, 280.0f, Red, 20, &Vignette);
	AddFade(3, 280.0f, Red, 20, &Vignette);
	AddFade(0, 200.0f, Red, 16, &Vignette);
	AddFade(1, 200.0f, Red, 16, &Vignette);
	for (UImage* Img : Vignette) { Img->SetRenderOpacity(0.0f); }

	BuildWorldTags();
	BuildTopLeft();
	BuildTopRight();
	BuildBottomRight();
	BuildLegend();
	BuildBanner();
	Root = Top;
	BuildEndScreen();
}

void USIHUDWidget::BuildTopLeft()
{
	// Ops-board plate: goal label, rule, the big stopped count, then one pip per bomb in the wave.
	UVerticalBox* Board = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ObjectiveLabel = Txt(LOCTEXT("ObjLabel", "STOP THE FLYING BOMBS"), Style->SmallSize, Style->Accent, ESIWeight::Medium, Style->PlateLabelTracking);
	Board->AddChildToVerticalBox(ObjectiveLabel);
	if (UVerticalBoxSlot* S = Pad(Board->AddChildToVerticalBox(Rule(2.0f)), FMargin(0.0f, 6.0f, 0.0f, 2.0f))) { S->SetHorizontalAlignment(HAlign_Fill); }
	UHorizontalBox* Nums = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StoppedBig = Txt(FText::FromString(TEXT("0")), Style->PlateNumberSize, Style->Text, ESIWeight::Light);
	StoppedTotal = Txt(FText::FromString(TEXT("/ 8")), Style->SmallSize, Style->DimText, ESIWeight::Light);
	Nums->AddChildToHorizontalBox(StoppedBig);
	if (UHorizontalBoxSlot* S = Pad(Nums->AddChildToHorizontalBox(StoppedTotal), FMargin(10.0f, 0.0f, 0.0f, 12.0f))) { S->SetVerticalAlignment(VAlign_Bottom); }
	UTextBlock* StoppedWord = Txt(LOCTEXT("StoppedWord", "STOPPED"), Style->SmallSize, Style->DimText, ESIWeight::Medium, Style->PlateLabelTracking);
	if (UHorizontalBoxSlot* S = Pad(Nums->AddChildToHorizontalBox(StoppedWord), FMargin(14.0f, 0.0f, 0.0f, 12.0f))) { S->SetVerticalAlignment(VAlign_Bottom); }
	Pad(Board->AddChildToVerticalBox(Nums), FMargin(0.0f, -2.0f, 0.0f, 4.0f));

	PipBrushes[(int32)EPip::Empty] = PlateBrush(FLinearColor::Transparent, Style->PipEmpty, 1.5f, Style->PipSize.Y * 0.5f);
	PipBrushes[(int32)EPip::Stopped] = PlateBrush(Style->Accent, WithAlpha(Style->Accent, 1.0f), 1.0f, Style->PipSize.Y * 0.5f);
	PipBrushes[(int32)EPip::Squadron] = PlateBrush(Style->PipSquadron, Style->PipSquadron, 1.0f, Style->PipSize.Y * 0.5f);
	PipBrushes[(int32)EPip::Homes] = PlateBrush(Style->Danger, Style->Danger, 1.0f, Style->PipSize.Y * 0.5f);
	for (FSlateBrush& B : PipBrushes) { B.ImageSize = Style->PipSize; }
	UHorizontalBox* Pips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 i = 0; i < FMath::Max(1, Style->MaxWavePips); ++i)  // only the wave's Total of them show
	{
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Pip->SetBrush(PipBrushes[(int32)EPip::Empty]);
		if (UHorizontalBoxSlot* S = Pad(Pips->AddChildToHorizontalBox(Pip), FMargin(i == 0 ? 0.0f : Style->PipGap, 0.0f, 0.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
		WavePips.Add(Pip);
		PipShown.Add(EPip::Empty);
	}
	Board->AddChildToVerticalBox(Pips);

	// What to do right now (changes with the situation): the line a new player reads first, on a slim plate with an amber edge.
	StepText = Txt(FText::GetEmpty(), Style->PromptSize, Style->Text, ESIWeight::Medium, 10);
	StepPlateWidget = MakePlate(StepText, StepPlate, false, true, Style->PlateMinWidth);
	StepPlateWidget->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakePlate(Board, StoppedPlate, true, false, Style->PlateMinWidth))) { S->SetHorizontalAlignment(HAlign_Left); }
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(StepPlateWidget), FMargin(0.0f, Style->PlateGap, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Left); }
	Place(Col, FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(Style->EdgeMargin, Style->EdgeMargin));
}

void USIHUDWidget::BuildTopRight()
{
	// Clock plate, compact (user 2026-11-17: "make this HUD compact"): TIME and the digits on one line.
	UHorizontalBox* Clk = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TimerLabel = Txt(LOCTEXT("TimeLabel", "TIME"), Style->SmallSize, Style->Accent, ESIWeight::Medium, Style->PlateLabelTracking);
	if (UHorizontalBoxSlot* S = Pad(Clk->AddChildToHorizontalBox(TimerLabel), FMargin(0.0f, 0.0f, 16.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
	TimerText = Txt(FText::FromString(TEXT("1:30")), Style->ValueSize + 4, Style->Text, ESIWeight::Light);
	TimerText->SetRenderTransformPivot(FVector2D(1.0f, 0.5f));
	if (UHorizontalBoxSlot* S = Clk->AddChildToHorizontalBox(TimerText)) { S->SetVerticalAlignment(VAlign_Center); }

	// Homes plate: one cottage per allowed hit; each turns red when a bomb lands on homes.
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(Txt(LOCTEXT("HomesLabel", "HOMES HIT"), Style->SmallSize, Style->DimText, ESIWeight::Medium, Style->PlateLabelTracking)), FMargin(0.0f, 0.0f, 16.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
	for (int32 i = 0; i < Style->MaxHomePips; ++i)  // only the match's HitsToLose of them show
	{
		TArray<TObjectPtr<UImage>> Parts;
		UWidget* House = MakeHouse(Parts);
		if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(House), FMargin(i == 0 ? 0.0f : 10.0f, 0.0f, 0.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
		Houses.Add(Parts);
		HouseRoots.Add(House);
	}
	UVerticalBox* RowRight = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = RowRight->AddChildToVerticalBox(Row)) { S->SetHorizontalAlignment(HAlign_Right); }

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakePlate(Clk, TimerPlate, true, false, 0.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(MakePlate(RowRight, HomesPlate, false, false, 0.0f)), FMargin(0.0f, Style->PlateGap, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	Place(Col, FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-Style->EdgeMargin, Style->EdgeMargin));
}

void USIHUDWidget::BuildBottomRight()
{
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AutoplayText = Txt(LOCTEXT("AutoplayTag", "AUTOPLAY  ·  ANY KEY TAKES OVER"),Style->LabelSize, Style->DimText, ESIWeight::Medium, Style->LabelTracking);
	AutoplayText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(AutoplayText), FMargin(0.0f, 0.0f, 0.0f, 10.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	// Instrument plate: two dial-style readouts side by side, a rule, then framed segmented gauges.
	UVerticalBox* Panel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Dials = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	auto Dial = [this, Dials](UTextBlock*& Value, const FText& Label, const FText& Unit, bool bFirst)
	{
		if (!bFirst)
		{
			// Hairline divider between the two instruments.
			UImage* Div = Blk(WithAlpha(Style->Text, 0.25f), FVector2D(1.0f, 2.0f));
			if (UHorizontalBoxSlot* S = Pad(Dials->AddChildToHorizontalBox(Div), FMargin(20.0f, 4.0f))) { S->SetVerticalAlignment(VAlign_Fill); }
		}
		UVerticalBox* Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		if (UVerticalBoxSlot* S = Cell->AddChildToVerticalBox(Txt(Label, Style->SmallSize, Style->Accent, ESIWeight::Medium, Style->PlateLabelTracking))) { S->SetHorizontalAlignment(HAlign_Right); }
		UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Value = Txt(FText::FromString(TEXT("0")), Style->ValueSize, Style->Text, ESIWeight::Light);
		R->AddChildToHorizontalBox(Value);
		if (UHorizontalBoxSlot* S = Pad(R->AddChildToHorizontalBox(Txt(Unit, Style->SmallSize, Style->DimText, ESIWeight::Medium, Style->PlateLabelTracking)), FMargin(8.0f, 0.0f, 0.0f, 6.0f))) { S->SetVerticalAlignment(VAlign_Bottom); }
		if (UVerticalBoxSlot* S = Pad(Cell->AddChildToVerticalBox(R), FMargin(0.0f, -2.0f, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
		if (UHorizontalBoxSlot* S = Dials->AddChildToHorizontalBox(Cell)) { S->SetVerticalAlignment(VAlign_Bottom); }
	};
	UTextBlock* Speed = nullptr;
	UTextBlock* Alt = nullptr;
	Dial(Speed, LOCTEXT("AirspeedLabel", "AIRSPEED"), LOCTEXT("Mph", "MPH"), true);
	Dial(Alt, LOCTEXT("AltLabel", "ALTITUDE"), LOCTEXT("Ft", "FT"), false);
	SpeedText = Speed;
	AltText = Alt;
	if (UVerticalBoxSlot* S = Panel->AddChildToVerticalBox(Dials)) { S->SetHorizontalAlignment(HAlign_Right); }
	if (UVerticalBoxSlot* S = Pad(Panel->AddChildToVerticalBox(Rule(2.0f)), FMargin(0.0f, 8.0f, 0.0f, 10.0f))) { S->SetHorizontalAlignment(HAlign_Fill); }
	auto Gauge = [this, Panel](FBar& Bar, const FText& Label, bool bFirst)
	{
		UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LabelCol = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelCol->SetMinDesiredWidth(96.0f);
		LabelCol->SetContent(Txt(Label, Style->SmallSize, Style->DimText, ESIWeight::Medium, Style->PlateLabelTracking));
		if (UHorizontalBoxSlot* S = Pad(R->AddChildToHorizontalBox(LabelCol), FMargin(0.0f, 0.0f, 12.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
		if (UHorizontalBoxSlot* S = R->AddChildToHorizontalBox(MakeBar(Bar))) { S->SetVerticalAlignment(VAlign_Center); }
		if (UVerticalBoxSlot* S = Pad(Panel->AddChildToVerticalBox(R), FMargin(0.0f, bFirst ? 0.0f : 8.0f, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	};
	Gauge(HullBar, LOCTEXT("HullLabel", "HULL"), true);
	Gauge(BoostBar, LOCTEXT("BoostLabel", "BOOST"), false);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(MakePlate(Panel, InstrumentPlate, true, false, 0.0f))) { S->SetHorizontalAlignment(HAlign_Right); }
	Place(Col, FAnchors(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-Style->EdgeMargin, -Style->EdgeMargin));
}

void USIHUDWidget::BuildLegend()
{
	// One strip of key caps along the bottom centre (user 2026-11-17: "place controls at bottom"), same look as the start screen.
	struct FDef { FName Id; FText Key; FText Free; FText Slot; };
	const TArray<FDef> Defs = {
		{ TEXT("Pitch"), LOCTEXT("KeyWS", "W  S"), LOCTEXT("LegDive", "DIVE / CLIMB"), LOCTEXT("LegEase", "EASE DOWN / UP") },
		{ TEXT("Bank"), LOCTEXT("KeyAD", "A  D"), LOCTEXT("LegBank", "BANK"), LOCTEXT("LegSlide", "SLIDE IN / OUT") },
		{ TEXT("Trim"), LOCTEXT("KeyQE", "Q  E"), LOCTEXT("LegTrim", "TRIM"), LOCTEXT("LegTrimSlot", "LEVEL THE WAKE") },
		{ TEXT("Boost"), LOCTEXT("KeyShift", "SHIFT"), LOCTEXT("LegBoost", "BOOST"), LOCTEXT("LegBoostSlot", "BOOST") },
		{ TEXT("Brake"), LOCTEXT("KeySpace", "SPACE"), LOCTEXT("LegBrake", "AIRBRAKE"), LOCTEXT("LegBreak", "BREAK OFF") },
	};
	const int32 Size = Style->LabelSize - 4;
	LegendBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	// Fill-only frames throughout so the whole strip fades with LegendBox (rounded-box outlines ignore render opacity).
	UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Band->SetBrush(PlateBrush(FLinearColor(0.01f, 0.012f, 0.008f, 0.55f), FLinearColor::Transparent, 0.0f, 2.0f));
	Band->SetPadding(FMargin(20.0f, 7.0f, 22.0f, 8.0f));
	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Band->SetContent(Line);
	LegendBox->AddChildToVerticalBox(Band);
	if (UVerticalBoxSlot* S = LegendBox->AddChildToVerticalBox(Hair(WithAlpha(Style->Accent, 0.6f)))) { S->SetHorizontalAlignment(HAlign_Fill); }
	for (int32 i = 0; i < Defs.Num(); ++i)
	{
		const FDef& D = Defs[i];
		UBorder* Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Cap->SetBrush(PlateBrush(WithAlpha(Style->Text, 0.55f), FLinearColor::Transparent, 0.0f, 3.0f));
		Cap->SetPadding(FMargin(1.5f));
		UBorder* CapIn = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		CapIn->SetBrush(PlateBrush(FLinearColor(0.02f, 0.025f, 0.02f, 0.9f), FLinearColor::Transparent, 0.0f, 2.0f));
		CapIn->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 1.0f));
		UTextBlock* Key = Txt(D.Key, Size, Style->Text, ESIWeight::Medium, 80);
		CapIn->SetContent(Key);
		Cap->SetContent(CapIn);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Cap)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(i == 0 ? 0.0f : 26.0f, 0.0f, 9.0f, 0.0f)); }
		UTextBlock* Label = Txt(D.Free, Size, Style->DimText, ESIWeight::Medium, 140);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Label)) { S->SetVerticalAlignment(VAlign_Center); }
		Legend.Add({ D.Id, Key, Label, D.Free, D.Slot, 0.0f });
	}
	Place(LegendBox, FAnchors(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -Style->EdgeMargin * 0.5f));
}
void USIHUDWidget::BuildBanner()
{
	if (Style->bEdgeScrims)
	{
		AddFade(4, 230.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 26, &BannerBand, 0.3f);
	}
	for (UImage* I : BannerBand) { I->SetRenderOpacity(0.0f); }
	// A dispatch slip on the ops board (user 2026-11-17: "improve this red text make it theme based"): dark card, a coloured
	// edge and rule for the news, a small kicker line, cream headline. Fills only, so the card fades with BannerBox.
	BannerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BannerKicker = Txt(FText::GetEmpty(), Style->SmallSize, Style->Accent, ESIWeight::Medium, 320);
	BannerText = Txt(FText::GetEmpty(), FMath::RoundToInt(Style->HeadlineSize * 0.85f), Style->Text, ESIWeight::Medium, 300);
	BannerText->SetJustification(ETextJustify::Center);
	BannerRule = Blk(Style->Accent, FVector2D(72.0f, 2.0f));
	SubText = Txt(FText::GetEmpty(), Style->BodySize, Style->DimText, ESIWeight::Medium, 40);
	SubText->SetJustification(ETextJustify::Center);
	const TPair<UWidget*, FMargin> Parts[] = { { BannerKicker.Get(), FMargin(0.0f, 0.0f, 0.0f, 4.0f) }, { BannerText.Get(), FMargin(0.0f) },
		{ BannerRule.Get(), FMargin(0.0f, 8.0f, 0.0f, 10.0f) }, { SubText.Get(), FMargin(0.0f) } };
	for (const TPair<UWidget*, FMargin>& P : Parts)
	{
		if (UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(P.Key)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(P.Value); }
	}
	UBorder* Slip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Slip->SetBrush(PlateBrush(FLinearColor(0.022f, 0.026f, 0.02f, 0.82f), FLinearColor::Transparent, 0.0f, 2.0f));
	Slip->SetPadding(FMargin(46.0f, 16.0f, 46.0f, 18.0f));
	Slip->SetContent(Inner);
	UOverlay* SlipO = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SlipO->AddChildToOverlay(Slip);
	BannerEdge = Blk(Style->Accent, FVector2D(4.0f, 4.0f));
	if (UOverlaySlot* S = SlipO->AddChildToOverlay(BannerEdge)) { S->SetHorizontalAlignment(HAlign_Left); S->SetVerticalAlignment(VAlign_Fill); }
	const EHorizontalAlignment TH[] = { HAlign_Right, HAlign_Right };
	const EVerticalAlignment TV[] = { VAlign_Top, VAlign_Bottom };
	for (int32 c = 0; c < 2; ++c)
	{
		for (const FVector2D Sz : { FVector2D(16.0f, 2.0f), FVector2D(2.0f, 16.0f) })
		{
			if (UOverlaySlot* S = SlipO->AddChildToOverlay(Blk(Style->PlateTick, Sz))) { S->SetHorizontalAlignment(TH[c]); S->SetVerticalAlignment(TV[c]); }
		}
	}
	BannerBox->AddChildToVerticalBox(SlipO);
	BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	BannerBox->SetRenderOpacity(0.0f);
	Place(BannerBox, FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, Style->EdgeMargin + 4.0f)); // top centre, clear of the slot readout

	PromptBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UImage* Tick = Blk(Style->Accent, FVector2D(24.0f, 2.0f));
	if (UVerticalBoxSlot* S = Pad(PromptBox->AddChildToVerticalBox(Tick), FMargin(0.0f, 0.0f, 0.0f, 8.0f))) { S->SetHorizontalAlignment(HAlign_Center); }
	PromptText = Txt(FText::GetEmpty(), Style->BodySize, Style->Text, ESIWeight::Light, 20);
	PromptText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = PromptBox->AddChildToVerticalBox(PromptText)) { S->SetHorizontalAlignment(HAlign_Center); }
	PromptBox->SetRenderOpacity(0.0f);
	PromptBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(PromptBox, FAnchors(0.5f, 0.78f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

void USIHUDWidget::BuildWorldTags()
{
	for (int32 i = 0; i < 8; ++i)
	{
		FBombTag Tag;
		Tag.Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UImage* D = Blk(Style->Accent, FVector2D(8.0f, 8.0f));
		D->SetRenderTransformAngle(45.0f);
		if (UVerticalBoxSlot* S = Tag.Root->AddChildToVerticalBox(D)) { S->SetHorizontalAlignment(HAlign_Center); }
		Tag.Text = Txt(FText::GetEmpty(), Style->SmallSize, Style->Text, ESIWeight::Medium, 160);
		if (UVerticalBoxSlot* S = Pad(Tag.Root->AddChildToVerticalBox(Tag.Text), FMargin(0.0f, 6.0f, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Center); }
		Tag.Root->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Tag.Root)) { S->SetAlignment(FVector2D(0.5f, 0.0f)); S->SetAutoSize(true); }
		BombTags.Add(Tag);
	}
	for (int32 i = 0; i < 4; ++i)
	{
		FBombTag Tag;
		Tag.Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Tag.Text = Txt(FText::GetEmpty(), Style->SmallSize, Style->DimText, ESIWeight::Medium, Style->PlateLabelTracking);
		if (UVerticalBoxSlot* S = Tag.Root->AddChildToVerticalBox(Tag.Text)) { S->SetHorizontalAlignment(HAlign_Center); }
		Tag.Root->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Tag.Root)) { S->SetAlignment(FVector2D(0.5f, 1.0f)); S->SetAutoSize(true); }
		WingTags.Add(Tag);
	}

	// Slot readout: [ gap IN ] between bracket marks, a band meter (needle against the hold band), the hold line.
	SlotTag = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* GapRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	GapText = Txt(FText::FromString(TEXT("6")), Style->HeadlineSize, Style->Text, ESIWeight::Light);
	GapUnit = Txt(LOCTEXT("In", "IN"), Style->SmallSize, Style->DimText, ESIWeight::Medium, Style->PlateLabelTracking);
	if (UHorizontalBoxSlot* S = Pad(GapRow->AddChildToHorizontalBox(MakeBracket(true)), FMargin(0.0f, 0.0f, 14.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
	// Room for two digits, right-aligned: "24" ran into the unit when it grew from one digit.
	GapText->SetJustification(ETextJustify::Right);
	USizeBox* GapNumBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	GapNumBox->SetMinDesiredWidth(Style->HeadlineSize * 1.2f);
	GapNumBox->SetContent(GapText);
	if (UHorizontalBoxSlot* S = GapRow->AddChildToHorizontalBox(GapNumBox)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Pad(GapRow->AddChildToHorizontalBox(GapUnit), FMargin(14.0f, 0.0f, 0.0f, 14.0f))) { S->SetVerticalAlignment(VAlign_Bottom); }
	if (UHorizontalBoxSlot* S = Pad(GapRow->AddChildToHorizontalBox(MakeBracket(false)), FMargin(14.0f, 0.0f, 0.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
	if (UVerticalBoxSlot* S = SlotTag->AddChildToVerticalBox(GapRow)) { S->SetHorizontalAlignment(HAlign_Center); }

	const float MW = Style->GapMeterWidth;
	const float MH = Style->GapMeterHeight;
	USizeBox* MeterBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MeterBox->SetWidthOverride(MW);
	MeterBox->SetHeightOverride(MH + 10.0f);
	UCanvasPanel* Meter = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	MeterBox->SetContent(Meter);
	auto Put = [Meter](UWidget* W, const FVector2D& Pos, const FVector2D& Size)
	{
		if (UCanvasPanelSlot* S = Meter->AddChildToCanvas(W)) { S->SetAutoSize(false); S->SetPosition(Pos); S->SetSize(Size); }
	};
	Put(Blk(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), FVector2D(MW, MH)), FVector2D(0.0f, 5.0f), FVector2D(MW, MH));
	MeterBand = Blk(Style->BandZone, FVector2D(10.0f, MH));
	Put(MeterBand, FVector2D(0.0f, 5.0f), FVector2D(10.0f, MH));
	MeterIdeal = Blk(Style->Accent, FVector2D(2.0f, MH));
	Put(MeterIdeal, FVector2D(0.0f, 5.0f), FVector2D(2.0f, MH));
	MeterNeedle = Blk(Style->Text, FVector2D(3.0f, MH + 10.0f));
	Put(MeterNeedle, FVector2D(0.0f, 0.0f), FVector2D(3.0f, MH + 10.0f));
	if (UVerticalBoxSlot* S = Pad(SlotTag->AddChildToVerticalBox(MeterBox), FMargin(0.0f, 8.0f, 0.0f, 6.0f))) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = SlotTag->AddChildToVerticalBox(MakeLine(HoldLine, MW, 3.0f, Style->Accent))) { S->SetHorizontalAlignment(HAlign_Center); }

	// Wake balance: Q [ red | level zone | red ] E, a needle that the bomb's wake keeps pushing off centre.
	{
		const float BW = Style->BalanceMeterWidth;
		const float BH = Style->BalanceMeterHeight;
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		BalanceKeyL = Txt(LOCTEXT("TrimQ", "Q"), Style->SmallSize, Style->DimText, ESIWeight::Medium);
		BalanceKeyR = Txt(LOCTEXT("TrimE", "E"), Style->SmallSize, Style->DimText, ESIWeight::Medium);
		USizeBox* BBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BBox->SetWidthOverride(BW);
		BBox->SetHeightOverride(BH + 12.0f);
		UCanvasPanel* BMeter = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		BBox->SetContent(BMeter);
		auto PutB = [BMeter](UWidget* W, const FVector2D& Pos, const FVector2D& Size)
		{
			if (UCanvasPanelSlot* S = BMeter->AddChildToCanvas(W)) { S->SetAutoSize(false); S->SetPosition(Pos); S->SetSize(Size); }
		};
		PutB(Blk(WithAlpha(Style->Danger, 0.55f), FVector2D(BW, BH)), FVector2D(0.0f, 6.0f), FVector2D(BW, BH));
		BalanceSafeZone = Blk(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), FVector2D(BW * 0.5f, BH));
		PutB(BalanceSafeZone, FVector2D(BW * 0.25f, 6.0f), FVector2D(BW * 0.5f, BH));
		PutB(Blk(Style->Accent, FVector2D(2.0f, BH)), FVector2D(BW * 0.5f - 1.0f, 6.0f), FVector2D(2.0f, BH));
		BalanceNeedle = Blk(Style->Text, FVector2D(4.0f, BH + 12.0f));
		PutB(BalanceNeedle, FVector2D(BW * 0.5f - 2.0f, 0.0f), FVector2D(4.0f, BH + 12.0f));
		if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(BalanceKeyL), FMargin(0.0f, 0.0f, 12.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BBox)) { S->SetVerticalAlignment(VAlign_Center); }
		if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(BalanceKeyR), FMargin(12.0f, 0.0f, 0.0f, 0.0f))) { S->SetVerticalAlignment(VAlign_Center); }
		BalanceRow = Row;
		if (UVerticalBoxSlot* S = Pad(SlotTag->AddChildToVerticalBox(Row), FMargin(0.0f, 8.0f, 0.0f, 0.0f))) { S->SetHorizontalAlignment(HAlign_Center); }
	}
	SlotTag->SetVisibility(ESlateVisibility::HitTestInvisible);
	SlotTag->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(SlotTag)) { S->SetAlignment(FVector2D(0.5f, 1.0f)); S->SetAutoSize(true); }

	// Predicted landing: the ground class on a small plate (outline goes red over homes).
	GroundText = Txt(FText::GetEmpty(), Style->SmallSize, Style->Text, ESIWeight::Medium, Style->PlateLabelTracking);
	UVerticalBox* GroundCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = GroundCol->AddChildToVerticalBox(GroundText)) { S->SetHorizontalAlignment(HAlign_Center); }
	GroundTag = MakePlate(GroundCol, GroundPlate, false, false, 0.0f);
	if (GroundPlate.Back) { GroundPlate.Back->SetPadding(FMargin(14.0f, 4.0f, 14.0f, 6.0f)); }
	GroundTag->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(GroundTag)) { S->SetAlignment(FVector2D(0.5f, 1.0f)); S->SetAutoSize(true); }
}

UImage* USIHUDWidget::Circle(float Size, const FLinearColor& Colour)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush B = PlateBrush(FLinearColor::White, FLinearColor::Transparent, 0.0f, Size * 0.5f);
	B.ImageSize = FVector2D(Size);
	Img->SetBrush(B);
	Img->SetColorAndOpacity(Colour);
	return Img;
}

UBorder* USIHUDWidget::Hair(const FLinearColor& Colour)
{
	// A 1 px rule that stretches to its slot.
	UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	B->SetBrush(PlateBrush(Colour, FLinearColor::Transparent, 0.0f, 0.0f));
	B->SetPadding(FMargin(0.0f, 0.5f));
	return B;
}

UWidget* USIHUDWidget::KeyCap(const FText& Key)
{
	// Frames are nested fills: rounded-box outlines ignore render opacity.
	UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Line->SetBrush(PlateBrush(WithAlpha(Style->Text, 0.6f), FLinearColor::Transparent, 0.0f, 3.0f));
	Line->SetPadding(FMargin(1.5f));
	UBorder* In = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	In->SetBrush(PlateBrush(FLinearColor(0.02f, 0.025f, 0.02f, 0.92f), FLinearColor::Transparent, 0.0f, 2.0f));
	In->SetPadding(FMargin(9.0f, 0.0f, 9.0f, 1.0f));
	In->SetContent(Txt(Key, Style->SmallSize, Style->Text, ESIWeight::Medium, 80));
	Line->SetContent(In);
	return Line;
}

UWidget* USIHUDWidget::Card(UWidget* Content, const FMargin& CardPad)
{
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Line->SetBrush(PlateBrush(WithAlpha(Style->Text, 0.22f), FLinearColor::Transparent, 0.0f, 3.0f));
	Line->SetPadding(FMargin(1.0f));
	UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Fill->SetBrush(PlateBrush(FLinearColor(0.022f, 0.026f, 0.02f, 0.88f), FLinearColor::Transparent, 0.0f, 2.0f));
	Fill->SetPadding(CardPad);
	Fill->SetContent(Content);
	Line->SetContent(Fill);
	O->AddChildToOverlay(Line);
	const EHorizontalAlignment H[] = { HAlign_Left, HAlign_Right, HAlign_Left, HAlign_Right };
	const EVerticalAlignment V[] = { VAlign_Top, VAlign_Top, VAlign_Bottom, VAlign_Bottom };
	for (int32 c = 0; c < 4; ++c)
	{
		for (const FVector2D Sz : { FVector2D(18.0f, 2.0f), FVector2D(2.0f, 18.0f) })
		{
			if (UOverlaySlot* S = O->AddChildToOverlay(Blk(Style->Accent, Sz))) { S->SetHorizontalAlignment(H[c]); S->SetVerticalAlignment(V[c]); }
		}
	}
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	return O;
}

void USIHUDWidget::BuildEndScreen()
{
	Dim = Blk(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), FVector2D(8.0f));
	Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
	Dim->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim)) { S->SetAnchors(FAnchors(0, 0, 1, 1)); S->SetOffsets(FMargin(0.0f)); }

	// --- Story moment, bottom-left: the headline in the world's voice, an amber rule, the story line.
	UVerticalBox* Story = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	EndTitle = Txt(FText::GetEmpty(), FMath::RoundToInt(Style->HeadlineSize * 1.6f), Style->Text, ESIWeight::Light, 260);
	EndReason = Txt(FText::GetEmpty(), Style->BodySize + 3, Style->Accent, ESIWeight::Medium, 60);
	Story->AddChildToVerticalBox(EndTitle);
	Pad(Story->AddChildToVerticalBox(Blk(Style->Accent, FVector2D(150.0f, 2.0f))), FMargin(6.0f, 4.0f, 0.0f, 12.0f));
	Pad(Story->AddChildToVerticalBox(EndReason), FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	Story->SetVisibility(ESlateVisibility::Collapsed);
	EndStory = Story;
	Place(Story, FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(Style->EdgeMargin + 54.0f, -(Style->EdgeMargin + 70.0f)));

	// --- Combat report card, right.
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(Txt(LOCTEXT("EndHead", "COMBAT REPORT"), Style->PlateNumberSize, Style->Text, ESIWeight::Light, 320)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Bottom);
	}
	EndSortie = Txt(FText::GetEmpty(), Style->SmallSize, Style->DimText, ESIWeight::Medium, 220);
	if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(EndSortie)) { S->SetVerticalAlignment(VAlign_Bottom); S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f)); }
	Body->AddChildToVerticalBox(Head);
	if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(Hair(WithAlpha(Style->Accent, 0.8f)))) { S->SetHorizontalAlignment(HAlign_Fill); S->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 12.0f)); }

	UHorizontalBox* Main = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Stats = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const FText Labels[] = { LOCTEXT("StStopped", "BOMBS STOPPED"), LOCTEXT("StYours", "BY YOUR WINGTIP"), LOCTEXT("StHomes", "HOMES HIT"), LOCTEXT("StTime", "FLIGHT TIME") };
	EndStatValues.Reset();
	for (int32 i = 0; i < 4; ++i)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Blk(Style->Accent, FVector2D(5.0f, 5.0f)))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f)); }
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Txt(Labels[i], Style->LabelSize, Style->DimText, ESIWeight::Medium, 200))) { S->SetVerticalAlignment(VAlign_Center); S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
		if (i == 0)
		{
			EndBestTag = Txt(LOCTEXT("NewBest", "NEW BEST"), Style->SmallSize, Style->Accent, ESIWeight::Medium, 200);
			if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(EndBestTag)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(16.0f, 0.0f, 14.0f, 0.0f)); }
		}
		UTextBlock* V = Txt(FText::FromString(TEXT("0")), Style->PlateNumberSize, Style->Text, ESIWeight::Medium, 40);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(V)) { S->SetVerticalAlignment(VAlign_Center); }
		EndStatValues.Add(V);
		Pad(Stats->AddChildToVerticalBox(Row), FMargin(0.0f, 3.0f));
		if (UVerticalBoxSlot* S = Stats->AddChildToVerticalBox(Hair(WithAlpha(Style->Text, 0.13f)))) { S->SetHorizontalAlignment(HAlign_Fill); }
	}
	if (UHorizontalBoxSlot* S = Main->AddChildToHorizontalBox(Stats)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }

	// Rating in world icons: three RAF roundels, lit by how the town came through.
	UVerticalBox* Rate = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Roundels = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	EndRoundels.Reset();
	EndRoundelBoxes.Reset();
	for (int32 r = 0; r < 3; ++r)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(40.0f);
		Box->SetHeightOverride(40.0f);
		UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		for (const float Sz : { 40.0f, 26.0f, 13.0f })
		{
			UImage* C = Circle(Sz, Style->PipEmpty);
			if (UOverlaySlot* S = O->AddChildToOverlay(C)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
			EndRoundels.Add(C);
		}
		Box->SetContent(O);
		Box->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Pad(Roundels->AddChildToHorizontalBox(Box), FMargin(r == 0 ? 0.0f : 9.0f, 0.0f, 0.0f, 0.0f));
		EndRoundelBoxes.Add(Box);
	}
	if (UVerticalBoxSlot* S = Rate->AddChildToVerticalBox(Roundels)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 10.0f)); }
	EndRank = Txt(FText::GetEmpty(), Style->ValueSize + 4, Style->Accent, ESIWeight::Medium, 160);
	if (UVerticalBoxSlot* S = Rate->AddChildToVerticalBox(EndRank)) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = Rate->AddChildToVerticalBox(Txt(LOCTEXT("Rating", "RATING"), Style->SmallSize, Style->DimText, ESIWeight::Medium, 300))) { S->SetHorizontalAlignment(HAlign_Center); }
	USizeBox* RateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RateBox->SetWidthOverride(210.0f);
	RateBox->SetContent(Rate);
	if (UHorizontalBoxSlot* S = Main->AddChildToHorizontalBox(RateBox)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(30.0f, 0.0f, 0.0f, 0.0f)); }
	Body->AddChildToVerticalBox(Main);

	// The wave, bomb by bomb.
	Pad(Body->AddChildToVerticalBox(Txt(LOCTEXT("WaveHead", "THE WAVE, BOMB BY BOMB"), Style->SmallSize, Style->DimText, ESIWeight::Medium, 240)), FMargin(0.0f, 22.0f, 0.0f, 8.0f));
	UHorizontalBox* Pips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	EndWavePips.Reset();
	for (int32 i = 0; i < Style->MaxWavePips; ++i)
	{
		UImage* P = Blk(Style->PipEmpty, FVector2D(30.0f, 12.0f));
		Pad(Pips->AddChildToHorizontalBox(P), FMargin(i == 0 ? 0.0f : 6.0f, 0.0f, 0.0f, 0.0f));
		EndWavePips.Add(P);
	}
	Body->AddChildToVerticalBox(Pips);
	UHorizontalBox* Key = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const TPair<FText, FLinearColor> Keys[] = { { LOCTEXT("KYours", "YOURS"), Style->Accent }, { LOCTEXT("KSquad", "SQUADRON"), Style->PipSquadron }, { LOCTEXT("KHomes", "ON HOMES"), Style->Danger } };
	for (int32 k = 0; k < 3; ++k)
	{
		if (UHorizontalBoxSlot* S = Key->AddChildToHorizontalBox(Blk(Keys[k].Value, FVector2D(12.0f, 8.0f)))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(k == 0 ? 0.0f : 22.0f, 0.0f, 8.0f, 0.0f)); }
		if (UHorizontalBoxSlot* S = Key->AddChildToHorizontalBox(Txt(Keys[k].Key, Style->SmallSize, Style->DimText, ESIWeight::Medium, 160))) { S->SetVerticalAlignment(VAlign_Center); }
	}
	Pad(Body->AddChildToVerticalBox(Key), FMargin(0.0f, 8.0f, 0.0f, 0.0f));

	// Buttons: [R] FLY AGAIN (amber plate) and [ESC] MENU.
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 b = 0; b < 2; ++b)
	{
		const bool bPrimary = b == 0;
		UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Plate->SetBrush(PlateBrush(bPrimary ? WithAlpha(Style->Accent, 0.34f) : WithAlpha(Style->Text, 0.08f), FLinearColor::Transparent, 0.0f, 2.0f));
		Plate->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
		UHorizontalBox* In = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = In->AddChildToHorizontalBox(Blk(bPrimary ? Style->Accent : WithAlpha(Style->Text, 0.35f), FVector2D(4.0f, 4.0f)))) { S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f)); }
		if (UHorizontalBoxSlot* S = In->AddChildToHorizontalBox(KeyCap(bPrimary ? LOCTEXT("KR", "R") : LOCTEXT("KEsc", "ESC")))) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(0.0f, 10.0f, 14.0f, 10.0f)); }
		UTextBlock* L = Txt(bPrimary ? LOCTEXT("FlyAgain", "FLY AGAIN") : LOCTEXT("ToMenu", "MENU"), Style->BodySize + 3, Style->Text, ESIWeight::Medium, 260);
		if (bPrimary) { EndAgainText = L; }
		if (UHorizontalBoxSlot* S = In->AddChildToHorizontalBox(L)) { S->SetVerticalAlignment(VAlign_Center); }
		Plate->SetContent(In);
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(Plate)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(b == 0 ? 0.0f : 14.0f, 0.0f, 0.0f, 0.0f)); }
	}
	Pad(Body->AddChildToVerticalBox(Buttons), FMargin(0.0f, 26.0f, 0.0f, 0.0f));

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Width->SetWidthOverride(680.0f);
	Width->SetContent(Body);
	EndPanel = Card(Width, FMargin(38.0f, 28.0f, 38.0f, 30.0f));
	EndPanel->SetVisibility(ESlateVisibility::Collapsed);
	Place(EndPanel, FAnchors(1.0f, 0.5f), FVector2D(1.0f, 0.5f), FVector2D(-(Style->EdgeMargin + 64.0f), -40.0f));
}
// ---------------------------------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------------------------------

USIMatchComponent* USIHUDWidget::GetMatch() const
{
	const AGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	return GM ? GM->FindComponentByClass<USIMatchComponent>() : nullptr;
}

void USIHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Bind();
}

void USIHUDWidget::Bind()
{
	if (bBound)
	{
		return;
	}
	USIMatchComponent* Match = GetMatch();
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Match || !Pawn)
	{
		return;
	}
	Match->OnStateChanged.AddUniqueDynamic(this, &USIHUDWidget::HandleState);
	Match->OnCountdownTick.AddUniqueDynamic(this, &USIHUDWidget::HandleCountdown);
	Match->OnBombResolved.AddUniqueDynamic(this, &USIHUDWidget::HandleResolved);
	Match->OnPrompt.AddUniqueDynamic(this, &USIHUDWidget::HandlePrompt);
	if (USIInputEchoComponent* Echo = PC->FindComponentByClass<USIInputEchoComponent>()) { Echo->OnInputVisualized.AddUniqueDynamic(this, &USIHUDWidget::HandleInput); }
	if (USIHealthComponent* H = Pawn->FindComponentByClass<USIHealthComponent>()) { H->OnDamaged.AddUniqueDynamic(this, &USIHUDWidget::HandleDamaged); }
	if (USIWingtipComponent* W = Pawn->FindComponentByClass<USIWingtipComponent>())
	{
		W->OnBombTipped.AddUniqueDynamic(this, &USIHUDWidget::HandleTipped);
		W->OnContact.AddUniqueDynamic(this, &USIHUDWidget::HandleContact);
	}
	StoppedTotal->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), Match->GetStats().Total)));
	LastState = Match->GetState();
	bBound = true;
}

void USIHUDWidget::NativeDestruct()
{
	if (USIMatchComponent* Match = GetMatch())
	{
		Match->OnStateChanged.RemoveAll(this);
		Match->OnCountdownTick.RemoveAll(this);
		Match->OnBombResolved.RemoveAll(this);
		Match->OnPrompt.RemoveAll(this);
	}
	bBound = false;
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------------------------

void USIHUDWidget::ShowEvent(const FText& Main, const FText& Sub, const FLinearColor& Colour, float Seconds, const FText& Kicker)
{
	// The headline stays cream; the news colour goes to the edge, the rule and the kicker.
	BannerText->SetText(Main);
	BannerText->SetColorAndOpacity(FSlateColor(Style->Text));
	BannerRule->SetColorAndOpacity(Colour);
	BannerEdge->SetColorAndOpacity(Colour);
	BannerKicker->SetText(Kicker);
	BannerKicker->SetColorAndOpacity(FSlateColor(Colour));
	BannerKicker->SetVisibility(Kicker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	SubText->SetText(Sub);
	SubText->SetVisibility(Sub.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	EventLeft = Seconds;
	EventAge = 0.0f;
}

void USIHUDWidget::HandleState(ESIMatchState NewState, ESIMatchState OldState)
{
	StateAge = 0.0f;
	LastState = NewState;
	if (NewState == ESIMatchState::Ready)
	{
		Outcomes.Reset();
		PlayerTipped.Reset();
	}
	if (NewState == ESIMatchState::Active)
	{
		ShowEvent(LOCTEXT("Go", "SCRAMBLE"), FText::GetEmpty(), Style->Accent, 1.1f, LOCTEXT("KickGo", "ALL FIGHTERS"));
	}
	if (NewState == ESIMatchState::Defeat)
	{
		VignetteAlpha = 1.0f;
	}
}

void USIHUDWidget::HandleCountdown(int32 SecondsLeft)
{
}

void USIHUDWidget::HandleResolved(const FSIBombResult& Result)
{
	if (Result.bTipped && !Result.Ground.bHit)
	{
		// The player's own tip if one of the bombs our wingtip tipped has just finished; otherwise a squadron mate's.
		EPip Who = EPip::Squadron;
		for (int32 i = 0; i < PlayerTipped.Num(); ++i)
		{
			const AActor* B = PlayerTipped[i].Get();
			const USITippableComponent* T = B ? B->FindComponentByClass<USITippableComponent>() : nullptr;
			if (!B || (T && T->GetState() == ESIAutopilotState::Done))
			{
				PlayerTipped.RemoveAt(i);
				Who = EPip::Stopped;
				break;
			}
		}
		Outcomes.Add(Who);
		StoppedPop = 1.0f;
		return;
	}
	Outcomes.Add(EPip::Homes);
	if (Result.bTipped)
	{
		// Drop the matching player tip so it is not credited to a later field landing.
		for (int32 i = 0; i < PlayerTipped.Num(); ++i)
		{
			const AActor* B = PlayerTipped[i].Get();
			const USITippableComponent* T = B ? B->FindComponentByClass<USITippableComponent>() : nullptr;
			if (!B || (T && T->GetState() == ESIAutopilotState::Done)) { PlayerTipped.RemoveAt(i); break; }
		}
	}
	TownPop = 1.0f;
	StoppedPop = Result.bTipped ? 1.0f : 0.0f;
	VignetteAlpha = FMath::Max(VignetteAlpha, 0.6f);
	const FText Where = SIZones::Label(Result.Ground.Kind);
	ShowEvent(Result.bTipped ? FText::Format(LOCTEXT("HitWhere", "{0} HIT"), Where) : LOCTEXT("GotThrough", "ONE GOT THROUGH"),
		Result.bTipped ? LOCTEXT("HitSub", "Tipped too early. That one counts.") : LOCTEXT("ThroughSub", "It reached the town."), Style->Danger, Style->BannerSeconds,
		LOCTEXT("KickDamage", "DAMAGE REPORT"));
}

void USIHUDWidget::HandlePrompt(FName Id, const FText& Text)
{
	PromptText->SetText(Text);
	PromptLeft = 3.4f;
	PromptAge = 0.0f;
}

void USIHUDWidget::HandleInput(FName KeyId, bool bSimulated)
{
	for (FLegendRow& R : Legend)
	{
		if (R.Id == KeyId)
		{
			R.Lit = 1.0f;
		}
	}
}

void USIHUDWidget::HandleDamaged(float Amount, ESIDamageSource Source, float HealthAfter)
{
	VignetteAlpha = FMath::Max(VignetteAlpha, FMath::Clamp(Amount / 25.0f, 0.5f, 1.0f));
}

void USIHUDWidget::HandleContact(FVector Where)
{
	VignetteAlpha = FMath::Max(VignetteAlpha, 0.6f);
}

void USIHUDWidget::HandleTipped(AActor* Bomb, FSIGroundClass Predicted)
{
	if (Bomb) { PlayerTipped.AddUnique(Bomb); }
	FPopup P;
	P.Text = Txt(FText::FromString(TEXT("+1")), Style->HeadlineSize, Predicted.bHit ? Style->Danger : Style->Accent, ESIWeight::Light);
	P.Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(P.Text)) { S->SetAlignment(FVector2D(0.5f, 1.0f)); S->SetAutoSize(true); }
	P.World = Bomb ? Bomb->GetActorLocation() : FVector::ZeroVector;
	Popups.Add(P);
}

// ---------------------------------------------------------------------------------------------
// Per frame
// ---------------------------------------------------------------------------------------------

bool USIHUDWidget::Project(const FVector& World, FVector2D& OutScreen) const
{
	APlayerController* PC = GetOwningPlayer();
	FVector2D Screen;
	if (!PC || !UGameplayStatics::ProjectWorldToScreen(PC, World, Screen, false))
	{
		return false;
	}
	const float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	OutScreen = Screen / Scale;
	return true;
}

void USIHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Bind();
	// Real time: hit-stop dilation must not freeze the HUD.
	const float Dt = FMath::Clamp(InDeltaTime, 0.0f, 0.1f);
	// Hidden under the start screen and the intro fly-in; eases in (about 200 ms) when the countdown begins.
	// Applied before the bind check: on the start screen there is no pawn yet, and the empty plates must not show.
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	const bool bFlowHides = !bBound || (Flow && (Flow->GetState() == EFactoryFlowState::Menu || Flow->GetState() == EFactoryFlowState::Intro || Flow->GetState() == EFactoryFlowState::None));
	FlowAlpha = FMath::FInterpConstantTo(FlowAlpha, bFlowHides ? 0.0f : 1.0f, Dt, 1.0f / FMath::Max(Style->FadeSeconds, 0.05f));
	Root->SetRenderOpacity(FlowAlpha);
	// Rounded-box outlines ignore the parent's render opacity (the plates' outlines still drew on the start screen
	// at opacity 0), so a faded-out HUD is also hidden outright.
	const ESlateVisibility RootVis = FlowAlpha > 0.001f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (Root->GetVisibility() != RootVis) { Root->SetVisibility(RootVis); }
	if (bFlowHides)
	{
		StateAge = 0.0f;
		MatchAge = 0.0f;
		return;
	}
	StateAge += Dt;
	MatchAge += Dt;
	UpdateReadouts(Dt);
	UpdateWorld(Dt);
	UpdateBanner(Dt);
	UpdateFeedback(Dt);
	UpdateEnd(Dt);
}

void USIHUDWidget::UpdateReadouts(float Dt)
{
	USIMatchComponent* Match = GetMatch();
	APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
	if (!Match)
	{
		return;
	}
	const FSIMatchStats& S = Match->GetStats();
	StoppedBig->SetText(FText::AsNumber(S.Stopped));
	StoppedTotal->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), S.Total)));
	StoppedBig->SetRenderScale(FVector2D(1.0f + 0.12f * StoppedPop));

	// Wave pips: resolved bombs in the order they came down, then the ones still to come.
	for (int32 i = 0; i < WavePips.Num(); ++i)
	{
		const EPip Want = i < Outcomes.Num() ? Outcomes[i] : EPip::Empty;
		if (PipShown[i] != Want)
		{
			PipShown[i] = Want;
			WavePips[i]->SetBrush(PipBrushes[(int32)Want]);
		}
		WavePips[i]->SetVisibility(i < S.Total ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		const bool bNewest = i == Outcomes.Num() - 1;
		const float Pop = bNewest ? (Want == EPip::Homes ? TownPop : StoppedPop) : 0.0f;
		WavePips[i]->SetRenderScale(FVector2D(1.0f + 0.5f * Pop));
	}

	const float Remaining = Match->GetTimeRemaining();
	const bool bLow = Match->GetState() == ESIMatchState::Active && Remaining <= Style->LowTimeSeconds;
	TimerText->SetText(FText::FromString(Clock(Remaining)));
	const float Pulse = bLow ? 0.5f + 0.5f * FMath::Cos(MatchAge * UE_TWO_PI * Style->LowTimePulseHz) : 0.0f;
	TimerText->SetColorAndOpacity(FSlateColor(bLow ? FMath::Lerp(Style->Danger, FMath::Lerp(Style->Danger, Style->Text, 0.35f), 1.0f - Pulse) : Style->Text));
	TimerText->SetRenderScale(FVector2D(1.0f + 0.05f * Pulse));
	TimerLabel->SetColorAndOpacity(FSlateColor(bLow ? Style->Danger : Style->Accent));
	SetPlateAlert(TimerPlate, bLow);

	for (int32 i = 0; i < Houses.Num(); ++i)
	{
		const bool bLit = i < S.Hits;
		const float Flash = (bLit && i == S.Hits - 1) ? TownPop : 0.0f;
		const FLinearColor C = bLit ? FMath::Lerp(Style->Danger, FLinearColor::White, Flash * 0.6f) : Style->PipEmpty;
		for (UImage* Part : Houses[i]) { if (Part) { Part->SetColorAndOpacity(C); } }
		HouseRoots[i]->SetRenderScale(FVector2D(1.0f + 0.5f * Flash));
		HouseRoots[i]->SetVisibility(i < Match->GetHitsToLose() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetPlateAlert(HomesPlate, S.Hits > 0 && S.Hits >= Match->GetHitsToLose() - 1);

	const USIFlightComponent* F = Pawn ? Pawn->FindComponentByClass<USIFlightComponent>() : nullptr;
	const USIHealthComponent* H = Pawn ? Pawn->FindComponentByClass<USIHealthComponent>() : nullptr;
	const USIWingtipComponent* W = Pawn ? Pawn->FindComponentByClass<USIWingtipComponent>() : nullptr;
	if (F)
	{
		SpeedText->SetText(FText::AsNumber(FMath::RoundToInt(F->GetSpeed() * 2.23694f)));
		FNumberFormattingOptions Opt;
		Opt.UseGrouping = true;
		AltText->SetText(FText::AsNumber(FMath::RoundToInt(F->GetAltitude() * 3.28084f / 10.0f) * 10, &Opt));
		const float Boost = F->GetBoostAvailable01();
		SetBar(BoostBar, Boost, F->IsBoosting() ? Style->Accent : Style->BoostFill);
	}
	if (H)
	{
		const float Hp = H->GetHealth01();
		FLinearColor C = Style->HullFill;
		if (Hp < Style->HullDangerAt)
		{
			C = FMath::Lerp(Style->Danger, WithAlpha(Style->Danger, 0.45f), 0.5f + 0.5f * FMath::Sin(MatchAge * 6.0f));
		}
		else if (Hp < Style->HullWarnAt)
		{
			C = Style->Accent;
		}
		SetBar(HullBar, Hp, C);
		SetPlateAlert(InstrumentPlate, Hp < Style->HullDangerAt && Match->GetState() == ESIMatchState::Active);
	}
	const AGameModeBase* GM = GetWorld()->GetAuthGameMode();
	const APlayerController* PC = GetOwningPlayer();
	const USIAutoplayComponent* Bot = PC ? PC->FindComponentByClass<USIAutoplayComponent>() : nullptr;
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	const bool bBotDriving = Bot && Bot->IsRunning() && (!Flow || Flow->IsBotDriving());
	AutoplayText->SetVisibility(bBotDriving && Style->bShowAutoplayTag ?ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// The step line: one short instruction for the situation the player is in right now.
	FText Step;
	if (Match->GetState() == ESIMatchState::Active)
	{
		const USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
		bool bAnyCruising = false;
		bool bAnyAhead = false;
		for (AActor* B : Air ? Air->GetBombs() : TArray<AActor*>())
		{
			const USITippableComponent* T = B ? B->FindComponentByClass<USITippableComponent>() : nullptr;
			const AActor* Claimer = Air ? Air->GetClaimer(B) : nullptr;
			const bool bMine = T && T->GetState() == ESIAutopilotState::Cruise && (Claimer ? Claimer == Pawn : !(Air && Air->IsLaneCovered(B->GetActorLocation().Y)));
			bAnyCruising |= bMine;
			bAnyAhead |= bMine && Pawn && FVector::DotProduct(B->GetActorLocation() - Pawn->GetActorLocation(), Pawn->GetActorForwardVector()) > 0.0f;
		}
		if (W && W->IsInSlot())
		{
			if (W->IsInBand())
			{
				Step = W->HasPreview() && W->GetPreviewClass().bHit
					? LOCTEXT("StepOverHomes", "Over homes: ease out until it reads FIELD")
					: LOCTEXT("StepHold", "Hold it there. It will roll into a dive");
			}
			else
			{
				Step = LOCTEXT("StepSlide", "Slide your wingtip under its wing");
			}
		}
		else if (bAnyCruising)
		{
			Step = bAnyAhead ? LOCTEXT("StepFind", "Fly alongside the marked bomb") : LOCTEXT("StepBehind", "Ease off (SPACE) and let the bomb catch up");
		}
		else
		{
			Step = LOCTEXT("StepWait", "More bombs inbound");
		}
	}
	if (!Step.EqualTo(LastStep))
	{
		LastStep = Step;
		StepText->SetText(Step);
		StepFlash = Step.IsEmpty() ? 0.0f : 1.0f;
		StepPlateWidget->SetVisibility(Step.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	StepFlash = FMath::Max(0.0f, StepFlash - Dt / 0.6f);
	StepText->SetColorAndOpacity(FSlateColor(FMath::Lerp(Style->Text, Style->Accent, StepFlash)));
	StepText->SetRenderTranslation(FVector2D(8.0f * StepFlash * StepFlash, 0.0f));

	// Legend: rows relabel in slot mode; a row brightens to the accent for a moment when its key fires; the whole
	// list settles to a quiet opacity once the player has had time to read it.
	const bool bSlot = W && W->IsInSlot();
	const float Idle = MatchAge > Style->LegendDimAfter ? Style->LegendIdleOpacity : 1.0f;
	LegendBox->SetRenderOpacity(FMath::FInterpTo(LegendBox->GetRenderOpacity(), Match->GetState() == ESIMatchState::Active || Match->GetState() == ESIMatchState::Ready ? FMath::Max(Idle, bSlot ? 0.85f : 0.0f) : 0.0f, Dt, 6.0f));
	for (FLegendRow& R : Legend)
	{
		R.Lit = FMath::Max(0.0f, R.Lit - Dt / FMath::Max(Style->KeyLitSeconds, 0.05f));
		R.Label->SetText(bSlot ? R.SlotLabel : R.FreeLabel);
		const bool bDisabled = bSlot && R.Id == TEXT("Boost");
		R.Key->SetColorAndOpacity(FSlateColor(FMath::Lerp(bDisabled ? Style->DimText : Style->Text, Style->Accent, R.Lit)));
		R.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(bDisabled ? WithAlpha(Style->DimText, 0.3f) : Style->DimText, Style->Text, R.Lit)));
	}
}

void USIHUDWidget::UpdateWorld(float Dt)
{
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
	const USIWingtipComponent* W = Pawn ? Pawn->FindComponentByClass<USIWingtipComponent>() : nullptr;
	USIMatchComponent* Match = GetMatch();
	const bool bPlaying = Match && Match->GetState() == ESIMatchState::Active;

	// Bomb tags: diamond + distance, hidden for the one we're slotted on.
	// The nearest cruising bomb is "the" target: labelled BOMB and kept on screen (pinned to the edge, or bottom centre
	// when it is behind). Others show a quieter distance tag only while on screen.
	TArray<AActor*> Bombs = Air ? Air->GetBombs() : TArray<AActor*>();
	// Bombs a squadron mate has taken get no marker: the tags show only what is left for the player.
	auto Cruising = [W, Air, Pawn](AActor* B)
	{
		const USITippableComponent* T = B ? B->FindComponentByClass<USITippableComponent>() : nullptr;
		const AActor* Claimer = Air ? Air->GetClaimer(B) : nullptr;
		return T && T->GetState() == ESIAutopilotState::Cruise && (!W || W->GetTarget() != B) && (Claimer ? Claimer == Pawn : !(Air && Air->IsLaneCovered(B->GetActorLocation().Y)));
	};
	int32 Nearest = INDEX_NONE;
	float NearestDist = TNumericLimits<float>::Max();
	for (int32 i = 0; Pawn && i < Bombs.Num(); ++i)
	{
		if (Cruising(Bombs[i]))
		{
			const float D = FVector::Dist(Bombs[i]->GetActorLocation(), Pawn->GetActorLocation());
			if (D < NearestDist) { NearestDist = D; Nearest = i; }
		}
	}
	const bool bSlotted = W && W->IsInSlot();
	const FVector2D View = UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	for (int32 i = 0; i < BombTags.Num(); ++i)
	{
		FBombTag& Tag = BombTags[i];
		AActor* B = i < Bombs.Num() ? Bombs[i] : nullptr;
		if (!bPlaying || !B || !Pawn || !Cruising(B))
		{
			Tag.Root->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const bool bMain = i == Nearest && !bSlotted;
		const float Miles = FVector::Dist(B->GetActorLocation(), Pawn->GetActorLocation()) / 160934.0f;
		const FString Dist = Miles < 0.1f ? FString::Printf(TEXT("%d YD"), FMath::RoundToInt(Miles * 1760.0f)) : FString::Printf(TEXT("%.1f MI"), Miles);
		FVector2D Screen;
		bool bOnScreen = Project(B->GetActorLocation() + FVector(0.0f, 0.0f, -400.0f), Screen);
		bool bBehind = false;
		if (PC && PC->PlayerCameraManager)
		{
			const FVector Local = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(B->GetActorLocation() - PC->PlayerCameraManager->GetCameraLocation());
			bBehind = Local.X <= 0.0f;
			bOnScreen = bOnScreen && !bBehind && Screen.X > 0.0f && Screen.Y > 0.0f && Screen.X < View.X && Screen.Y < View.Y;
			if (bMain && !bOnScreen)
			{
				if (bBehind)
				{
					Screen = FVector2D(View.X * 0.5f, View.Y - 150.0f);  // below the prompt line
				}
				else
				{
					// Direction on screen from the centre, clamped inside the margin.
					FVector2D Dir(Local.Y, -Local.Z);
					Dir = Dir.GetSafeNormal();
					const FVector2D Half = View * 0.5f - FVector2D(Style->EdgeMarkerMargin);
					const float Scale = FMath::Min(Half.X / FMath::Max(FMath::Abs(Dir.X), 0.001f), Half.Y / FMath::Max(FMath::Abs(Dir.Y), 0.001f));
					Screen = View * 0.5f + Dir * Scale;
					// Keep the edge marker off the corner plates and the controls strip (it slid under ALTITUDE).
					if (Screen.Y > View.Y - 300.0f && Screen.X > View.X - 420.0f) { Screen.X = FMath::Min(Screen.X, View.X - 440.0f); }
					if (Screen.Y < 250.0f && Screen.X > View.X - 420.0f) { Screen.X = FMath::Min(Screen.X, View.X - 440.0f); }
					if (Screen.Y < 250.0f && Screen.X < 600.0f) { Screen.X = FMath::Max(Screen.X, 620.0f); }
					Screen.Y = FMath::Min(Screen.Y, View.Y - 120.0f);
				}
			}
		}
		const bool bShow = bOnScreen || bMain;
		Tag.Root->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!bShow)
		{
			continue;
		}
		Tag.Text->SetText(FText::FromString(bMain ? (bBehind ? FString::Printf(TEXT("BOMB BEHIND  %s"), *Dist) : FString::Printf(TEXT("BOMB  %s"), *Dist)) : Dist));
		Tag.Root->SetRenderOpacity(bMain ? 1.0f : 0.55f);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Tag.Root->Slot)) { S->SetPosition(Screen + FVector2D(0.0f, bOnScreen ? 14.0f : 0.0f)); }
	}

	// Squadron callsigns: a quiet label over each hunter while it is in view and within range.
	{
		const TArray<AActor*> Planes = Air ? Air->GetAircraft() : TArray<AActor*>();
		for (int32 i = 0; i < WingTags.Num(); ++i)
		{
			FBombTag& Tag = WingTags[i];
			AActor* A = i < Planes.Num() ? Planes[i] : nullptr;
			FVector2D Screen = FVector2D::ZeroVector;
			bool bShow = bPlaying && A && Pawn && A->Tags.Num() > 1 && FVector::Dist(A->GetActorLocation(), Pawn->GetActorLocation()) < Style->WingTagRange * SI_CM
				&& Project(A->GetActorLocation() + FVector(0.0f, 0.0f, 500.0f), Screen);
			if (bShow && PC && PC->PlayerCameraManager)
			{
				const FVector Local = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(A->GetActorLocation() - PC->PlayerCameraManager->GetCameraLocation());
				bShow = Local.X > 0.0f && Screen.X > 40.0f && Screen.Y > 40.0f && Screen.X < View.X - 40.0f && Screen.Y < View.Y - 40.0f;
				// Never over the corner plates (user 2026-11-17: "HUD UI are overlapping").
				const bool bTopCorner = Screen.Y < 260.0f && (Screen.X < 600.0f || Screen.X > View.X - 420.0f);
				const bool bBottomRight = Screen.Y > View.Y - 300.0f && Screen.X > View.X - 420.0f;
				const bool bBottomStrip = Screen.Y > View.Y - 110.0f;
				bShow = bShow && !bTopCorner && !bBottomRight && !bBottomStrip;
			}
			Tag.Root->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bShow)
			{
				Tag.Text->SetText(FText::FromString(A->Tags[1].ToString().ToUpper()));
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Tag.Root->Slot)) { S->SetPosition(Screen - FVector2D(0.0f, 18.0f)); }
			}
		}
	}

	// Slot tag at the wingtips: the gap in inches and the hold line.
	const bool bSlot = bPlaying && W && W->IsInSlot();
	SlotTagAlpha = FMath::FInterpTo(SlotTagAlpha, bSlot ? 1.0f : 0.0f, Dt, 1.0f / FMath::Max(Style->FadeSeconds, 0.05f));
	SlotTag->SetRenderOpacity(SlotTagAlpha);
	if (W && SlotTagAlpha > 0.01f)
	{
		FVector2D Screen;
		if (Project((W->GetPlayerTip() + W->GetTargetTip()) * 0.5f + FVector(0.0f, 0.0f, 90.0f), Screen))
		{
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(SlotTag->Slot)) { S->SetPosition(Screen - FVector2D(0.0f, 40.0f)); }
		}
		const float GapIn = W->GetGapCm() / 2.54f;
		const bool bContact = W->GetGapCm() < W->BandGapMin && W->GetOverlap() > 0.05f;
		if (FMath::Abs(GapIn) < 24.0f)
		{
			GapText->SetText(FText::AsNumber(FMath::Max(0, FMath::RoundToInt(GapIn))));
			GapUnit->SetText(LOCTEXT("In", "IN"));
		}
		else
		{
			// Above the bomb's wing the gap reads as height above it, never a negative number.
			GapText->SetText(FText::AsNumber(FMath::Abs(FMath::RoundToInt(GapIn / 12.0f))));
			GapUnit->SetText(GapIn < 0.0f ? LOCTEXT("FeetAbove", "FT ABOVE") : LOCTEXT("Feet", "FT"));
		}
		const FLinearColor C = bContact ? Style->Danger : (W->IsInBand() ? Style->Accent : Style->Text);
		GapText->SetColorAndOpacity(FSlateColor(C));
		for (UImage* Part : BracketParts) { if (Part) { Part->SetColorAndOpacity(WithAlpha(C, 0.9f)); } }
		DisplayedHold = FMath::FInterpTo(DisplayedHold, W->GetProgress(), Dt, 12.0f);
		SetLine(HoldLine, DisplayedHold, Style->Accent);

		// Band meter: 0..GapMeterRangeIn inches; the hold band shaded, the ideal gap ticked, the needle at the live gap.
		const float MW = Style->GapMeterWidth;
		const float Range = FMath::Max(Style->GapMeterRangeIn, 1.0f);
		auto ToX = [MW, Range](float Inches) { return FMath::Clamp(Inches / Range, 0.0f, 1.0f) * MW; };
		const float BandLo = ToX(W->BandGapMin / 2.54f);
		const float BandHi = ToX(W->BandGapMax / 2.54f);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MeterBand->Slot)) { S->SetPosition(FVector2D(BandLo, 5.0f)); S->SetSize(FVector2D(FMath::Max(BandHi - BandLo, 2.0f), Style->GapMeterHeight)); }
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MeterIdeal->Slot)) { S->SetPosition(FVector2D(ToX(W->BandGapIdeal / 2.54f) - 1.0f, 5.0f)); }
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(MeterNeedle->Slot)) { S->SetPosition(FVector2D(ToX(GapIn) - 1.5f, 0.0f)); }
		MeterNeedle->SetColorAndOpacity(C);

		// Wake balance: safe zone sized from the tuning, needle eased a touch, red and key-hint pulse when off level.
		BalanceRow->SetVisibility(W->bWakeBalance ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (W->bWakeBalance)
		{
			const float BW = Style->BalanceMeterWidth;
			const float BH = Style->BalanceMeterHeight;
			const float Safe = FMath::Clamp(W->BalanceSafe, 0.05f, 1.0f);
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(BalanceSafeZone->Slot)) { S->SetPosition(FVector2D(BW * 0.5f * (1.0f - Safe), 6.0f)); S->SetSize(FVector2D(BW * Safe, BH)); }
			DisplayedBalance = FMath::FInterpTo(DisplayedBalance, FMath::Clamp(W->GetBalance(), -1.0f, 1.0f), Dt, 20.0f);
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(BalanceNeedle->Slot)) { S->SetPosition(FVector2D(BW * 0.5f * (1.0f + DisplayedBalance) - 2.0f, 0.0f)); }
			const bool bOff = !W->IsBalanceSafe();
			const float Pulse = 0.6f + 0.4f * FMath::Sin(StateAge * 14.0f);
			BalanceNeedle->SetColorAndOpacity(bOff ? WithAlpha(Style->Danger, Pulse) : Style->Text);
			// The key that brings it back lights up while the needle is out of the zone.
			BalanceKeyL->SetColorAndOpacity(FSlateColor(bOff && DisplayedBalance > 0.0f ? Style->Accent : Style->DimText));
			BalanceKeyR->SetColorAndOpacity(FSlateColor(bOff && DisplayedBalance < 0.0f ? Style->Accent : Style->DimText));
		}
	}

	// What's under the preview ring.
	const bool bGround = bSlot && W->HasPreview();
	GroundTag->SetVisibility(bGround ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bGround)
	{
		FVector2D Screen;
		if (Project(W->GetPreviewLocation() + FVector(0.0f, 0.0f, 300.0f), Screen))
		{
			// Keep it on screen: pinned just above the bottom edge if the ground point is below the view.
			Screen.X = FMath::Clamp(Screen.X, View.X * 0.3f, View.X * 0.7f);
			Screen.Y = FMath::Clamp(Screen.Y, 120.0f, View.Y - 190.0f);
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(GroundTag->Slot)) { S->SetPosition(Screen); }
			const FSIGroundClass G = W->GetPreviewClass();
			GroundText->SetText(SIZones::Label(G.Kind));
			GroundText->SetColorAndOpacity(FSlateColor(G.bHit ? Style->Danger : Style->Text));
			SetPlateAlert(GroundPlate, G.bHit);
		}
		else
		{
			GroundTag->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// +1 popups rise and fade.
	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		FPopup& P = Popups[i];
		P.Age += Dt;
		FVector2D Screen;
		const bool bVisible = Project(P.World, Screen);
		if (P.Age > 1.3f || !P.Text)
		{
			if (P.Text) { P.Text->RemoveFromParent(); }
			Popups.RemoveAt(i);
			continue;
		}
		P.Text->SetRenderOpacity(bVisible ? FMath::Clamp(P.Age / 0.12f, 0.0f, 1.0f) * (1.0f - FMath::Clamp((P.Age - 0.8f) / 0.5f, 0.0f, 1.0f)) : 0.0f);
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(P.Text->Slot)) { S->SetPosition(Screen - FVector2D(0.0f, 60.0f + 50.0f * EaseOut(P.Age / 1.3f))); }
	}
}

void USIHUDWidget::UpdateBanner(float Dt)
{
	USIMatchComponent* Match = GetMatch();
	if (!Match)
	{
		return;
	}
	float Opacity = 0.0f;
	float Slide = 0.0f;
	float Scale = 1.0f;
	switch (Match->GetState())
	{
	case ESIMatchState::Ready:
	{
		const float Remaining = Match->GetReadyRemaining();
		SubText->SetText(FText::Format(LOCTEXT("ObjSub", "Tip them over open fields. {0} hits on homes and it's over."), Match->GetHitsToLose()));
		BannerRule->SetColorAndOpacity(Style->Accent);
		if (Remaining > Match->GetCountdownSeconds())
		{
			BannerText->SetText(LOCTEXT("ObjTitle", "STOP THE FLYING BOMBS"));
			BannerText->SetColorAndOpacity(FSlateColor(Style->Text));
			Slide = 16.0f * (1.0f - EaseOut(StateAge / 0.25f));
		}
		else
		{
			const float Frac = Remaining - FMath::FloorToFloat(Remaining);
			BannerText->SetText(FText::AsNumber(FMath::Max(1, FMath::CeilToInt(Remaining))));
			BannerText->SetColorAndOpacity(FSlateColor(Style->Accent));
			Scale = 1.0f + 0.25f * Frac * Frac * Frac;
		}
		Opacity = FMath::Clamp(StateAge / 0.2f, 0.0f, 1.0f);
		break;
	}
	case ESIMatchState::Active:
		if (EventLeft > 0.0f)
		{
			EventLeft -= Dt;
			EventAge += Dt;
			Opacity = FMath::Clamp(EventLeft / Style->FadeSeconds, 0.0f, 1.0f) * FMath::Clamp(EventAge / 0.12f, 0.0f, 1.0f);
			Slide = 14.0f * (1.0f - EaseOut(EventAge / 0.2f));
		}
		break;
	default:
		break;
	}
	BannerText->SetRenderScale(FVector2D(Scale));
	BannerBox->SetRenderOpacity(Opacity);
	BannerBox->SetRenderTranslation(FVector2D(0.0f, Slide));
	for (UImage* I : BannerBand) { I->SetRenderOpacity(Opacity); }

	// Prompts: lower centre, one at a time, yield to banners.
	if (PromptLeft > 0.0f)
	{
		PromptLeft -= Dt;
		PromptAge += Dt;
	}
	const float PromptOpacity = PromptLeft > 0.0f && Opacity < 0.05f ? FMath::Clamp(PromptLeft / Style->FadeSeconds, 0.0f, 1.0f) * FMath::Clamp(PromptAge / Style->FadeSeconds, 0.0f, 1.0f) : 0.0f;
	PromptBox->SetRenderOpacity(PromptOpacity);
	PromptBox->SetRenderTranslation(FVector2D(0.0f, 10.0f * (1.0f - EaseOut(PromptAge / 0.22f))));
}

void USIHUDWidget::UpdateFeedback(float Dt)
{
	USIMatchComponent* Match = GetMatch();
	const APawn* Pawn = GetOwningPlayer() ? GetOwningPlayer()->GetPawn() : nullptr;
	const USIHealthComponent* H = Pawn ? Pawn->FindComponentByClass<USIHealthComponent>() : nullptr;
	VignetteAlpha = FMath::Max(0.0f, VignetteAlpha - 1.6f * Dt);
	float V = VignetteAlpha;
	if (H && Match && Match->GetState() == ESIMatchState::Active && H->GetHealth01() < 0.35f)
	{
		V = FMath::Max(V, 0.18f + 0.12f * FMath::Abs(FMath::Sin(MatchAge * 3.0f)));
	}
	for (UImage* Img : Vignette) { Img->SetRenderOpacity(V); }
	StoppedPop = FMath::Max(0.0f, StoppedPop - 3.0f * Dt);
	TownPop = FMath::Max(0.0f, TownPop - 1.5f * Dt);
}

void USIHUDWidget::UpdateEnd(float Dt)
{
	// Session best and sortie count survive the level reload on restart.
	static int32 GBestStopped = -1;
	static int32 GSortie = 0;
	USIMatchComponent* Match = GetMatch();
	if (!Match)
	{
		return;
	}
	const bool bEnd = Match->GetState() == ESIMatchState::Victory || Match->GetState() == ESIMatchState::Defeat;
	const float T = bEnd ? EaseOut(StateAge / 0.45f) : 0.0f;
	const float TStory = bEnd ? EaseOut((StateAge - 0.2f) / 0.5f) : 0.0f;
	Dim->SetRenderOpacity(Style->EndDim * T);
	// The play HUD steps aside (Hidden too: rounded-box outlines ignore render opacity).
	PlayLayer->SetRenderOpacity(1.0f - T);
	const ESlateVisibility PlayVis = T > 0.6f ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible;
	if (PlayLayer->GetVisibility() != PlayVis) { PlayLayer->SetVisibility(PlayVis); }
	const ESlateVisibility Vis = bEnd ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	EndPanel->SetVisibility(Vis);
	EndStory->SetVisibility(Vis);
	if (!bEnd)
	{
		bEndLatched = false;
		return;
	}
	const FSIMatchStats& S = Match->GetStats();
	if (!bEndLatched)
	{
		bEndLatched = true;
		EndSortieNo = ++GSortie;
		bEndNewBest = GBestStopped >= 0 && S.Stopped > GBestStopped;
		GBestStopped = FMath::Max(GBestStopped, S.Stopped);
	}
	const bool bWin = Match->GetState() == ESIMatchState::Victory;
	FText Title;
	switch (Match->GetDefeatReason())
	{
	case ESIDefeatReason::Flak:     Title = LOCTEXT("TFlak", "SHOT DOWN"); break;
	case ESIDefeatReason::Cable:    Title = LOCTEXT("TCable", "CAUGHT A CABLE"); break;
	case ESIDefeatReason::WingTorn: Title = LOCTEXT("TWing", "WING TORN"); break;
	case ESIDefeatReason::Ground:   Title = LOCTEXT("TGround", "CRASHED"); break;
	case ESIDefeatReason::TimeUp:   Title = LOCTEXT("TTime", "OUT OF TIME"); break;
	default:                       Title = LOCTEXT("TTown", "THE TOWN WAS HIT"); break;
	}
	EndTitle->SetText(bWin ? LOCTEXT("TWin", "WAVE STOPPED") : Title);
	EndReason->SetText(Match->GetReasonText());
	EndReason->SetColorAndOpacity(FSlateColor(bWin ? Style->Accent : Style->Danger));
	EndSortie->SetText(FText::FromString(FString::Printf(TEXT("SORTIE %03d"), EndSortieNo)));

	int32 Yours = 0;
	for (const EPip P : Outcomes) { Yours += P == EPip::Stopped ? 1 : 0; }
	if (EndStatValues.Num() == 4)
	{
		EndStatValues[0]->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), S.Stopped, S.Total)));
		EndStatValues[1]->SetText(FText::AsNumber(Yours));
		EndStatValues[2]->SetText(FText::AsNumber(S.Hits));
		EndStatValues[2]->SetColorAndOpacity(FSlateColor(S.Hits > 0 ? Style->Danger : Style->Text));
		EndStatValues[3]->SetText(FText::FromString(Clock(S.Elapsed)));
	}
	EndBestTag->SetVisibility(bEndNewBest ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// Rating: three roundels for a clean sheet, one less per home hit; none when the round is lost.
	const int32 Lit = bWin ? FMath::Clamp(3 - S.Hits, 1, 3) : 0;
	const FText Ranks[] = { LOCTEXT("R0", "GROUNDED"), LOCTEXT("R1", "CLOSE CALL"), LOCTEXT("R2", "STEADY"), LOCTEXT("R3", "ACE") };
	EndRank->SetText(Ranks[Lit]);
	EndRank->SetColorAndOpacity(FSlateColor(Lit > 0 ? Style->Accent : Style->Danger));
	const FLinearColor Ring[] = { FLinearColor(0.03f, 0.07f, 0.26f, 1.0f), Style->Text, FLinearColor(0.55f, 0.04f, 0.03f, 1.0f) };
	for (int32 r = 0; r < EndRoundelBoxes.Num(); ++r)
	{
		const float Pop = EaseOut((StateAge - 0.55f - 0.22f * r) / 0.3f);
		const bool bOn = r < Lit;
		for (int32 c = 0; c < 3; ++c)
		{
			if (EndRoundels.IsValidIndex(r * 3 + c)) { EndRoundels[r * 3 + c]->SetColorAndOpacity(bOn ? Ring[c] : WithAlpha(Style->PipEmpty, c == 1 ? 0.12f : 0.22f)); }
		}
		const float Scale = bOn ? FMath::Lerp(1.5f, 1.0f, Pop) : 1.0f;
		EndRoundelBoxes[r]->SetRenderScale(FVector2D(Scale));
		EndRoundelBoxes[r]->SetRenderOpacity(bOn ? Pop : 1.0f);
	}

	for (int32 i = 0; i < EndWavePips.Num(); ++i)
	{
		UImage* P = EndWavePips[i];
		P->SetVisibility(i < S.Total ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		FLinearColor C = Style->PipEmpty * FLinearColor(1.0f, 1.0f, 1.0f, 0.5f);
		if (Outcomes.IsValidIndex(i))
		{
			C = Outcomes[i] == EPip::Stopped ? Style->Accent : (Outcomes[i] == EPip::Homes ? Style->Danger : Style->PipSquadron);
		}
		P->SetColorAndOpacity(C);
		P->SetRenderOpacity(EaseOut((StateAge - 0.3f - 0.04f * i) / 0.2f));
	}
	EndAgainText->SetText(bWin ? LOCTEXT("FlyAgain", "FLY AGAIN") : LOCTEXT("TryAgain", "TRY AGAIN"));

	EndPanel->SetRenderOpacity(T);
	EndPanel->SetRenderTranslation(FVector2D(60.0f * (1.0f - T), 0.0f));
	EndStory->SetRenderOpacity(TStory);
	EndStory->SetRenderTranslation(FVector2D(-40.0f * (1.0f - TStory), 0.0f));
}
#undef LOCTEXT_NAMESPACE
