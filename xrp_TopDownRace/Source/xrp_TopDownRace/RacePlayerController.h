#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RacePlayerController.generated.h"

/**
 * One per local player / controller index. Polls its own controller (and the keyboard, for player 1),
 * joins the race on any button press, then drives its car. Always views the shared track camera.
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

	bool bRoomListenerApplied = false;
	bool bJoinButtonHeld = false;
	float AutoDriveLogTimer = 0.0f;
	int32 AutoDriveFrames = 0;
	FVector AutoDriveLastLocation = FVector::ZeroVector;
	bool bAutoDriveHasLastLocation = false;
	float AutoDriveSteerSign = 1.0f;
	float AutoDriveProbeTimer = 0.0f;
	float AutoDriveFlipHold = 0.0f;
	FVector AutoDriveProbeLocation = FVector::ZeroVector;
	int32 AutoDriveEscapes = 0;
};
