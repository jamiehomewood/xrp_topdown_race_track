#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "RaceEngineSynth.generated.h"

/**
 * Procedural car engine voice (no sound assets). Mono, so the audio engine pans it across the room's
 * speakers by the car's direction from the listener. Pitch follows speed, roar follows throttle,
 * and TriggerImpact adds a scrape/thump burst.
 */
UCLASS(ClassGroup = Race, meta = (BlueprintSpawnableComponent))
class XRP_TOPDOWNRACE_API URaceEngineSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	URaceEngineSynth(const FObjectInitializer& ObjectInitializer);

	/** Game thread. SpeedAlpha: 0..1 of top speed. Load: 0..1 (throttle or brake held). */
	void SetEngineState(float SpeedAlpha, float Load);

	/** Game thread. Strength 0..1. */
	void TriggerImpact(float Strength);

	/** Game thread. Multiplies the engine's frequency so each car has its own voice. */
	void SetVoicePitch(float PitchScale);

	/** RMS of the last generated audio block (any thread). Diagnostics: shows the synth is producing sound. */
	float GetRecentLevel() const { return RecentLevel.load(std::memory_order_relaxed); }

protected:
	virtual bool Init(int32& InSampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	// Everything below is only touched on the audio render thread (via SynthCommand).
	float SampleRate = 48000.0f;
	double EnginePhase = 0.0;
	double ThumpPhase = 0.0;
	float Rpm = 0.0f;
	float TargetRpm = 0.0f;
	float Load = 0.0f;
	float TargetLoad = 0.0f;
	float PitchScale = 1.0f;
	float ImpactEnvelope = 0.0f;
	float RoarFilter = 0.0f;
	float ScrapeFilter = 0.0f;
	uint32 NoiseState = 0x9E3779B9u;

	std::atomic<float> RecentLevel{ 0.0f };
};
