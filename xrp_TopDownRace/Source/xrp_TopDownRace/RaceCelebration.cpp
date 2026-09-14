#include "RaceCelebration.h"

#include "Algo/Reverse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "RaceDotFont.h"

namespace
{
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* ConfettiMaterialPath = TEXT("/Game/RaceTrack/Materials/M_RaceLight.M_RaceLight");
	const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const TCHAR* LitMaterialPath = TEXT("/Game/LPRiverForest/Materials/Common/MI_LowPoly.MI_LowPoly");

	// Confetti over the room floor (5 m x 6 m = 5000 x 6000 UU), falling from the roof. The Igloo cameras sit at
	// the eye point above the room centre and each wall camera sees up to 45 degrees above it, so a piece at
	// horizontal distance D (along the wall's axis) comes into view below EyeHeight + D: spawning just above that
	// makes every piece appear where the wall meets the ceiling.
	constexpr float RainHalfX = 2400.0f;
	constexpr float RainHalfY = 2900.0f;
	constexpr float EyeHeight = 1700.0f;
	constexpr float EyeClearance = 500.0f;     // keep pieces from falling right past the cameras
	constexpr float RoofMargin = 120.0f;
	constexpr float FirstWaveStagger = 1300.0f; // extra height spread for the opening burst, so it pours in
	constexpr float RefillStagger = 400.0f;
	constexpr float FallSpeedMin = 420.0f;      // fast enough that pieces from the far walls land before the show ends
	constexpr float FallSpeedMax = 580.0f;
	constexpr float FloorZ = 6.0f;
	constexpr float RainSeconds = 7.0f;   // keep re-launching landed pieces this long
	constexpr int32 PiecesPerColour = 90;
	const FVector PieceSize(55.0f, 34.0f, 3.0f); // UU (5.5 x 3.4 cm in the room)

	// Unlit emissive colours, bright enough to pop against the grass and road.
	const FLinearColor ConfettiColours[] = {
		FLinearColor(3.0f, 0.15f, 0.1f), FLinearColor(3.0f, 2.4f, 0.1f), FLinearColor(0.2f, 0.9f, 3.0f),
		FLinearColor(0.3f, 3.0f, 0.4f), FLinearColor(3.0f, 0.4f, 2.2f), FLinearColor(3.0f, 3.0f, 3.0f),
	};

	// Stage: floats just above the kerb walls in the middle of the room and turns slowly.
	constexpr float StageZ = 64.0f;
	constexpr float StageRadius = 1100.0f;
	constexpr float StageThickness = 26.0f;
	constexpr float StageTurnRate = 12.0f;      // degrees per second
	constexpr float PopInSeconds = 0.7f;
	constexpr float LetterPixel = 19.0f;
	constexpr float LetterDepth = 30.0f;
	constexpr float LetterRowOffset = 640.0f;   // each line of block letters, from the stage centre
	constexpr float DigitPixel = 14.0f;
	constexpr float StepWidth = 300.0f;
	constexpr float StepDepth = 260.0f;
	constexpr float PlinthSize = 190.0f;
	constexpr float PlinthHeight = 60.0f;
	constexpr float TrophyScale = 1.7f;
	const FLinearColor GoldColor(1.0f, 0.62f, 0.1f);

	struct FPodiumStep
	{
		float X;
		float Height;
		float Grey;
		TCHAR Digit;
	};
	const FPodiumStep PodiumSteps[] = { { 0.0f, 150.0f, 0.85f, '1' }, { -StepWidth, 100.0f, 0.6f, '2' }, { StepWidth, 64.0f, 0.45f, '3' } };

	// Chequered flag, waved from a stand beside the finish line out over the road.
	constexpr float KerbWidth = 40.0f;
	constexpr float FlagSideGap = 240.0f;
	constexpr float FlagPivotHeight = 380.0f;
	constexpr float FlagPoleLength = 520.0f;
	constexpr float FlagAttachFrom = 0.3f;      // cloth along the outer part of the pole
	constexpr float FlagClothLength = 480.0f;
	constexpr float FlagSwingRate = 5.0f;       // radians per second
	constexpr float FlagSwingDegrees = 55.0f;
	constexpr float FlagPoleLift = 12.0f;       // degrees above horizontal
	constexpr float FlagDroop = 0.15f;          // how much the cloth hangs rather than flies (flatter reads from above)
	constexpr int32 FlagCellsAlong = 6;
	constexpr int32 FlagCellsOut = 8;

	/** Flat-shaded polygons for one procedural mesh section. */
	struct FMeshBuffers
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;

		void AddPolygon(TArray<FVector, TInlineAllocator<4>> Corners, const FVector& Normal)
		{
			// Drop repeated corners (e.g. where a lathe profile touches the axis).
			for (int32 Index = Corners.Num() - 1; Index > 0; --Index)
			{
				if (FVector::DistSquared(Corners[Index], Corners[Index - 1]) < 0.01)
				{
					Corners.RemoveAt(Index);
				}
			}
			if (Corners.Num() > 2 && FVector::DistSquared(Corners.Last(), Corners[0]) < 0.01)
			{
				Corners.Pop();
			}
			if (Corners.Num() < 3)
			{
				return;
			}
			// Front faces wind the opposite way to right-handed anticlockwise about the normal.
			const FVector Face = FVector::CrossProduct(Corners[1] - Corners[0], Corners[2] - Corners[0]);
			if ((Face | Normal) > 0.0)
			{
				Algo::Reverse(Corners);
			}
			const int32 Base = Vertices.Num();
			for (const FVector& Corner : Corners)
			{
				Vertices.Add(Corner);
				Normals.Add(Normal);
				UVs.Add(FVector2D(Corner.X / 100.0, Corner.Y / 100.0));
			}
			for (int32 Index = 1; Index + 1 < Corners.Num(); ++Index)
			{
				Triangles.Append({ Base, Base + Index, Base + Index + 1 });
			}
		}
	};

	void CreateSection(UProceduralMeshComponent* Mesh, int32 Section, const FMeshBuffers& Buffers)
	{
		Mesh->CreateMeshSection(Section, Buffers.Vertices, Buffers.Triangles, Buffers.Normals, Buffers.UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	}

	float EaseOutBack(float T)
	{
		const float Overshoot = 1.70158f;
		T -= 1.0f;
		return T * T * ((Overshoot + 1.0f) * T + Overshoot) + 1.0f;
	}
}

ARaceCelebration::ARaceCelebration()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Turntable = CreateDefaultSubobject<USceneComponent>(TEXT("Turntable"));
	Turntable->SetupAttachment(RootComponent);

	FlagRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FlagRoot"));
	FlagRoot->SetupAttachment(RootComponent);
}

UMaterialInstanceDynamic* ARaceCelebration::LitMaterial(const FLinearColor& Color, float Roughness, float Specular)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, LitMaterialPath);
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
	for (const TCHAR* Parameter : { TEXT("BaseColor"), TEXT("Highlight"), TEXT("Paint") })
	{
		Material->SetVectorParameterValue(Parameter, Color);
	}
	Material->SetVectorParameterValue(TEXT("AO"), Color * 0.75f);
	Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	Material->SetScalarParameterValue(TEXT("Specular"), Specular);
	Materials.Add(Material);
	return Material;
}

UStaticMeshComponent* ARaceCelebration::AddMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& Transform, UMaterialInterface* Material, bool bCastShadow)
{
	if (!Mesh)
	{
		return nullptr;
	}
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetMaterial(0, Material);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(bCastShadow);
	Component->SetupAttachment(Parent);
	Component->SetRelativeTransform(Transform);
	Component->RegisterComponent();
	Parts.Add(Component);
	return Component;
}

UInstancedStaticMeshComponent* ARaceCelebration::AddVoxels(USceneComponent* Parent, UMaterialInterface* Material)
{
	UInstancedStaticMeshComponent* Voxels = NewObject<UInstancedStaticMeshComponent>(this);
	Voxels->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubePath));
	Voxels->SetMaterial(0, Material);
	Voxels->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Voxels->SetCastShadow(true);
	Voxels->SetupAttachment(Parent);
	Voxels->RegisterComponent();
	Parts.Add(Voxels);
	return Voxels;
}

void ARaceCelebration::BeginPlay()
{
	Super::BeginPlay();
	Turntable->SetRelativeLocation(FVector(0.0f, 0.0f, StageZ));

	// Confetti: one instanced group per colour.
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	UMaterialInterface* ConfettiBase = LoadObject<UMaterialInterface>(nullptr, ConfettiMaterialPath);
	if (!ConfettiBase)
	{
		ConfettiBase = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath);
	}
	if (Cube && ConfettiBase)
	{
		for (const FLinearColor& Colour : ConfettiColours)
		{
			UMaterialInstanceDynamic* ColourMaterial = UMaterialInstanceDynamic::Create(ConfettiBase, this);
			ColourMaterial->SetVectorParameterValue(TEXT("Color"), Colour);
			Materials.Add(ColourMaterial);

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

	BuildStage();
	BuildTrophy();

	// Flag: a stand, a pole and the chequered cloth (white and black squares as two sections, both sides drawn).
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderPath);
	AddMesh(FlagRoot, Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, FlagPivotHeight * 0.5f), FVector(0.14f, 0.14f, FlagPivotHeight / 100.0f)),
		LitMaterial(FLinearColor(0.12f, 0.12f, 0.13f), 0.6f), true);
	FlagPole = AddMesh(FlagRoot, Cylinder, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.07f, 0.07f, FlagPoleLength / 100.0f)),
		LitMaterial(FLinearColor(0.75f, 0.75f, 0.78f), 0.35f, 0.8f), true);
	FlagCloth = NewObject<UProceduralMeshComponent>(this);
	FlagCloth->SetupAttachment(FlagRoot);
	FlagCloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlagCloth->RegisterComponent();
	FlagCloth->SetMaterial(0, LitMaterial(FLinearColor(0.9f, 0.9f, 0.9f), 0.8f));
	FlagCloth->SetMaterial(1, LitMaterial(FLinearColor(0.015f, 0.015f, 0.015f), 0.8f));
	Parts.Add(FlagCloth);

	Turntable->SetVisibility(false, true);
	FlagRoot->SetVisibility(false, true);
}

void ARaceCelebration::BuildStage()
{
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderPath);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	UMaterialInterface* Gold = LitMaterial(GoldColor, 0.3f, 0.9f);

	// Dark round stage with a gold rim.
	AddMesh(Turntable, Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, StageThickness * 0.35f),
		FVector((StageRadius + 35.0f) / 50.0f, (StageRadius + 35.0f) / 50.0f, StageThickness * 0.7f / 100.0f)), Gold, true);
	AddMesh(Turntable, Cylinder, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, StageThickness * 0.5f),
		FVector(StageRadius / 50.0f, StageRadius / 50.0f, StageThickness / 100.0f)), LitMaterial(FLinearColor(0.02f, 0.025f, 0.035f), 0.7f), true);

	// Podium: 2nd | 1st | 3rd, with gold place numbers on top.
	UInstancedStaticMeshComponent* Digits = AddVoxels(Turntable, Gold);
	for (const FPodiumStep& Step : PodiumSteps)
	{
		AddMesh(Turntable, Cube, FTransform(FRotator::ZeroRotator, FVector(Step.X, 0.0f, StageThickness + Step.Height * 0.5f),
			FVector(StepWidth, StepDepth, Step.Height) / 100.0f), LitMaterial(FLinearColor(Step.Grey, Step.Grey, Step.Grey + 0.03f), 0.6f), true);
		if (Step.Digit == '1')
		{
			continue; // the trophy stands on the top step
		}
		for (int32 GlyphY = 0; GlyphY < RaceDotFont::GlyphHeight; ++GlyphY)
		{
			for (int32 GlyphX = 0; GlyphX < RaceDotFont::GlyphWidth; ++GlyphX)
			{
				if (RaceDotFont::IsLit(Step.Digit, GlyphX, GlyphY))
				{
					const FVector Location(Step.X - (GlyphX - 2) * DigitPixel, (3 - GlyphY) * DigitPixel, StageThickness + Step.Height + 4.0f);
					Digits->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(DigitPixel * 0.92f, DigitPixel * 0.92f, 8.0f) / 100.0f));
				}
			}
		}
	}

	// Black plinth for the trophy on the top step.
	AddMesh(Turntable, Cube, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, StageThickness + PodiumSteps[0].Height + PlinthHeight * 0.5f),
		FVector(PlinthSize, PlinthSize, PlinthHeight) / 100.0f), LitMaterial(FLinearColor(0.02f, 0.02f, 0.02f), 0.4f, 0.7f), true);

	NameMaterial = LitMaterial(FLinearColor::White, 0.45f, 0.7f);
	NameLetters = AddVoxels(Turntable, NameMaterial);
	GoldLetters = AddVoxels(Turntable, Gold);
}

void ARaceCelebration::BuildTrophy()
{
	Trophy = NewObject<UProceduralMeshComponent>(this);
	Trophy->SetupAttachment(Turntable);
	Trophy->SetRelativeLocation(FVector(0.0f, 0.0f, StageThickness + PodiumSteps[0].Height + PlinthHeight));
	Trophy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trophy->RegisterComponent();
	Parts.Add(Trophy);

	FMeshBuffers Gold;

	// Cup: a profile (radius, height) revolved in 12 flat facets. Order: up the outside, across the rim, down inside.
	const FVector2D Profile[] = {
		{ 80, 0 }, { 80, 18 }, { 42, 28 }, { 26, 48 }, { 18, 90 }, { 18, 135 }, { 38, 148 }, { 22, 162 }, { 30, 180 },
		{ 72, 212 }, { 102, 252 }, { 116, 300 }, { 122, 328 }, { 106, 328 }, { 98, 300 }, { 72, 252 }, { 40, 230 }, { 0, 222 },
	};
	constexpr int32 Facets = 12;
	auto OnCup = [](const FVector2D& RadiusHeight, float Angle)
	{
		return FVector(RadiusHeight.X * FMath::Cos(Angle), RadiusHeight.X * FMath::Sin(Angle), RadiusHeight.Y) * TrophyScale;
	};
	for (int32 Index = 0; Index + 1 < UE_ARRAY_COUNT(Profile); ++Index)
	{
		const FVector2D& From = Profile[Index];
		const FVector2D& To = Profile[Index + 1];
		for (int32 Facet = 0; Facet < Facets; ++Facet)
		{
			const float Angle0 = UE_TWO_PI * Facet / Facets;
			const float Angle1 = UE_TWO_PI * (Facet + 1) / Facets;
			const float Middle = (Angle0 + Angle1) * 0.5f;
			const FVector Radial(FMath::Cos(Middle), FMath::Sin(Middle), 0.0f);
			// Outward for the outside, up for the rim, towards the axis for the inside.
			const FVector Normal = (Radial * (To.Y - From.Y) - FVector::UpVector * (To.X - From.X)).GetSafeNormal();
			Gold.AddPolygon({ OnCup(From, Angle0), OnCup(From, Angle1), OnCup(To, Angle1), OnCup(To, Angle0) }, Normal);
		}
	}

	// Handles: a square tube bent round an arc on each side, both ends buried in the cup.
	constexpr int32 ArcSteps = 9;
	constexpr float ArcRadius = 60.0f;
	constexpr float Tube = 9.0f;
	for (const float Side : { 1.0f, -1.0f })
	{
		const FVector ArcCentre(Side * 100.0f, 0.0f, 260.0f);
		auto Ring = [&](int32 Step, TArray<FVector, TInlineAllocator<4>>& OutCorners, FVector& OutAxis)
		{
			const float Angle = FMath::DegreesToRadians(FMath::Lerp(-120.0f, 95.0f, float(Step) / ArcSteps));
			const FVector Radial(Side * FMath::Cos(Angle), 0.0f, FMath::Sin(Angle));
			const FVector Across(0.0f, 1.0f, 0.0f);
			OutAxis = ArcCentre + Radial * ArcRadius;
			OutCorners = { OutAxis + (Radial + Across) * Tube, OutAxis + (-Radial + Across) * Tube, OutAxis + (-Radial - Across) * Tube, OutAxis + (Radial - Across) * Tube };
		};
		for (int32 Step = 0; Step < ArcSteps; ++Step)
		{
			TArray<FVector, TInlineAllocator<4>> RingA;
			TArray<FVector, TInlineAllocator<4>> RingB;
			FVector AxisA;
			FVector AxisB;
			Ring(Step, RingA, AxisA);
			Ring(Step + 1, RingB, AxisB);
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const int32 Next = (Corner + 1) % 4;
				const FVector Centre = (RingA[Corner] + RingA[Next] + RingB[Next] + RingB[Corner]) * 0.25f;
				const FVector Normal = (Centre - (AxisA + AxisB) * 0.5f).GetSafeNormal();
				Gold.AddPolygon({ RingA[Corner] * TrophyScale, RingA[Next] * TrophyScale, RingB[Next] * TrophyScale, RingB[Corner] * TrophyScale }, Normal);
			}
		}
	}

	CreateSection(Trophy, 0, Gold);
	Trophy->SetMaterial(0, LitMaterial(GoldColor, 0.3f, 0.9f));
}

void ARaceCelebration::BuildLetters(const FString& WinnerName)
{
	if (!NameLetters || !GoldLetters)
	{
		return;
	}
	NameLetters->ClearInstances();
	GoldLetters->ClearInstances();

	// "<NAME> WINS!" lying on the stage in front of the podium, and again on the far side turned round.
	const FString Words = WinnerName + TEXT(" WINS!");
	const float HalfWidth = RaceDotFont::TextWidth(Words) * LetterPixel * 0.5f;
	const FVector PixelScale = FVector(LetterPixel * 0.94f, LetterPixel * 0.94f, LetterDepth) / 100.0f;
	for (const float Side : { 1.0f, -1.0f })
	{
		for (int32 Index = 0; Index < Words.Len(); ++Index)
		{
			for (int32 GlyphY = 0; GlyphY < RaceDotFont::GlyphHeight; ++GlyphY)
			{
				for (int32 GlyphX = 0; GlyphX < RaceDotFont::GlyphWidth; ++GlyphX)
				{
					if (!RaceDotFont::IsLit(Words[Index], GlyphX, GlyphY))
					{
						continue;
					}
					// Reading along -X for someone on the -Y side (tops of the letters away from them); seen from above
					// (the floor camera) that reads the right way round, not mirrored.
					FVector Location(HalfWidth - (Index * RaceDotFont::Advance + GlyphX + 0.5f) * LetterPixel,
						-LetterRowOffset + (RaceDotFont::GlyphHeight * 0.5f - GlyphY - 0.5f) * LetterPixel,
						StageThickness + LetterDepth * 0.5f + 1.0f);
					if (Side > 0.0f)
					{
						Location.X = -Location.X;
						Location.Y = -Location.Y;
					}
					(Index < WinnerName.Len() ? NameLetters : GoldLetters)->AddInstance(FTransform(FRotator::ZeroRotator, Location, PixelScale));
				}
			}
		}
	}
}

void ARaceCelebration::UpdateFlag(float Time)
{
	if (!FlagCloth || !FlagPole)
	{
		return;
	}
	// The pole sweeps side to side over the road; the cloth trails the swing and hangs as it turns.
	const float Swing = FMath::Sin(Time * FlagSwingRate);
	const float Trail = -FMath::Cos(Time * FlagSwingRate);
	const FVector Direction = FRotator(FlagPoleLift, Swing * FlagSwingDegrees, 0.0f).Vector();
	const FVector Pivot(0.0f, 0.0f, FlagPivotHeight);
	FlagPole->SetRelativeLocationAndRotation(Pivot + Direction * FlagPoleLength * 0.5f, FRotationMatrix::MakeFromZ(Direction).Rotator());

	const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
	const FVector Out = (Side * Trail + FVector::DownVector * FlagDroop).GetSafeNormal();
	const FVector ClothNormal = FVector::CrossProduct(Direction, Out).GetSafeNormal();
	auto ClothPoint = [&](int32 Along, int32 Outward)
	{
		const float U = float(Along) / FlagCellsAlong;
		const float V = float(Outward) / FlagCellsOut;
		const FVector Attach = Pivot + Direction * FlagPoleLength * (FlagAttachFrom + (1.0f - FlagAttachFrom) * U);
		const float Ripple = FMath::Sin(V * 3.5f * UE_PI - Time * 9.0f + U * 2.0f) * 36.0f * V;
		return Attach + Out * FlagClothLength * V + ClothNormal * Ripple;
	};

	FMeshBuffers Squares[2];
	for (int32 Along = 0; Along < FlagCellsAlong; ++Along)
	{
		for (int32 Outward = 0; Outward < FlagCellsOut; ++Outward)
		{
			const FVector A = ClothPoint(Along, Outward);
			const FVector B = ClothPoint(Along + 1, Outward);
			const FVector C = ClothPoint(Along + 1, Outward + 1);
			const FVector D = ClothPoint(Along, Outward + 1);
			FVector Normal = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
			if (Normal.IsNearlyZero())
			{
				Normal = ClothNormal;
			}
			FMeshBuffers& Square = Squares[(Along + Outward) % 2];
			Square.AddPolygon({ A, B, C, D }, Normal);
			Square.AddPolygon({ A, B, C, D }, -Normal);
		}
	}
	CreateSection(FlagCloth, 0, Squares[0]);
	CreateSection(FlagCloth, 1, Squares[1]);
}

void ARaceCelebration::LaunchPiece(FConfettiPiece& Piece, bool bFirstWave)
{
	float X = 0.0f;
	float Y = 0.0f;
	float WallAxisDistance = 0.0f;
	do
	{
		X = FMath::FRandRange(-RainHalfX, RainHalfX);
		Y = FMath::FRandRange(-RainHalfY, RainHalfY);
		WallAxisDistance = FMath::Max(FMath::Abs(X), FMath::Abs(Y));
	}
	while (WallAxisDistance < EyeClearance);

	const float Stagger = FMath::FRandRange(0.0f, bFirstWave ? FirstWaveStagger : RefillStagger);
	Piece.Location = FVector(X, Y, EyeHeight + WallAxisDistance + RoofMargin + Stagger);
	Piece.Velocity = FVector(FMath::FRandRange(-40.0f, 40.0f), FMath::FRandRange(-40.0f, 40.0f), -FMath::FRandRange(FallSpeedMin, FallSpeedMax));
	Piece.Rotation = FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f));
	Piece.Spin = FRotator(FMath::FRandRange(-360.0f, 360.0f), FMath::FRandRange(-180.0f, 180.0f), FMath::FRandRange(-360.0f, 360.0f));
	Piece.FlutterPhase = FMath::FRandRange(0.0f, UE_TWO_PI);
	Piece.bLanded = false;
}

void ARaceCelebration::Celebrate(const FString& WinnerName, FColor Color, const FVector2D& FinishLine, const FVector2D& FinishDirection, float RoadHalfWidth)
{
	ShowTime = 0.0f;
	bShowing = true;

	if (NameMaterial)
	{
		const FLinearColor Linear(Color);
		for (const TCHAR* Parameter : { TEXT("BaseColor"), TEXT("Highlight"), TEXT("Paint") })
		{
			NameMaterial->SetVectorParameterValue(Parameter, Linear);
		}
		NameMaterial->SetVectorParameterValue(TEXT("AO"), Linear * 0.75f);
	}
	BuildLetters(WinnerName);
	Turntable->SetRelativeScale3D(FVector(0.01f));
	Turntable->SetVisibility(true, true);

	// Flag stand beside the finish line, facing out over the road.
	const FVector2D Across(-FinishDirection.Y, FinishDirection.X);
	const FVector2D Base = FinishLine + Across * (RoadHalfWidth + KerbWidth + FlagSideGap);
	FlagRoot->SetWorldLocationAndRotation(FVector(Base, 0.0), FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(-Across.Y, -Across.X)), 0.0f));
	UpdateFlag(0.0f);
	FlagRoot->SetVisibility(true, true);

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
	Turntable->SetVisibility(false, true);
	FlagRoot->SetVisibility(false, true);
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

	// The stage pops up and turns slowly, so the name reads from every corner in turn.
	Turntable->SetRelativeScale3D(FVector(FMath::Max(EaseOutBack(FMath::Clamp(ShowTime / PopInSeconds, 0.0f, 1.0f)), 0.01f)));
	Turntable->SetRelativeRotation(FRotator(0.0f, ShowTime * StageTurnRate, 0.0f));

	UpdateFlag(ShowTime);
	UpdateConfetti(DeltaSeconds);
}
