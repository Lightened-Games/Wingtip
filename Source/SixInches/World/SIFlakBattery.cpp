// Six Inches - friendly flak.

#include "World/SIFlakBattery.h"
#include "World/SIAirspaceSubsystem.h"
#include "Components/SIHealthComponent.h"
#include "Audio/SISynth.h"
#include "Feedback/SIPuffActor.h"
#include "Feedback/SICameraShakes.h"
#include "SixInches.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ASIFlakBattery::ASIFlakBattery()
{
	PrimaryActorTick.bCanEverTick = true;
	Pit = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pit"));
	Pit->SetCollisionProfileName(TEXT("BlockAll"));
	Pit->SetCanEverAffectNavigation(true);
	RootComponent = Pit;
	PitMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_FlakPit.SM_FlakPit")));
}

void ASIFlakBattery::BeginPlay()
{
	if (UStaticMesh* M = PitMesh.LoadSynchronous())
	{
		Pit->SetStaticMesh(M);
	}
	Super::BeginPlay();
	Rng.Initialize(GetUniqueID() * 7 + 3);
	Cooldown = Rng.FRandRange(0.5f, FireInterval + FireJitter);
}

void ASIFlakBattery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
	if (!Air || !Air->bHostilitiesActive)
	{
		return;
	}
	Cooldown -= DeltaSeconds;
	if (Cooldown > 0.0f)
	{
		return;
	}
	Cooldown = FireInterval + Rng.FRandRange(-FireJitter, FireJitter) * 0.5f;
	const FVector Me = GetActorLocation();
	const float RangeCm = Range * SI_CM;

	// The player first if inside the belt; otherwise any bomb inside it (they fire at everything in the sky).
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player && !Player->IsHidden() && Player->GetActorLocation().X >= Air->FlakStartX && FVector::Dist(Player->GetActorLocation(), Me) < RangeCm)
	{
		const USIHealthComponent* H = Player->FindComponentByClass<USIHealthComponent>();
		if (!H || !H->IsDepleted())
		{
			Fire(Player, true);
			return;
		}
	}
	for (AActor* Bomb : Air->GetBombs())
	{
		if (Bomb && Bomb->GetActorLocation().X >= Air->FlakStartX && FVector::Dist(Bomb->GetActorLocation(), Me) < RangeCm)
		{
			Fire(Bomb, false);
			return;
		}
	}
}

void ASIFlakBattery::Fire(AActor* Target, bool bPlayer)
{
	if (Target != LastTarget.Get())
	{
		LastTarget = Target;
		ShotsAtTarget = 0;
	}
	++ShotsAtTarget;
	const float Error = FMath::Max(MinError, BaseError / (1.0f + ShotsAtTarget * ErrorDecayPerShot)) * SI_CM;
	const FVector Aim = Target->GetActorLocation() + Target->GetVelocity() * ShellTime + Rng.GetUnitVector() * Error * Rng.FRandRange(0.4f, 1.0f);

	// Muzzle flash and the thump from the ground.
	FSIPuffSpec Muzzle;
	Muzzle.Material = ESIPuffMaterial::Glow;
	Muzzle.Life = 0.18f;
	Muzzle.SizeStart = 500.0f;
	Muzzle.SizeEnd = 900.0f;
	Muzzle.Color = FLinearColor(1.0f, 0.7f, 0.3f);
	Muzzle.Glow = 3.0f;
	ASIPuffActor::Spawn(GetWorld(), Muzzle, GetActorLocation() + FVector(300.0f, 0.0f, 400.0f));
	USIAudioLibrary::PlaySynthAtLocation(this, USIAudioLibrary::Flak(), GetActorLocation(), 0.5f, 0.7f, 2500.0f);

	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Aim]() { Burst(Aim); }), ShellTime, false);
}

void ASIFlakBattery::Burst(const FVector& At)
{
	UWorld* World = GetWorld();
	FSIPuffSpec Flash;
	Flash.Material = ESIPuffMaterial::Glow;
	Flash.Life = 0.25f;
	Flash.SizeStart = 1400.0f;
	Flash.SizeEnd = 2400.0f;
	Flash.Color = FLinearColor(1.0f, 0.68f, 0.3f);
	Flash.Glow = 3.2f;
	ASIPuffActor::Spawn(World, Flash, At);
	FSIPuffSpec Smoke;
	Smoke.Count = 3;
	Smoke.Spread = 300.0f;
	Smoke.Life = 7.0f;
	Smoke.SizeStart = 1200.0f;
	Smoke.SizeEnd = 3800.0f;
	Smoke.Color = FLinearColor(0.035f, 0.035f, 0.035f);
	Smoke.Opacity = 0.95f;
	ASIPuffActor::Spawn(World, Smoke, At);
	USIAudioLibrary::PlaySynthAtLocation(this, USIAudioLibrary::Flak(), At, 1.0f, 1.0f, 3000.0f);

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}
	const float D = FVector::Dist(Player->GetActorLocation(), At) / SI_CM;
	if (D < BurstRadius)
	{
		if (USIHealthComponent* H = Player->FindComponentByClass<USIHealthComponent>())
		{
			const float Dmg = MaxDamage * FMath::Pow(1.0f - D / BurstRadius, 1.2f);
			UE_LOG(LogSixInches, Display, TEXT("[SixInches] Flak hit: %.1f m, %.0f damage"), D, Dmg);
			H->ApplyDamage(Dmg, ESIDamageSource::Flak);
		}
	}
	else if (D < ShakeRadius)
	{
		UGameplayStatics::PlayWorldCameraShake(this, USIShakeFlak::StaticClass(), At, BurstRadius * SI_CM, ShakeRadius * SI_CM, 1.0f);
	}
}
