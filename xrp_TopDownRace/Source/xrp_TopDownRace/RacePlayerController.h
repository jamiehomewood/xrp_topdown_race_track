#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RacePlayerController.generated.h"

class ARaceGameMode;

/** A menu direction key: fires on the press, then repeatedly while held (after a short delay). */
struct FRaceMenuKeyRepeat
{
	bool bHeld = false;
	float Timer = 0.0f;

	bool Update(bool bDown, float DeltaTime)
	{
		if (!bDown)
		{
			bHeld = false;
			return false;
		}
		if (!bHeld)
		{
			bHeld = true;
			Timer = 0.35f;
			return true;
		}
		Timer -= DeltaTime;
		if (Timer <= 0.0f)
		{
			Timer = 0.09f;
			return true;
		}
		return false;
	}
};

/**
 * One per local player / controller index. Polls its own controller (and the keyboard, for player 1).
 * Its car races under computer control until the player presses a button, which hands them the car;
 * after a long idle it goes back to the computer. Always views the shared track camera.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARacePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARacePlayerController();

	virtual void PlayerTick(float DeltaTime) override;

	/** Controller index this player listens to, which is also its car / grid slot. */
	int32 GetSlotIndex() const;

private:
	/** Puts the audio listener at the room centre facing Race Settings > AudioFrontYaw (not on the high top-down camera). */
	void ApplyRoomAudioListener();

	/** While this player has the settings menu open: sticks, D-pad, arrows and buttons drive the menu. */
	void UpdateSettingsMenuInput(ARaceGameMode* GameMode, int32 Slot, float DeltaTime);

	bool bRoomListenerApplied = false;
	bool bJoinButtonHeld = false;
	float IdleTime = 0.0f;

	bool bMenuToggleHeld = false;
	bool bMenuConfirmHeld = false;
	bool bMenuBackHeld = false;
	FRaceMenuKeyRepeat MenuUp;
	FRaceMenuKeyRepeat MenuDown;
	FRaceMenuKeyRepeat MenuLeft;
	FRaceMenuKeyRepeat MenuRight;

	// race.AutoDrive test aid state
	float AutoDriveLogTimer = 0.0f;
	int32 AutoDriveFrames = 0;
	FVector AutoDriveLastLocation = FVector::ZeroVector;
	bool bAutoDriveHasLastLocation = false;
	float AutoDriveSteerSign = 1.0f;
	float AutoDriveProbeTimer = 0.0f;
	float AutoDriveFlipHold = 0.0f;
	FVector AutoDriveProbeLocation = FVector::ZeroVector;
	int32 AutoDriveEscapes = 0;
	bool bHandlingTestPlaced = false;
	float HandlingTestTime = 0.0f;
	float HandlingLogTimer = 0.0f;
};
