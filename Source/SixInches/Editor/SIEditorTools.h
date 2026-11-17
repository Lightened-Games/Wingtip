// Six Inches - editor-only helpers for the content scripts (Python can't build brushes or font faces from TTFs).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SIEditorTools.generated.h"

class UFont;
class UFontFace;

UCLASS()
class SIXINCHES_API USIEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Fill a runtime-cached composite font with named typefaces. */
	UFUNCTION(BlueprintCallable, Category = "SixInches|Editor")
	static bool SetupCompositeFont(UFont* Font, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces);

	/** Font faces straight from .ttf files plus a composite font at FontAssetPath (no Slate needed). Python saves. */
	UFUNCTION(BlueprintCallable, Category = "SixInches|Editor")
	static UFont* CreateFontFromFiles(const FString& FontAssetPath, const TArray<FName>& TypefaceNames, const TArray<FString>& TtfFiles);

	/** A BlockingVolume with a real cube brush, tagged SI.Bounds. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "SixInches|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label);
};
