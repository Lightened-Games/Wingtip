#include "FactoryLegendWidget.h"
#include "FactoryKitStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Spacer.h"

TSharedRef<SWidget> UFactoryLegendWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UFactoryLegendWidget::BuildTree()
{
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	Rows.Reset();
	if (S->bLegendStrip)
	{
		BuildStrip();
	}
	else
	{
		List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LegendList"));
		UCanvasPanelSlot* LS = Root->AddChildToCanvas(List);
		LS->SetAutoSize(true);
		SetMenuPlacement(bMenuPlacement);
	}

	FlashText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("KeyFlash"));
	FactoryKitStyle::Apply(FlashText, S->TitleTypeface, S->SmallSize, S->AccentColor, 80);
	UCanvasPanelSlot* FS = Root->AddChildToCanvas(FlashText);
	FS->SetAnchors(FAnchors(0.0f, 1.0f));
	FS->SetAlignment(FVector2D(0.0f, 1.0f));
	FS->SetPosition(FVector2D(S->LegendMargin.X, -S->LegendMargin.Y));
	FS->SetAutoSize(true);
	FlashText->SetRenderOpacity(0.0f);

	if (List && !S->LegendTitle.IsEmpty())
	{
		UTextBlock* Head = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LegendTitle"));
		FactoryKitStyle::Apply(Head, S->TitleTypeface, S->SmallSize, S->AccentColor, 120);
		Head->SetText(S->LegendTitle);
		HeadText = Head;
		if (UVerticalBoxSlot* HS0 = List->AddChildToVerticalBox(Head)) { HS0->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f)); }
	}
	for (const FFactoryLegendRow& R : (List ? Pending : TArray<FFactoryLegendRow>()))
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyBox->SetMinDesiredWidth(S->SmallSize * 6.0f); // key column fits "MOUSE" / "SPACE" (PlazaDuty lesson)
		UTextBlock* K = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(K, S->TitleTypeface, S->SmallSize, S->TextColor, 80);
		K->SetText(R.KeyText);
		KeyBox->AddChild(K);
		H->AddChildToHorizontalBox(KeyBox);
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(L, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
		L->SetText(R.Label);
		H->AddChildToHorizontalBox(L);
		UVerticalBoxSlot* VS = List->AddChildToVerticalBox(H);
		VS->SetPadding(FMargin(0.0f, 3.0f));
		Rows.Add({ R, K, L, 0.0f });
	}

	Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AutoplayHint"));
	FactoryKitStyle::Apply(Hint, S->BodyTypeface, S->SmallSize, S->DimColor, 60);
	Hint->SetText(S->AutoplayHintText);
	UCanvasPanelSlot* HS = Root->AddChildToCanvas(Hint);
	HS->SetAnchors(FAnchors(1.0f, 1.0f));
	HS->SetAlignment(FVector2D(1.0f, 1.0f));
	HS->SetPosition(FVector2D(-S->LegendMargin.X, -S->LegendMargin.Y));
	HS->SetAutoSize(true);
	Hint->SetRenderOpacity(0.0f);
}

namespace
{
	FSlateBrush FillBrush(const FLinearColor& Fill, float Radius)
	{
		FSlateBrush B;
		B.DrawAs = Radius > 0.0f ? ESlateBrushDrawType::RoundedBox : ESlateBrushDrawType::Image;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		B.OutlineSettings.Width = 0.0f;
		return B;
	}
}

void UFactoryLegendWidget::BuildStrip()
{
	// One band across the bottom: [KEY] LABEL  [KEY] LABEL ... Frames are nested fills (rounded-box outlines ignore render opacity).
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	const int32 Size = S->LegendStripSize;
	Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LegendStrip"));
	Strip->SetBrush(FillBrush(S->LegendStripFill, 0.0f));
	Strip->SetPadding(FMargin(24.0f, 0.0f));
	Strip->SetHorizontalAlignment(HAlign_Center);
	Strip->SetVerticalAlignment(VAlign_Center);
	const float BandH = Size * 3.4f;
	UCanvasPanelSlot* SS = Root->AddChildToCanvas(Strip);
	SS->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
	SS->SetAlignment(FVector2D(0.0f, 1.0f));
	SS->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, BandH));
	// Amber hairline along the band's top edge, inside the band so it fades with it.
	UVerticalBox* Band = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Strip->SetPadding(FMargin(0.0f));
	Strip->SetHorizontalAlignment(HAlign_Fill);
	Strip->SetVerticalAlignment(VAlign_Fill);
	Strip->SetContent(Band);
	UBorder* Hair = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Hair->SetBrush(FillBrush(FLinearColor(S->AccentColor.R, S->AccentColor.G, S->AccentColor.B, 0.6f), 0.0f));
	Hair->SetPadding(FMargin(0.0f, 0.5f));
	Band->AddChildToVerticalBox(Hair)->SetHorizontalAlignment(HAlign_Fill);
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* HS0 = Band->AddChildToVerticalBox(H);
	HS0->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HS0->SetHorizontalAlignment(HAlign_Center);
	HS0->SetVerticalAlignment(VAlign_Center);

	if (!S->LegendTitle.IsEmpty())
	{
		UTextBlock* Head = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LegendTitle"));
		FactoryKitStyle::Apply(Head, S->TitleTypeface, Size, S->AccentColor, 260);
		Head->SetText(S->LegendTitle);
		HeadText = Head;
		UHorizontalBoxSlot* HS = H->AddChildToHorizontalBox(Head);
		HS->SetVerticalAlignment(VAlign_Center);
		HS->SetPadding(FMargin(0.0f, 0.0f, 34.0f, 0.0f));
	}
	const FLinearColor CapLine(S->TextColor.R, S->TextColor.G, S->TextColor.B, 0.55f);
	for (int32 i = 0; i < Pending.Num(); ++i)
	{
		const FFactoryLegendRow& R = Pending[i];
		UBorder* Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Cap->SetBrush(FillBrush(CapLine, 3.0f));
		Cap->SetPadding(FMargin(1.5f));
		UBorder* CapIn = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		CapIn->SetBrush(FillBrush(FLinearColor(0.02f, 0.025f, 0.02f, 0.9f), 2.0f));
		CapIn->SetPadding(FMargin(8.0f, 1.0f, 8.0f, 2.0f));
		UTextBlock* K = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(K, S->TitleTypeface, Size, S->TextColor, 80);
		K->SetText(R.KeyText);
		CapIn->SetContent(K);
		Cap->SetContent(CapIn);
		UHorizontalBoxSlot* CS = H->AddChildToHorizontalBox(Cap);
		CS->SetVerticalAlignment(VAlign_Center);
		CS->SetPadding(FMargin(i == 0 ? 0.0f : 30.0f, 0.0f, 10.0f, 0.0f));
		UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		FactoryKitStyle::Apply(L, S->TitleTypeface, Size, S->DimColor, 140);
		L->SetText(FText::FromString((R.ShortLabel.IsEmpty() ? R.Label : R.ShortLabel).ToString().ToUpper()));
		H->AddChildToHorizontalBox(L)->SetVerticalAlignment(VAlign_Center);
		Rows.Add({ R, K, L, 0.0f });
	}
}

void UFactoryLegendWidget::SetRows(const TArray<FFactoryLegendRow>& InRows)
{
	Pending = InRows; // call before AddToViewport (the flow subsystem does)
}

void UFactoryLegendWidget::Pulse(FName ActionId)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.ActionId == ActionId) { R.Glow = 1.0f; R.Flash = 1.0f; }
	}
}

void UFactoryLegendWidget::PulseKey(const FKey& Key)
{
	for (FRowWidgets& R : Rows)
	{
		if (R.Data.Keys.Contains(Key.GetFName())) { R.Glow = 1.0f; R.Flash = 1.0f; }
	}
}

void UFactoryLegendWidget::SetAutoplayHint(bool bVisible)
{
	if (bVisible && !bHint) { HintTime = 0.0f; }
	bHint = bVisible;
}

void UFactoryLegendWidget::SetLegendVisible(bool bVisible)
{
	bLegendVisible = bVisible;
}

void UFactoryLegendWidget::SetMenuPlacement(bool bMenu)
{
	bMenuPlacement = bMenu;
	UCanvasPanelSlot* LS = List ? Cast<UCanvasPanelSlot>(List->Slot) : nullptr;
	if (!LS)
	{
		return;
	}
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	LS->SetAnchors(FAnchors(bMenu ? 1.0f : 0.0f, 1.0f));
	LS->SetAlignment(FVector2D(bMenu ? 1.0f : 0.0f, 1.0f));
	LS->SetPosition(bMenu ? FVector2D(-S->LegendMargin.X, -S->LegendMargin.Y - S->MenuLegendLift) : FVector2D(S->LegendMargin.X, -S->LegendMargin.Y));
}

void UFactoryLegendWidget::SetFlashEnabled(bool bEnabled)
{
	bFlash = bEnabled;
	if (!bFlash)
	{
		for (FRowWidgets& R : Rows) { R.Flash = 0.0f; }
	}
}

void UFactoryLegendWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const UFactoryKitSettings* S = UFactoryKitSettings::Get();
	LegendAlpha = FactoryKitStyle::EaseTo(LegendAlpha, bLegendVisible ? 1.0f : 0.0f, InDeltaTime, S->MotionSeconds);
	for (FRowWidgets& R : Rows)
	{
		R.Glow = FMath::Max(0.0f, R.Glow - InDeltaTime / FMath::Max(S->LegendPulseSeconds, 0.05f));
		const float G = FMath::InterpEaseOut(0.0f, 1.0f, R.Glow, 2.0f);
		R.Key->SetColorAndOpacity(FSlateColor(FMath::Lerp(S->TextColor, S->AccentColor, G)));
		R.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(S->DimColor, S->TextColor, G)));
		R.Key->SetRenderOpacity(LegendAlpha);
		R.Label->SetRenderOpacity(LegendAlpha);
	}
	if (HeadText) { HeadText->SetRenderOpacity(LegendAlpha); }
	if (Strip)
	{
		Strip->SetRenderOpacity(LegendAlpha);
		const ESlateVisibility V = LegendAlpha > 0.01f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
		if (Strip->GetVisibility() != V) { Strip->SetVisibility(V); }
	}
	if (FlashText)
	{
		// Pressed keys, newest first, each fading over KeyFlashSeconds.
		FString Keys;
		float Alpha = 0.0f;
		for (FRowWidgets& R : Rows)
		{
			R.Flash = FMath::Max(0.0f, R.Flash - InDeltaTime / FMath::Max(S->KeyFlashSeconds, 0.05f));
			if (bFlash && R.Flash > 0.02f)
			{
				Keys += (Keys.IsEmpty() ? TEXT("") : TEXT("    ")) + R.Data.KeyText.ToString();
				Alpha = FMath::Max(Alpha, FMath::InterpEaseOut(0.0f, 1.0f, R.Flash, 2.0f));
			}
		}
		if (!Keys.IsEmpty() && Keys != LastFlash)
		{
			FlashText->SetText(FText::FromString(Keys));
			LastFlash = Keys;
		}
		FlashText->SetRenderOpacity(Alpha);
	}
	if (Hint)
	{
		HintTime += InDeltaTime;
		const float Target = !bHint ? 0.0f : (HintTime < S->AutoplayHintFullSeconds ? 1.0f : S->AutoplayHintDimAlpha);
		HintAlpha = FactoryKitStyle::EaseTo(HintAlpha, Target, InDeltaTime, S->MotionSeconds);
		Hint->SetRenderOpacity(HintAlpha);
	}
}
