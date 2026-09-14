#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "RaceSignalSynth.generated.h"

/**
 * Non-spatial beeps for race signals (start lights) and the speaker test. Renders 8 channels in 7.1 order
 * (front L, front R, centre, sub, back L, back R, side L, side R), so a beep can come from every speaker at once
 * or from a single channel. The mixer folds the 7.1 signal down to whatever layout the output device has.
 * Several beeps can sound at once (they are mixed), so overlapping signals don't cut each other off.
 */
UCLASS(ClassGroup = Race, meta = (BlueprintSpawnableComponent))
class XRP_TOPDOWNRACE_API URaceSignalSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	URaceSignalSynth(const FObjectInitializer& ObjectInitializer);

	static constexpr int32 NumOutputChannels = 8;
	static constexpr int32 LowFrequencyChannel = 3;
	static constexpr uint32 AllSpeakers = 0xFFu & ~(1u << LowFrequencyChannel);

	/** Game thread. Plays a tone on the channels in ChannelMask (one bit per 7.1 channel). */
	void Beep(float Frequency, float Seconds, float Volume, uint32 ChannelMask = AllSpeakers);

	/** Speaker name for a 7.1 channel index. */
	static const TCHAR* ChannelName(int32 Channel);

protected:
	virtual bool Init(int32& InSampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	struct FVoice
	{
		double Phase = 0.0;
		float Frequency = 880.0f;
		float Duration = 0.0f;
		float Elapsed = 0.0f;
		float Gain = 0.0f;
		uint32 Mask = 0;

		bool IsSounding() const { return Elapsed < Duration; }
	};

	static constexpr int32 MaxVoices = 4;

	// Audio render thread state (set through SynthCommand).
	float SampleRate = 48000.0f;
	FVoice Voices[MaxVoices];
};
