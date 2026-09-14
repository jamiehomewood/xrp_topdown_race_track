#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceCelebration.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Winner show for the room. Beyond each side wall (the walls without a position board), facing into the room so they
 * are seen upright on the wall projection: a podium with a big low-poly gold trophy turning on the top step and the
 * winner's name in block letters above it. Plus a chequered flag waved beside the finish line and confetti pouring
 * from the roof.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceCelebration : public AActor
{
	GENERATED_BODY()

public:
	ARaceCelebration();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Start the show: WinnerName in the winner's colour followed by "WINS!" in gold; the flag at the finish line. */
	void Celebrate(const FString& WinnerName, FColor Color, const FVector2D& FinishLine, const FVector2D& FinishDirection, float RoadHalfWidth);

	/** Clear everything away (next race). */
	void Stop();

private:
	struct FConfettiPiece
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FRotator Spin = FRotator::ZeroRotator;
		float FlutterPhase = 0.0f;
		bool bLanded = false;
	};

	/** One podium, trophy and name board, facing into the room from beyond a side wall. */
	void BuildDisplay(float Side);
	void BuildLetters(const FString& WinnerName);
	void UpdateFlag(float Time);
	void LaunchPiece(FConfettiPiece& Piece, bool bFirstWave);
	void UpdateConfetti(float DeltaSeconds);

	/** A lit low-poly material in a flat colour. */
	UMaterialInstanceDynamic* LitMaterial(const FLinearColor& Color, float Roughness, float Specular = 0.5f);
	UStaticMeshComponent* AddMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bCastShadow);
	UInstancedStaticMeshComponent* AddVoxels(USceneComponent* Parent, UMaterialInterface* Material);

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<USceneComponent> FlagRoot;

	/** Per side wall: the display root, its trophy, and the name / "WINS!" letters. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Displays;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> Trophies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> NameLetters;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> GoldLetters;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> FlagCloth;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlagPole;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NameMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GoldMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Parts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ConfettiGroups;

	TArray<TArray<FConfettiPiece>> Pieces; // per colour group
	TArray<FTransform> TransformScratch;
	float ShowTime = 0.0f;
	bool bShowing = false;
};
