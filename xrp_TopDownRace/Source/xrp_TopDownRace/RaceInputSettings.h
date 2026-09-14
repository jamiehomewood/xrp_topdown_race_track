#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RaceInputSettings.generated.h"

/**
 * Project Settings > Game > Race Input.
 * Stored in Config/DefaultGame.ini under [/Script/xrp_TopDownRace.RaceInputSettings].
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Race Input"))
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
};
