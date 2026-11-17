// Six Inches - echoes "a key fired" events (human or bot) so the HUD legend can light the matching row.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SIInputEchoComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSIOnInputVisualized, FName, KeyId, bool, bSimulated);

UCLASS(ClassGroup = (SixInches), meta = (BlueprintSpawnableComponent))
class SIXINCHES_API USIInputEchoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USIInputEchoComponent() { PrimaryComponentTick.bCanEverTick = false; }

	UPROPERTY(BlueprintAssignable, Category = "Input")
	FSIOnInputVisualized OnInputVisualized;

	/** True while the latest flight input came from slot-mode nudges (the legend relabels its rows). */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	bool bSlotMode = false;

	void Echo(FName KeyId, bool bSimulated) { OnInputVisualized.Broadcast(KeyId, bSimulated); }
};
