// Six Inches - barrage balloon.

#include "World/SIBalloon.h"
#include "World/SIAirspaceSubsystem.h"
#include "SixInches.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ASIBalloon::ASIBalloon()
{
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add(TEXT("SI.Balloon"));
	Winch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Winch"));
	RootComponent = Winch;
	Winch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Winch->SetVisibility(false);

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetupAttachment(Winch);
	Hull->SetCollisionProfileName(TEXT("BlockAll"));
	Hull->SetMobility(EComponentMobility::Movable);

	Cable = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cable"));
	Cable->SetupAttachment(Winch);
	Cable->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cable->CastShadow = false;

	HullMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_Balloon.SM_Balloon")));
	CableMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/SixInches/Meshes/SM_Cable.SM_Cable")));
}

void ASIBalloon::BeginPlay()
{
	if (UStaticMesh* M = HullMesh.LoadSynchronous()) { Hull->SetStaticMesh(M); }
	if (UStaticMesh* M = CableMesh.LoadSynchronous()) { Cable->SetStaticMesh(M); }
	Super::BeginPlay();
	Phase = FMath::FRandRange(0.0f, 6.28f);
	const float H = Altitude * SI_CM;
	Hull->SetRelativeLocation(FVector(0.0f, 0.0f, H + 395.0f));
	// The cable mesh is a 1 m tall cylinder: stretch it from the winch to the balloon's belly.
	Cable->SetRelativeLocation(FVector::ZeroVector);
	Cable->SetRelativeScale3D(FVector(1.0f, 1.0f, H / 100.0f));
	if (USIAirspaceSubsystem* Air = GetWorld()->GetSubsystem<USIAirspaceSubsystem>())
	{
		Air->RegisterCable(GetActorLocation() + FVector(0.0f, 0.0f, H), GetActorLocation());
	}
}

void ASIBalloon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float H = Altitude * SI_CM + BobAmplitude * SI_CM * FMath::Sin(Age * 0.4f + Phase);
	Hull->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, H + 395.0f),
		FRotator(SwayDegrees * 0.4f * FMath::Sin(Age * 0.3f + Phase), SwayDegrees * FMath::Sin(Age * 0.17f + Phase), 0.0f));
}
