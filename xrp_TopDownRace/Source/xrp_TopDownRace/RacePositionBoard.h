#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RacePositionBoard.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/** Colours an LED position board can light its dots in. */
enum class ERaceBoardColor : uint8
{
	Off,
	White,
	Amber,
	Green,
	Purple,
	Red,
	Gold,
	Dim,
	Car1,
	Car2,
	Car3,
	Car4,
	Count
};

/**
 * An LED dot-matrix position board, like a race track's timing tower: a canvas of Columns x Rows dots drawn with
 * RaceDotFont, shown on one or more physical boards (black housing, metal frame, legs to the ground) - the same
 * picture on each. Draw with Clear / DrawText / FillRect, then Commit.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARacePositionBoard : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 Columns = 170;
	static constexpr int32 Rows = 57;

	ARacePositionBoard();

	/**
	 * Adds a board. FaceTransform places the centre of the dot area with its X axis towards the viewers and Z up;
	 * Width is the width of the dot area. Legs run down to GroundZ.
	 */
	void AddFace(const FTransform& FaceTransform, float Width, float GroundZ);

	/** Colour for a car slot's dots (Car1..Car4). */
	void SetCarColor(int32 Slot, const FLinearColor& Color);

	void Clear();
	void DrawText(int32 X, int32 Y, const FString& Text, ERaceBoardColor Color, int32 Scale = 1);
	void DrawTextRight(int32 RightX, int32 Y, const FString& Text, ERaceBoardColor Color, int32 Scale = 1);
	void DrawTextCentred(int32 CentreX, int32 Y, const FString& Text, ERaceBoardColor Color, int32 Scale = 1);
	void FillRect(int32 X, int32 Y, int32 Width, int32 Height, ERaceBoardColor Color);

	/** Shows what has been drawn since Clear (the lit dots are only rebuilt when the picture changed). */
	void Commit();

private:
	struct FFace
	{
		float Pitch = 10.0f;
		TArray<UInstancedStaticMeshComponent*> LitDots; // per colour; kept alive by DotGroups
	};

	FTransform DotTransform(const FFace& Face, int32 X, int32 Y, float Depth) const;
	UMaterialInterface* LedMaterial(ERaceBoardColor Color);
	UMaterialInterface* StructureMaterial(const FLinearColor& Color);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> FaceRoots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> DotGroups;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Structure;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LedMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> StructureMaterials;

	TArray<FFace> Faces;
	TArray<uint8> Canvas;
	TArray<uint8> Shown;
	FLinearColor CarColors[4];
};
