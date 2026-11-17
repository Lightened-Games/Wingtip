// Six Inches - decoupling interfaces (no casts to concrete game classes).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Data/SITypes.h"
#include "SIInterfaces.generated.h"

namespace SITick
{
	/** Run Tick after Other's pre-physics movers this frame (or stop). Anything placed relative to a bomb must read
	 *  where the bomb IS this frame: with no order the engine reshuffles same-group ticks as actors spawn, and the
	 *  follower alternated between 0 and 2 frames stale (metres of shake at 120 m/s). */
	/** QA comparison only (-SINoTickOrder): the old unordered ticks. */
	inline bool Disabled()
	{
		static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("SINoTickOrder"));
		return bOff;
	}

	inline void FollowMovers(FTickFunction& Tick, AActor* Other, bool bFollow)
	{
		if (!Other || Disabled())
		{
			return;
		}
		TInlineComponentArray<UActorComponent*> Comps(Other);
		for (UActorComponent* C : Comps)
		{
			if (C && C->PrimaryComponentTick.bCanEverTick && C->PrimaryComponentTick.TickGroup == TG_PrePhysics)
			{
				if (bFollow) { Tick.AddPrerequisite(C, C->PrimaryComponentTick); }
				else { Tick.RemovePrerequisite(C, C->PrimaryComponentTick); }
			}
		}
	}
}

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USITippable : public UInterface
{
	GENERATED_BODY()
};

/** Something that flies straight and level until a wingtip lifts its wing long enough (a flying bomb). */
class SIXINCHES_API ISITippable
{
	GENERATED_BODY()

public:
	virtual FSIWingFrame GetWingFrame() const = 0;
	virtual bool IsTippable() const = 0;
	/** Visual roll while a wing is being lifted. LiftedSide +1 = its right wing is being lifted. */
	virtual void SetLiftProgress(float Progress01, float LiftedSide) = 0;
	virtual void Tip(float LiftedSide) = 0;
	/** Where it would come down if tipped now (world, cm). */
	virtual FVector PredictTipImpact(float LiftedSide) const = 0;
	/** A small shove from a wing contact (cm/s, world). */
	virtual void Bump(const FVector& Velocity) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USIFlightControllable : public UInterface
{
	GENERATED_BODY()
};

/** A pawn that takes stick input. The player controller and the autoplay bot both drive it through this. */
class SIXINCHES_API ISIFlightControllable
{
	GENERATED_BODY()

public:
	virtual void SetFlightInput(const FSIFlightInput& Input) = 0;
	virtual FSIFlightInput GetFlightInput() const = 0;
};
