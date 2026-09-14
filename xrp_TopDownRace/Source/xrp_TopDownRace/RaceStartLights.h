#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceStartLights.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class URaceSignalSynth;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Five red start lights, drawn on one or more boards (a gantry over the start line for the floor projection,
 * large boards beyond the front and back walls for the wall projection). All boards show the same state.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceStartLights : public AActor
{
	GENERATED_BODY()

public:
	ARaceStartLights();

	virtual void BeginPlay() override;

	static constexpr int32 NumLights = 5;

	/** Non-spatial beeps from every speaker: one per light, a higher tone at lights out, and the speaker test. */
	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<URaceSignalSynth> SignalSound;

	/**
	 * Adds a board. BoardTransform places its centre; the lights run along the board's local +Y
	 * (first light at -Y, i.e. the viewer's left when +Y is the viewer's right) and face its local +Z.
	 */
	int32 AddBoard(const FTransform& BoardTransform, float LightRadius, float Spacing);

	/** Moves a board added earlier (the start gantry follows the start line when the track changes). */
	void SetBoardTransform(int32 BoardIndex, const FTransform& BoardTransform);

	/** 0 = all dark, 1..5 = that many lit, counting from the first light. Returns true if the count changed. */
	bool SetLitCount(int32 Count);

private:
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, const FTransform& RelativeTransform, UMaterialInterface* Material);
	UMaterialInterface* GetBaseMaterial();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LightMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

	/** Each board's meshes and their transforms relative to the board (the components are kept alive by Meshes). */
	struct FBoardParts
	{
		TArray<UStaticMeshComponent*> Components;
		TArray<FTransform> LocalTransforms;
	};
	TArray<FBoardParts> Boards;

	int32 LitCount = -1;
};
