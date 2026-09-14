#include "RaceCarPawn.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	constexpr float GroundClearance = 4.0f;
	constexpr float DefaultCarHalfHeight = 60.0f;

	/**
	 * Backward speed (UU/s) before steering switches to reverse sense. Must stay well above the small
	 * bounce a car pinned on a wall gets every frame, or its steering flips back and forth and it can't turn out.
	 */
	constexpr float ReverseSteerSpeed = 150.0f;
}

ARaceCarPawn::ARaceCarPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->SetBoxExtent(FVector(230.0f, 100.0f, DefaultCarHalfHeight));
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	RootComponent = Collision;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARaceCarPawn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMeshTransform();
}

void ARaceCarPawn::SetCarMesh(UStaticMesh* Mesh)
{
	Body->SetStaticMesh(Mesh);
	ApplyMeshTransform();
}

void ARaceCarPawn::ApplyMeshTransform()
{
	const FRotator MeshRotation(0.0f, MeshYawOffset, 0.0f);
	const UStaticMesh* Mesh = Body->GetStaticMesh();
	if (!Mesh)
	{
		Body->SetRelativeRotation(MeshRotation);
		return;
	}

	// Fit the box to the scaled, rotated mesh footprint (slightly inset so glancing contacts feel fair).
	const FBox Bounds = Mesh->GetBoundingBox();
	const FVector RotatedSize = MeshRotation.RotateVector(Bounds.GetSize() * CarScale).GetAbs();
	const FVector Extent(RotatedSize.X * 0.47f, RotatedSize.Y * 0.47f, DefaultCarHalfHeight * CarScale);
	Collision->SetBoxExtent(Extent);

	// Centre the mesh on the box and sit its lowest point on the road.
	FVector Offset = -MeshRotation.RotateVector(Bounds.GetCenter() * CarScale);
	Offset.Z = -(Extent.Z + GroundClearance) - Bounds.Min.Z * CarScale;
	Body->SetRelativeScale3D(FVector(CarScale));
	Body->SetRelativeLocationAndRotation(Offset, MeshRotation);
}

void ARaceCarPawn::SetDriveInput(float InThrottle, float InBrake, float InSteer)
{
	Throttle = FMath::Clamp(InThrottle, 0.0f, 1.0f);
	Brake = FMath::Clamp(InBrake, 0.0f, 1.0f);
	Steer = FMath::Clamp(InSteer, -1.0f, 1.0f);
}

void ARaceCarPawn::Activate(const FTransform& SpawnTransform)
{
	const FVector Extent = Collision->GetUnscaledBoxExtent();
	const FRotator Rotation(0.0f, SpawnTransform.Rotator().Yaw, 0.0f);
	FVector Location = SpawnTransform.GetLocation();
	Location.Z = RoadHeight + Extent.Z + GroundClearance;

	// Late joiners: if the grid slot is occupied, back up one car length at a time.
	const FCollisionShape Shape = FCollisionShape::MakeBox(Extent);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RaceCarActivate), false, this);
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		if (!GetWorld()->OverlapBlockingTestByChannel(Location, Rotation.Quaternion(), ECC_Pawn, Shape, Params))
		{
			break;
		}
		Location -= Rotation.Vector() * (Extent.X * 2.0f + 100.0f);
	}

	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = FVector::ZeroVector;
	SetDriveInput(0.0f, 0.0f, 0.0f);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	bActive = true;
}

void ARaceCarPawn::Deactivate()
{
	bActive = false;
	Velocity = FVector::ZeroVector;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

bool ARaceCarPawn::IsBlockedAt(const FVector& Location, const FQuat& Rotation) const
{
	// Slightly inset so resting contact left by the last sweep doesn't count as blocked.
	const FCollisionShape Shape = FCollisionShape::MakeBox(Collision->GetUnscaledBoxExtent() - FVector(2.0f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RaceCarBlocked), false, this);
	return GetWorld()->OverlapBlockingTestByChannel(Location, Rotation, ECC_Pawn, Shape, Params);
}

bool ARaceCarPawn::FindFreeSpotNearby(FVector& Location, const FQuat& Rotation) const
{
	// Smallest nudge that fits. Prefer backing off the wall we just hit, but also try the compass directions:
	// in corners and car pile-ups the way out is often not straight back along one wall normal.
	TArray<FVector, TInlineAllocator<9>> Directions;
	if (!LastWallNormal.IsNearlyZero() && TimeSinceWallHit < 1.0f)
	{
		Directions.Add(LastWallNormal);
	}
	for (int32 Step = 0; Step < 8; ++Step)
	{
		const float Angle = Step * UE_PI / 4.0f;
		Directions.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f));
	}

	for (float Push = 4.0f; Push <= 60.0f; Push += 4.0f)
	{
		for (const FVector& Direction : Directions)
		{
			const FVector Candidate = Location + Direction * Push;
			if (!IsBlockedAt(Candidate, Rotation))
			{
				Location = Candidate;
				return true;
			}
		}
	}
	return false;
}

void ARaceCarPawn::ApplyYaw(float YawDelta)
{
	if (FMath::IsNearlyZero(YawDelta))
	{
		return;
	}

	const FQuat NewRotation = FRotator(0.0f, GetActorRotation().Yaw + YawDelta, 0.0f).Quaternion();
	FVector Location = GetActorLocation();
	if (IsBlockedAt(Location, NewRotation) && !FindFreeSpotNearby(Location, NewRotation))
	{
		return; // Wedged with nowhere to go: skip this frame's turn rather than pushing into the wall.
	}
	SetActorLocationAndRotation(Location, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

void ARaceCarPawn::ResolvePenetration()
{
	FVector Location = GetActorLocation();
	const FQuat Rotation = GetActorQuat();
	if (IsBlockedAt(Location, Rotation) && FindFreeSpotNearby(Location, Rotation))
	{
		SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ARaceCarPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}

	const float Dt = FMath::Min(DeltaSeconds, 1.0f / 20.0f);
	TimeSinceWallHit += Dt;
	ResolvePenetration();

	FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	float ForwardSpeed = FVector::DotProduct(Velocity, Forward);

	// Steering: car-relative, scaled in at low speed (but never below MinSteerScale while driving),
	// reversed when reversing or holding brake from a standstill.
	const bool bDriving = Throttle > 0.0f || Brake > 0.0f;
	float SteerScale = FMath::Clamp(FMath::Abs(ForwardSpeed) / FullSteerSpeed, 0.0f, 1.0f);
	if (bDriving)
	{
		SteerScale = FMath::Max(SteerScale, MinSteerScale);
	}
	const bool bReversing = ForwardSpeed < -ReverseSteerSpeed || (Brake > 0.0f && Throttle <= 0.0f && ForwardSpeed <= 1.0f);
	ApplyYaw(Steer * TurnRate * SteerScale * (bReversing ? -1.0f : 1.0f) * Dt);
	Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	// Longitudinal: throttle forward, brake then reverse, otherwise coast to a stop.
	ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	ForwardSpeed += Throttle * Acceleration * Dt;
	if (Brake > 0.0f)
	{
		ForwardSpeed -= Brake * (ForwardSpeed > 0.0f ? BrakeDeceleration : Acceleration * 0.6f) * Dt;
	}
	if (Throttle <= 0.0f && Brake <= 0.0f)
	{
		ForwardSpeed = FMath::Sign(ForwardSpeed) * FMath::Max(FMath::Abs(ForwardSpeed) - CoastDeceleration * Dt, 0.0f);
	}
	ForwardSpeed = FMath::Clamp(ForwardSpeed, -MaxReverseSpeed, MaxSpeed);

	// Lateral: bleed off sideways slide for arcade grip.
	const float LateralSpeed = FMath::FInterpTo(FVector::DotProduct(Velocity, Right), 0.0f, Dt, Grip);
	Velocity = Forward * ForwardSpeed + Right * LateralSpeed;

	// Move with sweep; on impact, bounce off and slide along the wall / other car.
	const FVector Delta = Velocity * Dt;
	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		const FVector Normal = Hit.Normal.GetSafeNormal2D();
		LastWallNormal = Normal;
		TimeSinceWallHit = 0.0f;
		const float IntoWall = FVector::DotProduct(Velocity, Normal);
		if (IntoWall < 0.0f)
		{
			Velocity -= (1.0f + WallBounce) * IntoWall * Normal;
		}
		const FVector Remaining = FVector::VectorPlaneProject(Delta * (1.0f - Hit.Time), Normal);
		AddActorWorldOffset(Remaining, true);
	}
}
