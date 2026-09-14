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
 * Winner show for the room: a round stage turning slowly above the middle of the track with a podium, a low-poly gold
 * trophy and the winner's name in chunky block letters (laid out both ways round so every corner can read it); a
 * chequered flag waved beside the finish line; and confetti pouring from the roof.
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

	void BuildStage();
	void BuildTrophy();
	void BuildLetters(const FString& WinnerName);
	void UpdateFlag(float Time);
	void LaunchPiece(FConfettiPiece& Piece, bool bFirstWave);
	void UpdateConfetti(float DeltaSeconds);

	/** A lit low-poly material in a flat colour. */
	UMaterialInstanceDynamic* LitMaterial(const FLinearColor& Color, float Roughness, float Specular = 0.5f);
	UStaticMeshComponent* AddMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bCastShadow);
	UInstancedStaticMeshComponent* AddVoxels(USceneComponent* Parent, UMaterialInterface* Material);

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<USceneComponent> Turntable;

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<USceneComponent> FlagRoot;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Trophy;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> FlagCloth;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlagPole;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> NameLetters;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> GoldLetters;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NameMaterial;

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
