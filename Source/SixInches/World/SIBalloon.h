// Six Inches - a barrage balloon on a steel cable. The cable is a hazard: touching it loses the plane.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SIBalloon.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

UCLASS()
class SIXINCHES_API ASIBalloon : public AActor
{
	GENERATED_BODY()

public:
	ASIBalloon();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Balloon")
	TObjectPtr<UStaticMeshComponent> Winch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Balloon")
	TObjectPtr<UStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Balloon")
	TObjectPtr<UStaticMeshComponent> Cable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> HullMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Art")
	TSoftObjectPtr<UStaticMesh> CableMesh;

	/** Height of the balloon above its winch (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float Altitude = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float BobAmplitude = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SwayDegrees = 4.0f;

protected:
	virtual void BeginPlay() override;

private:
	float Age = 0.0f;
	float Phase = 0.0f;
};
