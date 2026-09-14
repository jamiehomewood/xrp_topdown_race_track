#include "RaceCarPawn.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RaceEngineSynth.h"
#include "RaceInputSettings.h"
#include "Sound/SoundAttenuation.h"

namespace
{
	constexpr float GroundClearance = 4.0f;
	constexpr float DefaultCarHalfHeight = 60.0f;

	/**
	 * Backward speed (UU/s) before steering switches to reverse sense. Must stay well above the small
	 * bounce a car pinned on a wall gets every frame, or its steering flips back and forth and it can't turn out.
	 */
	constexpr float ReverseSteerSpeed = 150.0f;

	/** Engine pitch per grid slot, so the four cars are distinguishable by ear. */
	constexpr float EngineVoicePitches[] = { 1.0f, 0.8f, 1.22f, 0.9f };

	/** Minimum closing speed (UU/s) for a hit to make a crash sound, and the gap between crash sounds. */
	constexpr float ImpactSoundMinSpeed = 300.0f;
	constexpr float ImpactSoundInterval = 0.15f;

	constexpr float MaxSpinRate = 720.0f;

	/** Sideways slide speed (UU/s) where the tyres start to squeal, and the extra slide for full squeal. */
	constexpr float SkidStartSpeed = 250.0f;
	constexpr float SkidFullRange = 700.0f;

	const TCHAR* MarkerMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* MarkerMaterialPath = TEXT("/Game/RaceTrack/Materials/M_RaceLight.M_RaceLight");
	const TCHAR* MarkerFallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FColor CpuBadgeColor(170, 170, 170);
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

	// Number badge lying on the roof: faces the sky, top of the digit towards the car's nose.
	Badge = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Badge"));
	Badge->SetupAttachment(Collision);
	Badge->SetRelativeRotation(FRotationMatrix::MakeFromXZ(FVector::UpVector, FVector::ForwardVector).Rotator());
	Badge->SetHorizontalAlignment(EHTA_Center);
	Badge->SetVerticalAlignment(EVRTA_TextCenter);
	Badge->SetWorldSize(110.0f);
	Badge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Badge->SetCastShadow(false);

	PlayerMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerMarker"));
	PlayerMarker->SetupAttachment(Collision);
	PlayerMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayerMarker->SetCastShadow(false);
	PlayerMarker->SetVisibility(false);

	// Spatialized at the car: the listener sits at the room centre (see ARacePlayerController), so the
	// audio engine pans each car to the speakers in its direction.
	EngineSound = CreateDefaultSubobject<URaceEngineSynth>(TEXT("EngineSound"));
	EngineSound->SetupAttachment(Collision);
	EngineSound->bAutoActivate = false;
	EngineSound->bAllowSpatialization = true;
	EngineSound->bOverrideAttenuation = true;
	FSoundAttenuationSettings& Attenuation = EngineSound->AttenuationOverrides;
	Attenuation.bAttenuate = true;
	Attenuation.bSpatialize = true;
	Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation.AttenuationShapeExtents = FVector(800.0f, 0.0f, 0.0f); // full volume within 8 m (0.8 m in the room)
	Attenuation.FalloffDistance = 6000.0f;                              // gentle fade: a car at the far end is still clearly heard
	Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	// Cars crossing right past the room centre blend across all speakers instead of flicking between them.
	Attenuation.NonSpatializedRadiusStart = 500.0f;
	Attenuation.NonSpatializedRadiusEnd = 150.0f;
	Attenuation.bEnableOcclusion = false;
	Attenuation.bEnableReverbSend = false;
}

void ARaceCarPawn::SetEngineVoice(int32 Slot)
{
	EngineSound->SetVoicePitch(EngineVoicePitches[FMath::Abs(Slot) % UE_ARRAY_COUNT(EngineVoicePitches)]);
}

void ARaceCarPawn::SetPlayerIdentity(int32 Slot, FColor Color)
{
	SlotNumber = Slot;
	PlayerColor = Color;

	if (!MarkerMaterial)
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MarkerMaterialPath);
		if (!Material)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, MarkerFallbackMaterialPath);
		}
		if (Material)
		{
			MarkerMaterial = UMaterialInstanceDynamic::Create(Material, this);
			PlayerMarker->SetMaterial(0, MarkerMaterial);
		}
		PlayerMarker->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MarkerMeshPath));
		ApplyMeshTransform();
	}
	UpdateIdentityVisuals();
}

void ARaceCarPawn::SetPlayerControlled(bool bInPlayerControlled)
{
	bPlayerControlled = bInPlayerControlled;
	UpdateIdentityVisuals();
}

void ARaceCarPawn::UpdateIdentityVisuals()
{
	if (bPlayerControlled)
	{
		Badge->SetText(FText::FromString(FString::Printf(TEXT("P%d"), SlotNumber + 1)));
		Badge->SetTextRenderColor(PlayerColor);
	}
	else
	{
		Badge->SetText(FText::AsNumber(SlotNumber + 1));
		Badge->SetTextRenderColor(CpuBadgeColor);
	}
	PlayerMarker->SetVisibility(bPlayerControlled);
}

void ARaceCarPawn::SetControlsLocked(bool bLocked)
{
	bControlsLocked = bLocked;
	if (bLocked)
	{
		Velocity = FVector::ZeroVector;
		AngularVelocity = 0.0f;
	}
}

void ARaceCarPawn::PlaceOnGrid(const FTransform& GridTransform)
{
	FVector Location = GridTransform.GetLocation();
	Location.Z = RoadHeight + Collision->GetUnscaledBoxExtent().Z + GroundClearance;
	SetActorLocationAndRotation(Location, FRotator(0.0f, GridTransform.Rotator().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = FVector::ZeroVector;
	AngularVelocity = 0.0f;
	DraftTarget = 0.0f;
	DraftFactor = 0.0f;
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

	// Scale the model to the standard car length, then fit the box to its footprint (slightly inset so glancing
	// contacts feel fair).
	const FBox Bounds = Mesh->GetBoundingBox();
	const float MeshLength = MeshRotation.RotateVector(Bounds.GetSize()).GetAbs().X;
	CarScale = MeshLength > 1.0f ? CarLength / MeshLength : 1.0f;
	const FVector RotatedSize = MeshRotation.RotateVector(Bounds.GetSize() * CarScale).GetAbs();
	const FVector Extent(RotatedSize.X * 0.47f, RotatedSize.Y * 0.47f, DefaultCarHalfHeight * CarScale);
	Collision->SetBoxExtent(Extent);

	// Centre the mesh on the box and sit its lowest point on the road.
	FVector Offset = -MeshRotation.RotateVector(Bounds.GetCenter() * CarScale);
	Offset.Z = -(Extent.Z + GroundClearance) - Bounds.Min.Z * CarScale;
	Body->SetRelativeScale3D(FVector(CarScale));
	Body->SetRelativeLocationAndRotation(Offset, MeshRotation);
	Badge->SetRelativeLocation(FVector(0.0f, 0.0f, Extent.Z + 50.0f));

	// Player marker: a flat glowing oval on the road, sticking out round the car so it shows from above.
	PlayerMarker->SetRelativeLocation(FVector(0.0f, 0.0f, -(Extent.Z + GroundClearance) + 3.0f));
	PlayerMarker->SetRelativeScale3D(FVector(Extent.X * 2.0f * 1.45f / 100.0f, Extent.Y * 2.0f * 2.3f / 100.0f, 0.02f));
}

void ARaceCarPawn::SetDriveInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake)
{
	Throttle = FMath::Clamp(InThrottle, 0.0f, 1.0f);
	Brake = FMath::Clamp(InBrake, 0.0f, 1.0f);
	Steer = FMath::Clamp(InSteer, -1.0f, 1.0f);
	bHandbrake = bInHandbrake;
}

void ARaceCarPawn::Activate(const FTransform& SpawnTransform)
{
	const FVector Extent = Collision->GetUnscaledBoxExtent();
	const FRotator Rotation(0.0f, SpawnTransform.Rotator().Yaw, 0.0f);
	FVector Location = SpawnTransform.GetLocation();
	Location.Z = RoadHeight + Extent.Z + GroundClearance;

	// If the spot is taken, back up one car length at a time.
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
	AngularVelocity = 0.0f;
	SetDriveInput(0.0f, 0.0f, 0.0f);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	bActive = true;

	EngineSound->SetVolumeMultiplier(GetDefault<URaceInputSettings>()->EngineVolume);
	EngineSound->SetEngineState(0.0f, 0.0f);
	EngineSound->Start();
}

void ARaceCarPawn::Deactivate()
{
	bActive = false;
	Velocity = FVector::ZeroVector;
	AngularVelocity = 0.0f;
	DraftTarget = 0.0f;
	DraftFactor = 0.0f;
	if (EngineSound->IsPlaying())
	{
		EngineSound->Stop();
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ARaceCarPawn::ApplyBump(const FVector& DeltaVelocity, const FVector& WorldContactPoint)
{
	const FVector FlatDelta(DeltaVelocity.X, DeltaVelocity.Y, 0.0f);
	Velocity += FlatDelta;
	AddSpin(WorldContactPoint - GetActorLocation(), FlatDelta);
	LastWallNormal = FlatDelta.GetSafeNormal();
	TimeSinceWallHit = 0.0f;
}

float ARaceCarPawn::GetSlipAngle() const
{
	if (Velocity.Size2D() < 50.0f)
	{
		return 0.0f;
	}
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, Velocity.GetSafeNormal2D()), -1.0f, 1.0f)));
}

void ARaceCarPawn::AddSpin(const FVector& Offset, const FVector& Impulse)
{
	// 2D cross product: positive turns +X towards +Y, which is a positive yaw.
	AngularVelocity = FMath::Clamp(AngularVelocity + ImpactSpin * (Offset.X * Impulse.Y - Offset.Y * Impulse.X), -MaxSpinRate, MaxSpinRate);
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
	TimeSinceImpactSound += Dt;

	const float DraftRate = DraftTarget > DraftFactor ? DraftBuildRate : DraftFadeRate;
	DraftFactor += (DraftTarget - DraftFactor) * (1.0f - FMath::Exp(-DraftRate * Dt));

	if (bPlayerControlled && MarkerMaterial)
	{
		const float Pulse = 1.6f + 0.9f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.0f);
		MarkerMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(PlayerColor) * Pulse);
	}

	if (bControlsLocked)
	{
		// Waiting on the grid: stay put, but let the throttle rev the engine.
		Velocity = FVector::ZeroVector;
		AngularVelocity = 0.0f;
		EngineSound->SetEngineState(Throttle * 0.6f, Throttle);
		return;
	}

	ResolvePenetration();

	FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	float ForwardSpeed = FVector::DotProduct(Velocity, Forward);

	// Yaw: steering asks for a turn rate; hits add spin on top which settles back over time. The handbrake lets
	// the back step out: much more rotation, even at a crawl, and the spin lingers (doughnuts).
	const bool bDriving = Throttle > 0.0f || Brake > 0.0f || bHandbrake;
	float SteerScale = FMath::Clamp(FMath::Abs(ForwardSpeed) / FullSteerSpeed, 0.0f, 1.0f);
	if (bDriving)
	{
		SteerScale = FMath::Max(SteerScale, MinSteerScale);
	}
	// Less turn rate as speed builds: the nose can't outrun the tyres at full speed (no snap oversteer).
	SteerScale *= FMath::Lerp(1.0f, HighSpeedTurnScale, FMath::Clamp(FMath::Abs(ForwardSpeed) / MaxSpeed, 0.0f, 1.0f));
	if (bHandbrake)
	{
		SteerScale = FMath::Max(SteerScale, 0.9f) * HandbrakeTurnBoost;
	}
	const bool bReversing = ForwardSpeed < -ReverseSteerSpeed || (Brake > 0.0f && Throttle <= 0.0f && ForwardSpeed <= 1.0f);
	const float SteerRate = Steer * TurnRate * SteerScale * (bReversing ? -1.0f : 1.0f);
	// Snappy while the stick is held (the player is in charge); spin from a hit only settles slowly when not steering.
	const bool bSteering = FMath::Abs(Steer) > 0.05f;
	const float YawResponse = bHandbrake ? SpinRecovery * 0.4f : (bSteering ? SteerResponse : SpinRecovery);
	AngularVelocity = FMath::FInterpTo(AngularVelocity, SteerRate, Dt, YawResponse);
	ApplyYaw(AngularVelocity * Dt);
	Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);

	// Longitudinal: throttle forward, brake then reverse, otherwise coast to a stop. A slipstream raises
	// both top speed and acceleration, which is what lets a following car close in and pass.
	const float TopSpeed = MaxSpeed * (1.0f + DraftTopSpeedBonus * DraftFactor);
	const float DriveAcceleration = Acceleration * (1.0f + DraftAccelerationBonus * DraftFactor);
	ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	ForwardSpeed += Throttle * DriveAcceleration * Dt;
	if (Brake > 0.0f)
	{
		ForwardSpeed -= Brake * (ForwardSpeed > 0.0f ? BrakeDeceleration : Acceleration * 0.6f) * Dt;
	}
	if ((Throttle <= 0.0f && Brake <= 0.0f) || bHandbrake)
	{
		const float Drag = (bHandbrake ? HandbrakeDrag : 0.0f) + ((Throttle <= 0.0f && Brake <= 0.0f) ? CoastDeceleration : 0.0f);
		ForwardSpeed = FMath::Sign(ForwardSpeed) * FMath::Max(FMath::Abs(ForwardSpeed) - Drag * Dt, 0.0f);
	}
	ForwardSpeed = FMath::Clamp(ForwardSpeed, -MaxReverseSpeed, TopSpeed);

	// Lateral: bleed off sideways slide. Low grip (handbrake) keeps the car sliding in its old direction.
	float LateralSpeed = FVector::DotProduct(Velocity, Right);
	const float SlideSpeed = FMath::Abs(LateralSpeed);
	LateralSpeed = FMath::FInterpTo(LateralSpeed, 0.0f, Dt, bHandbrake ? HandbrakeGrip : Grip);
	Velocity = Forward * ForwardSpeed + Right * LateralSpeed;

	// Move with sweep; on impact, bounce and spin, then slide along whatever we hit.
	const FVector Delta = Velocity * Dt;
	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		const FVector Normal = Hit.Normal.GetSafeNormal2D();
		const FVector ContactOffset = Hit.ImpactPoint - GetActorLocation();
		LastWallNormal = Normal;
		TimeSinceWallHit = 0.0f;

		float ClosingSpeed = 0.0f;
		if (ARaceCarPawn* OtherCar = Cast<ARaceCarPawn>(Hit.GetActor()))
		{
			// Car-to-car: share the impulse so both cars bounce apart, and both get spun by where they were hit.
			ClosingSpeed = FVector::DotProduct(Velocity - OtherCar->Velocity, Normal);
			if (ClosingSpeed < 0.0f)
			{
				const FVector Impulse = Normal * (-(1.0f + CarBounce) * ClosingSpeed * 0.5f);
				Velocity += Impulse;
				AddSpin(ContactOffset, Impulse);
				OtherCar->ApplyBump(-Impulse, Hit.ImpactPoint);
			}
		}
		else
		{
			ClosingSpeed = FVector::DotProduct(Velocity, Normal);
			if (ClosingSpeed < 0.0f)
			{
				const FVector Impulse = Normal * (-(1.0f + WallBounce) * ClosingSpeed);
				Velocity += Impulse;
				AddSpin(ContactOffset, Impulse);
			}
		}

		if (ClosingSpeed < -ImpactSoundMinSpeed && TimeSinceImpactSound >= ImpactSoundInterval)
		{
			TimeSinceImpactSound = 0.0f;
			EngineSound->TriggerImpact(FMath::Clamp(-ClosingSpeed / MaxSpeed * 1.5f, 0.2f, 1.0f));
		}

		const FVector Remaining = FVector::VectorPlaneProject(Delta * (1.0f - Hit.Time), Normal);
		AddActorWorldOffset(Remaining, true);
	}

	float Skid = FMath::Clamp((SlideSpeed - SkidStartSpeed) / SkidFullRange, 0.0f, 1.0f);
	if (bHandbrake && GetSpeed() > 150.0f)
	{
		Skid = FMath::Max(Skid, 0.5f);
	}
	EngineSound->SetEngineState(FMath::Abs(FVector::DotProduct(Velocity, Forward)) / MaxSpeed, FMath::Max(Throttle, Brake), Skid);
}
