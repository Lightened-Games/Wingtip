// Six Inches - module header.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSixInches, Log, All);

/** Unreal units per metre: gameplay tuning is authored in metres, the world runs in centimetres. */
constexpr float SI_CM = 100.0f;
