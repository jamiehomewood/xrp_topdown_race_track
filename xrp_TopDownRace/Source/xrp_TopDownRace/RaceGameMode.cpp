#include "RaceGameMode.h"

#include "AudioDevice.h"
#include "AudioMixerBlueprintLibrary.h"
#include "AudioMixerDevice.h"
#include "RaceSignalSynth.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RaceCarPawn.h"
#include "RaceDisplay.h"
#include "RaceEngineSynth.h"
#include "RaceInputSettings.h"
#include "RacePlayerController.h"
#include "RaceStartLights.h"
#include "TextureResource.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogRace, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarRaceDraftEnabled(
		TEXT("race.DraftEnabled"), -1,
		TEXT("Slipstream override for testing: -1 = use Race Settings, 0 = off, 1 = on."));

	TAutoConsoleVariable<FString> CVarRaceCaptureAt(
		TEXT("race.CaptureAt"), TEXT(""),
		TEXT("Test aid: comma-separated seconds after start (e.g. \"8,40\") at which to save the Igloo floor/wall camera images to Saved/RaceCaptures and take a desktop screenshot."));

	TAutoConsoleVariable<float> CVarRaceRecordAudio(
		TEXT("race.RecordAudio"), 0.0f,
		TEXT("Test aid: 5 s into play, record the master audio mix for this many seconds to Saved/BouncedWavFiles/RaceAudio.wav, logging car positions every 0.1 s (race.AudioTrace) to check speaker panning."));

	TAutoConsoleVariable<int32> CVarRaceAudioSweep(
		TEXT("race.AudioSweep"), 0,
		TEXT("Test aid, with race.RecordAudio: hold car 1 on a circle around the audio listener, stepping 45 degrees every 1.5 s, so each speaker direction gets a clean measurement."));

	TAutoConsoleVariable<float> CVarRaceSpeakerTest(
		TEXT("race.SpeakerTest"), 0.0f,
		TEXT("Speaker test: beep each output channel in turn (front left, front right, centre, back left/right, side left/right), naming it on the wall banner. ")
		TEXT("Value = seconds of play before it starts (1 = now). Resets to 0 when started."));

	// Start-light signals.
	constexpr float LightBeepHz = 880.0f;
	constexpr float LightBeepSeconds = 0.25f;
	constexpr float GoBeepHz = 1320.0f;
	constexpr float GoBeepSeconds = 0.7f;
	constexpr float SpeakerTestHz = 1000.0f;
	constexpr float SpeakerTestBeepSeconds = 0.8f;
	constexpr float SpeakerTestStepSeconds = 1.5f;
	constexpr int32 MinRoomAudioChannels = 6;

	TAutoConsoleVariable<int32> CVarRaceIglooCameraReport(
		TEXT("race.IglooCameraReport"), 0,
		TEXT("Log each Igloo capture camera's direction, capture mode and sampled image brightness 8 seconds into play."));

	/** Capture cameras whose view points more upward than this (forward Z) count as the ceiling camera. */
	constexpr float CeilingForwardZ = 0.7f;

	// Start line and grid (track_geom.START_LINE_X on the first straight).
	constexpr float StartLineX = 300.0f;
	constexpr float StartStraightY = -1950.0f;

	// Race timing (seconds).
	constexpr float GetReadySeconds = 4.0f;
	constexpr float LightInterval = 1.0f;
	constexpr float LightsHoldMin = 0.4f;
	constexpr float LightsHoldMax = 2.0f;
	constexpr float FinishGraceSeconds = 15.0f;
	constexpr float ResultsSeconds = 8.0f;
	constexpr float GoBannerSeconds = 2.0f;

	// Sector gates as fractions of the lap; a lap only counts after passing both in order.
	constexpr float Sector1Start = 0.30f;
	constexpr float Sector1End = 0.45f;
	constexpr float Sector2Start = 0.60f;
	constexpr float Sector2End = 0.75f;

	// Slipstream: a car counts as drafting when the car ahead is within this box behind it, heading the same way.
	constexpr float DraftMinGap = 150.0f;       // centre to centre, along the follower's heading
	constexpr float DraftRange = 1000.0f;       // strength fades to zero at this distance
	constexpr float DraftWidth = 130.0f;        // max sideways offset between the cars
	constexpr float DraftMinLeaderSpeed = 400.0f;
	constexpr float DraftMinHeadingDot = 0.8f;

	// Computer driver.
	constexpr float DriverPassDraft = 0.55f;    // slipstream strength at which it pulls out to overtake
	constexpr float DriverPassLane = 220.0f;
	constexpr float DriverStuckSpeed = 120.0f;
	constexpr float DriverStuckSeconds = 1.0f;
	constexpr float DriverReverseSeconds = 0.9f;
	constexpr float DriverSkillMin = 0.85f;
	constexpr float DriverMistakeGapMin = 4.0f;       // seconds between mistakes for the weakest driver...
	constexpr float DriverMistakeGapMax = 12.0f;      // ...scaled up to this for the best
	constexpr float DriverWobbleAtWorstSkill = 0.35f; // steering wobble amplitude at DriverSkillMin

	const TCHAR* MistakeName(EDriverMistake Mistake)
	{
		switch (Mistake)
		{
		case EDriverMistake::LateBraking: return TEXT("late braking");
		case EDriverMistake::RunWide: return TEXT("running wide");
		case EDriverMistake::Oversteer: return TEXT("oversteer");
		case EDriverMistake::Hesitation: return TEXT("hesitation");
		default: return TEXT("none");
		}
	}

	// Room layout (1 physical cm = 10 UU; the front wall is +Y). Wall displays stand just beyond the room walls
	// (y = +-3000) but inside the tree ring, so the wall projection shows them near their real size. Everything
	// sits above the horizon (eye height 1700 UU) so it reads against the sky, not the trees.
	constexpr float WallDisplayDistance = 3150.0f;
	constexpr float WallLightsHeight = 2780.0f;
	constexpr float WallBannerHeight = 2420.0f;
	constexpr float WallFirstRowHeight = 2230.0f;
	constexpr float WallRowSpacing = 140.0f;
	constexpr int32 WallRows = 4;

	// Floor panels in the room corners (walkway, outside the fence line), on a dark pad.
	constexpr float PanelHeightAboveFloor = 175.0f;
	constexpr float PanelLineSpacing = 105.0f;
	constexpr int32 PanelLinesPerSlot = 3;

	FColor SlotColor(int32 Slot)
	{
		static const FColor Colors[] = { FColor(255, 210, 0), FColor(255, 70, 60), FColor(40, 200, 255), FColor(90, 255, 90) };
		return Colors[FMath::Abs(Slot) % UE_ARRAY_COUNT(Colors)];
	}

	FColor Dimmed(FColor Color)
	{
		return FColor(Color.R * 7 / 10, Color.G * 7 / 10, Color.B * 7 / 10);
	}

	const TCHAR* PhaseName(ERacePhase Phase)
	{
		switch (Phase)
		{
		case ERacePhase::GetReady: return TEXT("GetReady");
		case ERacePhase::Lights: return TEXT("Lights");
		case ERacePhase::Racing: return TEXT("Racing");
		case ERacePhase::Finishing: return TEXT("Finishing");
		case ERacePhase::Results: return TEXT("Results");
		}
		return TEXT("?");
	}

	FString FormatTime(float Seconds)
	{
		if (Seconds <= 0.0f)
		{
			return TEXT("--.-");
		}
		if (Seconds < 60.0f)
		{
			return FString::Printf(TEXT("%.1f"), Seconds);
		}
		const int32 Minutes = FMath::FloorToInt(Seconds / 60.0f);
		return FString::Printf(TEXT("%d:%04.1f"), Minutes, Seconds - Minutes * 60.0f);
	}
}

ARaceGameMode::ARaceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerControllerClass = ARacePlayerController::StaticClass();
	DefaultPawnClass = nullptr;

	// Every road vehicle in the Fab car pack (the pack's aeroplane is left out). Each race picks four at random.
	const TCHAR* CarsRoot = TEXT("/Game/Fab/Mobile_Optimize-Free_Low_Poly_Cars");
	for (const TCHAR* Name : { TEXT("Sport_Car_39"), TEXT("N_Muscle_Car_10"), TEXT("Hatchback_Car_15"), TEXT("Police_Car_N_4"),
		TEXT("Classic_Car_9"), TEXT("N_Van_10"), TEXT("Pick_Up_11"), TEXT("Military_Vehicle_3"), TEXT("Monster_Truck_15") })
	{
		CarMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("%s/%s/StaticMeshes/%s.%s"), CarsRoot, Name, Name, Name))));
	}

	// 2 x 2 grid behind the start line, facing the race direction (+X).
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Lane = 0; Lane < 2; ++Lane)
		{
			const FVector Location(StartLineX - 450.0f - Row * 650.0f, StartStraightY + (Lane == 0 ? -170.0f : 170.0f), 0.0f);
			GridSlots.Add(FTransform(FRotator::ZeroRotator, Location));
		}
	}
}

void ARaceGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// No default pawn: every car is always on track, and a player takes theirs over with a button press.
}

void ARaceGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureCars();
	SpawnRaceProps();

	// One shared top-down view; the Igloo Manager handles the room projection.
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->SetForceDisableSplitscreen(true);
	}

	// The desktop window is only a monitor; spend the GPU on the Igloo capture cameras instead.
	if (IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		ScreenPercentage->Set(GetDefault<URaceInputSettings>()->DesktopViewScreenPercentage, ECVF_SetByGameSetting);
	}

	for (int32 ControllerId = 0; ControllerId < MaxPlayers; ++ControllerId)
	{
		if (!UGameplayStatics::GetPlayerControllerFromID(this, ControllerId))
		{
			UGameplayStatics::CreatePlayer(this, ControllerId, true);
		}
	}
	UE_LOG(LogRace, Log, TEXT("Race ready: %d cars, lap length %.0f UU, %d laps. Press any button on a controller to take a car."),
		Cars.Num(), TrackPath.GetLapLength(), GetDefault<URaceInputSettings>()->RaceLaps);
	LogAudioDevice();

	EnterPhase(ERacePhase::GetReady);
}

void ARaceGameMode::LogAudioDevice()
{
	FAudioDeviceHandle DeviceHandle = GetWorld()->GetAudioDevice();
	if (!DeviceHandle.IsValid())
	{
		UE_LOG(LogRace, Warning, TEXT("race.Audio: no audio device (sound disabled?)"));
		return;
	}
	const Audio::FMixerDevice* MixerDevice = static_cast<Audio::FMixerDevice*>(DeviceHandle.GetAudioDevice());
	const Audio::FAudioPlatformDeviceInfo& Info = MixerDevice->GetPlatformDeviceInfo();
	AudioDeviceName = Info.Name;
	AudioChannelCount = Info.NumChannels;

	TArray<FString> ChannelNames;
	for (const EAudioMixerChannel::Type Channel : Info.OutputChannelArray)
	{
		ChannelNames.Add(EAudioMixerChannel::ToString(Channel));
	}
	UE_LOG(LogRace, Log, TEXT("race.Audio output device '%s': %d channels (%s), %d Hz"),
		*Info.Name, Info.NumChannels, *FString::Join(ChannelNames, TEXT(", ")), Info.SampleRate);
	if (AudioChannelCount < MinRoomAudioChannels)
	{
		UE_LOG(LogRace, Warning, TEXT("race.Audio only %d output channels: the room's speakers need the multichannel device as Windows' default output, set to 7.1."),
			AudioChannelCount);
	}
}

void ARaceGameMode::PlaySignal(float Frequency, float Seconds)
{
	if (StartLights && StartLights->SignalSound)
	{
		StartLights->SignalSound->Beep(Frequency, Seconds, 0.6f * GetDefault<URaceInputSettings>()->SignalVolume);
	}
}

void ARaceGameMode::UpdateSpeakerTest(float DeltaSeconds)
{
	const float StartAfter = CVarRaceSpeakerTest.GetValueOnGameThread();
	if (SpeakerTestChannel == INDEX_NONE && StartAfter > 0.0f && CaptureClock >= StartAfter)
	{
		// Reset at console priority: a lower-priority reset is ignored after the value was typed in the console.
		CVarRaceSpeakerTest->Set(0.0f, ECVF_SetByConsole);
		SpeakerTestChannel = 0;
		SpeakerTestTimer = 0.0f;
		UE_LOG(LogRace, Log, TEXT("race.SpeakerTest start (device '%s', %d channels)"), *AudioDeviceName, AudioChannelCount);
	}
	if (SpeakerTestChannel == INDEX_NONE)
	{
		return;
	}

	SpeakerTestTimer -= DeltaSeconds;
	if (SpeakerTestTimer > 0.0f)
	{
		return;
	}
	if (SpeakerTestChannel == URaceSignalSynth::LowFrequencyChannel)
	{
		++SpeakerTestChannel; // the sub isn't a directional speaker
	}
	if (SpeakerTestChannel >= URaceSignalSynth::NumOutputChannels || !StartLights || !StartLights->SignalSound)
	{
		SpeakerTestChannel = INDEX_NONE;
		DiagnosticBanner.Reset();
		UE_LOG(LogRace, Log, TEXT("race.SpeakerTest done"));
		return;
	}

	StartLights->SignalSound->Beep(SpeakerTestHz, SpeakerTestBeepSeconds, 0.6f * GetDefault<URaceInputSettings>()->SignalVolume, 1u << SpeakerTestChannel);
	DiagnosticBanner = FString::Printf(TEXT("SPEAKER TEST: %s  (%d CH)"), URaceSignalSynth::ChannelName(SpeakerTestChannel), AudioChannelCount);
	UE_LOG(LogRace, Log, TEXT("race.SpeakerTest channel %d %s"), SpeakerTestChannel, URaceSignalSynth::ChannelName(SpeakerTestChannel));
	SpeakerTestTimer = SpeakerTestStepSeconds;
	++SpeakerTestChannel;
}

void ARaceGameMode::SpawnRaceProps()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	StartLights = GetWorld()->SpawnActor<ARaceStartLights>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (StartLights)
	{
		// Gantry over the start line for the floor projection: lights across the road, facing up.
		StartLights->AddBoard(FTransform(FRotator::ZeroRotator, FVector(StartLineX, StartStraightY, 260.0f)), 55.0f, 125.0f);
		// Big boards beyond the front and back walls, facing the room; first light on the viewer's left.
		StartLights->AddBoard(FTransform(FRotationMatrix::MakeFromZY(FVector(0.0f, -1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f)).ToQuat(),
			FVector(0.0f, WallDisplayDistance, WallLightsHeight)), 150.0f, 380.0f);
		StartLights->AddBoard(FTransform(FRotationMatrix::MakeFromZY(FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f)).ToQuat(),
			FVector(0.0f, -WallDisplayDistance, WallLightsHeight)), 150.0f, 380.0f);
		StartLights->SetLitCount(0);
	}

	Display = GetWorld()->SpawnActor<ARaceDisplay>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (Display)
	{
		// A floor panel in each room corner (walkway, outside the track), reading from the corner towards the
		// centre. Order: front-left, front-right, back-right, back-left (front wall = +Y, its left = +X).
		const FVector Corners[] = { FVector(2260.0f, 2760.0f, PanelHeightAboveFloor), FVector(-2260.0f, 2760.0f, PanelHeightAboveFloor),
			FVector(-2260.0f, -2760.0f, PanelHeightAboveFloor), FVector(2260.0f, -2760.0f, PanelHeightAboveFloor) };
		for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
		{
			const FVector& Corner = Corners[Slot % UE_ARRAY_COUNT(Corners)];
			const FVector Up = FVector(-Corner.X, -Corner.Y, 0.0f).GetSafeNormal();
			Display->AddFloorBacking(Corner - FVector(0.0f, 0.0f, 10.0f), Up, 760.0f, PanelLineSpacing * PanelLinesPerSlot + 60.0f);
			for (int32 Line = 0; Line < PanelLinesPerSlot; ++Line)
			{
				const float Offset = (1 - Line) * PanelLineSpacing; // first line furthest into the room
				PanelLines.Add(Display->AddFloorLine(Corner + Up * Offset, Up, Line == 0 ? 95.0f : 78.0f));
			}
		}

		// Banner and leaderboard beyond the front and back walls, on a dark board.
		for (const float Side : { 1.0f, -1.0f })
		{
			const FVector Facing(0.0f, -Side, 0.0f);
			const float Y = Side * WallDisplayDistance;
			const float BoardTop = WallBannerHeight + 130.0f;
			const float BoardBottom = WallFirstRowHeight - (WallRows - 1) * WallRowSpacing - 90.0f;
			Display->AddWallBacking(FVector(0.0f, Y, (BoardTop + BoardBottom) * 0.5f), Facing, 2900.0f, BoardTop - BoardBottom);
			WallLines.Add(Display->AddWallLine(FVector(0.0f, Y, WallBannerHeight), Facing, 200.0f));
			for (int32 Row = 0; Row < WallRows; ++Row)
			{
				WallLines.Add(Display->AddWallLine(FVector(0.0f, Y, WallFirstRowHeight - Row * WallRowSpacing), Facing, 120.0f));
			}
		}
	}
}

float ARaceGameMode::Now() const
{
	return GetWorld()->GetTimeSeconds();
}

void ARaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Igloo spawns its cameras during its own BeginPlay and may recreate them (e.g. on a resolution change).
	IglooScanTimer -= DeltaSeconds;
	if (IglooScanTimer <= 0.0f)
	{
		IglooScanTimer = 2.0f;
		RefreshIglooCameras();
	}

	// Re-assert every frame in case Igloo's blueprint turns per-frame capture back on.
	for (const TWeakObjectPtr<USceneCaptureComponent2D>& Capture : DisabledCeilingCaptures)
	{
		if (Capture.IsValid())
		{
			Capture->bCaptureEveryFrame = false;
			Capture->bCaptureOnMovement = false;
		}
	}

	UpdateRacePhase(DeltaSeconds);
	UpdateLapsAndPositions();
	UpdateDrafting();
	DriveComputerCars(DeltaSeconds);

	DisplayRefreshTimer -= DeltaSeconds;
	if (DisplayRefreshTimer <= 0.0f)
	{
		DisplayRefreshTimer = 0.1f;
		RefreshDisplays();
	}

	UpdateAudioRecording(DeltaSeconds);
	UpdateSpeakerTest(DeltaSeconds);
	UpdateCaptures(DeltaSeconds);

	if (!bIglooReported && CVarRaceIglooCameraReport.GetValueOnGameThread() > 0)
	{
		IglooReportTimer += DeltaSeconds;
		if (IglooReportTimer >= 8.0f)
		{
			bIglooReported = true;
			ReportIglooCameras();
		}
	}
}

void ARaceGameMode::UpdateRacePhase(float DeltaSeconds)
{
	PhaseTime += DeltaSeconds;

	switch (Phase)
	{
	case ERacePhase::GetReady:
		if (PhaseTime >= GetReadySeconds)
		{
			EnterPhase(ERacePhase::Lights);
		}
		break;

	case ERacePhase::Lights:
		if (StartLights)
		{
			const int32 Lit = FMath::Min(FMath::FloorToInt(PhaseTime / LightInterval), ARaceStartLights::NumLights);
			if (StartLights->SetLitCount(Lit) && Lit > 0)
			{
				PlaySignal(LightBeepHz, LightBeepSeconds);
			}
		}
		if (PhaseTime >= LightInterval * ARaceStartLights::NumLights + LightsHoldTime)
		{
			EnterPhase(ERacePhase::Racing);
		}
		break;

	case ERacePhase::Racing:
		break;

	case ERacePhase::Finishing:
	{
		bool bEveryoneDone = true;
		for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
		{
			bEveryoneDone &= Stats[Slot].bFinished;
		}
		if (bEveryoneDone || PhaseTime >= FinishGraceSeconds)
		{
			EnterPhase(ERacePhase::Results);
		}
		break;
	}

	case ERacePhase::Results:
		if (PhaseTime >= ResultsSeconds)
		{
			EnterPhase(ERacePhase::GetReady);
		}
		break;
	}
}

void ARaceGameMode::EnterPhase(ERacePhase NewPhase)
{
	Phase = NewPhase;
	PhaseTime = 0.0f;
	UE_LOG(LogRace, Log, TEXT("race.Phase %s"), PhaseName(NewPhase));

	switch (NewPhase)
	{
	case ERacePhase::GetReady:
		RandomiseCarModels();
		for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
		{
			Cars[Slot]->PlaceOnGrid(GridSlots.IsValidIndex(Slot) ? GridSlots[Slot] : FTransform::Identity);
			Cars[Slot]->SetControlsLocked(true);
			Stats[Slot] = FRaceCarStats();
			ComputerDrivers[Slot] = FComputerDriverState();
			ComputerDrivers[Slot].Skill = FMath::FRandRange(DriverSkillMin, 1.0f);
			ComputerDrivers[Slot].NextMistakeIn = FMath::FRandRange(3.0f, 9.0f);
			ComputerDrivers[Slot].WobblePhase = FMath::FRandRange(0.0f, 100.0f);
		}
		if (StartLights) { StartLights->SetLitCount(0); }
		break;

	case ERacePhase::Lights:
		LightsHoldTime = FMath::FRandRange(LightsHoldMin, LightsHoldMax);
		break;

	case ERacePhase::Racing:
		FinishCount = 0;
		WinnerSlot = INDEX_NONE;
		bHavePreviousRaceDistance = false;
		for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
		{
			// Lap 1 is timed from lights out; the grid sits behind the line, so the first crossing isn't a lap.
			Stats[Slot] = FRaceCarStats();
			Stats[Slot].Lap = 1;
			Stats[Slot].LapStartTime = Now();
			Cars[Slot]->SetControlsLocked(false);
		}
		if (StartLights) { StartLights->SetLitCount(0); }
		PlaySignal(GoBeepHz, GoBeepSeconds);
		break;

	case ERacePhase::Finishing:
		break;

	case ERacePhase::Results:
		for (const TObjectPtr<ARaceCarPawn>& Car : Cars)
		{
			Car->SetControlsLocked(true);
		}
		break;
	}
	RefreshDisplays();
}

void ARaceGameMode::UpdateLapsAndPositions()
{
	const float LapLength = TrackPath.GetLapLength();
	const int32 RaceLaps = GetDefault<URaceInputSettings>()->RaceLaps;
	const bool bTiming = Phase == ERacePhase::Racing || Phase == ERacePhase::Finishing;
	const float Time = Now();

	TArray<int32> Order;
	for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
	{
		ARaceCarPawn* Car = Cars[Slot];
		FRaceCarStats& S = Stats[Slot];
		const FVector Location = Car->GetActorLocation();
		const float P = TrackPath.WrapDistance(TrackPath.GetDistanceAlong(FVector2D(Location.X, Location.Y)) - TrackPath.GetStartLineDistance());

		if (bTiming && S.bHasProgress && !S.bFinished)
		{
			if (P > Sector1Start * LapLength && P < Sector1End * LapLength)
			{
				S.Checkpoints |= 1;
			}
			if ((S.Checkpoints & 1) && P > Sector2Start * LapLength && P < Sector2End * LapLength)
			{
				S.Checkpoints |= 2;
			}

			const bool bCrossedLine = S.Progress > 0.8f * LapLength && P < 0.2f * LapLength;
			if (bCrossedLine)
			{
				if (S.Checkpoints == 3)
				{
					S.LastLapTime = Time - S.LapStartTime;
					S.BestLapTime = S.BestLapTime > 0.0f ? FMath::Min(S.BestLapTime, S.LastLapTime) : S.LastLapTime;
					S.LapStartTime = Time;
					UE_LOG(LogRace, Log, TEXT("race.Lap car %d (%s) completed lap %d in %.2f s (best %.2f)"),
						Slot + 1, Car->IsPlayerControlled() ? TEXT("player") : TEXT("cpu"), S.Lap, S.LastLapTime, S.BestLapTime);
					if (S.Lap >= RaceLaps)
					{
						S.bFinished = true;
						S.FinishPosition = ++FinishCount;
						UE_LOG(LogRace, Log, TEXT("race.Finish car %d position %d"), Slot + 1, S.FinishPosition);
						if (WinnerSlot == INDEX_NONE)
						{
							WinnerSlot = Slot;
						}
						if (Phase == ERacePhase::Racing)
						{
							EnterPhase(ERacePhase::Finishing);
						}
					}
					else
					{
						++S.Lap;
					}
				}
				S.Checkpoints = 0;
			}
		}

		S.Progress = P;
		S.bHasProgress = true;
		// Still behind the line (on the grid) counts as negative progress rather than nearly a full lap.
		const float Along = (S.Checkpoints == 0 && P > 0.8f * LapLength) ? P - LapLength : P;
		S.RaceDistance = S.bFinished ? 1.0e9f - S.FinishPosition : FMath::Max(S.Lap - 1, 0) * LapLength + Along;
		Order.Add(Slot);
	}

	Order.Sort([this](int32 A, int32 B) { return Stats[A].RaceDistance > Stats[B].RaceDistance; });
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		Stats[Order[Index]].Position = Index + 1;
	}

	// Log overtakes between racing cars (used to check the slipstream works).
	if (bTiming && bHavePreviousRaceDistance)
	{
		for (int32 A : Order)
		{
			for (int32 B : Order)
			{
				if (A != B && !Stats[A].bFinished && !Stats[B].bFinished &&
					PreviousRaceDistance[A] < PreviousRaceDistance[B] && Stats[A].RaceDistance > Stats[B].RaceDistance)
				{
					UE_LOG(LogRace, Log, TEXT("race.Overtake car %d passes car %d on lap %d (passer draft %.2f, speed %.0f vs %.0f)"),
						A + 1, B + 1, Stats[A].Lap, Cars[A]->GetDraftFactor(), Cars[A]->GetSpeed(), Cars[B]->GetSpeed());
				}
			}
		}
	}
	for (int32 Slot = 0; Slot < Stats.Num(); ++Slot)
	{
		PreviousRaceDistance[Slot] = Stats[Slot].RaceDistance;
	}
	bHavePreviousRaceDistance = bTiming;
}

void ARaceGameMode::UpdateDrafting()
{
	const int32 Override = CVarRaceDraftEnabled.GetValueOnGameThread();
	const bool bDraftingOn = Override >= 0 ? Override > 0 : GetDefault<URaceInputSettings>()->bDraftingEnabled;

	for (const TObjectPtr<ARaceCarPawn>& Follower : Cars)
	{
		float Strength = 0.0f;
		if (bDraftingOn && !Follower->AreControlsLocked())
		{
			const FVector FollowerForward = Follower->GetActorForwardVector().GetSafeNormal2D();
			const FVector FollowerRight(-FollowerForward.Y, FollowerForward.X, 0.0f);
			for (const TObjectPtr<ARaceCarPawn>& Leader : Cars)
			{
				if (Leader == Follower)
				{
					continue;
				}
				const FVector ToLeader = (Leader->GetActorLocation() - Follower->GetActorLocation()) * FVector(1.0f, 1.0f, 0.0f);
				const float Along = FVector::DotProduct(ToLeader, FollowerForward);
				const float Sideways = FMath::Abs(FVector::DotProduct(ToLeader, FollowerRight));
				const float Heading = FVector::DotProduct(FollowerForward, Leader->GetActorForwardVector().GetSafeNormal2D());
				if (Along > DraftMinGap && Along < DraftRange && Sideways < DraftWidth && Heading > DraftMinHeadingDot &&
					Leader->GetSpeed() > DraftMinLeaderSpeed)
				{
					Strength = FMath::Max(Strength, 1.0f - (Along - DraftMinGap) / (DraftRange - DraftMinGap));
				}
			}
		}
		Follower->SetDraftTarget(Strength);
	}
}

void ARaceGameMode::DriveComputerCars(float DeltaSeconds)
{
	for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
	{
		if (!Cars[Slot]->IsPlayerControlled())
		{
			float Throttle = 0.0f;
			float Brake = 0.0f;
			float Steer = 0.0f;
			bool bHandbrake = false;
			ComputeComputerDriverInput(Slot, DeltaSeconds, Throttle, Brake, Steer, bHandbrake);
			Cars[Slot]->SetDriveInput(Throttle, Brake, Steer, bHandbrake);
		}
	}
}

void ARaceGameMode::ComputeComputerDriverInput(int32 SlotIndex, float DeltaTime, float& OutThrottle, float& OutBrake, float& OutSteer, bool& bOutHandbrake)
{
	OutThrottle = 0.0f;
	OutBrake = 0.0f;
	OutSteer = 0.0f;
	bOutHandbrake = false;
	if (!Cars.IsValidIndex(SlotIndex) || !ComputerDrivers.IsValidIndex(SlotIndex))
	{
		return;
	}
	const ARaceCarPawn* Car = Cars[SlotIndex];
	FComputerDriverState& Driver = ComputerDrivers[SlotIndex];

	if (Car->AreControlsLocked())
	{
		OutThrottle = 0.6f; // rev on the grid
		return;
	}

	const FVector Location3D = Car->GetActorLocation();
	const FVector2D Location(Location3D.X, Location3D.Y);
	const FVector2D Forward = FVector2D(Car->GetActorForwardVector().X, Car->GetActorForwardVector().Y).GetSafeNormal();
	const float Along = TrackPath.GetDistanceAlong(Location);
	const float Speed = Car->GetSpeed();

	auto AngleTo = [&Location, &Forward](const FVector2D& Target)
	{
		const FVector2D ToTarget = (Target - Location).GetSafeNormal();
		return float(FMath::RadiansToDegrees(FMath::Atan2(Forward.X * ToTarget.Y - Forward.Y * ToTarget.X, Forward | ToTarget)));
	};

	FVector2D Direction;
	FVector2D Aim = TrackPath.GetPointAtDistance(Along + 350.0f + Speed * 0.3f, &Direction);

	// Backing out of a wall or pile-up: reverse, turning the nose back towards the track.
	if (Driver.ReverseTime > 0.0f)
	{
		Driver.ReverseTime -= DeltaTime;
		OutBrake = 1.0f;
		OutSteer = Driver.ReverseSteer;
		return;
	}
	Driver.StuckTime = Speed < DriverStuckSpeed ? Driver.StuckTime + DeltaTime : 0.0f;
	if (Driver.StuckTime > DriverStuckSeconds)
	{
		Driver.StuckTime = 0.0f;
		Driver.ReverseTime = DriverReverseSeconds;
		// Reversing flips the steering sense, so steer away from the aim to swing the nose towards it.
		const float AimAngle = AngleTo(Aim);
		Driver.ReverseSteer = FMath::Abs(AimAngle) < 5.0f ? 1.0f : -FMath::Sign(AimAngle);
		UE_LOG(LogRace, Log, TEXT("race.Driver car %d stuck, backing out"), SlotIndex + 1);
		return;
	}

	// Mistakes: every so often (more often for weaker drivers) the driver fluffs something for a moment.
	if (Driver.MistakeTimeLeft > 0.0f)
	{
		Driver.MistakeTimeLeft -= DeltaTime;
		if (Driver.MistakeTimeLeft <= 0.0f)
		{
			Driver.Mistake = EDriverMistake::None;
		}
	}
	else if ((Driver.NextMistakeIn -= DeltaTime) <= 0.0f && Speed > 500.0f)
	{
		Driver.Mistake = static_cast<EDriverMistake>(FMath::RandRange(1, 4));
		Driver.MistakeTimeLeft = FMath::FRandRange(0.6f, 1.4f);
		const float SkillAlpha = (Driver.Skill - DriverSkillMin) / (1.0f - DriverSkillMin);
		Driver.NextMistakeIn = FMath::Lerp(DriverMistakeGapMin, DriverMistakeGapMax, SkillAlpha) * FMath::FRandRange(0.7f, 1.3f);
		UE_LOG(LogRace, Log, TEXT("race.Mistake car %d: %s"), SlotIndex + 1, MistakeName(Driver.Mistake));
	}

	// Which way the next corner turns (positive = right), used for running wide.
	const float CornerAngleSigned = AngleTo(TrackPath.GetPointAtDistance(Along + 900.0f + Speed * 0.45f));
	const float CornerAngle = FMath::Abs(CornerAngleSigned);

	// Deep in a slipstream means tucked right behind someone: move across to pass (even slots right, odd left).
	const float PassSide = (SlotIndex % 2 == 0) ? 1.0f : -1.0f;
	float TargetLane = Car->GetDraftFactor() > DriverPassDraft ? PassSide * DriverPassLane : 0.0f;
	if (Driver.Mistake == EDriverMistake::RunWide)
	{
		TargetLane = -FMath::Sign(CornerAngleSigned) * 320.0f; // outside of the corner
	}
	Driver.Lane = FMath::FInterpTo(Driver.Lane, TargetLane, DeltaTime, 2.0f);
	Aim += FVector2D(-Direction.Y, Direction.X) * Driver.Lane;

	// Imperfect hands: a slow wobble that grows as skill drops.
	Driver.WobblePhase += DeltaTime;
	const float SkillAlpha = (Driver.Skill - DriverSkillMin) / (1.0f - DriverSkillMin);
	const float Wobble = DriverWobbleAtWorstSkill * (1.0f - SkillAlpha) *
		(FMath::Sin(Driver.WobblePhase * 1.7f) * 0.6f + FMath::Sin(Driver.WobblePhase * 4.3f + 1.3f) * 0.4f);
	OutSteer = FMath::Clamp(AngleTo(Aim) / 25.0f + Wobble, -1.0f, 1.0f);

	// Look further ahead to judge the next corner and slow down for it; skill scales corner and top speed.
	float CornerSpeed = (CornerAngle > 55.0f ? 850.0f : (CornerAngle > 30.0f ? 1250.0f : TNumericLimits<float>::Max())) * Driver.Skill;
	if (Driver.Mistake == EDriverMistake::LateBraking)
	{
		CornerSpeed *= 1.45f;
	}
	const float TopSpeed = Car->MaxSpeed * (1.0f + Car->DraftTopSpeedBonus * Car->GetDraftFactor()) * Driver.Skill;
	OutThrottle = (Speed < CornerSpeed && Speed < TopSpeed) ? 1.0f : 0.0f;
	OutBrake = Speed > CornerSpeed + 250.0f ? 0.7f : 0.0f;

	if (Driver.Mistake == EDriverMistake::Hesitation)
	{
		OutThrottle *= 0.35f;
	}
	else if (Driver.Mistake == EDriverMistake::Oversteer && CornerAngle > 25.0f && Speed > 600.0f)
	{
		bOutHandbrake = Driver.MistakeTimeLeft > 0.8f; // a short snap of the handbrake, then catch it
		OutSteer = FMath::Clamp(OutSteer * 1.5f, -1.0f, 1.0f);
	}
}

void ARaceGameMode::RefreshDisplays()
{
	if (!Display)
	{
		return;
	}
	const int32 RaceLaps = GetDefault<URaceInputSettings>()->RaceLaps;
	const float Time = Now();
	const bool bRaceRunning = Phase == ERacePhase::Racing || Phase == ERacePhase::Finishing || Phase == ERacePhase::Results;

	TArray<int32> Order;
	for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
	{
		Order.Add(Slot);
	}
	Order.Sort([this](int32 A, int32 B) { return Stats[A].Position < Stats[B].Position; });

	// Floor panels, one per player corner.
	for (int32 Slot = 0; Slot < Cars.Num() && PanelLines.IsValidIndex((Slot + 1) * PanelLinesPerSlot - 1); ++Slot)
	{
		const ARaceCarPawn* Car = Cars[Slot];
		const FRaceCarStats& S = Stats[Slot];
		const FColor Color = SlotColor(Slot);
		const bool bPlayer = Car->IsPlayerControlled();
		FString Lines[PanelLinesPerSlot];

		const FString Position = bRaceRunning ? FString::Printf(TEXT("  P%d"), S.Position) : FString();
		Lines[0] = bPlayer ? FString::Printf(TEXT("PLAYER %d%s"), Slot + 1, *Position) : FString::Printf(TEXT("CAR %d  CPU%s"), Slot + 1, *Position);

		if (!bPlayer)
		{
			Lines[1] = TEXT("PRESS ANY BUTTON");
			Lines[2] = TEXT("TO DRIVE");
		}
		else if (!bRaceRunning)
		{
			Lines[1] = TEXT("ON THE GRID");
			Lines[2] = FString::Printf(TEXT("%d LAPS"), RaceLaps);
		}
		else if (S.bFinished)
		{
			Lines[1] = FString::Printf(TEXT("FINISHED P%d"), S.FinishPosition);
			Lines[2] = FString::Printf(TEXT("BEST %s"), *FormatTime(S.BestLapTime));
		}
		else
		{
			const float LapTime = Phase == ERacePhase::Results ? S.LastLapTime : Time - S.LapStartTime;
			Lines[1] = FString::Printf(TEXT("LAP %d/%d   %s"), S.Lap, RaceLaps, *FormatTime(LapTime));
			Lines[2] = FString::Printf(TEXT("BEST %s%s"), *FormatTime(S.BestLapTime), Car->GetDraftFactor() > 0.35f ? TEXT("   DRAFT") : TEXT(""));
		}

		for (int32 Line = 0; Line < PanelLinesPerSlot; ++Line)
		{
			const FColor LineColor = Line == 0 ? (bPlayer ? Color : Dimmed(Color)) : (bPlayer ? FColor::White : FColor(150, 150, 150));
			Display->SetLine(PanelLines[Slot * PanelLinesPerSlot + Line], Lines[Line], LineColor);
		}
	}

	// Wall banner and leaderboard.
	FString Banner;
	FColor BannerColor = FColor::White;
	switch (Phase)
	{
	case ERacePhase::GetReady:
		// Also flag a wrong audio device to whoever is running the room.
		Banner = AudioChannelCount > 0 && AudioChannelCount < MinRoomAudioChannels
			? FString::Printf(TEXT("GET READY   (AUDIO: %d CH)"), AudioChannelCount)
			: FString(TEXT("GET READY"));
		break;
	case ERacePhase::Lights: Banner = TEXT(""); break;
	case ERacePhase::Racing:
		if (PhaseTime < GoBannerSeconds)
		{
			Banner = TEXT("GO!");
			BannerColor = FColor(90, 255, 90);
		}
		else if (Order.Num() > 0)
		{
			Banner = FString::Printf(TEXT("LAP %d / %d"), FMath::Clamp(Stats[Order[0]].Lap, 1, RaceLaps), RaceLaps);
		}
		break;
	case ERacePhase::Finishing:
	case ERacePhase::Results:
		if (WinnerSlot != INDEX_NONE)
		{
			Banner = Cars[WinnerSlot]->IsPlayerControlled()
				? FString::Printf(TEXT("PLAYER %d WINS!"), WinnerSlot + 1)
				: FString::Printf(TEXT("CAR %d WINS!"), WinnerSlot + 1);
			BannerColor = SlotColor(WinnerSlot);
		}
		break;
	}

	if (!DiagnosticBanner.IsEmpty())
	{
		Banner = DiagnosticBanner;
		BannerColor = FColor::White;
	}

	const int32 LinesPerWall = 1 + WallRows;
	for (int32 Wall = 0; (Wall + 1) * LinesPerWall <= WallLines.Num(); ++Wall)
	{
		const int32 Base = Wall * LinesPerWall;
		Display->SetLine(WallLines[Base], Banner, BannerColor);
		for (int32 Row = 0; Row < WallRows; ++Row)
		{
			FString RowText;
			FColor RowColor = FColor::White;
			if (Order.IsValidIndex(Row))
			{
				const int32 Slot = Order[Row];
				const FRaceCarStats& S = Stats[Slot];
				const bool bPlayer = Cars[Slot]->IsPlayerControlled();
				const FString Name = bPlayer ? FString::Printf(TEXT("PLAYER %d"), Slot + 1) : FString::Printf(TEXT("CAR %d (CPU)"), Slot + 1);
				RowColor = SlotColor(Slot);
				if (!bRaceRunning)
				{
					RowText = FString::Printf(TEXT("%s  ON THE GRID"), *Name);
				}
				else if (S.bFinished)
				{
					RowText = FString::Printf(TEXT("%d.  %s   FINISHED   BEST %s"), S.Position, *Name, *FormatTime(S.BestLapTime));
				}
				else
				{
					RowText = FString::Printf(TEXT("%d.  %s   LAP %d/%d   BEST %s"), S.Position, *Name, S.Lap, RaceLaps, *FormatTime(S.BestLapTime));
				}
			}
			Display->SetLine(WallLines[Base + 1 + Row], RowText, RowColor);
		}
	}
}

void ARaceGameMode::UpdateCaptures(float DeltaSeconds)
{
	CaptureClock += DeltaSeconds;
	if (!bCaptureTimesParsed)
	{
		if (CaptureClock < 1.0f)
		{
			return; // let -ExecCmds set the cvar first
		}
		bCaptureTimesParsed = true;
		TArray<FString> Parts;
		CVarRaceCaptureAt.GetValueOnGameThread().ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			const float Seconds = FCString::Atof(*Part.TrimStartAndEnd());
			if (Seconds > 0.0f)
			{
				CaptureTimes.Add(Seconds);
			}
		}
		CaptureTimes.Sort();
	}

	while (CaptureTimes.IsValidIndex(NextCaptureIndex) && CaptureClock >= CaptureTimes[NextCaptureIndex])
	{
		const FString Prefix = FString::Printf(TEXT("t%03d"), FMath::RoundToInt(CaptureTimes[NextCaptureIndex]));
		ExportIglooCameras(Prefix);
		UKismetSystemLibrary::ExecuteConsoleCommand(this, FString::Printf(TEXT("HighResShot 1600x900 filename=RaceDesktop_%s"), *Prefix));
		UE_LOG(LogRace, Log, TEXT("race.CaptureAt %s: phase %s"), *Prefix, PhaseName(Phase));
		++NextCaptureIndex;
	}
}

void ARaceGameMode::ExportIglooCameras(const FString& Prefix)
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("RaceCaptures");
	for (const TWeakObjectPtr<USceneCaptureComponent2D>& Weak : KnownIglooCaptures)
	{
		USceneCaptureComponent2D* Capture = Weak.Get();
		UTextureRenderTarget2D* Target = Capture ? Capture->TextureTarget.Get() : nullptr;
		if (!Target)
		{
			continue;
		}

		// Name by direction in the room (front wall = +Y; facing it, left = +X).
		const FVector Forward = Capture->GetForwardVector();
		FString Name;
		if (Forward.Z > CeilingForwardZ) { continue; }
		else if (Forward.Z < -CeilingForwardZ) { Name = TEXT("floor"); }
		else if (Forward.Y > 0.7f) { Name = TEXT("front"); }
		else if (Forward.Y < -0.7f) { Name = TEXT("back"); }
		else if (Forward.X > 0.7f) { Name = TEXT("left"); }
		else { Name = TEXT("right"); }

		FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags Flags(RCM_UNorm, CubeFace_MAX);
		Flags.SetLinearToGamma(true);
		if (!Resource || !Resource->ReadPixels(Pixels, Flags) || Pixels.Num() != Target->SizeX * Target->SizeY)
		{
			continue;
		}
		for (FColor& Pixel : Pixels)
		{
			Pixel.A = 255;
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const FString Path = Directory / FString::Printf(TEXT("%s_%s.png"), *Prefix, *Name);
		if (FFileHelper::SaveArrayToFile(Png, *Path))
		{
			UE_LOG(LogRace, Log, TEXT("race.CaptureAt saved %s"), *Path);
		}
	}
}

void ARaceGameMode::UpdateAudioRecording(float DeltaSeconds)
{
	const float RecordSeconds = CVarRaceRecordAudio.GetValueOnGameThread();
	if (RecordSeconds <= 0.0f || AudioRecordingState == EAudioRecordingState::Done)
	{
		return;
	}

	AudioRecordingTimer += DeltaSeconds;
	if (AudioRecordingState == EAudioRecordingState::Waiting)
	{
		if (AudioRecordingTimer >= 5.0f)
		{
			UAudioMixerBlueprintLibrary::StartRecordingOutput(this, RecordSeconds);
			AudioRecordingState = EAudioRecordingState::Recording;
			AudioRecordingTimer = 0.0f;
			AudioTraceTimer = 0.0f;
			UE_LOG(LogRace, Log, TEXT("race.RecordAudio: recording %.1f s, listener yaw %.0f"), RecordSeconds, GetDefault<URaceInputSettings>()->AudioFrontYaw);
		}
		return;
	}

	if (CVarRaceAudioSweep.GetValueOnGameThread() > 0 && Cars.Num() > 0)
	{
		// Car 1 on an 18 m circle round the listener, 45 degree steps relative to the room's front.
		const int32 Step = FMath::FloorToInt(AudioRecordingTimer / 1.5f);
		const float WorldAngle = FMath::DegreesToRadians(GetDefault<URaceInputSettings>()->AudioFrontYaw + 45.0f * Step);
		const FVector Current = Cars[0]->GetActorLocation();
		Cars[0]->SetActorLocation(FVector(1800.0f * FMath::Cos(WorldAngle), 1800.0f * FMath::Sin(WorldAngle), Current.Z), false, nullptr, ETeleportType::TeleportPhysics);
	}

	AudioTraceTimer += DeltaSeconds;
	if (AudioTraceTimer >= 0.1f)
	{
		AudioTraceTimer = 0.0f;
		for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
		{
			const FVector Location = Cars[Slot]->GetActorLocation();
			const URaceEngineSynth* Voice = Cars[Slot]->EngineSound;
			UE_LOG(LogRace, Log, TEXT("race.AudioTrace t=%.2f slot %d x %.0f y %.0f playing %d level %.3f"), AudioRecordingTimer, Slot, Location.X, Location.Y,
				Voice && Voice->IsPlaying() ? 1 : 0, Voice ? Voice->GetRecentLevel() : 0.0f);
		}
	}

	if (AudioRecordingTimer >= RecordSeconds)
	{
		UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile, TEXT("RaceAudio"), TEXT(""));
		AudioRecordingState = EAudioRecordingState::Done;
		UE_LOG(LogRace, Log, TEXT("race.RecordAudio: saved Saved/BouncedWavFiles/RaceAudio.wav"));
	}
}

void ARaceGameMode::RefreshIglooCameras()
{
	const bool bRenderCeiling = GetDefault<URaceInputSettings>()->bRenderIglooCeilingCamera;
	bool bFoundNew = false;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->GetClass()->GetName().StartsWith(TEXT("IglooPanoCamera")))
		{
			continue;
		}
		USceneCaptureComponent2D* Capture = It->FindComponentByClass<USceneCaptureComponent2D>();
		if (!Capture || KnownIglooCaptures.Contains(Capture))
		{
			continue;
		}
		KnownIglooCaptures.Add(Capture);
		bFoundNew = true;

		const FVector Forward = Capture->GetForwardVector();
		const bool bCeiling = Forward.Z > CeilingForwardZ;
		UE_LOG(LogRace, Log, TEXT("Igloo camera %s looks (%.2f, %.2f, %.2f), capture every frame %d%s"),
			*It->GetName(), Forward.X, Forward.Y, Forward.Z, Capture->bCaptureEveryFrame ? 1 : 0,
			bCeiling ? (bRenderCeiling ? TEXT(" [ceiling, rendering]") : TEXT(" [ceiling, disabled]")) : TEXT(""));

		if (bCeiling && !bRenderCeiling)
		{
			DisableCapture(Capture);
			DisabledCeilingCaptures.Add(Capture);
		}
	}

	if (bFoundNew && DisabledCeilingCaptures.Num() == 0 && !bRenderCeiling && !bWarnedNoCeilingCamera)
	{
		bWarnedNoCeilingCamera = true;
		UE_LOG(LogRace, Warning, TEXT("No upward-facing Igloo camera found among %d; nothing to disable."), KnownIglooCaptures.Num());
	}
}

void ARaceGameMode::DisableCapture(USceneCaptureComponent2D* Capture)
{
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;

	// If Igloo still triggers captures manually, render nothing: no primitives, no sky or fog.
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Reset();
	Capture->ShowOnlyComponents.Reset();
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetCloud(false);

	if (Capture->TextureTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, Capture->TextureTarget, FLinearColor::Black);
	}
}

void ARaceGameMode::ReportIglooCameras()
{
	for (const TWeakObjectPtr<USceneCaptureComponent2D>& Weak : KnownIglooCaptures)
	{
		USceneCaptureComponent2D* Capture = Weak.Get();
		if (!Capture)
		{
			continue;
		}
		const FVector Forward = Capture->GetForwardVector();
		UTextureRenderTarget2D* Target = Capture->TextureTarget;
		if (!Target)
		{
			UE_LOG(LogRace, Log, TEXT("Igloo camera report: looks (%.2f, %.2f, %.2f), no render target"), Forward.X, Forward.Y, Forward.Z);
			continue;
		}

		// Average a 5 x 5 grid of pixels (enough to tell a black tile from a rendered one).
		FLinearColor Sum = FLinearColor::Black;
		const int32 Grid = 5;
		for (int32 GX = 0; GX < Grid; ++GX)
		{
			for (int32 GY = 0; GY < Grid; ++GY)
			{
				const int32 X = (Target->SizeX - 1) * (GX + 0.5f) / Grid;
				const int32 Y = (Target->SizeY - 1) * (GY + 0.5f) / Grid;
				Sum += FLinearColor(UKismetRenderingLibrary::ReadRenderTargetPixel(this, Target, X, Y));
			}
		}
		Sum /= float(Grid * Grid);
		UE_LOG(LogRace, Log, TEXT("Igloo camera report: looks (%.2f, %.2f, %.2f), target %dx%d, capture every frame %d, mean colour (%.2f, %.2f, %.2f)"),
			Forward.X, Forward.Y, Forward.Z, Target->SizeX, Target->SizeY, Capture->bCaptureEveryFrame ? 1 : 0, Sum.R, Sum.G, Sum.B);
	}
}

void ARaceGameMode::EnsureCars()
{
	if (Cars.Num() > 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
	{
		const FTransform SlotTransform = GridSlots.IsValidIndex(Slot) ? GridSlots[Slot] : FTransform::Identity;
		ARaceCarPawn* Car = GetWorld()->SpawnActor<ARaceCarPawn>(ARaceCarPawn::StaticClass(), SlotTransform, Params);
		if (!Car)
		{
			continue;
		}
		if (CarMeshes.Num() > 0)
		{
			Car->SetCarMesh(CarMeshes[Slot % CarMeshes.Num()].LoadSynchronous());
		}
		Car->SetEngineVoice(Slot);
		Car->SetPlayerIdentity(Slot, SlotColor(Slot));
		Car->Activate(SlotTransform);
		Cars.Add(Car);
	}
	Stats.SetNum(Cars.Num());
	ComputerDrivers.SetNum(Cars.Num());
	PreviousRaceDistance.SetNumZeroed(Cars.Num());
}

void ARaceGameMode::RandomiseCarModels()
{
	if (CarMeshes.Num() == 0)
	{
		return;
	}
	TArray<int32> Picks;
	for (int32 Index = 0; Index < CarMeshes.Num(); ++Index)
	{
		Picks.Add(Index);
	}
	// Shuffle, then deal one model per car (models repeat only if there are more cars than models).
	for (int32 Index = Picks.Num() - 1; Index > 0; --Index)
	{
		Picks.Swap(Index, FMath::RandRange(0, Index));
	}
	TArray<FString> Chosen;
	for (int32 Slot = 0; Slot < Cars.Num(); ++Slot)
	{
		const TSoftObjectPtr<UStaticMesh>& Model = CarMeshes[Picks[Slot % Picks.Num()]];
		if (UStaticMesh* Mesh = Model.LoadSynchronous())
		{
			Cars[Slot]->SetCarMesh(Mesh);
			Chosen.Add(FString::Printf(TEXT("car %d %s"), Slot + 1, *Mesh->GetName()));
		}
	}
	UE_LOG(LogRace, Log, TEXT("race.Cars %s"), *FString::Join(Chosen, TEXT(", ")));
}

ARaceCarPawn* ARaceGameMode::GetCarForSlot(int32 SlotIndex)
{
	EnsureCars();
	return Cars.IsValidIndex(SlotIndex) ? Cars[SlotIndex].Get() : nullptr;
}

void ARaceGameMode::JoinRace(ARacePlayerController* Player)
{
	const int32 Slot = Player ? Player->GetSlotIndex() : INDEX_NONE;
	ARaceCarPawn* Car = GetCarForSlot(Slot);
	if (!Car || Car->IsPlayerControlled())
	{
		return;
	}
	Car->SetPlayerControlled(true);
	UE_LOG(LogRace, Log, TEXT("race.Join player %d takes car %d (%s)."), Slot + 1, Slot + 1, PhaseName(Phase));
	RefreshDisplays();
}

void ARaceGameMode::ReleaseCar(int32 SlotIndex)
{
	ARaceCarPawn* Car = GetCarForSlot(SlotIndex);
	if (!Car || !Car->IsPlayerControlled())
	{
		return;
	}
	Car->SetPlayerControlled(false);
	if (ComputerDrivers.IsValidIndex(SlotIndex))
	{
		ComputerDrivers[SlotIndex].StuckTime = 0.0f;
		ComputerDrivers[SlotIndex].ReverseTime = 0.0f;
	}
	UE_LOG(LogRace, Log, TEXT("race.Release car %d goes back to the computer (player idle)."), SlotIndex + 1);
	RefreshDisplays();
}

AActor* ARaceGameMode::GetTrackCamera()
{
	if (TrackCamera)
	{
		return TrackCamera;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TrackCameraTag))
		{
			TrackCamera = *It;
			return TrackCamera;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
		FVector(0.0f, 0.0f, FallbackCameraHeight), FRotator(-90.0f, 0.0f, 0.0f), Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
		Camera->Tags.Add(TrackCameraTag);
	}
	TrackCamera = Camera;
	return TrackCamera;
}
