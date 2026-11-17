// Six Inches - billboard flipbook FX.

#include "Feedback/SIPuffActor.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"

namespace
{
	UMaterialInterface* LoadPuffMaterial(ESIPuffMaterial Kind, bool bSoftDot)
	{
		static const TCHAR* Smoke = TEXT("/Game/SixInches/Materials/MI_SI_Smoke.MI_SI_Smoke");
		static const TCHAR* Flash = TEXT("/Game/SixInches/Materials/MI_SI_Flash.MI_SI_Flash");
		static const TCHAR* Dot = TEXT("/Game/SixInches/Materials/MI_SI_Dot.MI_SI_Dot");
		const TCHAR* Path = Kind == ESIPuffMaterial::Smoke ? Smoke : (bSoftDot ? Dot : Flash);
		return Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, Path));
	}
}

ASIPuffActor::ASIPuffActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Tags.Add(TEXT("NoCollide"));
	SetCanBeDamaged(false);
}

ASIPuffActor* ASIPuffActor::Spawn(UWorld* World, const FSIPuffSpec& Spec, const FVector& Location, int32 Seed)
{
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Live cap: hundreds of lingering flak and smoke puffs late in a round cost render-thread spikes. Over the cap the
	// oldest puff fades out quickly (no pop).
	static TArray<TWeakObjectPtr<ASIPuffActor>> LivePuffs;
	LivePuffs.RemoveAll([World](const TWeakObjectPtr<ASIPuffActor>& P) { return !P.IsValid() || P->GetWorld() != World || P->IsActorBeingDestroyed(); });
	const int32 Cap = GetDefault<ASIPuffActor>()->MaxLivePuffs;
	for (int32 i = 0; LivePuffs.Num() - i >= Cap && i < LivePuffs.Num(); ++i)
	{
		LivePuffs[i]->FadeOutSoon();
	}
	ASIPuffActor* Puff = World->SpawnActor<ASIPuffActor>(ASIPuffActor::StaticClass(), FTransform(Location), Params);
	if (Puff)
	{
		Puff->Init(Spec, Seed ? Seed : FMath::Rand());
		LivePuffs.Add(Puff);
	}
	return Puff;
}

void ASIPuffActor::Init(const FSIPuffSpec& InSpec, int32 Seed)
{
	Spec = InSpec;
	Vel = Spec.Velocity;
	FRandomStream R(Seed);
	PhaseOffset = R.FRand();
	UMaterialInterface* Base = LoadPuffMaterial(Spec.Material, Spec.bSoftDot);
	if (!Base)
	{
		SetLifeSpan(0.1f);
		return;
	}
	MID = UMaterialInstanceDynamic::Create(Base, this);
	MID->SetVectorParameterValue(TEXT("Tint"), Spec.Color);
	MID->SetScalarParameterValue(TEXT("Glow"), FMath::Min(Spec.Glow, 3.5f));
	MID->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
	MID->SetScalarParameterValue(TEXT("Phase"), 0.0f);
	const int32 N = FMath::Max(1, Spec.Count);
	for (int32 i = 0; i < N; ++i)
	{
		UMaterialBillboardComponent* B = NewObject<UMaterialBillboardComponent>(this);
		B->SetupAttachment(RootComponent);
		const FVector Off = Spec.Spread > 0.0f ? R.VRand() * R.FRandRange(0.2f, 1.0f) * Spec.Spread : FVector::ZeroVector;
		B->SetRelativeLocation(Off);
		B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		B->CastShadow = false;
		B->AddElement(MID, nullptr, false, Spec.SizeStart, Spec.SizeStart, nullptr);
		B->RegisterComponent();
		Sprites.Add(B);
		Offsets.Add(Off);
		SizeJitter.Add(R.FRandRange(0.8f, 1.2f));
	}
	SetLifeSpan(Spec.Life + Spec.Delay + 0.2f);
	SetActorHiddenInGame(Spec.Delay > 0.0f);
}

void ASIPuffActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Live = Age - Spec.Delay;
	if (Live < 0.0f || !MID)
	{
		return;
	}
	SetActorHiddenInGame(false);
	const float T = FMath::Clamp(Live / FMath::Max(Spec.Life, 0.01f), 0.0f, 1.0f);
	Vel *= FMath::Exp(-Spec.Drag * DeltaSeconds);
	AddActorWorldOffset((Vel + FVector(0.0f, 0.0f, Spec.Rise)) * DeltaSeconds);

	const float Ease = 1.0f - FMath::Square(1.0f - T);
	const float Size = FMath::Lerp(Spec.SizeStart, Spec.SizeEnd, Ease);
	const float FadeIn = FMath::Clamp(Live / 0.12f, 0.0f, 1.0f);
	float FadeOut = 1.0f;
	if (bFadingOut)
	{
		FadeAge += DeltaSeconds;
		FadeOut = 1.0f - FMath::Clamp(FadeAge / FMath::Max(QuickFadeSeconds, 0.05f), 0.0f, 1.0f);
		if (FadeOut <= 0.0f)
		{
			Destroy();
			return;
		}
	}
	MID->SetScalarParameterValue(TEXT("Opacity"), Spec.Opacity * FadeIn * FadeOut * (1.0f - FMath::Pow(T, 2.5f)));
	MID->SetScalarParameterValue(TEXT("Phase"), FMath::Min(T * 0.98f + 0.0f, 0.999f));
	// Resizing a billboard rebuilds its render proxy: only do it when the size has moved noticeably (smoke grows slowly).
	if (FMath::Abs(Size - LastSize) > FMath::Max(LastSize, 1.0f) * ResizeStep)
	{
		LastSize = Size;
		for (int32 i = 0; i < Sprites.Num(); ++i)
		{
			UMaterialBillboardComponent* B = Sprites[i];
			if (B && B->Elements.Num() > 0)
			{
				const float S = Size * SizeJitter[i];
				B->Elements[0].BaseSizeX = S;
				B->Elements[0].BaseSizeY = S;
				B->MarkRenderStateDirty();
			}
		}
	}
}

void ASIPuffActor::FadeOutSoon()
{
	if (!bFadingOut)
	{
		bFadingOut = true;
		FadeAge = 0.0f;
	}
}
