#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RaceGameMode.generated.h"

class ACameraActor;
class ARaceCarPawn;
class ARacePlayerController;
class UStaticMesh;

/**
 * 4-player local arcade race. Creates one local player per controller index (no split screen),
 * spawns one inactive car per slot, and activates a slot's car when that player presses a button.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARaceGameMode();

	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	ARaceCarPawn* GetCarForSlot(int32 SlotIndex);
	void JoinRace(ARacePlayerController* Player);

	/** Level camera tagged TrackCameraTag, or a top-down camera spawned above the track centre. */
	AActor* GetTrackCamera();

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<ARaceCarPawn>> Cars;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TrackCamera;
};
