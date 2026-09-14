#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceTrackBuilder.generated.h"

class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
struct FRaceTrackPath;

/**
 * Builds the circuit in the world at runtime from a track path: grey road, kerb walls (with collision, like the
 * editor-built walls), start line, a fence round the outside, start posts, and low scenery scattered over the rest of
 * the room floor. The level's own editor-built track (Tools/TrackGen) is hidden while this is in use; the grass
 * ground, the trees beyond the room walls and the horizon stay.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceTrackBuilder : public AActor
{
	GENERATED_BODY()

public:
	ARaceTrackBuilder();

	/** Replaces everything built for the previous track. ScenerySeed makes the scenery scatter repeatable. */
	void Build(const FRaceTrackPath& Path, int32 ScenerySeed);

	/** Hides the level's editor-built track and the scenery on the room floor. Returns how many actors were hidden. */
	static int32 HideLevelTrack(UWorld* World);

private:
	void BuildFences(const FRaceTrackPath& Path, FRandomStream& Random);
	void BuildNarrowingScenery(const FRaceTrackPath& Path, FRandomStream& Random);
	void BuildFloorScenery(const FRaceTrackPath& Path, FRandomStream& Random);
	void AddProp(const FString& MeshPath, const FVector2D& Location, float Yaw, float Scale, bool bCastShadow);

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<UProceduralMeshComponent> TrackMesh;

	/** Scenery pieces: plain mesh components, since the Fab / nature pack materials aren't enabled for instancing. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Props;

	/** Loaded scenery models (null for one that failed to load, so it only warns once). */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UStaticMesh>> PropMeshes;
};
