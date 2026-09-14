#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RaceHUD.generated.h"

/**
 * Desktop overlay. Only the PC monitor sees this (Igloo projects scene captures, which never include the HUD);
 * the room gets the same settings menu on in-world boards on the front and back walls.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
