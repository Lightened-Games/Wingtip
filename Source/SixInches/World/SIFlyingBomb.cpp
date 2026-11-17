// Six Inches - flying bomb actor.

#include "World/SIFlyingBomb.h"
#include "World/SIAirspaceSubsystem.h"
#include "Audio/SISynth.h"
#include "Feedback/SIPuffActor.h"
#include "Feedback/SICameraShakes.h"
#include "SixInches.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

ASIFlyingBomb::ASIFlyingBomb()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Tags.Add(TEXT("SI.Bomb"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetCanEverAffectNavigation(false);
	RootComponent = Body;

	Chevron = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chevron"));
	Chevron->SetupAttachment(Body);
	Chevron->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Chevron->SetUsingAbsoluteRotation(true);
	Chevron->CastShadow = false;
	Chevron->ComponentTags.Add(TEXT("NoCollide"));

	Flame = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("Flame"));
	Flame->SetupAttachment(Body);
	Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flame->CastShadow = false;

	Autopilot = CreateDefaultSubobject<USITippableComponent>(TEXT("Autopilot"));

	BodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_FlyingBomb.SM_FlyingBomb")));
	ChevronMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_Chevron.SM_Chevron")));
	ChevronMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/SixInches/Materials/MI_SI_MarkerAccent.MI_SI_MarkerAccent")));
	FlameMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/SixInches/Materials/MI_SI_Dot.MI_SI_Dot")));
}

void ASIFlyingBomb::BeginPlay()
{
	if (UStaticMesh* M = BodyMesh.LoadSynchronous())
	{
		Body->SetStaticMesh(M);
		if (const UStaticMeshSocket* S = M->FindSocket(TEXT("TipL"))) { TipLeftLocal = S->RelativeLocation; }
		if (const UStaticMeshSocket* S = M->FindSocket(TEXT("TipR"))) { TipRightLocal = S->RelativeLocation; }
		if (const UStaticMeshSocket* S = M->FindSocket(TEXT("Jet"))) { JetLocal = S->RelativeLocation; }
	}
	if (UStaticMesh* M = ChevronMesh.LoadSynchronous())
	{
		Chevron->SetStaticMesh(M);
		if (UMaterialInterface* Mat = ChevronMaterial.LoadSynchronous())
		{
			Chevron->SetMaterial(0, Mat);
		}
	}
	if (UMaterialInterface* Mat = FlameMaterial.LoadSynchronous())
	{
		FlameMID = UMaterialInstanceDynamic::Create(Mat, this);
		FlameMID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(1.0f, 0.55f, 0.22f));
		FlameMID->SetScalarParameterValue(TEXT("Glow"), FlameGlow);
		FlameMID->SetScalarParameterValue(TEXT("Opacity"), 1.0f);
		Flame->AddElement(FlameMID, nullptr, false, FlameSize * 1.2f, FlameSize * 1.2f, nullptr);
	}
	Flame->SetRelativeLocation(JetLocal + FVector(25.0f, 0.0f, 0.0f)); // inside the tailpipe: behind it, it read as a shaded ball capping the pipe (user 2026-11-17)
	Super::BeginPlay();

	USILoopSynth* S = nullptr;
	JetAudio = USIAudioLibrary::CreateLoop(this, Body, ESILoopKind::PulseJet, 100 + static_cast<int32>(GetUniqueID() % 1000), true, JetFalloff, S);
	JetSynth = S;
	if (JetSynth)
	{
		JetSynth->SetLoopVolume(JetVolume);
		JetSynth->SetDrive(0.5f);
	}
	Autopilot->OnImpact.AddDynamic(this, &ASIFlyingBomb::HandleImpact);
	Autopilot->OnStateChanged.AddDynamic(this, &ASIFlyingBomb::HandleState);
	if (USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>())
	{
		Air->RegisterBomb(this);
	}
}

void ASIFlyingBomb::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* W = GetWorld())
	{
		if (USIAirspaceSubsystem* Air = W->GetSubsystem<USIAirspaceSubsystem>())
		{
			Air->UnregisterBomb(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

FSIWingFrame ASIFlyingBomb::GetWingFrame() const
{
	FSIWingFrame W;
	const FTransform T = GetActorTransform();
	W.Location = T.GetLocation();
	W.Forward = Autopilot ? FRotator(Autopilot->GetFlightRotation().Pitch, Autopilot->GetFlightRotation().Yaw, 0.0f).Vector() : GetActorForwardVector();
	W.Right = GetActorRightVector();
	W.Up = GetActorUpVector();
	W.TipLeft = T.TransformPosition(TipLeftLocal);
	W.TipRight = T.TransformPosition(TipRightLocal);
	W.Speed = Autopilot ? Autopilot->GetSpeed() : 0.0f;
	return W;
}

bool ASIFlyingBomb::IsTippable() const
{
	return Autopilot && Autopilot->GetState() == ESIAutopilotState::Cruise;
}

void ASIFlyingBomb::SetLiftProgress(float Progress01, float LiftedSide)
{
	if (Autopilot) { Autopilot->SetLiftProgress(Progress01, LiftedSide); }
}

void ASIFlyingBomb::Tip(float LiftedSide)
{
	if (Autopilot) { Autopilot->Tip(LiftedSide); }
}

FVector ASIFlyingBomb::PredictTipImpact(float LiftedSide) const
{
	return Autopilot ? Autopilot->PredictTipImpact(LiftedSide) : GetActorLocation();
}

void ASIFlyingBomb::Bump(const FVector& Velocity)
{
	if (Autopilot) { Autopilot->Bump(Velocity); }
}

void ASIFlyingBomb::HandleState(ESIAutopilotState NewState)
{
	if (NewState == ESIAutopilotState::Tipped || NewState == ESIAutopilotState::TownDive)
	{
		// The famous silence: the jet sputters a moment longer, then cuts.
		EngineCutTimer = NewState == ESIAutopilotState::Tipped ? 0.6f : 0.2f;
		Chevron->SetVisibility(false);
	}
}

void ASIFlyingBomb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FlickerTime += DeltaSeconds;
	if (EngineCutTimer > 0.0f)
	{
		EngineCutTimer -= DeltaSeconds;
		if (EngineCutTimer <= 0.0f)
		{
			if (JetAudio) { JetAudio->FadeOut(0.25f, 0.0f); }
			Flame->SetVisibility(false);
		}
	}
	// A small hot core in the nozzle. The flicker runs every frame through the material (cheap) instead of resizing the
	// billboard, which rebuilt its render proxy and so only stepped 20 times a second (user 2026-11-17: "these jets are lagging").
	if (FlameMID && Flame->IsVisible())
	{
		const float Pulse = FMath::Abs(FMath::Sin(FlickerTime * 47.0f * UE_PI));
		const float F = FMath::Clamp(0.55f + 0.35f * Pulse + 0.2f * FMath::PerlinNoise1D(FlickerTime * 9.0f), 0.3f, 1.0f);
		FlameMID->SetScalarParameterValue(TEXT("Opacity"), F);
		FlameMID->SetScalarParameterValue(TEXT("Glow"), FlameGlow * (0.7f + 0.5f * F));
	}

	// World marker: an amber chevron above the bomb, scaled so it stays readable from far away.
	if (Chevron->IsVisible() || (Autopilot && Autopilot->GetState() == ESIAutopilotState::Cruise))
	{
		const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
		const float Dist = Cam ? FVector::Dist(Cam->GetCameraLocation(), GetActorLocation()) : 100000.0f;
		const bool bShow = Dist > ChevronHideDistance && Autopilot && Autopilot->GetState() == ESIAutopilotState::Cruise;
		Chevron->SetVisibility(bShow);
		if (bShow)
		{
			const float Scale = FMath::Clamp(Dist / 100000.0f * ChevronScalePerKm, 0.6f, 9.0f);
			const FVector CamLoc = Cam ? Cam->GetCameraLocation() : GetActorLocation();
			const float Yaw = (CamLoc - GetActorLocation()).Rotation().Yaw;
			Chevron->SetWorldLocationAndRotation(GetActorLocation() + FVector(0.0f, 0.0f, ChevronHeight * FMath::Max(1.0f, Scale * 0.6f)), FRotator(0.0f, Yaw, 0.0f));
			Chevron->SetWorldScale3D(FVector(Scale));
		}
	}
}

void ASIFlyingBomb::HandleImpact(FVector Location, bool bTipped)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Flash, fireball, dirt, then a column of smoke that hangs over the spot (and the turbulence that comes with it).
	FSIPuffSpec Flash;
	Flash.Material = ESIPuffMaterial::Glow;
	Flash.Count = 3;
	Flash.Spread = 900.0f;
	Flash.Life = 0.9f;
	Flash.SizeStart = 2500.0f;
	Flash.SizeEnd = 6000.0f;
	Flash.Color = FLinearColor(1.0f, 0.62f, 0.28f);
	Flash.Glow = 3.0f;
	ASIPuffActor::Spawn(World, Flash, Location + FVector(0.0f, 0.0f, 800.0f));

	FSIPuffSpec Dirt;
	Dirt.Count = 6;
	Dirt.Spread = 1500.0f;
	Dirt.Life = 3.5f;
	Dirt.SizeStart = 2000.0f;
	Dirt.SizeEnd = 5500.0f;
	Dirt.Color = FLinearColor(0.16f, 0.12f, 0.08f);
	Dirt.Opacity = 0.95f;
	Dirt.Rise = 900.0f;
	ASIPuffActor::Spawn(World, Dirt, Location + FVector(0.0f, 0.0f, 1000.0f));

	for (int32 i = 0; i < 6; ++i)
	{
		FSIPuffSpec Col;
		Col.Count = 2;
		Col.Spread = 1200.0f;
		Col.Delay = 0.4f + i * 0.5f;
		Col.Life = TurbulenceLife * 0.9f;
		Col.SizeStart = 3500.0f + i * 600.0f;
		Col.SizeEnd = 9000.0f + i * 1500.0f;
		Col.Color = FLinearColor(0.22f, 0.21f, 0.2f);
		Col.Opacity = 0.75f - i * 0.07f;
		Col.Rise = 700.0f + i * 220.0f;
		Col.Velocity = FVector(0.0f, 0.0f, 1500.0f);
		ASIPuffActor::Spawn(World, Col, Location + FVector(0.0f, 0.0f, 1500.0f + i * 1800.0f), Autopilot->Index * 31 + i);
	}

	if (USIAirspaceSubsystem* Air = World->GetSubsystem<USIAirspaceSubsystem>())
	{
		Air->AddTurbulence(Location, TurbulenceRadius, TurbulenceHeight, TurbulenceLife, 1.0f);
		Air->UnregisterBomb(this);
	}

	// The boom reaches the listener at the speed of sound.
	const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const float Dist = Cam ? FVector::Dist(Cam->GetCameraLocation(), Location) / SI_CM : 0.0f;
	const float Delay = FMath::Clamp(Dist / FMath::Max(SoundSpeed, 1.0f), 0.0f, 6.0f);
	const FVector At = Location;
	FTimerHandle Handle;
	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, At]()
	{
		if (WeakWorld.IsValid())
		{
			USIAudioLibrary::PlaySynthAtLocation(WeakWorld.Get(), USIAudioLibrary::Explosion(), At, 1.0f, 1.0f, 9000.0f);
		}
	}), FMath::Max(Delay, 0.01f), false);
	UGameplayStatics::PlayWorldCameraShake(this, USIShakeExplosion::StaticClass(), Location, ShakeInnerRadius, ShakeOuterRadius, 1.0f);

	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Bomb %d impact at %s (tipped %d)"), Autopilot->Index, *Location.ToString(), bTipped ? 1 : 0);
	SetActorHiddenInGame(true);
	if (JetAudio) { JetAudio->Stop(); }
	SetLifeSpan(0.5f);
}
