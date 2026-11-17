#include "FactoryKitSettings.h"

#define LOCTEXT_NAMESPACE "FactoryKit"

UFactoryKitSettings::UFactoryKitSettings()
{
	CategoryName = TEXT("Game");
	Title = LOCTEXT("Title", "UNTITLED");
	PlayText = LOCTEXT("Play", "PLAY");
	OptionsText = LOCTEXT("Options", "OPTIONS");
	QuitText = LOCTEXT("Quit", "QUIT");
	BackText = LOCTEXT("Back", "BACK");
	MenuHintText = LOCTEXT("MenuHint", "↑ ↓  select      ENTER  confirm      ESC  back");
	AutoplayHintText = LOCTEXT("AutoplayHint", "autoplay · any key to take over");

	// Defaults that read well on a human-sized character; each game tunes them in DefaultGame.ini.
	FFactoryShot Close;   // slow sweep across the hero's front three-quarter
	Close.StartOffset = FVector(260.0f, 170.0f, 115.0f);
	Close.EndOffset = FVector(240.0f, -150.0f, 105.0f);
	Close.LookAtOffset = FVector(0.0f, 0.0f, 85.0f);
	Close.StartFOV = 38.0f; Close.EndFOV = 36.0f; Close.Duration = 8.0f;
	FFactoryShot Wide;    // the hero small in the level
	Wide.StartOffset = FVector(-520.0f, 420.0f, 300.0f);
	Wide.EndOffset = FVector(-480.0f, -380.0f, 280.0f);
	Wide.LookAtOffset = FVector(0.0f, 0.0f, 60.0f);
	Wide.StartFOV = 50.0f; Wide.EndFOV = 50.0f; Wide.Duration = 9.0f;
	MenuShots = { Close, Wide };

	FFactoryShot Intro;   // establishing high shot that descends behind the hero
	Intro.StartOffset = FVector(260.0f, 260.0f, 210.0f);   // in front, high three-quarter on the hero
	Intro.EndOffset = FVector(-280.0f, 70.0f, 160.0f);     // swings round behind the shoulder (the gameplay view)
	Intro.LookAtOffset = FVector(60.0f, 0.0f, 100.0f);
	Intro.StartFOV = 50.0f; Intro.EndFOV = 70.0f; Intro.Duration = 3.5f;
	IntroShots = { Intro };

	FFactoryShot Win;     // orbit across the hero's face
	// Offsets are from the actor origin (a character's capsule centre, about 90 cm up). The look-at sits to the hero's
	// right so the hero lands in the right third, clear of a centred end-screen panel.
	Win.StartOffset = FVector(300.0f, -140.0f, 60.0f);
	Win.EndOffset = FVector(290.0f, 90.0f, 75.0f);
	Win.LookAtOffset = FVector(0.0f, 60.0f, 55.0f);
	Win.StartFOV = 46.0f; Win.EndFOV = 42.0f; Win.Duration = 6.0f;
	VictoryShots = { Win };

	FFactoryShot Lose;    // slow push-in
	Lose.StartOffset = FVector(430.0f, 60.0f, 110.0f);
	Lose.EndOffset = FVector(250.0f, 30.0f, 80.0f);
	Lose.LookAtOffset = FVector(0.0f, 60.0f, 50.0f);
	Lose.StartFOV = 45.0f; Lose.EndFOV = 38.0f; Lose.Duration = 5.0f;
	DefeatShots = { Lose };
}

#undef LOCTEXT_NAMESPACE
