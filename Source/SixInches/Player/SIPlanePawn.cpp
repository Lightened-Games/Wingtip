// Six Inches - the player's fighter.

#include "Player/SIPlanePawn.h"
#include "Components/SIFlightComponent.h"
#include "Components/SIWingtipComponent.h"
#include "Components/SIHealthComponent.h"
#include "Components/SIGameFeelComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/SIAirspaceSubsystem.h"
#include "Interfaces/SIInterfaces.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "SixInches.h"

ASIPlanePawn::ASIPlanePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->ComponentTags.Add(TEXT("SI.Body"));
	Body->SetCanEverAffectNavigation(false);
	RootComponent = Body;

	PropDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropDisc"));
	PropDisc->SetupAttachment(Body);
	PropDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PropDisc->SetRelativeLocation(FVector(512.0f, 0.0f, 6.0f));
	PropDisc->CastShadow = false;
	PropDisc->ComponentTags.Add(TEXT("NoCollide"));

	PreviewRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewRing"));
	PreviewRing->SetupAttachment(Body);
	PreviewRing->SetUsingAbsoluteLocation(true);
	PreviewRing->SetUsingAbsoluteRotation(true);
	PreviewRing->SetUsingAbsoluteScale(true);
	PreviewRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewRing->CastShadow = false;
	PreviewRing->SetVisibility(false);
	PreviewRing->ComponentTags.Add(TEXT("NoCollide"));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Body);
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->SetFieldOfView(ChaseFOV);
	Camera->bConstrainAspectRatio = false;

	Flight = CreateDefaultSubobject<USIFlightComponent>(TEXT("Flight"));
	Wingtip = CreateDefaultSubobject<USIWingtipComponent>(TEXT("Wingtip"));
	Health = CreateDefaultSubobject<USIHealthComponent>(TEXT("Health"));
	GameFeel = CreateDefaultSubobject<USIGameFeelComponent>(TEXT("GameFeel"));

	BodyMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_Fighter.SM_Fighter")));
	PropMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_PropDisc.SM_PropDisc")));
	RingMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_Ring.SM_Ring")));
	RingMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/SixInches/Materials/MI_SI_MarkerAccent.MI_SI_MarkerAccent")));
}

void ASIPlanePawn::BeginPlay()
{
	if (UStaticMesh* M = BodyMesh.LoadSynchronous())
	{
		Body->SetStaticMesh(M);
		if (M->FindSocket(TEXT("Prop")))
		{
			PropDisc->SetRelativeLocation(M->FindSocket(TEXT("Prop"))->RelativeLocation);
		}
	}
	if (UStaticMesh* M = PropMesh.LoadSynchronous())
	{
		PropDisc->SetStaticMesh(M);
	}
	if (UStaticMesh* M = RingMesh.LoadSynchronous())
	{
		PreviewRing->SetStaticMesh(M);
		if (UMaterialInterface* Mat = RingMaterial.LoadSynchronous())
		{
			RingMID = UMaterialInstanceDynamic::Create(Mat, this);
			PreviewRing->SetMaterial(0, RingMID);
		}
	}
	// Components read the body sockets in their BeginPlay, so the mesh is set first.
	Super::BeginPlay();
	// The wingtip sets the slot position, the flight applies it: same frame, always in that order (no shake).
	if (Flight && Wingtip && !SITick::Disabled())
	{
		Flight->PrimaryComponentTick.AddPrerequisite(Wingtip, Wingtip->PrimaryComponentTick);
	}
}

FVector ASIPlanePawn::GetVelocity() const
{
	return Flight ? Flight->GetVelocity() : FVector::ZeroVector;
}

void ASIPlanePawn::SetFlightInput(const FSIFlightInput& Input)
{
	CurrentInput = Input;
	if (Flight) { Flight->SetInput(Input); }
	if (Wingtip) { Wingtip->SetInput(Input); }
}

void ASIPlanePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (PropDisc && Flight && !Flight->IsCrashed())
	{
		PropDisc->AddLocalRotation(FRotator(0.0f, 0.0f, PropSpinDegPerSec * DeltaSeconds));
	}
	UpdateCamera(DeltaSeconds);
	UpdatePreviewRing(DeltaSeconds);
	UpdatePaceAssist();

	// QA (-SIJitterLog): stutter of our position relative to the bomb we're beside (change of the per-frame step,
	// normalised to 60 fps; smooth sliding scores ~0), worst per second.
	static const bool bJitterLog = FParse::Param(FCommandLine::Get(), TEXT("SIJitterLog"));
	const AActor* JBomb = bJitterLog && Wingtip && Wingtip->IsInSlot() ? Wingtip->GetTarget() : nullptr;
	if (JBomb)
	{
		const FVector Rel = GetActorLocation() - JBomb->GetActorLocation();
		const FVector Step = (Rel - JitterLastRel) / FMath::Max(DeltaSeconds * 60.0f, 0.01f);
		if (JitterFrames >= 2)
		{
			JitterMax = FMath::Max(JitterMax, FVector::Dist(Step, JitterLastStep));
		}
		JitterLastStep = Step;
		JitterLastRel = Rel;
		++JitterFrames;
		JitterTimer += DeltaSeconds;
		if (JitterTimer >= 1.0f)
		{
			UE_LOG(LogSixInches, Display, TEXT("[SixInches][Jitter] stutter %.1f cm"), JitterMax);
			JitterTimer = 0.0f;
			JitterMax = 0.0f;
		}
	}
	else
	{
		JitterFrames = 0;
	}
}

void ASIPlanePawn::UpdatePaceAssist()
{
	// Human pilots only (the autoplay flies its own throttle).
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	if (!bPaceAssist || !Flight || !Wingtip || Wingtip->IsInSlot() || (Flow && Flow->IsBotDriving()))
	{
		return;
	}
	const USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>();
	if (!Air)
	{
		return;
	}
	const FVector P = GetActorLocation();
	const FVector Fwd = FRotator(0.0f, Flight->GetFlightRotation().Yaw, 0.0f).Vector();
	float Best = TNumericLimits<float>::Max();
	float Want = 0.0f;
	for (AActor* B : Air->GetBombs())
	{
		const ISITippable* T = Cast<ISITippable>(B);
		const AActor* Claimer = B ? Air->GetClaimer(B) : nullptr;
		if (!T || !T->IsTippable() || (Claimer && Claimer != this))
		{
			continue;
		}
		const FSIWingFrame W = T->GetWingFrame();
		const FVector BF = FRotator(0.0f, W.Forward.Rotation().Yaw, 0.0f).Vector();
		if (FVector::DotProduct(BF, Fwd) < FMath::Cos(FMath::DegreesToRadians(PaceAssistMaxAngle)))
		{
			continue;  // only when flying the same way as the bomb
		}
		const FVector D = W.Location - P;
		const float Along = FVector::DotProduct(D, BF) / SI_CM;  // + = bomb ahead of us
		const float Lateral = FVector::DotProduct(D, FVector::CrossProduct(FVector::UpVector, BF).GetSafeNormal()) / SI_CM;
		if (FMath::Abs(Along) > PaceAssistRange || FMath::Abs(Lateral) > PaceAssistRange * 0.4f)
		{
			continue;
		}
		const float Dist = FMath::Abs(Along) + FMath::Abs(Lateral);
		if (Dist < Best)
		{
			Best = Dist;
			Want = W.Speed + FMath::Clamp(Along * PaceAssistClosing, -PaceAssistMaxClosing, PaceAssistMaxClosing);
		}
	}
	if (Best < TNumericLimits<float>::Max())
	{
		Flight->SetPaceAssist(Want);
	}
}

void ASIPlanePawn::UpdatePreviewRing(float Dt)
{
	const bool bShow = Wingtip && Wingtip->IsInSlot() && Wingtip->HasPreview() && !Flight->IsCrashed();
	RingAlpha = FMath::FInterpConstantTo(RingAlpha, bShow ? 1.0f : 0.0f, Dt, 4.0f);
	PreviewRing->SetVisibility(RingAlpha > 0.01f);
	if (RingAlpha <= 0.01f || !Wingtip)
	{
		return;
	}
	const FVector At = Wingtip->GetPreviewLocation() + FVector(0.0f, 0.0f, 150.0f);
	const FVector Cur = PreviewRing->GetComponentLocation();
	const FVector Smoothed = Cur.IsNearlyZero() || FVector::Dist(Cur, At) > 50000.0f ? At : FMath::VInterpTo(Cur, At, Dt, 8.0f);
	const float Pulse = 1.0f + 0.04f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.0f);
	PreviewRing->SetWorldLocationAndRotation(Smoothed, FRotator::ZeroRotator);
	PreviewRing->SetWorldScale3D(FVector(PreviewRingRadius * Pulse, PreviewRingRadius * Pulse, 1.0f));
	if (RingMID)
	{
		RingMID->SetVectorParameterValue(TEXT("Tint"), Wingtip->GetPreviewClass().bHit ? RingDangerColor : RingSafeColor);
		RingMID->SetScalarParameterValue(TEXT("Opacity"), RingAlpha);
	}
}

void ASIPlanePawn::UpdateCamera(float Dt)
{
	if (!Camera || !Flight)
	{
		return;
	}
	if (Flight->IsCrashed())
	{
		// Hold the last view, turn to watch the wreck.
		const FRotator Look = (GetActorLocation() - CamPos).Rotation();
		CamRot = FMath::RInterpTo(CamRot, Look, Dt, 3.0f);
		Camera->SetWorldLocationAndRotation(CamPos, CamRot);
		return;
	}
	const FVector P = GetActorLocation();
	const FRotator R = Flight->GetFlightRotation();
	const FRotator Heading(R.Pitch * 0.6f, R.Yaw, 0.0f);
	const FVector Fwd = Heading.Vector();
	const FVector ChasePos = P - Fwd * ChaseDistance + FVector::UpVector * ChaseHeight;
	const FVector ChaseLook = P + Fwd * ChaseLookAhead;

	FVector Pos = ChasePos;
	FVector Look = ChaseLook;
	float Fov = ChaseFOV + SpeedFOVKick * Flight->GetSpeed01();
	float Roll = R.Roll * CameraRollFollow;

	const float Blend = Wingtip ? FMath::SmoothStep(0.0f, 1.0f, Wingtip->GetSlotBlend()) : 0.0f;
	if (Blend > 0.0f && Wingtip)
	{
		// Anchor on the bomb's smooth flight path plus an eased offset to the wingtip midpoint. Following the raw
		// midpoint (turbulence wobble, the bomb's lift roll, stick corrections) made the whole frame vibrate
		// (user, 2026-11-13: "both the planes feel laggy and shaky as if vibrating").
		FVector A = P;
		const AActor* Bomb = Wingtip->IsInSlot() ? Wingtip->GetTarget() : nullptr;
		if (Bomb)
		{
			const FVector Local = (Wingtip->GetPlayerTip() + Wingtip->GetTargetTip()) * 0.5f - Bomb->GetActorLocation();
			SlotAnchorLocal = bSlotAnchorInit ? FMath::VInterpTo(SlotAnchorLocal, Local, Dt, SlotAnchorSmoothing) : Local;
			bSlotAnchorInit = true;
			A = Bomb->GetActorLocation() + SlotAnchorLocal;
			SlotYaw = FMath::FInterpTo(SlotYaw, SlotYaw + FMath::FindDeltaAngleDegrees(SlotYaw, Bomb->GetActorRotation().Yaw), Dt, SlotAnchorSmoothing);
		}
		else
		{
			bSlotAnchorInit = false;
			SlotYaw = R.Yaw;
		}
		const FVector F = FRotator(0.0f, Bomb ? SlotYaw : R.Yaw, 0.0f).Vector();
		const FVector Rt = FVector::CrossProduct(FVector::UpVector, F).GetSafeNormal();
		const float Side = Wingtip->GetSide();
		const FVector SlotPos = A - F * SlotBack + Rt * Side * SlotOut + FVector::UpVector * SlotUp;
		// Look ahead and a little down, toward where the bomb would come down (the preview ring).
		const FVector SlotLook = A + F * SlotLookAhead - FVector::UpVector * 120.0f;
		Pos = FMath::Lerp(ChasePos, SlotPos, Blend);
		Look = FMath::Lerp(ChaseLook, SlotLook, Blend);
		Fov = FMath::Lerp(Fov, SlotFOV, Blend);
		Roll = FMath::Lerp(Roll, 0.0f, Blend);  // a level horizon in the slot (the plane's own roll stays visible)
	}

	if (!bCamInit)
	{
		CamPos = Pos;
		bCamInit = true;
	}
	// Lag in the plane's frame: the camera trails a little in turns but never loses the plane.
	const float Lag = FMath::Lerp(ChaseLag, 14.0f, Blend);
	CamPos = FMath::VInterpTo(CamPos, Pos, Dt, Lag);
	const FVector Off = CamPos - Pos;
	const float MaxOff = FMath::Lerp(600.0f, 60.0f, Blend);
	if (Off.Size() > MaxOff)
	{
		CamPos = Pos + Off.GetSafeNormal() * MaxOff;
	}
	FRotator Want = (Look - CamPos).Rotation();
	Want.Pitch -= SlotPitchDown * Blend;  // slot cam tips down a little so the ground ahead (the preview ring) shows
	Want.Roll = Roll;
	CamRot = FMath::RInterpTo(CamRot, Want, Dt, FMath::Lerp(10.0f, SlotRotationRate, Blend));
	Camera->SetWorldLocationAndRotation(CamPos, CamRot);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, Fov, Dt, 4.0f));
}
