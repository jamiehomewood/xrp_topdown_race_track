#include "RaceDisplay.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// Same unlit material as the start lights (fallback: engine shape material), driven to a near-black grey.
	const TCHAR* BackingMaterialPath = TEXT("/Game/RaceTrack/Materials/M_RaceLight.M_RaceLight");
	const TCHAR* BackingFallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const FLinearColor BackingColor(0.012f, 0.012f, 0.016f);
	constexpr float BackingThickness = 6.0f;
}

ARaceDisplay::ARaceDisplay()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

int32 ARaceDisplay::AddFloorLine(const FVector& Location, const FVector& UpDirection, float TextHeight)
{
	// A text render faces its +X with its top along +Z: face the sky, top pointing along UpDirection.
	const FRotator Rotation = FRotationMatrix::MakeFromXZ(FVector::UpVector, UpDirection.GetSafeNormal2D()).Rotator();
	return AddLine(Location, Rotation, TextHeight);
}

int32 ARaceDisplay::AddWallLine(const FVector& Location, const FVector& FacingDirection, float TextHeight)
{
	const FRotator Rotation = FRotationMatrix::MakeFromXZ(FacingDirection.GetSafeNormal2D(), FVector::UpVector).Rotator();
	return AddLine(Location, Rotation, TextHeight);
}

void ARaceDisplay::AddFloorBacking(const FVector& Location, const FVector& UpDirection, float Width, float Depth)
{
	AddBacking(Location, FRotationMatrix::MakeFromXZ(FVector::UpVector, UpDirection.GetSafeNormal2D()).Rotator(), Width, Depth);
}

void ARaceDisplay::AddWallBacking(const FVector& Location, const FVector& FacingDirection, float Width, float Height)
{
	const FVector Facing = FacingDirection.GetSafeNormal2D();
	AddBacking(Location - Facing * 25.0f, FRotationMatrix::MakeFromXZ(Facing, FVector::UpVector).Rotator(), Width, Height);
}

void ARaceDisplay::AddBacking(const FVector& Location, const FRotator& Rotation, float Width, float Height)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	if (!BackingMaterial)
	{
		UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, BackingMaterialPath);
		if (!Material)
		{
			Material = LoadObject<UMaterialInterface>(nullptr, BackingFallbackMaterialPath);
		}
		if (Material)
		{
			BackingMaterial = UMaterialInstanceDynamic::Create(Material, this);
			BackingMaterial->SetVectorParameterValue(TEXT("Color"), BackingColor);
		}
	}
	if (!Cube || !BackingMaterial)
	{
		return;
	}

	// Engine cube is 100 UU, centred. Local X = thickness (towards the viewer), Y = width, Z = height.
	UStaticMeshComponent* Board = NewObject<UStaticMeshComponent>(this);
	Board->SetStaticMesh(Cube);
	Board->SetMaterial(0, BackingMaterial);
	Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Board->SetCastShadow(false);
	Board->SetupAttachment(RootComponent);
	Board->SetRelativeTransform(FTransform(Rotation, Location, FVector(BackingThickness / 100.0f, Width / 100.0f, Height / 100.0f)));
	Board->RegisterComponent();
}

int32 ARaceDisplay::AddLine(const FVector& Location, const FRotator& Rotation, float TextHeight)
{
	UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
	Text->SetupAttachment(RootComponent);
	Text->SetRelativeLocationAndRotation(Location, Rotation);
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetWorldSize(TextHeight);
	Text->SetCastShadow(false);
	Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Text->SetText(FText::GetEmpty());
	Text->RegisterComponent();

	LineText.Add(FString());
	LineColor.Add(FColor::White);
	return Lines.Add(Text);
}

void ARaceDisplay::SetLine(int32 LineIndex, const FString& Text, FColor Color)
{
	if (!Lines.IsValidIndex(LineIndex) || !Lines[LineIndex])
	{
		return;
	}
	if (LineText[LineIndex] != Text)
	{
		LineText[LineIndex] = Text;
		Lines[LineIndex]->SetText(FText::FromString(Text));
	}
	if (LineColor[LineIndex] != Color)
	{
		LineColor[LineIndex] = Color;
		Lines[LineIndex]->SetTextRenderColor(Color);
	}
}
