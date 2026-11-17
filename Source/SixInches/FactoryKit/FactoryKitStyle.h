// FactoryKit shared text styling: the theme font, soft shadow, no boxes.

#pragma once

#include "CoreMinimal.h"
#include "FactoryKitSettings.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/CompositeFont.h"
#include "Engine/FontFace.h"
#include "Styling/SlateBrush.h"

namespace FactoryKitStyle
{
	/** A single UFontFace wrapped in a composite font (cached; the face is rooted so the composite stays valid). */
	inline TSharedPtr<const FCompositeFont> FaceComposite(const FSoftObjectPath& Path)
	{
		static TMap<FSoftObjectPath, TSharedPtr<const FCompositeFont>> Cache;
		if (const TSharedPtr<const FCompositeFont>* Found = Cache.Find(Path))
		{
			return *Found;
		}
		UFontFace* Face = Cast<UFontFace>(Path.TryLoad());
		if (!Face)
		{
			return nullptr;
		}
		Face->AddToRoot();
		TSharedPtr<FCompositeFont> C = MakeShared<FCompositeFont>();
		C->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular"))).Font = FFontData(Face);
		Cache.Add(Path, C);
		return C;
	}

	inline FSlateFontInfo Font(FName Typeface, int32 Size, int32 LetterSpacing = 0)
	{
		const UFactoryKitSettings* S = UFactoryKitSettings::Get();
		FSlateFontInfo Info;
		// FontFace mode: FontObject is the title face and BodyFontObject the body face (both UFontFace assets).
		const FSoftObjectPath& FacePath = (Typeface != S->TitleTypeface && S->BodyFontObject.IsValid()) ? S->BodyFontObject : S->FontObject;
		const UObject* FontObj = FacePath.IsValid() ? FacePath.TryLoad() : nullptr;
		if (FontObj && FontObj->IsA<UFontFace>())
		{
			Info = FSlateFontInfo(FaceComposite(FacePath), Size, TEXT("Regular"));
		}
		else if (FontObj)
		{
			Info = FSlateFontInfo(FontObj, Size, Typeface);
		}
		else
		{
			Info = FCoreStyle::GetDefaultFontStyle(Typeface, Size);
		}
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}

	inline void Apply(UTextBlock* T, FName Typeface, int32 Size, const FLinearColor& Color, int32 LetterSpacing = 0)
	{
		const UFactoryKitSettings* S = UFactoryKitSettings::Get();
		T->SetFont(Font(Typeface, Size, LetterSpacing));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(S->ShadowOffset);
		T->SetShadowColorAndOpacity(S->ShadowColor);
	}

	/** An image brush of an exact size: a texture, or a flat colour when Texture is null. */
	inline FSlateBrush SizedBrush(const FVector2D& Size, const FLinearColor& Tint, UObject* Texture = nullptr)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::Image;
		B.ImageSize = Size;
		B.TintColor = FSlateColor(Tint);
		if (Texture) { B.SetResourceObject(Texture); }
		return B;
	}

	/** Ease-out step toward Target over about Seconds. */
	inline float EaseTo(float Current, float Target, float DeltaTime, float Seconds)
	{
		const float Rate = 4.6f / FMath::Max(Seconds, 0.01f); // 99 % of the way in Seconds
		return FMath::Lerp(Current, Target, 1.0f - FMath::Exp(-Rate * DeltaTime));
	}
}
