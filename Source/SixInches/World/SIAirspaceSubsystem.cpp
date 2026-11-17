// Six Inches - airspace subsystem.

#include "World/SIAirspaceSubsystem.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace
{
	int32 ZonePriority(ESIZoneKind Kind)
	{
		switch (Kind)
		{
		case ESIZoneKind::Church:  return 8;
		case ESIZoneKind::Village: return 7;
		case ESIZoneKind::Town:    return 6;
		case ESIZoneKind::Farm:    return 5;
		case ESIZoneKind::River:   return 4;
		case ESIZoneKind::Woods:   return 3;
		case ESIZoneKind::Sea:     return 2;
		default:                   return 1;
		}
	}

	bool InsideZone(const FSIZoneRow& Z, const FVector& P)
	{
		const float Dx = P.X - Z.X;
		const float Dy = P.Y - Z.Y;
		if (Z.Radius > 0.0f)
		{
			return Dx * Dx + Dy * Dy <= Z.Radius * Z.Radius;
		}
		const float Rad = FMath::DegreesToRadians(Z.Yaw);
		const float Lx = Dx * FMath::Cos(Rad) + Dy * FMath::Sin(Rad);
		const float Ly = -Dx * FMath::Sin(Rad) + Dy * FMath::Cos(Rad);
		return FMath::Abs(Lx) <= Z.HalfX && FMath::Abs(Ly) <= Z.HalfY;
	}
}

FSIGroundClass USIAirspaceSubsystem::Classify(const FVector& Location) const
{
	FSIGroundClass Out;
	int32 Best = 0;
	for (const FSIZoneRow& Z : Zones)
	{
		const int32 Priority = ZonePriority(Z.Kind);
		if (Priority > Best && InsideZone(Z, Location))
		{
			Best = Priority;
			Out.Kind = Z.Kind;
		}
	}
	Out.bHit = SIZones::CountsAsHit(Out.Kind);
	return Out;
}

float USIAirspaceSubsystem::GetGroundHeight(const FVector& Location, bool bTerrainOnly) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SIGround), true);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const FVector Start(Location.X, Location.Y, 400000.0f);
	const FVector End(Location.X, Location.Y, -50000.0f);
	// Multi-trace so the ceiling BlockingVolume and FX (tagged) are skipped.
	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, Start, End, Objects, Params);
	for (const FHitResult& H : Hits)
	{
		const AActor* A = H.GetActor();
		if (A && (A->ActorHasTag(TEXT("SI.Bounds")) || A->ActorHasTag(TEXT("NoCollide"))))
		{
			continue;
		}
		if (bTerrainOnly && !(A && A->ActorHasTag(TEXT("SI.Terrain"))))
		{
			continue;
		}
		return H.ImpactPoint.Z;
	}
	return 0.0f;
}

void USIAirspaceSubsystem::RegisterBomb(AActor* Bomb)
{
	Bombs.AddUnique(Bomb);
}

void USIAirspaceSubsystem::UnregisterBomb(AActor* Bomb)
{
	Bombs.RemoveAll([Bomb](const TWeakObjectPtr<AActor>& A) { return !A.IsValid() || A.Get() == Bomb; });
	Claims.Remove(Bomb);
}

void USIAirspaceSubsystem::ClaimBomb(AActor* Bomb, AActor* By)
{
	if (Bomb && By)
	{
		Claims.Add(Bomb, By);
	}
}

void USIAirspaceSubsystem::ReleaseBomb(AActor* Bomb, AActor* By)
{
	if (const TWeakObjectPtr<AActor>* C = Claims.Find(Bomb))
	{
		if (!C->IsValid() || C->Get() == By)
		{
			Claims.Remove(Bomb);
		}
	}
}

void USIAirspaceSubsystem::ReleaseAllClaims(AActor* By)
{
	for (auto It = Claims.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid() || It.Value().Get() == By)
		{
			It.RemoveCurrent();
		}
	}
}

AActor* USIAirspaceSubsystem::GetClaimer(const AActor* Bomb) const
{
	const TWeakObjectPtr<AActor>* C = Claims.Find(Bomb);
	return C ? C->Get() : nullptr;
}

TArray<AActor*> USIAirspaceSubsystem::GetBombs() const
{
	TArray<AActor*> Out;
	for (const TWeakObjectPtr<AActor>& A : Bombs)
	{
		if (A.IsValid())
		{
			Out.Add(A.Get());
		}
	}
	return Out;
}

void USIAirspaceSubsystem::AddTurbulence(const FVector& Center, float Radius, float Height, float Life, float Strength)
{
	FSITurbulence T;
	T.Center = Center;
	T.Radius = Radius;
	T.Height = Height;
	T.Life = Life;
	T.Strength = Strength;
	T.BornTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Turbulence.Add(T);
}

float USIAirspaceSubsystem::SampleTurbulence(const FVector& Location) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	float Total = 0.0f;
	for (const FSITurbulence& T : Turbulence)
	{
		const float Age = Now - T.BornTime;
		if (Age > T.Life || Age < 0.0f)
		{
			continue;
		}
		const float Dz = Location.Z - T.Center.Z;
		if (Dz < -2000.0f || Dz > T.Height)
		{
			continue;
		}
		const float D = FVector2D::Distance(FVector2D(Location), FVector2D(T.Center)) / FMath::Max(T.Radius, 1.0f);
		if (D >= 1.0f)
		{
			continue;
		}
		const float Fade = FMath::Clamp(Age / 1.5f, 0.0f, 1.0f) * (1.0f - FMath::Square(Age / T.Life));
		// Hot air rises: strongest low in the column, spreading as it climbs.
		const float HeightFade = 1.0f - 0.5f * FMath::Clamp(Dz / T.Height, 0.0f, 1.0f);
		Total += T.Strength * Fade * HeightFade * (1.0f - D * D);
	}
	return Total;
}

void USIAirspaceSubsystem::RegisterCable(const FVector& Top, const FVector& Bottom)
{
	FSICable C;
	C.Top = Top;
	C.Bottom = Bottom;
	Cables.Add(C);
}

float USIAirspaceSubsystem::DistanceToNearestCable(const FVector& Location) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FSICable& C : Cables)
	{
		const FVector P = FMath::ClosestPointOnSegment(Location, C.Bottom, C.Top);
		Best = FMath::Min(Best, FVector::Dist(P, Location));
	}
	return Best;
}
