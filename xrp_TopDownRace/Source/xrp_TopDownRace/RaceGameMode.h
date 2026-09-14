#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RaceTrackPath.h"
#include "RaceGameMode.generated.h"

class ACameraActor;
class ARaceCarPawn;
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
	Finishing,  // a car has won; the others get a little time to finish
	Results,    // everyone stopped, winner shown, then back to GetReady
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

/** The computer driver's memory for one car. */
struct FComputerDriverState
{
	float Lane = 0.0f;           // sideways offset from the centreline (pulling out to overtake)
	float StuckTime = 0.0f;
	float ReverseTime = 0.0f;    // backing out of a wall / pile-up
	float ReverseSteer = 1.0f;
	float Skill = 1.0f;          // scales cornering and top speed, re-rolled each race
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

	UPROPERTY(EditAnywhere, Category = "Race")
	int32 MaxPlayers = 4;

	/** Car model per slot (wraps if there are fewer entries than players). */
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
	float Now() const;

	// Race flow
	void UpdateRacePhase(float DeltaSeconds);
	void EnterPhase(ERacePhase NewPhase);
	void UpdateLapsAndPositions();
	void UpdateDrafting();
	void DriveComputerCars(float DeltaSeconds);
	void RefreshDisplays();

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

	FRaceTrackPath TrackPath;

	ERacePhase Phase = ERacePhase::GetReady;
	float PhaseTime = 0.0f;
	float LightsHoldTime = 1.0f;
	int32 FinishCount = 0;
	int32 WinnerSlot = INDEX_NONE;
	TArray<FRaceCarStats> Stats;
	TArray<FComputerDriverState> ComputerDrivers;
	TArray<float> PreviousRaceDistance;
	bool bHavePreviousRaceDistance = false;

	UPROPERTY(Transient)
	TObjectPtr<ARaceStartLights> StartLights;

	UPROPERTY(Transient)
	TObjectPtr<ARaceDisplay> Display;

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
