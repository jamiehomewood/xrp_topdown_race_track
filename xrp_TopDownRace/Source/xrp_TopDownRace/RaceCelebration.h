#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceCelebration.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;

/**
 * Winner show for the room: the announcement floating just above the middle of the track, slowly turning so every
 * corner of the room can read it, and confetti raining down over the whole floor and settling on it.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceCelebration : public AActor
{
	GENERATED_BODY()

public:
	ARaceCelebration();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Start the show: announcement in the winner's colour and a burst of confetti. */
	void Celebrate(const FString& Text, FColor Color);

	/** Clear the announcement and all confetti (next race). */
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

	void LaunchPiece(FConfettiPiece& Piece, bool bSpreadHeight);
	void UpdateConfetti(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Race")
	TObjectPtr<UTextRenderComponent> Announcement;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ConfettiGroups;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ConfettiMaterials;

	TArray<TArray<FConfettiPiece>> Pieces; // per colour group
	TArray<FTransform> TransformScratch;
	FString AnnouncementText;
	float ShowTime = 0.0f;
	bool bShowing = false;
};
