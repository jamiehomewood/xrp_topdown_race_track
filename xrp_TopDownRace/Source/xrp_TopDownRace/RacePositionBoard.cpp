#include "RacePositionBoard.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RaceDotFont.h"

namespace
{
	const TCHAR* PlanePath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* LedMaterialPath = TEXT("/Game/RaceTrack/Materials/M_RaceLight.M_RaceLight");                 // unlit; "Color" = emissive
	const TCHAR* StructureMaterialPath = TEXT("/Game/LPRiverForest/Materials/Common/MI_LowPoly.MI_LowPoly");   // lit low-poly

	constexpr float DotFill = 0.74f;         // LED size / dot spacing
	constexpr float CarColorBrightness = 4.0f;
	constexpr float OffDotDepth = 1.0f;      // in front of the housing
	constexpr float LitDotDepth = 2.5f;      // in front of the unlit dots

	FLinearColor Palette(ERaceBoardColor Color)
	{
		switch (Color)
		{
		case ERaceBoardColor::White: return FLinearColor(4.2f, 4.2f, 3.9f);
		case ERaceBoardColor::Amber: return FLinearColor(6.0f, 2.3f, 0.2f);
		case ERaceBoardColor::Green: return FLinearColor(0.5f, 5.0f, 0.6f);
		case ERaceBoardColor::Purple: return FLinearColor(3.0f, 0.5f, 5.5f);
		case ERaceBoardColor::Red: return FLinearColor(6.0f, 0.3f, 0.2f);
		case ERaceBoardColor::Gold: return FLinearColor(6.0f, 3.6f, 0.4f);
		case ERaceBoardColor::Dim: return FLinearColor(0.45f, 0.38f, 0.28f);
		default: return FLinearColor(0.03f, 0.028f, 0.025f); // an unlit LED
		}
	}

	/** The engine plane faces +Z; turn it to face the board's +X. */
	const FQuat DotRotation = FRotationMatrix::MakeFromZX(FVector::ForwardVector, FVector::UpVector).ToQuat();
}

ARacePositionBoard::ARacePositionBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Canvas.Init(uint8(ERaceBoardColor::Off), Columns * Rows);
	for (FLinearColor& Color : CarColors)
	{
		Color = FLinearColor::White;
	}
}

UMaterialInterface* ARacePositionBoard::LedMaterial(ERaceBoardColor Color)
{
	const int32 Index = int32(Color);
	if (LedMaterials.Num() < int32(ERaceBoardColor::Count))
	{
		LedMaterials.SetNum(int32(ERaceBoardColor::Count));
	}
	if (!LedMaterials[Index])
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, LedMaterialPath);
		if (!Base)
		{
			return nullptr;
		}
		LedMaterials[Index] = UMaterialInstanceDynamic::Create(Base, this);
		const bool bCar = Color >= ERaceBoardColor::Car1 && Color <= ERaceBoardColor::Car4;
		LedMaterials[Index]->SetVectorParameterValue(TEXT("Color"),
			bCar ? CarColors[Index - int32(ERaceBoardColor::Car1)] * CarColorBrightness : Palette(Color));
	}
	return LedMaterials[Index];
}

UMaterialInterface* ARacePositionBoard::StructureMaterial(const FLinearColor& Color)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, StructureMaterialPath);
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
	for (const TCHAR* Parameter : { TEXT("BaseColor"), TEXT("Highlight"), TEXT("Paint") })
	{
		Material->SetVectorParameterValue(Parameter, Color);
	}
	Material->SetVectorParameterValue(TEXT("AO"), Color * 0.8f);
	Material->SetScalarParameterValue(TEXT("Roughness"), 0.55f);
	StructureMaterials.Add(Material);
	return Material;
}

void ARacePositionBoard::SetCarColor(int32 Slot, const FLinearColor& Color)
{
	if (Slot < 0 || Slot >= UE_ARRAY_COUNT(CarColors))
	{
		return;
	}
	CarColors[Slot] = Color;
	const int32 Index = int32(ERaceBoardColor::Car1) + Slot;
	if (LedMaterials.IsValidIndex(Index) && LedMaterials[Index])
	{
		LedMaterials[Index]->SetVectorParameterValue(TEXT("Color"), Color * CarColorBrightness);
	}
}

void ARacePositionBoard::AddFace(const FTransform& FaceTransform, float Width, float GroundZ)
{
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, PlanePath);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	if (!Plane || !Cube)
	{
		return;
	}

	FFace& Face = Faces.AddDefaulted_GetRef();
	Face.Pitch = Width / Columns;
	const float Pitch = Face.Pitch;
	const float DotsHeight = Pitch * Rows;

	USceneComponent* FaceRoot = NewObject<USceneComponent>(this);
	FaceRoot->SetupAttachment(RootComponent);
	FaceRoot->SetRelativeTransform(FaceTransform);
	FaceRoot->RegisterComponent();
	FaceRoots.Add(FaceRoot);

	auto AddBlock = [&](const FVector& Centre, const FVector& Size, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this);
		Block->SetStaticMesh(Cube);
		Block->SetMaterial(0, Material);
		Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Block->SetCastShadow(false);
		Block->SetupAttachment(FaceRoot);
		Block->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Centre, Size / 100.0f));
		Block->RegisterComponent();
		Structure.Add(Block);
	};

	// Black housing, metal frame with a red top rail, and two legs down to the ground.
	const float HalfW = Width * 0.5f + Pitch * 3.0f;
	const float HalfH = DotsHeight * 0.5f + Pitch * 3.0f;
	const float Rail = Pitch * 1.6f;
	AddBlock(FVector(-25.0f, 0.0f, 0.0f), FVector(40.0f, HalfW * 2.0f, HalfH * 2.0f), StructureMaterial(FLinearColor(0.012f, 0.012f, 0.015f)));
	UMaterialInterface* Metal = StructureMaterial(FLinearColor(0.22f, 0.23f, 0.25f));
	AddBlock(FVector(-8.0f, 0.0f, HalfH + Rail * 0.5f), FVector(34.0f, HalfW * 2.0f + Rail * 2.0f, Rail), StructureMaterial(FLinearColor(0.55f, 0.02f, 0.02f)));
	AddBlock(FVector(-8.0f, 0.0f, -HalfH - Rail * 0.5f), FVector(34.0f, HalfW * 2.0f + Rail * 2.0f, Rail), Metal);
	for (const float Side : { -1.0f, 1.0f })
	{
		AddBlock(FVector(-8.0f, Side * (HalfW + Rail * 0.5f), 0.0f), FVector(34.0f, Rail, HalfH * 2.0f), Metal);
	}
	const float LegTop = -HalfH - Rail;
	const float LegLength = (FaceTransform.GetLocation().Z - GroundZ) + LegTop;
	if (LegLength > 0.0f)
	{
		UMaterialInterface* LegMaterial = StructureMaterial(FLinearColor(0.08f, 0.08f, 0.09f));
		for (const float Side : { -1.0f, 1.0f })
		{
			AddBlock(FVector(-25.0f, Side * Width * 0.35f, LegTop - LegLength * 0.5f), FVector(40.0f, 70.0f, LegLength), LegMaterial);
		}
	}

	auto NewDots = [&](ERaceBoardColor Color)
	{
		UInstancedStaticMeshComponent* Dots = NewObject<UInstancedStaticMeshComponent>(this);
		Dots->SetStaticMesh(Plane);
		Dots->SetMaterial(0, LedMaterial(Color));
		Dots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dots->SetCastShadow(false);
		Dots->SetupAttachment(FaceRoot);
		Dots->RegisterComponent();
		DotGroups.Add(Dots);
		return Dots;
	};

	// The whole matrix of unlit LEDs, then one group per lit colour just in front of it.
	TArray<FTransform> AllDots;
	AllDots.Reserve(Columns * Rows);
	for (int32 Y = 0; Y < Rows; ++Y)
	{
		for (int32 X = 0; X < Columns; ++X)
		{
			AllDots.Add(DotTransform(Face, X, Y, OffDotDepth));
		}
	}
	NewDots(ERaceBoardColor::Off)->AddInstances(AllDots, false, false);

	Face.LitDots.SetNum(int32(ERaceBoardColor::Count));
	for (int32 Color = 1; Color < int32(ERaceBoardColor::Count); ++Color)
	{
		Face.LitDots[Color] = NewDots(ERaceBoardColor(Color));
	}
	Shown.Reset(); // redraw everything on the next Commit
}

FTransform ARacePositionBoard::DotTransform(const FFace& Face, int32 X, int32 Y, float Depth) const
{
	// Face space: X towards the viewers, Y to the viewers' left, Z up. Column 0 is on the viewers' left, row 0 at the top.
	const FVector Location(Depth, (Columns * 0.5f - X - 0.5f) * Face.Pitch, (Rows * 0.5f - Y - 0.5f) * Face.Pitch);
	return FTransform(DotRotation, Location, FVector(Face.Pitch * DotFill / 100.0f));
}

void ARacePositionBoard::Clear()
{
	Canvas.Init(uint8(ERaceBoardColor::Off), Columns * Rows);
}

void ARacePositionBoard::FillRect(int32 X, int32 Y, int32 Width, int32 Height, ERaceBoardColor Color)
{
	for (int32 Row = FMath::Max(Y, 0); Row < FMath::Min(Y + Height, Rows); ++Row)
	{
		for (int32 Column = FMath::Max(X, 0); Column < FMath::Min(X + Width, Columns); ++Column)
		{
			Canvas[Row * Columns + Column] = uint8(Color);
		}
	}
}

void ARacePositionBoard::DrawText(int32 X, int32 Y, const FString& Text, ERaceBoardColor Color, int32 Scale)
{
	Scale = FMath::Max(Scale, 1);
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		const int32 Left = X + Index * RaceDotFont::Advance * Scale;
		for (int32 GlyphY = 0; GlyphY < RaceDotFont::GlyphHeight; ++GlyphY)
		{
			for (int32 GlyphX = 0; GlyphX < RaceDotFont::GlyphWidth; ++GlyphX)
			{
				if (RaceDotFont::IsLit(Text[Index], GlyphX, GlyphY))
				{
					FillRect(Left + GlyphX * Scale, Y + GlyphY * Scale, Scale, Scale, Color);
				}
			}
		}
	}
}

void ARacePositionBoard::DrawTextRight(int32 RightX, int32 Y, const FString& Text, ERaceBoardColor Color, int32 Scale)
{
	DrawText(RightX - RaceDotFont::TextWidth(Text, Scale), Y, Text, Color, Scale);
}

void ARacePositionBoard::DrawTextCentred(int32 CentreX, int32 Y, const FString& Text, ERaceBoardColor Color, int32 Scale)
{
	DrawText(CentreX - RaceDotFont::TextWidth(Text, Scale) / 2, Y, Text, Color, Scale);
}

void ARacePositionBoard::Commit()
{
	if (Canvas == Shown)
	{
		return;
	}
	Shown = Canvas;

	TArray<TArray<FTransform>> ByColor;
	ByColor.SetNum(int32(ERaceBoardColor::Count));
	for (FFace& Face : Faces)
	{
		for (TArray<FTransform>& Transforms : ByColor)
		{
			Transforms.Reset();
		}
		for (int32 Y = 0; Y < Rows; ++Y)
		{
			for (int32 X = 0; X < Columns; ++X)
			{
				if (const uint8 Color = Canvas[Y * Columns + X])
				{
					ByColor[Color].Add(DotTransform(Face, X, Y, LitDotDepth));
				}
			}
		}
		for (int32 Color = 1; Color < int32(ERaceBoardColor::Count); ++Color)
		{
			if (UInstancedStaticMeshComponent* Dots = Face.LitDots.IsValidIndex(Color) ? Face.LitDots[Color] : nullptr)
			{
				Dots->ClearInstances();
				if (ByColor[Color].Num() > 0)
				{
					Dots->AddInstances(ByColor[Color], false, false);
				}
			}
		}
	}
}
