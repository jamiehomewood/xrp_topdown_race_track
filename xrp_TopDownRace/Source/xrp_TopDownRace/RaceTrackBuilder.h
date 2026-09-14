#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceTheme.h"
#include "RaceTrackBuilder.generated.h"

class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FRaceTrackPath;

/**
 * Builds the circuit in the world at runtime from a track path, dressed in a scenery theme: grey road, kerb walls
 * (with collision, like the editor-built walls), start line, a fence round the outside, start posts, and low scenery
 * over the rest of the room floor. River Forest also builds its own ground, streams, a lake and a tree ring beyond
 * the walls (Countryside uses the ones saved in the level). The level's own editor-built track (Tools/TrackGen) and
 * floor scenery are hidden.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceTrackBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARaceTrackBuilder();

	/** Replaces everything built for the previous track. ScenerySeed makes the scenery scatter repeatable. */
	void Build(const FRaceTrackPath& Path, int32 ScenerySeed, ERaceThemeKind Theme, ERaceSeasonKind Season);

	/** Hides the level's editor-built track and the scenery on the room floor. Returns how many actors were hidden. */
	static int32 HideLevelTrack(UWorld* World);

	/** Shows or hides the level's countryside backdrop: trees beyond the walls, hills, mountains and the ground tile. */
	static void SetLevelBackdropVisible(UWorld* World, bool bVisible);

private:
	using FTaken = TArray<TPair<FVector2D, float>>;

	void BuildGround();
	void BuildFences(const FRaceTrackPath& Path, FRandomStream& Random);
	void BuildNarrowingScenery(const FRaceTrackPath& Path, FRandomStream& Random);
	void BuildStreams(const FRaceTrackPath& Path, FRandomStream& Random, FTaken& Taken);
	void BuildFloorScenery(const FRaceTrackPath& Path, FRandomStream& Random, FTaken& Taken);
	void BuildBackdrop(FRandomStream& Random);

	/** A scenery piece in the current season (seasonal materials, snow pines). */
	UStaticMeshComponent* AddProp(const FString& MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bCastShadow);
	UStaticMesh* LoadMesh(const FString& MeshPath);

	/** A plain coloured material for the ground and water (the pack's own water needs effects this project turns off). */
	class UMaterialInstanceDynamic* MakeFlatMaterial(const FLinearColor& Color, float Roughness);

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<UProceduralMeshComponent> TrackMesh;

	/** Scenery pieces: plain mesh components (not every pack's materials are enabled for instancing). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Props;

	/** Loaded scenery models (null for one that failed to load, so it only warns once). */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UStaticMesh>> PropMeshes;

	const FRaceThemeContent* Content = nullptr;
	ERaceSeasonKind CurrentSeason = ERaceSeasonKind::Summer;
};
