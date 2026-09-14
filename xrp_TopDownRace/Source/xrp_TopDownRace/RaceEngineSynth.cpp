#include "RaceEngineSynth.h"

namespace
{
	constexpr float IdleHz = 32.0f;        // firing frequency at a standstill
	constexpr float TopSpeedHz = 115.0f;   // firing frequency at MaxSpeed
	constexpr float OutputGain = 0.4f;
	constexpr float RpmResponse = 4.0f;    // 1/s: engine note glides rather than jumps
	constexpr float LoadResponse = 12.0f;  // 1/s
	constexpr float ImpactDecaySeconds = 0.12f;
	constexpr float ThumpHz = 58.0f;

	float WhiteNoise(uint32& State)
	{
		State = State * 1664525u + 1013904223u;
		return float(State >> 8) * (2.0f / 16777216.0f) - 1.0f;
	}

	float SoftClip(float X)
	{
		return X / (1.0f + FMath::Abs(X));
	}
}

URaceEngineSynth::URaceEngineSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
}

bool URaceEngineSynth::Init(int32& InSampleRate)
{
	NumChannels = 1;
	SampleRate = float(InSampleRate);
	return true;
}

void URaceEngineSynth::SetEngineState(float SpeedAlpha, float InLoad)
{
	const float NewRpm = FMath::Clamp(SpeedAlpha, 0.0f, 1.0f);
	const float NewLoad = FMath::Clamp(InLoad, 0.0f, 1.0f);
	SynthCommand([this, NewRpm, NewLoad]()
	{
		TargetRpm = NewRpm;
		TargetLoad = NewLoad;
	});
}

void URaceEngineSynth::TriggerImpact(float Strength)
{
	const float NewStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	SynthCommand([this, NewStrength]()
	{
		ImpactEnvelope = FMath::Max(ImpactEnvelope, NewStrength);
		ThumpPhase = 0.0;
	});
}

void URaceEngineSynth::SetVoicePitch(float InPitchScale)
{
	const float NewPitch = FMath::Max(0.25f, InPitchScale);
	SynthCommand([this, NewPitch]()
	{
		PitchScale = NewPitch;
	});
}

int32 URaceEngineSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float Dt = 1.0f / SampleRate;
	const float RpmStep = 1.0f - FMath::Exp(-Dt * RpmResponse);
	const float LoadStep = 1.0f - FMath::Exp(-Dt * LoadResponse);
	const float ImpactDecay = FMath::Exp(-Dt / ImpactDecaySeconds);
	double SumSquares = 0.0;

	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		Rpm += (TargetRpm - Rpm) * RpmStep;
		Load += (TargetLoad - Load) * LoadStep;

		// Engine tone: buzzy sawtooth at the firing frequency plus fundamental and octave for body.
		const float Hz = (IdleHz + (TopSpeedHz - IdleHz) * Rpm) * PitchScale;
		EnginePhase += Hz * Dt;
		EnginePhase -= FMath::FloorToDouble(EnginePhase);
		const float Phase = float(EnginePhase);
		const float Tone = 0.45f * (2.0f * Phase - 1.0f)
			+ 0.35f * FMath::Sin(UE_TWO_PI * Phase)
			+ 0.2f * FMath::Sin(2.0f * UE_TWO_PI * Phase);

		// Exhaust roar: low-passed noise pulsed by each firing, louder under throttle.
		RoarFilter += (WhiteNoise(NoiseState) - RoarFilter) * 0.2f;
		const float Roar = RoarFilter * (1.0f - Phase) * (0.25f + 1.1f * Load);

		const float EngineLevel = 0.35f + 0.45f * Rpm + 0.2f * Load;
		float Sample = (Tone * (0.4f + 0.6f * Load) + Roar) * EngineLevel;

		// Wall/car hit: bright scrape noise plus a short low thump.
		if (ImpactEnvelope > 0.001f)
		{
			ScrapeFilter += (WhiteNoise(NoiseState) - ScrapeFilter) * 0.7f;
			ThumpPhase += ThumpHz * Dt;
			Sample += ImpactEnvelope * (0.9f * ScrapeFilter + 0.8f * FMath::Sin(UE_TWO_PI * float(ThumpPhase)));
			ImpactEnvelope *= ImpactDecay;
		}

		OutAudio[Index] = SoftClip(Sample * 1.6f) * OutputGain;
		SumSquares += double(OutAudio[Index]) * OutAudio[Index];
	}
	RecentLevel.store(NumSamples > 0 ? float(FMath::Sqrt(SumSquares / NumSamples)) : 0.0f, std::memory_order_relaxed);
	return NumSamples;
}
