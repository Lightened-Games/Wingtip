// Six Inches - procedural audio.

#include "Audio/SISynth.h"
#include "SixInches.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace SISynthDsp
{
	constexpr float StopTailSeconds = 0.1f;
	constexpr float Int16Scale = 32767.0f;
	constexpr float CrackleDecaySeconds = 0.004f;
	constexpr float EndFadeInverseFraction = 20.0f;
	constexpr float DriveSmoothingPerSecond = 3.0f;
	constexpr float VolumeSmoothingPerSecond = 8.0f;
	constexpr float LoopRate = 48000.0f;

	float OnePole(float& State, float Input, float CutoffHz, float Rate)
	{
		const float Coeff = 1.0f - FMath::Exp(-UE_TWO_PI * FMath::Max(CutoffHz, 10.0f) / Rate);
		State += Coeff * (Input - State);
		return State;
	}

	float Saw(double Phase) { return static_cast<float>(2.0 * Phase - 1.0); }
	float Sine(double Phase) { return FMath::Sin(static_cast<float>(Phase) * UE_TWO_PI); }
	void Advance(double& Phase, float Freq, float Rate) { Phase += static_cast<double>(Freq) / Rate; Phase -= FMath::FloorToDouble(Phase); }
}

// ---------------------------------------------------------------------------------------------
// One-shot
// ---------------------------------------------------------------------------------------------

USISynthSound::USISynthSound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bLooping = false;
}

void USISynthSound::InitSynth(const FSISynthPreset& InPreset, int32 InSeed, float InPitchScale)
{
	Preset = InPreset;
	Random.Initialize(InSeed);
	PitchScale = FMath::Max(InPitchScale, 0.05f);
	SampleCursor = 0;
	Phase = 0.0;
	LowpassState = HighpassState = HighpassPrevInput = CrackleEnv = 0.0f;
	NumChannels = 1;
	SetSampleRate(static_cast<uint32>(Preset.SampleRate));
	Duration = Preset.Duration;
	bLooping = false;
}

int32 USISynthSound::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * static_cast<int32>(sizeof(int16)));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());

	const float Rate = static_cast<float>(Preset.SampleRate);
	const int64 Total = static_cast<int64>(Preset.Duration * Rate);
	const float CrackleDecay = FMath::Exp(-1.0f / (SISynthDsp::CrackleDecaySeconds * Rate));

	for (int32 i = 0; i < NumSamples; ++i, ++SampleCursor)
	{
		if (SampleCursor >= Total)
		{
			Out[i] = 0;
			continue;
		}
		const float T = static_cast<float>(SampleCursor) / Rate;
		const float Alpha = static_cast<float>(SampleCursor) / static_cast<float>(FMath::Max<int64>(Total, 1));

		float LocalT = T;
		float NoteOffset = 0.0f;
		if (Preset.NoteSemitones.Num() > 0)
		{
			const int32 NoteIndex = FMath::Min(static_cast<int32>(T / Preset.NoteDuration), Preset.NoteSemitones.Num() - 1);
			LocalT = T - NoteIndex * Preset.NoteDuration;
			NoteOffset = Preset.NoteSemitones[NoteIndex];
		}

		float Env;
		if (Preset.bReverseEnvelope)
		{
			Env = FMath::Pow(Alpha, 2.0f) * FMath::Clamp((1.0f - Alpha) * SISynthDsp::EndFadeInverseFraction, 0.0f, 1.0f);
		}
		else
		{
			const float Attack = Preset.AttackTime > 0.0f ? FMath::Clamp(LocalT / Preset.AttackTime, 0.0f, 1.0f) : 1.0f;
			const float Decay = FMath::Exp(-Preset.DecayRate * FMath::Max(0.0f, LocalT - Preset.AttackTime));
			const float Fade = FMath::Clamp((1.0f - Alpha) * SISynthDsp::EndFadeInverseFraction, 0.0f, 1.0f);
			Env = Attack * Decay * Fade;
		}

		float Sample = 0.0f;
		if (Preset.ToneMix > 0.0f)
		{
			const float Base = Preset.StartFrequency * FMath::Pow(Preset.EndFrequency / Preset.StartFrequency, Alpha);
			const float Wobble = Preset.WobbleRate > 0.0f ? Preset.WobbleDepth * FMath::Sin(UE_TWO_PI * Preset.WobbleRate * T) : 0.0f;
			const float Freq = Base * PitchScale * FMath::Pow(2.0f, (NoteOffset + Wobble) / 12.0f);
			Phase += static_cast<double>(Freq) / Rate;
			Phase -= FMath::FloorToDouble(Phase);
			const float Angle = static_cast<float>(Phase) * UE_TWO_PI;
			const float Harm = FMath::Sin(Angle * 3.0f) / 3.0f + FMath::Sin(Angle * 5.0f) / 5.0f;
			Sample += Preset.ToneMix * (FMath::Sin(Angle) + Preset.ToneHarmonics * Harm);
		}

		if (Preset.NoiseMix > 0.0f || Preset.CrackleChance > 0.0f)
		{
			float Noise = Random.FRandRange(-1.0f, 1.0f);
			if (Preset.CrackleChance > 0.0f && Random.FRand() < Preset.CrackleChance)
			{
				CrackleEnv = 1.0f;
			}
			Noise += CrackleEnv * Random.FRandRange(-3.0f, 3.0f);
			CrackleEnv *= CrackleDecay;

			const float Cutoff = FMath::Lerp(Preset.NoiseLowpassStart, Preset.NoiseLowpassEnd, Alpha) * PitchScale;
			float Filtered = SISynthDsp::OnePole(LowpassState, Noise, Cutoff, Rate);
			if (Preset.NoiseHighpass > 0.0f)
			{
				const float Rc = 1.0f / (UE_TWO_PI * Preset.NoiseHighpass);
				const float Dt = 1.0f / Rate;
				const float HpCoeff = Rc / (Rc + Dt);
				HighpassState = HpCoeff * (HighpassState + Filtered - HighpassPrevInput);
				HighpassPrevInput = Filtered;
				Filtered = HighpassState;
			}
			Sample += Preset.NoiseMix * Filtered;
		}

		Out[i] = static_cast<int16>(FMath::Clamp(Sample * Env * Preset.Volume, -1.0f, 1.0f) * SISynthDsp::Int16Scale);
	}
	return NumSamples;
}

// ---------------------------------------------------------------------------------------------
// Loops
// ---------------------------------------------------------------------------------------------

USILoopSynth::USILoopSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bLooping = true;
}

void USILoopSynth::InitLoop(ESILoopKind InKind, int32 InSeed)
{
	Kind = InKind;
	Random.Initialize(InSeed);
	NumChannels = 1;
	SetSampleRate(48000);
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = true;
}

float USILoopSynth::RenderEngine(float Rate)
{
	// Big piston engine: firing pulses (12 cylinders) on a low saw, filtered, plus prop "thrum" and exhaust noise.
	using namespace SISynthDsp;
	const float D = SmoothedDrive;
	const float Fire = 62.0f + 46.0f * D;
	Advance(PhaseA, Fire, Rate);
	Advance(PhaseB, Fire * 0.25f, Rate);
	Advance(PhaseC, Fire * 1.5f, Rate);
	const float Pulse = FMath::Pow(FMath::Max(0.0f, Sine(PhaseA)), 3.0f) * 2.0f - 0.6f;
	const float Raw = 0.55f * Pulse + 0.3f * Saw(PhaseB) + 0.15f * Sine(PhaseC);
	const float Body = OnePole(Lowpass, Raw, 380.0f + 700.0f * D, Rate);
	const float Exhaust = OnePole(NoiseLp, Random.FRandRange(-1.0f, 1.0f), 900.0f + 900.0f * D, Rate);
	return 0.62f * Body + 0.16f * Exhaust * (0.5f + D);
}

float USILoopSynth::RenderPulseJet(float Rate)
{
	// The flying bomb's rasp: ~47 Hz combustion pulses, each a short noisy bang with a buzzing tail.
	using namespace SISynthDsp;
	const float PulseHz = 46.0f + 3.0f * SmoothedDrive;
	Advance(PhaseA, PulseHz, Rate);
	const float P = static_cast<float>(PhaseA);
	const float Env = FMath::Exp(-P * 7.0f);
	const float Noise = Random.FRandRange(-1.0f, 1.0f);
	const float Bang = OnePole(NoiseLp, Noise, 2400.0f, Rate) * Env;
	Advance(PhaseB, PulseHz * 3.0f, Rate);
	const float Buzz = (Saw(PhaseB) * 0.5f + Saw(PhaseA) * 0.5f) * (0.35f + 0.65f * Env);
	return OnePole(Lowpass, 0.75f * Bang + 0.45f * Buzz, 1800.0f, Rate);
}

float USILoopSynth::RenderWind(float Rate)
{
	using namespace SISynthDsp;
	const float D = SmoothedDrive;
	const float N = Random.FRandRange(-1.0f, 1.0f);
	const float Low = OnePole(NoiseLp, N, 300.0f + 1600.0f * D, Rate);
	const float High = OnePole(Lowpass2, N, 4000.0f, Rate) - OnePole(NoiseHp, N, 1500.0f, Rate);
	Advance(PhaseA, 0.37f, Rate);
	const float Gust = 0.75f + 0.25f * Sine(PhaseA);
	return (0.8f * Low + 0.12f * High * D) * Gust * (0.25f + 0.75f * D);
}

float USILoopSynth::RenderHold(float Rate)
{
	// Rising whistle that tracks the hold: the airflow building over the bomb's wing.
	using namespace SISynthDsp;
	const float D = SmoothedDrive;
	const float Freq = 330.0f * FMath::Pow(2.0f, D * 1.5f);
	Advance(PhaseA, Freq, Rate);
	Advance(PhaseB, Freq * 1.5f, Rate);
	Advance(PhaseC, 5.0f + 6.0f * D, Rate);
	const float Trem = 0.8f + 0.2f * Sine(PhaseC);
	const float Air = OnePole(NoiseLp, Random.FRandRange(-1.0f, 1.0f), Freq * 2.0f, Rate);
	return (0.45f * Sine(PhaseA) + 0.15f * Sine(PhaseB) + 0.4f * Air) * Trem * 0.6f;
}

int32 USILoopSynth::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * static_cast<int32>(sizeof(int16)));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());
	const float Rate = SISynthDsp::LoopRate;
	const float Target = Drive.load();
	const float TargetVol = LoopVolume.load();
	const float SmoothCoeff = 1.0f - FMath::Exp(-SISynthDsp::DriveSmoothingPerSecond / Rate);
	const float VolCoeff = 1.0f - FMath::Exp(-SISynthDsp::VolumeSmoothingPerSecond / Rate);

	for (int32 i = 0; i < NumSamples; ++i, ++SampleCursor)
	{
		SmoothedDrive += SmoothCoeff * (Target - SmoothedDrive);
		SmoothedVolume += VolCoeff * (TargetVol - SmoothedVolume);
		float S = 0.0f;
		switch (Kind)
		{
		case ESILoopKind::Engine:   S = RenderEngine(Rate); break;
		case ESILoopKind::PulseJet: S = RenderPulseJet(Rate); break;
		case ESILoopKind::Wind:     S = RenderWind(Rate); break;
		case ESILoopKind::HoldTone: S = RenderHold(Rate); break;
		}
		Out[i] = static_cast<int16>(FMath::Clamp(S * SmoothedVolume, -1.0f, 1.0f) * SISynthDsp::Int16Scale);
	}
	return NumSamples;
}

// ---------------------------------------------------------------------------------------------
// Library
// ---------------------------------------------------------------------------------------------

USISynthSound* USIAudioLibrary::CreateSynth(const UObject* WorldContextObject, const FSISynthPreset& Preset, float PitchScale)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || Preset.Volume <= 0.0f || !World->GetAudioDeviceRaw())
	{
		return nullptr;
	}
	USISynthSound* Sound = NewObject<USISynthSound>(GetTransientPackage());
	Sound->InitSynth(Preset, FMath::Rand(), PitchScale);
	return Sound;
}

USoundAttenuation* USIAudioLibrary::MakeAttenuation(UObject* Outer, float FalloffMetres)
{
	USoundAttenuation* Att = NewObject<USoundAttenuation>(Outer ? Outer : GetTransientPackage());
	Att->Attenuation.bAttenuate = true;
	Att->Attenuation.bSpatialize = true;
	Att->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Att->Attenuation.AttenuationShapeExtents = FVector(FalloffMetres * SI_CM * 0.05f, 0.0f, 0.0f);
	Att->Attenuation.FalloffDistance = FalloffMetres * SI_CM;
	Att->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	return Att;
}

UAudioComponent* USIAudioLibrary::PlaySynthAtLocation(const UObject* WorldContextObject, const FSISynthPreset& Preset, FVector Location, float VolumeMultiplier, float PitchScale, float FalloffMetres)
{
	USISynthSound* Sound = CreateSynth(WorldContextObject, Preset, PitchScale);
	if (!Sound)
	{
		return nullptr;
	}
	USoundAttenuation* Att = MakeAttenuation(Sound, FalloffMetres);
	UAudioComponent* Comp = UGameplayStatics::SpawnSoundAtLocation(WorldContextObject, Sound, Location, FRotator::ZeroRotator, VolumeMultiplier, 1.0f, 0.0f, Att);
	if (Comp)
	{
		Comp->StopDelayed(Preset.Duration + SISynthDsp::StopTailSeconds);
	}
	return Comp;
}

UAudioComponent* USIAudioLibrary::PlaySynth2D(const UObject* WorldContextObject, const FSISynthPreset& Preset, float VolumeMultiplier, float PitchScale)
{
	USISynthSound* Sound = CreateSynth(WorldContextObject, Preset, PitchScale);
	if (!Sound)
	{
		return nullptr;
	}
	UAudioComponent* Comp = UGameplayStatics::SpawnSound2D(WorldContextObject, Sound, VolumeMultiplier);
	if (Comp)
	{
		Comp->StopDelayed(Preset.Duration + SISynthDsp::StopTailSeconds);
	}
	return Comp;
}

UAudioComponent* USIAudioLibrary::CreateLoop(AActor* Owner, USceneComponent* AttachTo, ESILoopKind Kind, int32 Seed, bool bSpatial, float FalloffMetres, USILoopSynth*& OutSynth)
{
	OutSynth = nullptr;
	if (!Owner || !Owner->GetWorld() || !Owner->GetWorld()->GetAudioDeviceRaw())
	{
		return nullptr;
	}
	OutSynth = NewObject<USILoopSynth>(Owner);
	OutSynth->InitLoop(Kind, Seed);
	UAudioComponent* Comp = NewObject<UAudioComponent>(Owner);
	Comp->bAutoActivate = false;
	Comp->bAllowSpatialization = bSpatial;
	if (bSpatial)
	{
		Comp->AttenuationSettings = MakeAttenuation(Comp, FalloffMetres);
	}
	else
	{
		Comp->bIsUISound = false;
	}
	Comp->SetSound(OutSynth);
	if (AttachTo)
	{
		Comp->SetupAttachment(AttachTo);
	}
	Comp->RegisterComponent();
	Comp->Play();
	return Comp;
}

// ---------------------------------------------------------------------------------------------
// WAV dump (raw sources for asset-library/)
// ---------------------------------------------------------------------------------------------

namespace
{
	void WriteWav(const FString& Path, const TArray<int16>& Samples, int32 Rate)
	{
		TArray<uint8> Bytes;
		auto Put32 = [&Bytes](uint32 V) { Bytes.Append(reinterpret_cast<uint8*>(&V), 4); };
		auto Put16 = [&Bytes](uint16 V) { Bytes.Append(reinterpret_cast<uint8*>(&V), 2); };
		const uint32 DataBytes = Samples.Num() * 2;
		Bytes.Append(reinterpret_cast<const uint8*>("RIFF"), 4); Put32(36 + DataBytes);
		Bytes.Append(reinterpret_cast<const uint8*>("WAVE"), 4);
		Bytes.Append(reinterpret_cast<const uint8*>("fmt "), 4); Put32(16); Put16(1); Put16(1); Put32(Rate); Put32(Rate * 2); Put16(2); Put16(16);
		Bytes.Append(reinterpret_cast<const uint8*>("data"), 4); Put32(DataBytes);
		Bytes.Append(reinterpret_cast<const uint8*>(Samples.GetData()), DataBytes);
		FFileHelper::SaveArrayToFile(Bytes, *Path);
	}
}

void USIAudioLibrary::DumpAll(const FString& Directory)
{
	IFileManager::Get().MakeDirectory(*Directory, true);
	const TArray<TPair<FString, FSISynthPreset>> Presets = {
		{ TEXT("CountdownBeep"), CountdownBeep() }, { TEXT("Go"), Go() }, { TEXT("Fanfare"), Fanfare() }, { TEXT("Defeat"), Defeat() },
		{ TEXT("SlotWhoosh"), SlotWhoosh() }, { TEXT("TipCreak"), TipCreak() }, { TEXT("TipWhoosh"), TipWhoosh() }, { TEXT("Contact"), Contact() },
		{ TEXT("Explosion"), Explosion() }, { TEXT("Flak"), Flak() }, { TEXT("FlakHit"), FlakHit() }, { TEXT("Bump"), Bump() },
		{ TEXT("Boost"), Boost() }, { TEXT("Airbrake"), Airbrake() }, { TEXT("Prompt"), Prompt() }, { TEXT("PlusOne"), PlusOne() },
		{ TEXT("TownHit"), TownHit() }, { TEXT("Crash"), Crash() }, { TEXT("CableSnap"), CableSnap() },
	};
	for (const TPair<FString, FSISynthPreset>& P : Presets)
	{
		USISynthSound* S = NewObject<USISynthSound>(GetTransientPackage());
		S->InitSynth(P.Value, 1944, 1.0f);
		const int32 N = FMath::CeilToInt(P.Value.Duration * P.Value.SampleRate);
		TArray<uint8> Raw;
		S->OnGeneratePCMAudio(Raw, N);
		TArray<int16> Samples;
		Samples.Append(reinterpret_cast<const int16*>(Raw.GetData()), N);
		WriteWav(Directory / (TEXT("SI_") + P.Key + TEXT(".wav")), Samples, P.Value.SampleRate);
	}
	const TArray<TPair<FString, ESILoopKind>> Loops = { { TEXT("Engine"), ESILoopKind::Engine }, { TEXT("PulseJet"), ESILoopKind::PulseJet },
		{ TEXT("Wind"), ESILoopKind::Wind }, { TEXT("HoldTone"), ESILoopKind::HoldTone } };
	for (const TPair<FString, ESILoopKind>& L : Loops)
	{
		USILoopSynth* S = NewObject<USILoopSynth>(GetTransientPackage());
		S->InitLoop(L.Value, 1944);
		S->SetDrive(0.6f);
		S->SetLoopVolume(1.0f);
		const int32 N = 48000 * 6;
		TArray<uint8> Raw;
		S->OnGeneratePCMAudio(Raw, N);
		TArray<int16> Samples;
		Samples.Append(reinterpret_cast<const int16*>(Raw.GetData()), N);
		WriteWav(Directory / (TEXT("SI_Loop_") + L.Key + TEXT(".wav")), Samples, 48000);
	}
	UE_LOG(LogSixInches, Display, TEXT("[SixInches] Audio dumped to %s"), *Directory);
}

// ---------------------------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------------------------

FSISynthPreset USIAudioLibrary::CountdownBeep()
{
	FSISynthPreset P;
	P.Duration = 0.25f; P.Volume = 0.35f; P.DecayRate = 8.0f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.4f; P.StartFrequency = P.EndFrequency = 620.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::Go()
{
	FSISynthPreset P;
	P.Duration = 0.8f; P.Volume = 0.4f; P.DecayRate = 3.0f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.45f; P.StartFrequency = P.EndFrequency = 587.3f;
	P.NoteSemitones = { 0.0f, 5.0f, 12.0f }; P.NoteDuration = 0.08f;
	return P;
}

FSISynthPreset USIAudioLibrary::Fanfare()
{
	// Brass-ish call: open fifths and the octave.
	FSISynthPreset P;
	P.Duration = 2.0f; P.Volume = 0.42f; P.AttackTime = 0.02f; P.DecayRate = 1.6f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.7f; P.StartFrequency = P.EndFrequency = 392.0f;
	P.NoteSemitones = { 0.0f, 7.0f, 12.0f, 7.0f, 12.0f, 16.0f, 19.0f }; P.NoteDuration = 0.2f;
	return P;
}

FSISynthPreset USIAudioLibrary::Defeat()
{
	FSISynthPreset P;
	P.Duration = 2.0f; P.Volume = 0.42f; P.AttackTime = 0.02f; P.DecayRate = 1.0f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.5f; P.StartFrequency = 294.0f; P.EndFrequency = 147.0f;
	P.NoteSemitones = { 0.0f, -2.0f, -5.0f, -9.0f }; P.NoteDuration = 0.45f;
	return P;
}

FSISynthPreset USIAudioLibrary::SlotWhoosh()
{
	FSISynthPreset P;
	P.Duration = 0.9f; P.Volume = 0.4f; P.bReverseEnvelope = true;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 500.0f; P.NoiseLowpassEnd = 3500.0f; P.NoiseHighpass = 200.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::TipCreak()
{
	FSISynthPreset P;
	P.Duration = 0.6f; P.Volume = 0.35f; P.AttackTime = 0.01f; P.DecayRate = 4.0f;
	P.ToneMix = 0.7f; P.ToneHarmonics = 0.9f; P.StartFrequency = 180.0f; P.EndFrequency = 120.0f; P.WobbleRate = 26.0f; P.WobbleDepth = 1.5f;
	P.NoiseMix = 0.3f; P.NoiseLowpassStart = P.NoiseLowpassEnd = 1200.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::TipWhoosh()
{
	FSISynthPreset P;
	P.Duration = 1.4f; P.Volume = 0.5f; P.AttackTime = 0.05f; P.DecayRate = 1.8f;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 4000.0f; P.NoiseLowpassEnd = 400.0f;
	P.ToneMix = 0.3f; P.StartFrequency = 700.0f; P.EndFrequency = 180.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::Contact()
{
	FSISynthPreset P;
	P.Duration = 0.45f; P.Volume = 0.6f; P.AttackTime = 0.001f; P.DecayRate = 6.0f;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 7000.0f; P.NoiseLowpassEnd = 1800.0f; P.NoiseHighpass = 900.0f; P.CrackleChance = 0.02f;
	P.ToneMix = 0.4f; P.ToneHarmonics = 0.9f; P.StartFrequency = 1900.0f; P.EndFrequency = 1300.0f; P.WobbleRate = 40.0f; P.WobbleDepth = 0.8f;
	return P;
}

FSISynthPreset USIAudioLibrary::Explosion()
{
	FSISynthPreset P;
	P.Duration = 2.6f; P.Volume = 0.95f; P.AttackTime = 0.002f; P.DecayRate = 1.6f;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 2400.0f; P.NoiseLowpassEnd = 120.0f; P.CrackleChance = 0.004f;
	P.ToneMix = 0.6f; P.StartFrequency = 70.0f; P.EndFrequency = 30.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::Flak()
{
	FSISynthPreset P;
	P.Duration = 0.9f; P.Volume = 0.7f; P.AttackTime = 0.001f; P.DecayRate = 5.0f;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 3000.0f; P.NoiseLowpassEnd = 300.0f; P.CrackleChance = 0.01f;
	P.ToneMix = 0.4f; P.StartFrequency = 110.0f; P.EndFrequency = 55.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::FlakHit()
{
	FSISynthPreset P;
	P.Duration = 0.7f; P.Volume = 0.75f; P.AttackTime = 0.001f; P.DecayRate = 5.5f;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 9000.0f; P.NoiseLowpassEnd = 900.0f; P.NoiseHighpass = 400.0f; P.CrackleChance = 0.05f;
	return P;
}

FSISynthPreset USIAudioLibrary::Bump()
{
	FSISynthPreset P;
	P.Duration = 0.3f; P.Volume = 0.35f; P.AttackTime = 0.004f; P.DecayRate = 12.0f;
	P.NoiseMix = 0.8f; P.NoiseLowpassStart = 700.0f; P.NoiseLowpassEnd = 200.0f;
	P.ToneMix = 0.5f; P.StartFrequency = 90.0f; P.EndFrequency = 50.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::Boost()
{
	FSISynthPreset P;
	P.Duration = 0.9f; P.Volume = 0.35f; P.AttackTime = 0.08f; P.DecayRate = 2.0f;
	P.ToneMix = 0.8f; P.ToneHarmonics = 0.8f; P.StartFrequency = 85.0f; P.EndFrequency = 130.0f;
	P.NoiseMix = 0.4f; P.NoiseLowpassStart = 800.0f; P.NoiseLowpassEnd = 2000.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::Airbrake()
{
	FSISynthPreset P;
	P.Duration = 0.5f; P.Volume = 0.3f; P.AttackTime = 0.03f; P.DecayRate = 4.0f;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 2500.0f; P.NoiseLowpassEnd = 900.0f; P.NoiseHighpass = 300.0f;
	P.ToneMix = 0.25f; P.StartFrequency = 300.0f; P.EndFrequency = 220.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::Prompt()
{
	FSISynthPreset P;
	P.Duration = 0.3f; P.Volume = 0.22f; P.DecayRate = 9.0f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.1f; P.StartFrequency = P.EndFrequency = 880.0f;
	P.NoteSemitones = { 0.0f, 5.0f }; P.NoteDuration = 0.07f;
	return P;
}

FSISynthPreset USIAudioLibrary::PlusOne()
{
	FSISynthPreset P;
	P.Duration = 0.6f; P.Volume = 0.32f; P.DecayRate = 5.0f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.2f; P.StartFrequency = P.EndFrequency = 784.0f;
	P.NoteSemitones = { 0.0f, 4.0f, 7.0f }; P.NoteDuration = 0.07f;
	return P;
}

FSISynthPreset USIAudioLibrary::TownHit()
{
	FSISynthPreset P;
	P.Duration = 1.2f; P.Volume = 0.45f; P.AttackTime = 0.01f; P.DecayRate = 1.5f;
	P.ToneMix = 1.0f; P.ToneHarmonics = 0.8f; P.StartFrequency = P.EndFrequency = 196.0f;
	P.NoteSemitones = { 0.0f, -1.0f, 0.0f, -1.0f }; P.NoteDuration = 0.25f;
	return P;
}

FSISynthPreset USIAudioLibrary::Crash()
{
	FSISynthPreset P;
	P.Duration = 2.2f; P.Volume = 0.95f; P.AttackTime = 0.001f; P.DecayRate = 1.8f;
	P.NoiseMix = 1.0f; P.NoiseLowpassStart = 6000.0f; P.NoiseLowpassEnd = 150.0f; P.CrackleChance = 0.008f;
	P.ToneMix = 0.5f; P.StartFrequency = 95.0f; P.EndFrequency = 35.0f;
	return P;
}

FSISynthPreset USIAudioLibrary::CableSnap()
{
	FSISynthPreset P;
	P.Duration = 0.8f; P.Volume = 0.6f; P.AttackTime = 0.001f; P.DecayRate = 4.0f;
	P.ToneMix = 0.8f; P.ToneHarmonics = 0.6f; P.StartFrequency = 2200.0f; P.EndFrequency = 400.0f; P.WobbleRate = 60.0f; P.WobbleDepth = 2.0f;
	P.NoiseMix = 0.5f; P.NoiseLowpassStart = 8000.0f; P.NoiseLowpassEnd = 1000.0f;
	return P;
}
