#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RaceSettingsMenu.h"
#include "RaceTrackPath.h"
#include "RaceGameMode.generated.h"

class ACameraActor;
class ARaceCarPawn;
class ARaceCelebration;
class ARaceDisplay;
class ARacePlayerController;
class ARaceStartLights;
class USceneCaptureComponent2D;
class UStaticMesh;

/** Race flow. All four cars are always racing; players take cars over from the computer at any time. */
enum class ERacePhase : uint8
{
	GetReady,   // cars on the grid
	Lights,     // five red lights come on one by one, then go out at a random moment
	Racing,
	Results,    // ends the moment a car completes the last lap: cars roll to a stop, winner show, then back to GetReady
};

/** Lap timing and race position for one car. */
struct FRaceCarStats
{
	int32 Lap = 0;               // lap being driven (1-based); 0 before the race starts
	bool bFinished = false;
	int32 FinishPosition = 0;
	uint8 Checkpoints = 0;       // sector gates passed this lap (bits 0 and 1); a lap only counts with both
	float LapStartTime = 0.0f;
	float LastLapTime = 0.0f;
	float BestLapTime = 0.0f;
	float Progress = 0.0f;       // distance past the start line, 0..lap length
	bool bHasProgress = false;
	float RaceDistance = 0.0f;   // ordering key: completed laps x lap length + progress
	int32 Position = 0;
};

/** Ways a computer driver fluffs it for a moment. */
enum class EDriverMistake : uint8
{
	None,
	LateBraking,   // arrives at the corner too fast
	RunWide,       // drifts to the outside of the corner
	Oversteer,     // snaps the back out mid-corner
	Hesitation,    // lifts off the throttle
	Spin,          // loses the back end in a corner and goes right round (waits for a corner taken at speed)
};

/** The computer driver's memory for one car. */
struct FComputerDriverState
{
	float Lane = 0.0f;           // sideways offset from the centreline (pulling out to overtake)
	float StuckTime = 0.0f;
	float ReverseTime = 0.0f;    // backing out of a wall / pile-up
	float ReverseSteer = 1.0f;
	float Skill = 1.0f;          // pace (Race Settings CpuPaceMin..Max): scales cornering, top speed and mistake gaps; re-rolled each race
	EDriverMistake Mistake = EDriverMistake::None;
	float MistakeTimeLeft = 0.0f;
	bool bSpinPending = false;   // a spin is due at the next corner taken at speed
	float SpinYaw = 0.0f;        // degrees turned during the current spin (logged)
	float NextMistakeIn = 6.0f;
	float WobblePhase = 0.0f;
};

/**
 * 4-car local arcade race in the Igloo room. Spawns all four cars on the grid; cars without a player are
 * driven by the computer, and a player's button press takes their slot's car over. Runs the race (start
 * lights, laps, positions, slipstream) and the in-world displays. One local player per controller index,
 * no split screen.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARaceGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	ARaceCarPawn* GetCarForSlot(int32 SlotIndex);

	/** The player takes over their slot's car from the computer. */
	void JoinRace(ARacePlayerController* Player);

	/** The slot's car goes back to computer control. */
	void ReleaseCar(int32 SlotIndex);

	/** What the computer driver would do with this slot's car this frame. */
	void ComputeComputerDriverInput(int32 SlotIndex, float DeltaTime, float& OutThrottle, float& OutBrake, float& OutSteer, bool& bOutHandbrake);

	/** Level camera tagged TrackCameraTag, or a top-down camera spawned above the track centre. */
	AActor* GetTrackCamera();

	const FRaceTrackPath& GetTrackPath() const { return TrackPath; }

	/** Opens the settings menu for this player (Tab / controller View button), or closes it if it is theirs. */
	void ToggleSettingsMenu(int32 SlotIndex);
	bool IsSettingsMenuOpen() const { return SettingsMenu.IsOpen(); }
	int32 GetSettingsMenuOwner() const { return SettingsMenu.GetOwnerSlot(); }

	/** Menu navigation from the player who opened it: move the selection, change the value, or confirm. */
	void SettingsMenuInput(int32 SlotIndex, int32 Rows, int32 Steps, bool bConfirm);

	/** The menu text currently shown (also drawn on the desktop by ARaceHUD). */
	void GetSettingsMenuText(FString& OutTitle, TArray<FString>& OutRows, int32& OutSelectedRow, FString& OutHint) const;

	UPROPERTY(EditAnywhere, Category = "Race")
	int32 MaxPlayers = 4;

	/** Vehicle models to choose from; each race gives the cars different random picks from this list. */
	UPROPERTY(EditAnywhere, Category = "Race")
	TArray<TSoftObjectPtr<UStaticMesh>> CarMeshes;

	/** Grid slot per player, behind the start line (track_geom.START_LINE_X on the y = -1950 straight). */
	UPROPERTY(EditAnywhere, Category = "Race")
	TArray<FTransform> GridSlots;

	UPROPERTY(EditAnywhere, Category = "Race")
	FName TrackCameraTag = TEXT("TrackCamera");

	/** Height of the fallback top-down camera (UU). */
	UPROPERTY(EditAnywhere, Category = "Race")
	float FallbackCameraHeight = 5200.0f;

private:
	void EnsureCars();
	void SpawnRaceProps();

	/** Gives every car a different random model from CarMeshes (before placing them on the grid). */
	void RandomiseCarModels();
	float Now() const;

	// Race flow
	void UpdateRacePhase(float DeltaSeconds);
	void EnterPhase(ERacePhase NewPhase);
	void UpdateLapsAndPositions();
	void UpdateDrafting();
	void DriveComputerCars(float DeltaSeconds);
	void RefreshDisplays();

	/** "PLAYER n WINS!" / "CAR n WINS!" (empty before anyone has won). */
	FString GetWinnerText() const;

	/** Finds Igloo's capture cameras (spawned by the IglooManager at BeginPlay) and switches off the ceiling one. */
	void RefreshIglooCameras();
	void DisableCapture(USceneCaptureComponent2D* Capture);

	/** race.IglooCameraReport: log each Igloo camera's direction and sampled image brightness. */
	void ReportIglooCameras();

	/** race.CaptureAt: save the Igloo floor/wall camera images as PNGs plus a desktop screenshot at given times. */
	void UpdateCaptures(float DeltaSeconds);
	void ExportIglooCameras(const FString& Prefix);

	/** race.RecordAudio test aid: records the master mix and traces car positions for panning analysis. */
	void UpdateAudioRecording(float DeltaSeconds);

	/** Logs which audio device and how many output channels the game got (the room needs its multichannel device). */
	void LogAudioDevice();

	/** race.SpeakerTest: beep each output channel in turn, naming it on the wall banner. */
	void UpdateSpeakerTest(float DeltaSeconds);

	/** Start-light beep from every speaker. */
	void PlaySignal(float Frequency, float Seconds);

	/** Shows or hides the menu boards and updates their text. */
	void RefreshSettingsMenu();

	/** Pushes menu changes onto the cars already on track (handling values, engine volume). */
	void ApplySettingsToCars();

	/** race.MenuTest: open the menu, change a row, close it. */
	void UpdateMenuTest(float DeltaSeconds);

	FRaceSettingsMenu SettingsMenu;
	TArray<int32> MenuLines;    // per wall: title, VisibleRows rows, hint
	float MenuTestCloseTimer = 0.0f;

	FString AudioDeviceName;
	int32 AudioChannelCount = 0;
	int32 SpeakerTestChannel = INDEX_NONE;
	float SpeakerTestTimer = 0.0f;
	FString DiagnosticBanner;

	FRaceTrackPath TrackPath;

	ERacePhase Phase = ERacePhase::GetReady;
	float PhaseTime = 0.0f;
	float LightsHoldTime = 1.0f;
	int32 WinnerSlot = INDEX_NONE;
	int32 FanfareNote = 0;
	TArray<FRaceCarStats> Stats;
	TArray<FComputerDriverState> ComputerDrivers;
	TArray<float> PreviousRaceDistance;
	bool bHavePreviousRaceDistance = false;

	UPROPERTY(Transient)
	TObjectPtr<ARaceStartLights> StartLights;

	UPROPERTY(Transient)
	TObjectPtr<ARaceDisplay> Display;

	UPROPERTY(Transient)
	TObjectPtr<ARaceCelebration> Celebration;

	/** Settings menu boards on the front and back walls (hidden while the menu is closed). */
	UPROPERTY(Transient)
	TObjectPtr<ARaceDisplay> MenuDisplay;

	TArray<int32> PanelLines;   // PanelLinesPerSlot floor lines per player slot
	TArray<int32> WallLines;    // per wall: banner + one row per car
	float DisplayRefreshTimer = 0.0f;

	enum class EAudioRecordingState : uint8 { Waiting, Recording, Done };
	EAudioRecordingState AudioRecordingState = EAudioRecordingState::Waiting;
	float AudioRecordingTimer = 0.0f;
	float AudioTraceTimer = 0.0f;

	TArray<float> CaptureTimes;
	int32 NextCaptureIndex = 0;
	float CaptureClock = 0.0f;
	bool bCaptureTimesParsed = false;

	TArray<TWeakObjectPtr<USceneCaptureComponent2D>> KnownIglooCaptures;
	TArray<TWeakObjectPtr<USceneCaptureComponent2D>> DisabledCeilingCaptures;
	float IglooScanTimer = 1.0f;
	float IglooReportTimer = 0.0f;
	bool bIglooReported = false;
	bool bWarnedNoCeilingCamera = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ARaceCarPawn>> Cars;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TrackCamera;
};
