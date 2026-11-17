// Six Inches - data-driven world builder.

#include "World/SIWorldBuilder.h"
#include "World/SIAirspaceSubsystem.h"
#include "World/SIFlakBattery.h"
#include "World/SIBalloon.h"
#include "Data/SITypes.h"
#include "SixInches.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

ASIWorldBuilder::ASIWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);
	PropTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/SixInches/Data/DT_SIProps.DT_SIProps")));
	ZoneTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/SixInches/Data/DT_SIZones.DT_SIZones")));
}

void ASIWorldBuilder::BeginPlay()
{
	Super::BeginPlay();
	UWorld* World = GetWorld();
	USIAirspaceSubsystem* Air = World ? World->GetSubsystem<USIAirspaceSubsystem>() : nullptr;

	if (UDataTable* Zones = ZoneTable.LoadSynchronous())
	{
		TArray<FSIZoneRow*> Rows;
		Zones->GetAllRows<FSIZoneRow>(TEXT("SIZones"), Rows);
		TArray<FSIZoneRow> Copy;
		for (const FSIZoneRow* R : Rows) { Copy.Add(*R); }
		if (Air) { Air->SetZones(Copy); }
		UE_LOG(LogSixInches, Display, TEXT("[SixInches] Zones: %d"), Copy.Num());
	}
	else
	{
		UE_LOG(LogSixInches, Error, TEXT("[SixInches] DT_SIZones missing: every impact will read as FIELD."));
	}

	UDataTable* Props = PropTable.LoadSynchronous();
	if (!Props)
	{
		UE_LOG(LogSixInches, Error, TEXT("[SixInches] DT_SIProps missing: the countryside is empty."));
		return;
	}
	TArray<FSIPropRow*> Rows;
	Props->GetAllRows<FSIPropRow>(TEXT("SIProps"), Rows);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	int32 Missing = 0;
	for (const FSIPropRow* R : Rows)
	{
		const FVector Loc(R->X, R->Y, R->Z);
		const FRotator Rot(0.0f, R->Yaw, 0.0f);
		if (R->Mesh == TEXT("Flak"))
		{
			World->SpawnActor<ASIFlakBattery>(ASIFlakBattery::StaticClass(), Loc, Rot, Params);
			continue;
		}
		if (R->Mesh == TEXT("Balloon"))
		{
			World->SpawnActor<ASIBalloon>(ASIBalloon::StaticClass(), Loc, Rot, Params);
			continue;
		}
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* Found = Instancers.Find(R->Mesh);
		UHierarchicalInstancedStaticMeshComponent* H = Found ? Found->Get() : nullptr;
		if (!H)
		{
			const FString Path = FString::Printf(TEXT("%s/SM_%s.SM_%s"), *MeshFolder, *R->Mesh.ToString(), *R->Mesh.ToString());
			UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Path));
			if (!Mesh)
			{
				++Missing;
				continue;
			}
			H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, *FString::Printf(TEXT("HISM_%s"), *R->Mesh.ToString()));
			H->SetStaticMesh(Mesh);
			H->SetMobility(EComponentMobility::Static);
			// C2: everything the player could touch is solid and steers navigation.
			H->SetCollisionProfileName(TEXT("BlockAll"));
			H->SetCanEverAffectNavigation(true);
			// Small dressing (trees, hedges) is sub-pixel long before the 9 km limit but still costs render time near the town.
			const bool bSmall = Mesh->GetBounds().SphereRadius < SmallPropRadius * SI_CM;
			H->SetCullDistances(0, static_cast<int32>((bSmall ? SmallPropCullDistance : CullDistance) * SI_CM));
			if (bSmall)
			{
				// ~40k woodland instances: keep them out of the Lumen scene and distance-field lighting (they add
				// next to nothing to bounce light, and tracking them doubled the render thread late in the round).
				H->bAffectDynamicIndirectLighting = false;
				H->bAffectDistanceFieldLighting = false;
			}
			H->bCastDynamicShadow = true;
			H->SetupAttachment(RootComponent);
			H->RegisterComponent();
			Instancers.Add(R->Mesh, H);
		}
		H->AddInstance(FTransform(Rot, Loc, FVector(R->ScaleX, R->ScaleY, R->ScaleZ)), true);
		++InstanceCount;
	}
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] World built: %d instances over %d meshes (%d rows had no mesh)."), InstanceCount, Instancers.Num(), Missing);
}
