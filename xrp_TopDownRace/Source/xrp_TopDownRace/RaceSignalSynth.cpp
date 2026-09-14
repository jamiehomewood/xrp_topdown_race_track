#include "RaceSignalSynth.h"

namespace
{
	constexpr float AttackSeconds = 0.005f;
	constexpr float ReleaseSeconds = 0.03f;
}

URaceSignalSynth::URaceSignalSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = NumOutputChannels;
}

bool URaceSignalSynth::Init(int32& InSampleRate)
{
	NumChannels = NumOutputChannels;
	SampleRate = float(InSampleRate);
	return true;
}

const TCHAR* URaceSignalSynth::ChannelName(int32 Channel)
{
	static const TCHAR* Names[NumOutputChannels] = {
		TEXT("FRONT LEFT"), TEXT("FRONT RIGHT"), TEXT("CENTRE"), TEXT("SUBWOOFER"),
		TEXT("BACK LEFT"), TEXT("BACK RIGHT"), TEXT("SIDE LEFT"), TEXT("SIDE RIGHT"),
	};
	return (Channel >= 0 && Channel < NumOutputChannels) ? Names[Channel] : TEXT("?");
}

void URaceSignalSynth::Beep(float InFrequency, float Seconds, float Volume, uint32 ChannelMask)
{
	const float NewFrequency = FMath::Max(20.0f, InFrequency);
	const float NewDuration = FMath::Max(0.05f, Seconds);
	const float NewGain = FMath::Clamp(Volume, 0.0f, 1.0f);
	SynthCommand([this, NewFrequency, NewDuration, NewGain, ChannelMask]()
	{
		// Use a free voice, or steal the one closest to finishing.
		int32 Best = 0;
		for (int32 Index = 0; Index < MaxVoices; ++Index)
		{
			if (!Voices[Index].IsSounding())
			{
				Best = Index;
				break;
			}
			if (Voices[Index].Duration - Voices[Index].Elapsed < Voices[Best].Duration - Voices[Best].Elapsed)
			{
				Best = Index;
			}
		}
		FVoice& Voice = Voices[Best];
		Voice.Frequency = NewFrequency;
		Voice.Duration = NewDuration;
		Voice.Gain = NewGain;
		Voice.Mask = ChannelMask;
		Voice.Elapsed = 0.0f;
		Voice.Phase = 0.0;
	});
}

int32 URaceSignalSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	// NumSamples counts interleaved samples across all channels.
	const int32 NumFrames = NumSamples / NumOutputChannels;
	const float Dt = 1.0f / SampleRate;
	FMemory::Memzero(OutAudio, sizeof(float) * NumFrames * NumOutputChannels);

	for (FVoice& Voice : Voices)
	{
		for (int32 Frame = 0; Frame < NumFrames && Voice.IsSounding(); ++Frame)
		{
			const float Envelope = FMath::Min(Voice.Elapsed / AttackSeconds, 1.0f) *
				FMath::Clamp((Voice.Duration - Voice.Elapsed) / ReleaseSeconds, 0.0f, 1.0f);
			const float Angle = UE_TWO_PI * float(Voice.Phase);
			// Slightly square-ish tone so it cuts through the engines, like a timing-beacon beep.
			const float Sample = Voice.Gain * Envelope * (0.8f * FMath::Sin(Angle) + 0.2f * FMath::Sin(3.0f * Angle));
			Voice.Phase += Voice.Frequency * Dt;
			Voice.Phase -= FMath::FloorToDouble(Voice.Phase);
			Voice.Elapsed += Dt;

			float* FrameOut = OutAudio + Frame * NumOutputChannels;
			for (int32 Channel = 0; Channel < NumOutputChannels; ++Channel)
			{
				if (Voice.Mask & (1u << Channel))
				{
					FrameOut[Channel] += Sample;
				}
			}
		}
	}
	return NumSamples;
}
