// Six Inches - builds the countryside from data: DT_SIProps rows become instanced meshes (collision on, affecting
// navigation), flak batteries and balloons; DT_SIZones rows feed the ground classifier. Re-runnable; no code
// changes needed to move a farm or add a wood (edit Tools/generate_layout.py, regenerate the CSVs).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SIWorldBuilder.generated.h"

class UDataTable;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class SIXINCHES_API ASIWorldBuilder : public AActor
{
	GENERATED_BODY()

public:
	ASIWorldBuilder();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TSoftObjectPtr<UDataTable> PropTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TSoftObjectPtr<UDataTable> ZoneTable;

	/** Mesh asset folder: key "Tree" resolves to <MeshFolder>/SM_Tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FString MeshFolder = TEXT("/Game/SixInches/Meshes");

	/** Instances further than this (m) are culled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float CullDistance = 9000.0f;

	/** Meshes smaller than this bounding radius (m) count as small dressing... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SmallPropRadius = 9.0f;

	/** ...and are culled beyond this (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
	float SmallPropCullDistance = 3800.0f;

	int32 GetInstanceCount() const { return InstanceCount; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TMap<FName, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Instancers;

	int32 InstanceCount = 0;
};
