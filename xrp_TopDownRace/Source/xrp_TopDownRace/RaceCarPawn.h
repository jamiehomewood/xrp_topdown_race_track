#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RaceCarPawn.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class URaceEngineSynth;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Arcade top-down car with a GTA 2 style feel: slides, bounces off walls and other cars with a spin, and a
 * handbrake for doughnuts. Not possessed: a player's ARacePlayerController or the game mode's computer driver
 * feeds it input every frame. Units follow the track build (1 physical cm = 10 UU), so speeds are real-car scale.
 * Handling values are config: tune them in Config/DefaultGame.ini under [/Script/xrp_TopDownRace.RaceCarPawn].
 */
UCLASS(Config = Game)
class XRP_TOPDOWNRACE_API ARaceCarPawn : public APawn
{
	GENERATED_BODY()

public:
	ARaceCarPawn();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Swap the visual mesh and fit the collision box to it. */
	void SetCarMesh(UStaticMesh* Mesh);

	/** Throttle and brake 0..1, steer -1 (left) .. 1 (right), handbrake held. */
	void SetDriveInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake = false);

	/** Place on the track at SpawnTransform (backing up along the grid if the spot is taken) and enable. */
	void Activate(const FTransform& SpawnTransform);
	void Deactivate();
	bool IsActive() const { return bActive; }
	float GetSpeed() const { return Velocity.Size2D(); }
	float GetSpinRate() const { return AngularVelocity; }
	virtual FVector GetVelocity() const override { return Velocity; }

	/** Angle in degrees between where the car points and where it is actually going (0 = no slide). */
	float GetSlipAngle() const;

	/** Race control: while locked the car stays put and the throttle only revs the engine (grid, lights, results). */
	void SetControlsLocked(bool bLocked);
	bool AreControlsLocked() const { return bControlsLocked; }

	/** Teleport onto a grid slot, stopped. No overlap back-off: the whole grid is placed together. */
	void PlaceOnGrid(const FTransform& GridTransform);

	/** Slipstream strength wanted this frame, 0..1 (set by the game mode from the car ahead); the car eases towards it. */
	void SetDraftTarget(float Strength) { DraftTarget = FMath::Clamp(Strength, 0.0f, 1.0f); }
	float GetDraftFactor() const { return DraftFactor; }

	/** Slot number and player colour, used for the roof badge and the player marker. */
	void SetPlayerIdentity(int32 Slot, FColor Color);

	/** A player drives this car: glowing marker underneath and a coloured "P1" badge. Otherwise it's a CPU car. */
	void SetPlayerControlled(bool bInPlayerControlled);
	bool IsPlayerControlled() const { return bPlayerControlled; }

	/** A shove from another car: change in velocity and where it hit (adds spin). */
	void ApplyBump(const FVector& DeltaVelocity, const FVector& WorldContactPoint);

	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Number badge lying on the roof so it reads from above. */
	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	TObjectPtr<UTextRenderComponent> Badge;

	/** Glowing disc under a player's car, in the player's colour. */
	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	TObjectPtr<UStaticMeshComponent> PlayerMarker;

	/** Spatialized engine voice; plays while the car is in the race. */
	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	TObjectPtr<URaceEngineSynth> EngineSound;

	/** Give this car its own engine pitch (by grid slot). */
	void SetEngineVoice(int32 Slot);

	/**
	 * Every vehicle model is scaled to this length (UU, nose to tail), so a van or monster truck races on equal
	 * terms with a sports car. Collision and ride height follow the scaled mesh.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Car", meta = (ClampMin = "50.0"))
	float CarLength = 237.0f;

	/** Scale applied to the current mesh to reach CarLength. */
	UPROPERTY(VisibleAnywhere, Category = "Race|Car")
	float CarScale = 0.5f;

	/** Yaw applied to the mesh so its nose points down the actor's +X. */
	UPROPERTY(EditAnywhere, Category = "Race|Car")
	float MeshYawOffset = -90.0f;

	/** Top of the road surface (track_geom.ROAD_Z). */
	UPROPERTY(EditAnywhere, Category = "Race|Car")
	float RoadHeight = 2.0f;

	/** Top speed without a slipstream. Kept modest so a drafting car can catch up and pass on the straights. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float MaxSpeed = 1800.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float MaxReverseSpeed = 700.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float Acceleration = 1500.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float BrakeDeceleration = 3200.0f;

	/** Speed lost per second when coasting. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float CoastDeceleration = 800.0f;

	/** Degrees per second at full steer at low to mid speed (see HighSpeedTurnScale). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float TurnRate = 240.0f;

	/** Fraction of TurnRate left at top speed, so a flick at full speed doesn't swing the nose round (oversteer). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float HighSpeedTurnScale = 0.6f;

	/** Below this speed steering is scaled down, so a parked car can't spin on the spot. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float FullSteerSpeed = 450.0f;

	/** Steering kept at a standstill while throttle or brake is held, so a car pinned on a wall can turn away. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinSteerScale = 0.6f;

	/** How quickly the direction of travel follows the nose (higher = grippier, lower = driftier). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float Grip = 9.0f;

	/** How quickly the turn rate follows the stick while steering (higher = snappier). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float SteerResponse = 10.0f;

	/** How quickly spin from a hit settles when not steering (lower = spins last longer). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handling")
	float SpinRecovery = 5.0f;

	/** Grip with the handbrake on: the back steps out. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handbrake")
	float HandbrakeGrip = 0.7f;

	/** Turn rate multiplier with the handbrake on, available even at a crawl (doughnuts). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handbrake")
	float HandbrakeTurnBoost = 1.7f;

	/** Speed lost per second while the handbrake is held. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Handbrake")
	float HandbrakeDrag = 500.0f;

	/** Fraction of into-the-wall speed bounced back. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Collisions", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WallBounce = 0.55f;

	/** Bounciness of car-to-car hits (both cars share the impulse). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Collisions", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CarBounce = 0.6f;

	/** Spin from an off-centre hit: degrees per second per (UU offset x UU/s impulse). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Collisions", meta = (ClampMin = "0.0"))
	float ImpactSpin = 0.0025f;

	/** Extra top speed at full slipstream (0.22 = +22%). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Drafting", meta = (ClampMin = "0.0"))
	float DraftTopSpeedBonus = 0.22f;

	/** Extra acceleration at full slipstream (0.6 = +60%). */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Drafting", meta = (ClampMin = "0.0"))
	float DraftAccelerationBonus = 0.6f;

	/** How fast the slipstream builds up (1/s) when tucked in behind, and fades (1/s) after pulling out. */
	UPROPERTY(Config, EditAnywhere, Category = "Race|Drafting", meta = (ClampMin = "0.1"))
	float DraftBuildRate = 2.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Race|Drafting", meta = (ClampMin = "0.1"))
	float DraftFadeRate = 1.5f;

private:
	void ApplyMeshTransform();
	void UpdateIdentityVisuals();

	/** Turn by YawDelta; if that would push the box into a wall, slide away from the wall first. */
	void ApplyYaw(float YawDelta);

	/** Push out of any wall the car starts the frame inside (e.g. after a car-on-car shove). */
	void ResolvePenetration();

	bool IsBlockedAt(const FVector& Location, const FQuat& Rotation) const;

	/** Moves Location away from the last wall hit until the box fits at Rotation. */
	bool FindFreeSpotNearby(FVector& Location, const FQuat& Rotation) const;

	/** Spin from an impulse applied Offset away from the car's centre. */
	void AddSpin(const FVector& Offset, const FVector& Impulse);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;

	FVector Velocity = FVector::ZeroVector;
	float AngularVelocity = 0.0f; // yaw, degrees per second
	FVector LastWallNormal = FVector::ZeroVector;
	float TimeSinceWallHit = 1000.0f;
	float TimeSinceImpactSound = 1000.0f;
	float Throttle = 0.0f;
	float Brake = 0.0f;
	float Steer = 0.0f;
	bool bHandbrake = false;
	float DraftTarget = 0.0f;
	float DraftFactor = 0.0f;
	int32 SlotNumber = 0;
	FColor PlayerColor = FColor::White;
	bool bActive = false;
	bool bControlsLocked = false;
	bool bPlayerControlled = false;
};
