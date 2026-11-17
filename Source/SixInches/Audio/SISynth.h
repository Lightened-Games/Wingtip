// Six Inches - procedural audio (no third-party sounds). Adapted from Echo Lane's synth.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include <atomic>
#include "SISynth.generated.h"

class UAudioComponent;
class USoundAttenuation;

/** Parameters for a procedural one-shot. -SIDumpAudio renders every preset to WAV for asset-library/. */
USTRUCT(BlueprintType)
struct FSISynthPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.01"))
	float Duration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0"))
	float AttackTime = 0.005f;

	/** Exponential amplitude decay (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0"))
	float DecayRate = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ToneMix = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ToneHarmonics = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "1.0"))
	float StartFrequency = 440.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "1.0"))
	float EndFrequency = 440.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0"))
	float WobbleRate = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0"))
	float WobbleDepth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NoiseMix = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "10.0"))
	float NoiseLowpassStart = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "10.0"))
	float NoiseLowpassEnd = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0"))
	float NoiseHighpass = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrackleChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth")
	bool bReverseEnvelope = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth")
	TArray<float> NoteSemitones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "0.01"))
	float NoteDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth", meta = (ClampMin = "8000"))
	int32 SampleRate = 48000;
};

UCLASS()
class SIXINCHES_API USISynthSound : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	USISynthSound(const FObjectInitializer& ObjectInitializer);
	void InitSynth(const FSISynthPreset& InPreset, int32 InSeed, float InPitchScale);
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	FSISynthPreset Preset;
	FRandomStream Random;
	float PitchScale = 1.0f;
	int64 SampleCursor = 0;
	double Phase = 0.0;
	float LowpassState = 0.0f;
	float HighpassState = 0.0f;
	float HighpassPrevInput = 0.0f;
	float CrackleEnv = 0.0f;
};

UENUM(BlueprintType)
enum class ESILoopKind : uint8
{
	Engine,
	PulseJet,
	Wind,
	HoldTone
};

/** Endless procedural loop driven by a 0..1 value written from the game thread. */
UCLASS()
class SIXINCHES_API USILoopSynth : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	USILoopSynth(const FObjectInitializer& ObjectInitializer);
	void InitLoop(ESILoopKind InKind, int32 InSeed);
	void SetDrive(float Value) { Drive.store(FMath::Clamp(Value, 0.0f, 1.0f)); }
	void SetLoopVolume(float Value) { LoopVolume.store(FMath::Clamp(Value, 0.0f, 1.5f)); }
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

private:
	float RenderEngine(float Rate);
	float RenderPulseJet(float Rate);
	float RenderWind(float Rate);
	float RenderHold(float Rate);

	ESILoopKind Kind = ESILoopKind::Engine;
	FRandomStream Random;
	std::atomic<float> Drive{ 0.0f };
	std::atomic<float> LoopVolume{ 1.0f };
	float SmoothedDrive = 0.0f;
	float SmoothedVolume = 0.0f;
	int64 SampleCursor = 0;
	double PhaseA = 0.0;
	double PhaseB = 0.0;
	double PhaseC = 0.0;
	float Lowpass = 0.0f;
	float Lowpass2 = 0.0f;
	float NoiseLp = 0.0f;
	float NoiseHp = 0.0f;
};

UCLASS()
class SIXINCHES_API USIAudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SixInches|Audio", meta = (WorldContext = "WorldContextObject"))
	static UAudioComponent* PlaySynthAtLocation(const UObject* WorldContextObject, const FSISynthPreset& Preset, FVector Location, float VolumeMultiplier = 1.0f, float PitchScale = 1.0f, float FalloffMetres = 2500.0f);

	UFUNCTION(BlueprintCallable, Category = "SixInches|Audio", meta = (WorldContext = "WorldContextObject"))
	static UAudioComponent* PlaySynth2D(const UObject* WorldContextObject, const FSISynthPreset& Preset, float VolumeMultiplier = 1.0f, float PitchScale = 1.0f);

	/** Looping audio component on an actor, already playing. */
	static UAudioComponent* CreateLoop(AActor* Owner, USceneComponent* AttachTo, ESILoopKind Kind, int32 Seed, bool bSpatial, float FalloffMetres, USILoopSynth*& OutSynth);

	/** Distance attenuation with a natural log falloff (inner radius 5 % of the falloff). */
	static USoundAttenuation* MakeAttenuation(UObject* Outer, float FalloffMetres);

	/** Write every preset and 6 s of each loop to WAV files (asset-library raw sources). */
	static void DumpAll(const FString& Directory);

	static FSISynthPreset CountdownBeep();
	static FSISynthPreset Go();
	static FSISynthPreset Fanfare();
	static FSISynthPreset Defeat();
	static FSISynthPreset SlotWhoosh();
	static FSISynthPreset TipCreak();
	static FSISynthPreset TipWhoosh();
	static FSISynthPreset Contact();
	static FSISynthPreset Explosion();
	static FSISynthPreset Flak();
	static FSISynthPreset FlakHit();
	static FSISynthPreset Bump();
	static FSISynthPreset Boost();
	static FSISynthPreset Airbrake();
	static FSISynthPreset Prompt();
	static FSISynthPreset PlusOne();
	static FSISynthPreset TownHit();
	static FSISynthPreset Crash();
	static FSISynthPreset CableSnap();

private:
	static USISynthSound* CreateSynth(const UObject* WorldContextObject, const FSISynthPreset& Preset, float PitchScale);
};
