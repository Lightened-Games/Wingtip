// FactoryKit start screen: title + motif + tagline, PLAY / OPTIONS / QUIT, an options page. Built in C++, no boxes.
// Keyboard (arrows/WASD, Enter/Space, Esc, Left/Right on options) and mouse (hover + click). Owned by UFactoryFlowSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactoryMenuWidget.generated.h"

class UCanvasPanel;
class UVerticalBox;
class UTextBlock;
class UImage;
class UBorder;
class UFactoryFlowSubsystem;

UCLASS()
class UFactoryMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Init(UFactoryFlowSubsystem* InFlow) { Flow = InFlow; }
	/** Autoplay: highlight PLAY as if the player pressed it (visible in the video). */
	void PressPlayVisibly();
	void FadeOut();
	/** QA: open the options page (-FactoryKitShowOptions). */
	void ShowOptionsPage() { BuildPage(EPage::Options); }
	bool IsFadedOut() const { return bFadingOut && Opacity <= 0.01f; }

	/** Games whose sounds are not assets (runtime synth) set this; called with true on select, false on hover/adjust. */
	static TFunction<void(const UObject* /*Context*/, bool /*bSelect*/)> UISoundHook;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EPage : uint8 { Main, Options };
	enum class EItem : uint8 { Play, Options, Quit, Master, Music, Sfx, Sensitivity, InvertY, Quality, Window, Back };
	struct FRow
	{
		EItem Item;
		UBorder* HitArea = nullptr;
		UTextBlock* Text = nullptr;
		UImage* Bar = nullptr;
		UTextBlock* Value = nullptr; // options rows: the value in its own aligned column
		float Sel = 0.0f;
	};

	void BuildTree();
	void BuildPage(EPage Page);
	void AddRow(EItem Item);
	FText RowText(EItem Item) const;
	void Move(int32 Dir);
	void Activate();
	void Adjust(int32 Dir);
	void Back();
	void PlayUISound(bool bSelect) const;
	void RefreshTexts();

	UPROPERTY() TObjectPtr<UFactoryFlowSubsystem> Flow;
	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UVerticalBox> List;
	UPROPERTY() TObjectPtr<UTextBlock> PageTitle;
	UPROPERTY() TObjectPtr<UVerticalBox> Briefing;
	/** bMenuCard: the themed card the column sits on. */
	UPROPERTY() TObjectPtr<UWidget> Card;
	UWidget* WrapInCard(UWidget* Content);

	TArray<FRow> Rows;
	EPage Page = EPage::Main;
	int32 Index = 0;
	TEnumAsByte<EHorizontalAlignment> HAlign = HAlign_Left;
	float Opacity = 0.0f;
	bool bFadingOut = false;
	float PressFlash = 0.0f;
};
