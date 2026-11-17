// Six Inches - player game feel.

#include "Components/SIGameFeelComponent.h"
#include "Components/SIFlightComponent.h"
#include "Components/SIWingtipComponent.h"
#include "Components/SIHealthComponent.h"
#include "Audio/SISynth.h"
#include "Feedback/SICameraShakes.h"
#include "Feedback/SIPuffActor.h"
#include "SixInches.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

USIGameFeelComponent::USIGameFeelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USIGameFeelComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	Flight = Owner->FindComponentByClass<USIFlightComponent>();
	Wingtip = Owner->FindComponentByClass<USIWingtipComponent>();
	Health = Owner->FindComponentByClass<USIHealthComponent>();
	if (Health) { Health->OnDamaged.AddDynamic(this, &USIGameFeelComponent::HandleDamaged); }
	if (Wingtip)
	{
		Wingtip->OnSlotChanged.AddDynamic(this, &USIGameFeelComponent::HandleSlot);
		Wingtip->OnContact.AddDynamic(this, &USIGameFeelComponent::HandleContact);
		Wingtip->OnBombTipped.AddDynamic(this, &USIGameFeelComponent::HandleTipped);
	}
	if (Flight)
	{
		Flight->OnBuffet.AddDynamic(this, &USIGameFeelComponent::HandleBuffet);
		Flight->OnBoost.AddDynamic(this, &USIGameFeelComponent::HandleBoost);
		Flight->OnBrake.AddDynamic(this, &USIGameFeelComponent::HandleBrake);
		Flight->OnCrashed.AddDynamic(this, &USIGameFeelComponent::HandleCrashed);
	}
	USILoopSynth* S = nullptr;
	EngineAudio = USIAudioLibrary::CreateLoop(Owner, Owner->GetRootComponent(), ESILoopKind::Engine, 11, false, 0.0f, S);
	EngineSynth = S;
	WindAudio = USIAudioLibrary::CreateLoop(Owner, Owner->GetRootComponent(), ESILoopKind::Wind, 12, false, 0.0f, S);
	WindSynth = S;
	HoldAudio = USIAudioLibrary::CreateLoop(Owner, Owner->GetRootComponent(), ESILoopKind::HoldTone, 13, false, 0.0f, S);
	HoldSynth = S;
	if (HoldSynth) { HoldSynth->SetLoopVolume(0.0f); }
}

void USIGameFeelComponent::PlayShake(TSubclassOf<UCameraShakeBase> Shake, float Scale)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		PC->ClientStartCameraShake(Shake, Scale);
	}
}

void USIGameFeelComponent::HitStop()
{
	UWorld* World = GetWorld();
	if (!World || bHitStopActive || World->GetRealTimeSeconds() - LastHitStop < HitStopMinInterval)
	{
		return;
	}
	LastHitStop = World->GetRealTimeSeconds();
	bHitStopActive = true;
	UGameplayStatics::SetGlobalTimeDilation(this, HitStopDilation);
	FTimerHandle Handle;
	// Timers run in dilated time: HitStopSeconds of real time is HitStopSeconds * dilation of game time.
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		bHitStopActive = false;
	}), FMath::Max(HitStopSeconds * HitStopDilation, 0.001f), false);
}

void USIGameFeelComponent::HandleDamaged(float Amount, ESIDamageSource Source, float HealthAfter)
{
	// Hit-stop is for real hits (flak, cable). A wingtip scrape in the slot happens in a run of small contacts and
	// repeated 50 ms freezes read as lag there (user, 2026-11-13), so scrapes get the shake, sparks and sound only.
	if ((Source != ESIDamageSource::Contact || bHitStopOnContact) && Amount >= HitStopMinDamage)
	{
		HitStop();
	}
	PlayShake(Source == ESIDamageSource::Flak ? TSubclassOf<UCameraShakeBase>(USIShakeFlak::StaticClass()) : TSubclassOf<UCameraShakeBase>(USIShakeContact::StaticClass()),
		FMath::Clamp(Amount / 20.0f, 0.6f, 2.0f));
	if (Source == ESIDamageSource::Flak)
	{
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::FlakHit(), 0.9f);
	}
}

void USIGameFeelComponent::Vapour(const FVector& Where, float Scale)
{
	FSIPuffSpec Spec;
	Spec.Material = ESIPuffMaterial::Smoke;
	Spec.Life = 0.7f;
	Spec.SizeStart = 60.0f * Scale;
	Spec.SizeEnd = 260.0f * Scale;
	Spec.Color = FLinearColor(0.95f, 0.96f, 1.0f);
	Spec.Opacity = 0.35f;
	ASIPuffActor::Spawn(GetWorld(), Spec, Where);
}

void USIGameFeelComponent::HandleSlot(bool bInSlot, AActor* Target)
{
	if (bInSlot)
	{
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::SlotWhoosh());
		const AActor* Owner = GetOwner();
		if (Owner && Wingtip)
		{
			for (float S : { -1.0f, 1.0f })
			{
				Vapour(Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(Wingtip->GetInnerTipLocal(S)), 1.4f);
			}
		}
	}
}

void USIGameFeelComponent::HandleContact(FVector Where)
{
	USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::Contact());
	FSIPuffSpec Sparks;
	Sparks.Material = ESIPuffMaterial::Glow;
	Sparks.bSoftDot = true;
	Sparks.Count = 6;
	Sparks.Spread = 40.0f;
	Sparks.Life = 0.35f;
	Sparks.SizeStart = 30.0f;
	Sparks.SizeEnd = 6.0f;
	Sparks.Color = FLinearColor(1.0f, 0.75f, 0.35f);
	Sparks.Glow = 3.0f;
	Sparks.Velocity = FVector(-600.0f, 0.0f, -150.0f);
	ASIPuffActor::Spawn(GetWorld(), Sparks, Where);
}

void USIGameFeelComponent::HandleTipped(AActor* Bomb, FSIGroundClass Predicted)
{
	USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::TipCreak());
	USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::TipWhoosh(), 0.8f);
	USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::PlusOne());
	PlayShake(USIShakeContact::StaticClass(), 0.7f);
}

void USIGameFeelComponent::HandleBuffet(float Strength)
{
	// Beside a bomb the buffet shows in the planes themselves; a full camera shake on top read as vibration.
	PlayShake(USIShakeBuffet::StaticClass(), Strength * (Wingtip && Wingtip->IsInSlot() ? SlotBuffetShakeScale : 1.0f));
	USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::Bump(), FMath::Clamp(Strength, 0.3f, 1.0f));
}

void USIGameFeelComponent::HandleBoost(bool bOn)
{
	if (bOn)
	{
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::Boost());
	}
}

void USIGameFeelComponent::HandleBrake(bool bOn)
{
	if (bOn)
	{
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::Airbrake());
	}
}

void USIGameFeelComponent::HandleCrashed(ESIDamageSource Source)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	const FVector At = Owner->GetActorLocation();
	USIAudioLibrary::PlaySynth2D(this, Source == ESIDamageSource::Cable ? USIAudioLibrary::CableSnap() : USIAudioLibrary::Crash());
	if (Source == ESIDamageSource::Cable)
	{
		USIAudioLibrary::PlaySynth2D(this, USIAudioLibrary::Crash(), 0.8f);
	}
	FSIPuffSpec Fire;
	Fire.Material = ESIPuffMaterial::Glow;
	Fire.Count = 4;
	Fire.Spread = 500.0f;
	Fire.Life = 1.2f;
	Fire.SizeStart = 900.0f;
	Fire.SizeEnd = 2400.0f;
	Fire.Color = FLinearColor(1.0f, 0.55f, 0.2f);
	Fire.Glow = 2.5f;
	ASIPuffActor::Spawn(GetWorld(), Fire, At);
	FSIPuffSpec Smoke;
	Smoke.Count = 5;
	Smoke.Spread = 600.0f;
	Smoke.Life = 6.0f;
	Smoke.SizeStart = 1200.0f;
	Smoke.SizeEnd = 4000.0f;
	Smoke.Color = FLinearColor(0.06f, 0.06f, 0.06f);
	Smoke.Opacity = 0.9f;
	Smoke.Rise = 300.0f;
	ASIPuffActor::Spawn(GetWorld(), Smoke, At);
	PlayShake(USIShakeExplosion::StaticClass(), 2.0f);
	if (EngineAudio) { EngineAudio->Stop(); }
	if (HoldAudio) { HoldAudio->Stop(); }
}

void USIGameFeelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Flight)
	{
		return;
	}
	const float Speed01 = Flight->GetSpeed01();
	const bool bSlot = Wingtip && Wingtip->IsInSlot();
	if (EngineSynth)
	{
		EngineSynth->SetDrive(FMath::Clamp(Speed01 * 0.8f + (Flight->IsBoosting() ? 0.25f : 0.0f), 0.0f, 1.0f));
		EngineSynth->SetLoopVolume(Flight->IsCrashed() ? 0.0f : EngineVolume * (bSlot ? 0.75f : 1.0f));
	}
	if (WindSynth)
	{
		WindSynth->SetDrive(Speed01);
		WindSynth->SetLoopVolume(Flight->IsCrashed() ? 0.0f : WindVolume * (bSlot ? 1.6f : 1.0f) + 0.2f * Flight->GetTurbulence());
	}
	if (HoldSynth && Wingtip)
	{
		const float P = Wingtip->GetProgress();
		HoldSynth->SetDrive(P);
		HoldSynth->SetLoopVolume(bSlot && (Wingtip->IsInBand() || P > 0.02f) ? HoldVolume * (0.35f + 0.65f * P) : 0.0f);
	}

	// Vapour off the tips in hard pulls at speed, and steadily off the inner tip while in the band.
	VapourTimer -= DeltaTime;
	const AActor* Owner = GetOwner();
	if (VapourTimer <= 0.0f && Owner && !Flight->IsCrashed() && !Flight->IsLocked())
	{
		const bool bHardPull = FMath::Abs(Flight->GetInput().Pitch) > 0.6f && Flight->GetSpeed() > 170.0f;
		const bool bBand = Wingtip && Wingtip->IsInBand();
		if (bHardPull || bBand)
		{
			VapourTimer = VapourInterval * (bBand ? 2.0f : 1.0f);
			for (float S : { -1.0f, 1.0f })
			{
				if (bBand && Wingtip && S != Wingtip->GetSide())
				{
					continue;
				}
				Vapour(Owner->GetActorLocation() + Owner->GetActorRotation().RotateVector(Wingtip ? Wingtip->GetInnerTipLocal(S) : FVector(0.0f, 600.0f * S, 0.0f)), bBand ? 0.6f : 1.0f);
			}
		}
	}
}
