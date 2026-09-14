#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RaceInputSettings.generated.h"

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

	/** Left-stick values below this are ignored. */
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float StickDeadZone = 0.2f;

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
};
