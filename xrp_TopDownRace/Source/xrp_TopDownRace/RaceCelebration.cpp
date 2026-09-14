#include "RaceCelebration.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* MaterialPath = TEXT("/Game/RaceTrack/Materials/M_RaceLight.M_RaceLight");
	const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	// Confetti over the room floor (5 m x 6 m = 5000 x 6000 UU), starting below the Igloo eye point (1700 UU)
	// so nothing drifts right past the cameras.
	constexpr float RainHalfX = 2400.0f;
	constexpr float RainHalfY = 2900.0f;
	constexpr float RainTop = 1350.0f;
	constexpr float FloorZ = 6.0f;
	constexpr float RainSeconds = 6.0f;   // keep re-launching landed pieces this long
	constexpr int32 PiecesPerColour = 70;
	const FVector PieceSize(45.0f, 28.0f, 3.0f); // UU (4.5 x 2.8 cm in the room)

	// Unlit emissive colours, bright enough to pop against the grass and road.
	const FLinearColor ConfettiColours[] = {
		FLinearColor(3.0f, 0.15f, 0.1f), FLinearColor(3.0f, 2.4f, 0.1f), FLinearColor(0.2f, 0.9f, 3.0f),
		FLinearColor(0.3f, 3.0f, 0.4f), FLinearColor(3.0f, 0.4f, 2.2f), FLinearColor(3.0f, 3.0f, 3.0f),
	};

	constexpr float AnnouncementHeight = 150.0f; // just above the cars
	constexpr float AnnouncementSize = 320.0f;
	constexpr float AnnouncementTurnRate = 30.0f; // degrees per second
}

ARaceCelebration::ARaceCelebration()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Announcement = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Announcement"));
	Announcement->SetupAttachment(RootComponent);
	Announcement->SetHorizontalAlignment(EHTA_Center);
	Announcement->SetVerticalAlignment(EVRTA_TextCenter);
	Announcement->SetWorldSize(AnnouncementSize);
	Announcement->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Announcement->SetCastShadow(false);
	Announcement->SetVisibility(false);
}

void ARaceCelebration::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	if (!Material)
	{
		Material = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath);
	}
	if (!Cube || !Material)
	{
		return;
	}

	for (const FLinearColor& Colour : ConfettiColours)
	{
		UMaterialInstanceDynamic* ColourMaterial = UMaterialInstanceDynamic::Create(Material, this);
		ColourMaterial->SetVectorParameterValue(TEXT("Color"), Colour);
		ConfettiMaterials.Add(ColourMaterial);

		UInstancedStaticMeshComponent* Group = NewObject<UInstancedStaticMeshComponent>(this);
		Group->SetStaticMesh(Cube);
		Group->SetMaterial(0, ColourMaterial);
		Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Group->SetCastShadow(false);
		Group->SetMobility(EComponentMobility::Movable);
		Group->SetupAttachment(RootComponent);
		Group->RegisterComponent();
		Group->SetVisibility(false);
		ConfettiGroups.Add(Group);
		Pieces.AddDefaulted();
	}
}

void ARaceCelebration::LaunchPiece(FConfettiPiece& Piece, bool bSpreadHeight)
{
	Piece.Location = FVector(FMath::FRandRange(-RainHalfX, RainHalfX), FMath::FRandRange(-RainHalfY, RainHalfY),
		bSpreadHeight ? FMath::FRandRange(RainTop * 0.35f, RainTop) : RainTop + FMath::FRandRange(0.0f, 300.0f));
	Piece.Velocity = FVector(FMath::FRandRange(-40.0f, 40.0f), FMath::FRandRange(-40.0f, 40.0f), -FMath::FRandRange(140.0f, 240.0f));
	Piece.Rotation = FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f));
	Piece.Spin = FRotator(FMath::FRandRange(-360.0f, 360.0f), FMath::FRandRange(-180.0f, 180.0f), FMath::FRandRange(-360.0f, 360.0f));
	Piece.FlutterPhase = FMath::FRandRange(0.0f, UE_TWO_PI);
	Piece.bLanded = false;
}

void ARaceCelebration::Celebrate(const FString& Text, FColor Color)
{
	AnnouncementText = Text;
	Announcement->SetText(FText::FromString(Text));
	Announcement->SetTextRenderColor(Color);
	Announcement->SetVisibility(true);
	ShowTime = 0.0f;
	bShowing = true;

	for (int32 Group = 0; Group < ConfettiGroups.Num(); ++Group)
	{
		Pieces[Group].SetNum(PiecesPerColour);
		TransformScratch.Reset();
		for (FConfettiPiece& Piece : Pieces[Group])
		{
			LaunchPiece(Piece, true);
			TransformScratch.Add(FTransform(Piece.Rotation, Piece.Location, PieceSize / 100.0f));
		}
		ConfettiGroups[Group]->ClearInstances();
		ConfettiGroups[Group]->AddInstances(TransformScratch, false, false);
		ConfettiGroups[Group]->SetVisibility(true);
	}
}

void ARaceCelebration::Stop()
{
	bShowing = false;
	Announcement->SetVisibility(false);
	for (UInstancedStaticMeshComponent* Group : ConfettiGroups)
	{
		Group->ClearInstances();
		Group->SetVisibility(false);
	}
	for (TArray<FConfettiPiece>& Group : Pieces)
	{
		Group.Reset();
	}
}

void ARaceCelebration::UpdateConfetti(float DeltaSeconds)
{
	const bool bStillRaining = ShowTime < RainSeconds;
	for (int32 Group = 0; Group < ConfettiGroups.Num(); ++Group)
	{
		TransformScratch.Reset();
		for (FConfettiPiece& Piece : Pieces[Group])
		{
			if (Piece.bLanded && bStillRaining && FMath::FRand() < DeltaSeconds * 0.8f)
			{
				LaunchPiece(Piece, false); // keep the shower going for a while
			}
			if (!Piece.bLanded)
			{
				// Flutter: sideways sway while it tumbles down.
				Piece.FlutterPhase += DeltaSeconds * 3.0f;
				const FVector Sway(FMath::Sin(Piece.FlutterPhase) * 70.0f, FMath::Cos(Piece.FlutterPhase * 0.7f) * 50.0f, 0.0f);
				Piece.Location += (Piece.Velocity + Sway) * DeltaSeconds;
				Piece.Rotation += Piece.Spin * DeltaSeconds;
				if (Piece.Location.Z <= FloorZ)
				{
					// Settle flat on the floor.
					Piece.Location.Z = FloorZ;
					Piece.Rotation = FRotator(0.0f, Piece.Rotation.Yaw, 0.0f);
					Piece.bLanded = true;
				}
			}
			TransformScratch.Add(FTransform(Piece.Rotation, Piece.Location, PieceSize / 100.0f));
		}
		ConfettiGroups[Group]->BatchUpdateInstancesTransforms(0, TransformScratch, false, true, false);
	}
}

void ARaceCelebration::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bShowing)
	{
		return;
	}
	ShowTime += DeltaSeconds;

	// Lie flat above the middle of the track, turning slowly so it reads from every corner, with a gentle pulse.
	const float Yaw = ShowTime * AnnouncementTurnRate;
	const FVector TextUp = FRotator(0.0f, Yaw, 0.0f).Vector();
	Announcement->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, AnnouncementHeight),
		FRotationMatrix::MakeFromXZ(FVector::UpVector, TextUp).Rotator());
	Announcement->SetRelativeScale3D(FVector(1.0f + 0.08f * FMath::Sin(ShowTime * 6.0f)));

	UpdateConfetti(DeltaSeconds);
}
