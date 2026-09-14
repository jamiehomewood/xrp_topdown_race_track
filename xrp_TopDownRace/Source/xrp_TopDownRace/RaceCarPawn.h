#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RaceCarPawn.generated.h"

class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Arcade top-down car. Not possessed: its ARacePlayerController feeds it input every frame.
 * Cars start inactive (hidden, no collision) and are activated when a player joins.
 * Units follow the track build: 1 physical cm = 10 UU, so speeds are real-car scale.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceCarPawn : public APawn
{
	GENERATED_BODY()

public:
	ARaceCarPawn();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Swap the visual mesh and fit the collision box to it. */
	void SetCarMesh(UStaticMesh* Mesh);

	/** Throttle and brake 0..1, steer -1 (left) .. 1 (right). */
	void SetDriveInput(float InThrottle, float InBrake, float InSteer);

	/** Place on the track at SpawnTransform (backing up along the grid if the spot is taken) and enable. */
	void Activate(const FTransform& SpawnTransform);
	void Deactivate();
	bool IsActive() const { return bActive; }
	float GetSpeed() const { return Velocity.Size2D(); }

	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Uniform size of the car relative to the Fab mesh (real-car scale). Collision and ride height follow. */
	UPROPERTY(EditAnywhere, Category = "Race|Car", meta = (ClampMin = "0.1"))
	float CarScale = 0.5f;

	/** Yaw applied to the mesh so its nose points down the actor's +X. */
	UPROPERTY(EditAnywhere, Category = "Race|Car")
	float MeshYawOffset = -90.0f;

	/** Top of the road surface (track_geom.ROAD_Z). */
	UPROPERTY(EditAnywhere, Category = "Race|Car")
	float RoadHeight = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float MaxSpeed = 2600.0f;

	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float MaxReverseSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float Acceleration = 2200.0f;

	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float BrakeDeceleration = 4000.0f;

	/** Speed lost per second when coasting. */
	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float CoastDeceleration = 1000.0f;

	/** Degrees per second at full steer once above FullSteerSpeed. */
	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float TurnRate = 210.0f;

	/** Below this speed steering is scaled down, so a parked car can't spin on the spot. */
	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float FullSteerSpeed = 700.0f;

	/** How quickly sideways sliding is killed (higher = grippier, lower = driftier). */
	UPROPERTY(EditAnywhere, Category = "Race|Handling")
	float Grip = 6.0f;

	/** Fraction of into-the-wall velocity reflected back on impact. */
	UPROPERTY(EditAnywhere, Category = "Race|Handling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallBounce = 0.35f;

private:
	void ApplyMeshTransform();

	FVector Velocity = FVector::ZeroVector;
	float Throttle = 0.0f;
	float Brake = 0.0f;
	float Steer = 0.0f;
	bool bActive = false;
};
