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
	bool bJoinButtonHeld = false;
	float AutoDriveLogTimer = 0.0f;
};
