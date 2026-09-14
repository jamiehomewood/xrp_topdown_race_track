#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceDisplay.generated.h"

class UTextRenderComponent;

/**
 * In-world text for the room (Igloo projects scene captures, so screen HUDs never reach the walls or floor).
 * Floor lines lie flat for players standing round the track; wall lines stand just beyond a wall.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceDisplay : public AActor
{
	GENERATED_BODY()

public:
	ARaceDisplay();

	/** Text lying on the floor, its top pointing along UpDirection (e.g. from a player's corner toward the room centre). */
	int32 AddFloorLine(const FVector& Location, const FVector& UpDirection, float TextHeight);

	/** Upright text facing along FacingDirection (towards the room centre for a wall display). */
	int32 AddWallLine(const FVector& Location, const FVector& FacingDirection, float TextHeight);

	/** Updates a line; unchanged text/colour is skipped so the text mesh isn't rebuilt every refresh. */
	void SetLine(int32 LineIndex, const FString& Text, FColor Color);

	/** Dark board lying on the floor under floor text (Width along the reading direction, Depth along UpDirection). */
	void AddFloorBacking(const FVector& Location, const FVector& UpDirection, float Width, float Depth);

	/** Dark board standing behind wall text (it sits a little further from the viewer than Location). */
	void AddWallBacking(const FVector& Location, const FVector& FacingDirection, float Width, float Height);

private:
	int32 AddLine(const FVector& Location, const FRotator& Rotation, float TextHeight);
	void AddBacking(const FVector& Location, const FRotator& Rotation, float Width, float Height);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> Lines;

	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> BackingMaterial;

	TArray<FString> LineText;
	TArray<FColor> LineColor;
};
