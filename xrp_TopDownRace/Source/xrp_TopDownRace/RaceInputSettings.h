#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RaceInputSettings.generated.h"

/** How many rows of cells random tracks are laid out on (the room fits 2 columns and up to 4 rows). */
UENUM()
enum class ERaceTrackGrid : uint8
{
	Mixed UMETA(DisplayName = "Mixed (2-4 rows)"),
	TwoRows UMETA(DisplayName = "2 rows"),
	ThreeRows UMETA(DisplayName = "3 rows"),
	FourRows UMETA(DisplayName = "4 rows"),
};

/**
 * Project Settings > Game > Race Settings.
 * Stored in Config/DefaultGame.ini under [/Script/xrp_TopDownRace.RaceInputSettings].
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Race Settings"))
class XRP_TOPDOWNRACE_API URaceInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URaceInputSettings();

	/** Game controllers drive the cars: controller N drives car N. Any button on an idle controller joins the race. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	bool bGamepadInputEnabled = true;

	/** Keyboard drives car 1 (W/S or Up/Down = throttle/brake, A/D or Left/Right = steer, Enter/Space joins). */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	bool bKeyboardInputEnabled = true;

	/** Laps in a race. The race ends as soon as the first car completes them. */
	UPROPERTY(Config, EditAnywhere, Category = "Race", meta = (ClampMin = "1", ClampMax = "50"))
	int32 RaceLaps = 5;

	/** A new random circuit (road, walls, fences and floor scenery) at the start of every race; off = always the original track. */
	UPROPERTY(Config, EditAnywhere, Category = "Race")
	bool bNewTrackEachRace = true;

	/** Random tracks usually get one or two narrow stretches on straights where not every car fits through side by side. */
	UPROPERTY(Config, EditAnywhere, Category = "Race")
	bool bNarrowTrackSections = true;

	/**
	 * Grid random tracks are made on: Mixed picks 2, 3 or 4 rows each race; 2 rows gives compact ovals and L-shapes,
	 * 4 rows the longest and twistiest layouts (up to 10 corners).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Race")
	ERaceTrackGrid TrackGrid = ERaceTrackGrid::Mixed;

	/** Slipstream: a car close behind another goes faster, so it can pull out and overtake. */
	UPROPERTY(Config, EditAnywhere, Category = "Race")
	bool bDraftingEnabled = true;

	/**
	 * Pace of the computer drivers. Each race every CPU car gets a random fraction of full pace (top speed and
	 * cornering speed) between CpuPaceMin and CpuPaceMax.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Computer Drivers", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float CpuPaceMin = 0.78f;

	UPROPERTY(Config, EditAnywhere, Category = "Computer Drivers", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float CpuPaceMax = 0.9f;

	/** Seconds between a CPU driver's mistakes: the slowest driver at the min, the fastest at the max (+-30% random). */
	UPROPERTY(Config, EditAnywhere, Category = "Computer Drivers", meta = (ClampMin = "0.5"))
	float CpuMistakeGapMin = 3.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Computer Drivers", meta = (ClampMin = "0.5"))
	float CpuMistakeGapMax = 7.0f;

	/** Share of mistakes that are a full spin in a corner (the others: braking late, running wide, a twitch, lifting off). */
	UPROPERTY(Config, EditAnywhere, Category = "Computer Drivers", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CpuSpinChance = 0.35f;

	/**
	 * While a player is racing, CPU cars ahead of the best-placed player ease off, down to CpuEaseOffPace of their
	 * pace when a sixth of a lap ahead, so a player who drives well can catch up and win.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Computer Drivers")
	bool bCpuEaseOffWhenAhead = true;

	UPROPERTY(Config, EditAnywhere, Category = "Computer Drivers", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float CpuEaseOffPace = 0.85f;

	/** Left-stick values below this are ignored. */
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float StickDeadZone = 0.2f;

	/** Seconds without any input before a player's car goes back to computer control (0 = never). */
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "0.0"))
	float IdleReleaseSeconds = 60.0f;

	/**
	 * Render resolution (%) of the desktop game window. The Igloo Manager's Spout cameras are scene captures
	 * and always render at their own full size, so lowering this frees GPU time for the room without
	 * affecting the projection. 100 = full-resolution desktop view.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "10.0", ClampMax = "100.0"))
	float DesktopViewScreenPercentage = 50.0f;

	/**
	 * Render the Igloo Manager's upward-facing (ceiling) capture camera. The room has no ceiling projector, so it
	 * is off by default: the camera stops capturing and its tile in the Spout feed stays black. The feed layout
	 * is unchanged, so Igloo Server's warping still lines up.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	bool bRenderIglooCeilingCamera = false;

	/**
	 * Which way the room's front speakers face, as a world yaw in degrees. The listener sits at the centre of
	 * the room floor looking this way, so a car in that direction plays from the front speakers.
	 * 0 = world +X, the top of the desktop TrackCamera view (the start line's left-hand wall in that view).
	 * Rotate in steps of 90 to match the physical front wall. Windows must be set to the room's speaker layout (5.1).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float AudioFrontYaw = 0.0f;

	/** Overall level of the car engine sounds. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float EngineVolume = 1.0f;

	/** Level of the start-light beeps and the speaker test (these play from every speaker at once). */
	UPROPERTY(Config, EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SignalVolume = 1.0f;
};
