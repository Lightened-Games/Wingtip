// Six Inches - editor helpers (adapted from Echo Lane's EchoEditorTools).

#include "Editor/SIEditorTools.h"
#include "Engine/BlockingVolume.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#endif

bool USIEditorTools::SetupCompositeFont(UFont* Font, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces)
{
	if (!Font || TypefaceNames.Num() != Faces.Num() || Faces.Num() == 0)
	{
		return false;
	}
	Font->FontCacheType = EFontCacheType::Runtime;
	FTypeface& Typeface = Font->GetMutableInternalCompositeFont().DefaultTypeface;
	Typeface.Fonts.Reset();
	for (int32 i = 0; i < Faces.Num(); ++i)
	{
		if (Faces[i])
		{
			FTypefaceEntry& Entry = Typeface.Fonts.Add_GetRef(FTypefaceEntry(TypefaceNames[i]));
			Entry.Font = FFontData(Faces[i]);
		}
	}
	// No PostEditChange: it flushes the Slate font cache, which asserts under -run=pythonscript.
	Font->MarkPackageDirty();
	return Typeface.Fonts.Num() > 0;
}

UFont* USIEditorTools::CreateFontFromFiles(const FString& FontAssetPath, const TArray<FName>& TypefaceNames, const TArray<FString>& TtfFiles)
{
#if WITH_EDITOR
	if (TypefaceNames.Num() != TtfFiles.Num() || TtfFiles.Num() == 0)
	{
		return nullptr;
	}
	const FString Folder = FPackageName::GetLongPackagePath(FontAssetPath);
	TArray<UFontFace*> Faces;
	for (const FString& File : TtfFiles)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *File))
		{
			Faces.Add(nullptr);
			continue;
		}
		const FString Name = TEXT("FF_") + FPaths::GetBaseFilename(File).Replace(TEXT("-"), TEXT("_"));
		UPackage* FacePackage = CreatePackage(*(Folder / Name));
		UFontFace* Face = NewObject<UFontFace>(FacePackage, FName(*Name), RF_Public | RF_Standalone);
		Face->SourceFilename = File;
		Face->Hinting = EFontHinting::Default;
		Face->LoadingPolicy = EFontLoadingPolicy::Inline;
		Face->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(Bytes));
		FAssetRegistryModule::AssetCreated(Face);
		FacePackage->MarkPackageDirty();
		Faces.Add(Face);
	}
	UPackage* FontPackage = CreatePackage(*FontAssetPath);
	UFont* Font = NewObject<UFont>(FontPackage, FName(*FPackageName::GetShortName(FontAssetPath)), RF_Public | RF_Standalone);
	FAssetRegistryModule::AssetCreated(Font);
	return SetupCompositeFont(Font, TypefaceNames, Faces) ? Font : nullptr;
#else
	return nullptr;
#endif
}

AActor* USIEditorTools::SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABlockingVolume* Volume = World->SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(), FTransform(Location), Params);
	if (!Volume)
	{
		return nullptr;
	}
	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = HalfExtent.X * 2.0f;
	Builder->Y = HalfExtent.Y * 2.0f;
	Builder->Z = HalfExtent.Z * 2.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	Volume->SetActorLabel(Label);
	Volume->Tags.Add(TEXT("SI.Bounds"));
	return Volume;
#else
	return nullptr;
#endif
}
