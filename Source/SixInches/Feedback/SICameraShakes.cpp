// Six Inches - camera shake presets.

#include "Feedback/SICameraShakes.h"
#include "Shakes/WaveOscillatorCameraShakePattern.h"

namespace
{
	UWaveOscillatorCameraShakePattern* Make(UCameraShakeBase* Owner, const TCHAR* Name, float Duration, float LocAmp, float RotAmp, float Freq)
	{
		UWaveOscillatorCameraShakePattern* Pattern = Owner->CreateDefaultSubobject<UWaveOscillatorCameraShakePattern>(Name);
		Pattern->Duration = Duration;
		Pattern->BlendInTime = 0.02f;
		Pattern->BlendOutTime = Duration * 0.6f;
		Pattern->X.Amplitude = LocAmp * 0.3f;
		Pattern->X.Frequency = Freq;
		Pattern->Y.Amplitude = LocAmp;
		Pattern->Y.Frequency = Freq * 1.1f;
		Pattern->Z.Amplitude = LocAmp;
		Pattern->Z.Frequency = Freq * 0.9f;
		Pattern->Pitch.Amplitude = RotAmp;
		Pattern->Pitch.Frequency = Freq;
		Pattern->Yaw.Amplitude = RotAmp * 0.5f;
		Pattern->Yaw.Frequency = Freq * 1.2f;
		Pattern->Roll.Amplitude = RotAmp * 0.8f;
		Pattern->Roll.Frequency = Freq * 0.8f;
		Pattern->FOV.Amplitude = 0.0f;
		return Pattern;
	}
}

USIShakeContact::USIShakeContact(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(Make(this, TEXT("Contact"), 0.25f, 4.0f, 0.7f, 26.0f));
}

USIShakeFlak::USIShakeFlak(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(Make(this, TEXT("Flak"), 0.45f, 9.0f, 1.4f, 17.0f));
}

USIShakeExplosion::USIShakeExplosion(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(Make(this, TEXT("Explosion"), 0.8f, 10.0f, 1.2f, 9.0f));
}

USIShakeBuffet::USIShakeBuffet(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(Make(this, TEXT("Buffet"), 0.35f, 5.0f, 0.9f, 11.0f));
}
