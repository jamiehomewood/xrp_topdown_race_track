#include "RaceTrackBuilder.h"

#include "Algo/Reverse.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "RaceTrackPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaceTrack, Log, All);

namespace
{
	// Grey materials made by Tools/TrackGen/build_track.py.
	const TCHAR* RoadMaterialPath = TEXT("/Game/RaceTrack/Materials/MI_Track_Road.MI_Track_Road");
	const TCHAR* WallMaterialPath = TEXT("/Game/RaceTrack/Materials/MI_Track_Wall.MI_Track_Wall");
	const TCHAR* LineMaterialPath = TEXT("/Game/RaceTrack/Materials/MI_Track_Line.MI_Track_Line");

	constexpr int32 RoadSection = 0;
	constexpr int32 WallSection = 1;
	constexpr int32 LineSection = 2;
	constexpr float RoadZ = 2.0f;          // just above the ground
	constexpr float LineZ = 3.0f;
	constexpr float LineDepth = 80.0f;

	// Room floor (5 x 6 m) and the tags Tools/TrackGen puts on the level's own track and scenery.
	constexpr float RoomHalfX = 2500.0f;
	constexpr float RoomHalfY = 3000.0f;
	const FName LevelTrackTag(TEXT("TrackGen"));
	const FName LevelSceneryTag(TEXT("TrackGenScenery"));

	constexpr float FenceOffset = 90.0f;       // fence line beyond the outer kerb wall
	constexpr float FloorPropMargin = 60.0f;   // from the room edge
	constexpr float FloorPropTrackGap = 130.0f;

	// Streams: a strip of the pack's river plane across open grass.
	constexpr float StreamTrackGap = 120.0f;
	constexpr float StreamLengthMin = 500.0f;
	constexpr float StreamLengthMax = 1300.0f;
	constexpr float StreamWidthMin = 110.0f;
	constexpr float StreamWidthMax = 170.0f;

	// Backdrop beyond the walls (the wall projection), as build_scenery.build_backdrop.
	constexpr float BackdropInnerPad = 250.0f;
	constexpr float BackdropOuterPad = 4200.0f;
	constexpr float LakeGapToWall = 400.0f;
	constexpr float WaterRoughness = 0.6f;  // shinier water mirrors the bright sky at the low angle the walls see it from

	// Ground plane for themes that don't use the level's grass tile: 50 km across, well past the mountain ring.
	const TCHAR* PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* GroundMaterialPath = TEXT("/Game/LPRiverForest/Materials/Common/MI_LowPoly.MI_LowPoly");
	constexpr float GroundSize = 5000000.0f;

	bool IsLevelFloorScenery(const AStaticMeshActor* Actor, bool& bOutGroundTile)
	{
		const UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		bOutGroundTile = Mesh && Mesh->GetName() == TEXT("SM_Tile_Grass");
		const FVector Location = Actor->GetActorLocation();
		return !bOutGroundTile && FMath::Abs(Location.X) < RoomHalfX + 100.0f && FMath::Abs(Location.Y) < RoomHalfY + 100.0f;
	}

	/** Flat-shaded polygons for one procedural mesh section. */
	struct FMeshBuffers
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;

		void AddPolygon(TArray<FVector, TInlineAllocator<4>> Corners, const FVector& Normal)
		{
			// Front faces wind the opposite way to right-handed anticlockwise about the normal (as build_track.py).
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
				UVs.Add(FVector2D(Corner.X / 100.0, (Corner.Y + Corner.Z) / 100.0));
			}
			for (int32 Index = 1; Index + 1 < Corners.Num(); ++Index)
			{
				Triangles.Append({ Base, Base + Index, Base + Index + 1 });
			}
		}
	};

	/**
	 * A closed polyline offset to its left (+distance) or right (-distance) with mitred joins (track_geom.offset_loop),
	 * each point by its own distance.
	 */
	TArray<FVector2D> OffsetLoop(const TArray<FVector2D>& Points, const TArray<double>& Distances)
	{
		TArray<FVector2D> Out;
		const int32 Num = Points.Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FVector2D& Previous = Points[(Index + Num - 1) % Num];
			const FVector2D& Point = Points[Index];
			const FVector2D& Next = Points[(Index + 1) % Num];
			const FVector2D In = (Point - Previous).GetSafeNormal();
			const FVector2D OutDirection = (Next - Point).GetSafeNormal();
			const FVector2D Mitre = FVector2D(-In.Y - OutDirection.Y, In.X + OutDirection.X).GetSafeNormal();
			const double Scale = Distances[Index] / FMath::Max(0.2, Mitre.X * -OutDirection.Y + Mitre.Y * OutDirection.X);
			Out.Add(Point + Mitre * Scale);
		}
		return Out;
	}

	TArray<FVector2D> OffsetLoop(const TArray<FVector2D>& Points, double Distance)
	{
		TArray<double> Distances;
		Distances.Init(Distance, Points.Num());
		return OffsetLoop(Points, Distances);
	}

	double SignedArea(const TArray<FVector2D>& Loop)
	{
		double Area = 0.0;
		for (int32 Index = 0; Index < Loop.Num(); ++Index)
		{
			const FVector2D& A = Loop[Index];
			const FVector2D& B = Loop[(Index + 1) % Loop.Num()];
			Area += A.X * B.Y - B.X * A.Y;
		}
		return Area * 0.5;
	}

	/** Horizontal ring between two closed loops, facing up. */
	void AddBand(FMeshBuffers& Mesh, const TArray<FVector2D>& LoopA, const TArray<FVector2D>& LoopB, float Z)
	{
		for (int32 Index = 0; Index < LoopA.Num(); ++Index)
		{
			const int32 Next = (Index + 1) % LoopA.Num();
			Mesh.AddPolygon({ FVector(LoopA[Index], Z), FVector(LoopA[Next], Z), FVector(LoopB[Next], Z), FVector(LoopB[Index], Z) }, FVector::UpVector);
		}
	}

	/** Solid kerb wall: top plus both vertical sides. FaceLoop is the road-side edge. */
	void AddWall(FMeshBuffers& Mesh, const TArray<FVector2D>& FaceLoop, const TArray<FVector2D>& BackLoop, float Height)
	{
		AddBand(Mesh, FaceLoop, BackLoop, Height);
		for (int32 Index = 0; Index < FaceLoop.Num(); ++Index)
		{
			const int32 Next = (Index + 1) % FaceLoop.Num();
			for (const bool bFace : { true, false })
			{
				const TArray<FVector2D>& Loop = bFace ? FaceLoop : BackLoop;
				const TArray<FVector2D>& Other = bFace ? BackLoop : FaceLoop;
				const FVector2D Outward = ((Loop[Index] + Loop[Next]) * 0.5 - (Other[Index] + Other[Next]) * 0.5).GetSafeNormal();
				Mesh.AddPolygon({ FVector(Loop[Index], 0.0), FVector(Loop[Next], 0.0), FVector(Loop[Next], Height), FVector(Loop[Index], Height) },
					FVector(Outward, 0.0));
			}
		}
	}

	void CreateSection(UProceduralMeshComponent* Mesh, int32 Section, const FMeshBuffers& Buffers, bool bCollision, const TCHAR* MaterialPath)
	{
		Mesh->CreateMeshSection(Section, Buffers.Vertices, Buffers.Triangles, Buffers.Normals, Buffers.UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), bCollision);
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath))
		{
			Mesh->SetMaterial(Section, Material);
		}
		else
		{
			UE_LOG(LogRaceTrack, Warning, TEXT("race.Track missing material %s"), MaterialPath);
		}
	}

	const FRaceProp* PickWeighted(const TArray<FRaceProp>& Choices, FRandomStream& Random)
	{
		if (Choices.Num() == 0)
		{
			return nullptr;
		}
		float Total = 0.0f;
		for (const FRaceProp& Choice : Choices)
		{
			Total += Choice.Weight;
		}
		float Pick = Random.FRandRange(0.0f, Total);
		for (const FRaceProp& Choice : Choices)
		{
			Pick -= Choice.Weight;
			if (Pick <= 0.0f)
			{
				return &Choice;
			}
		}
		return &Choices.Last();
	}

	bool Overlaps(const TArray<TPair<FVector2D, float>>& Taken, const FVector2D& Point, float Radius)
	{
		for (const TPair<FVector2D, float>& Other : Taken)
		{
			if (FVector2D::Distance(Point, Other.Key) < Radius + Other.Value + 20.0f)
			{
				return true;
			}
		}
		return false;
	}
}

ARaceTrackBuilder::ARaceTrackBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	TrackMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TrackMesh"));
	TrackMesh->SetupAttachment(RootComponent);
	// Walls collide with the cars' sweeps exactly like the editor-built walls (triangle collision).
	TrackMesh->bUseComplexAsSimpleCollision = true;
	TrackMesh->bUseAsyncCooking = false;
	TrackMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
}

void ARaceTrackBuilder::Build(const FRaceTrackPath& Path, int32 ScenerySeed, ERaceThemeKind Theme, ERaceSeasonKind Season)
{
	const TArray<FVector2D>& Centre = Path.GetPoints();
	if (Centre.Num() < 3)
	{
		return;
	}

	// Centreline with extra points where the road width starts or stops changing, and the half width at each point.
	TArray<FVector2D> Profile;
	TArray<double> HalfWidths;
	const TArray<float> Breaks = Path.GetWidthBreakDistances();
	for (int32 Index = 0; Index < Centre.Num(); ++Index)
	{
		const float From = Path.GetPointDistance(Index);
		const float To = Index + 1 < Centre.Num() ? Path.GetPointDistance(Index + 1) : Path.GetLapLength();
		Profile.Add(Centre[Index]);
		HalfWidths.Add(Path.GetHalfWidthAt(From));
		for (const float Break : Breaks)
		{
			if (Break > From + 1.0f && Break < To - 1.0f)
			{
				Profile.Add(Path.GetPointAtDistance(Break));
				HalfWidths.Add(Path.GetHalfWidthAt(Break));
			}
		}
	}
	auto Offsets = [&HalfWidths](double Sign, double Extra)
	{
		TArray<double> Distances;
		for (const double HalfWidth : HalfWidths)
		{
			Distances.Add(Sign * (HalfWidth + Extra));
		}
		return Distances;
	};
	const float WallWidth = FRaceTrackPath::WallWidth;
	const TArray<FVector2D> Left = OffsetLoop(Profile, Offsets(1.0, 0.0));
	const TArray<FVector2D> Right = OffsetLoop(Profile, Offsets(-1.0, 0.0));
	const TArray<FVector2D> LeftBack = OffsetLoop(Profile, Offsets(1.0, WallWidth));
	const TArray<FVector2D> RightBack = OffsetLoop(Profile, Offsets(-1.0, WallWidth));

	FMeshBuffers Road;
	AddBand(Road, Left, Right, RoadZ);

	FMeshBuffers Walls;
	AddWall(Walls, Left, LeftBack, FRaceTrackPath::WallHeight);
	AddWall(Walls, Right, RightBack, FRaceTrackPath::WallHeight);

	FMeshBuffers Line;
	FVector2D Direction;
	const FVector2D StartLine = Path.GetPointAtDistance(Path.GetStartLineDistance(), &Direction);
	const FVector2D Across(-Direction.Y, Direction.X);
	const float LineHalfWidth = Path.GetHalfWidthAt(Path.GetStartLineDistance());
	auto LineCorner = [&](float Along, float Side)
	{
		return FVector(StartLine + Direction * Along + Across * Side, LineZ);
	};
	Line.AddPolygon({ LineCorner(-LineDepth * 0.5f, -LineHalfWidth), LineCorner(LineDepth * 0.5f, -LineHalfWidth),
		LineCorner(LineDepth * 0.5f, LineHalfWidth), LineCorner(-LineDepth * 0.5f, LineHalfWidth) }, FVector::UpVector);

	TrackMesh->ClearAllMeshSections();
	CreateSection(TrackMesh, RoadSection, Road, false, RoadMaterialPath);
	CreateSection(TrackMesh, WallSection, Walls, true, WallMaterialPath);
	CreateSection(TrackMesh, LineSection, Line, false, LineMaterialPath);

	// Scenery for the theme and season.
	for (UStaticMeshComponent* Prop : Props)
	{
		if (Prop)
		{
			Prop->DestroyComponent();
		}
	}
	Props.Reset();
	Content = &RaceTheme::Get(Theme);
	CurrentSeason = Season;
	SetLevelBackdropVisible(GetWorld(), Content->bUsesLevelBackdrop);

	FRandomStream Random(ScenerySeed);
	FTaken FloorTaken;
	if (Content->bBuildGround)
	{
		BuildGround();
	}
	BuildFences(Path, Random);
	BuildNarrowingScenery(Path, Random);
	BuildStreams(Path, Random, FloorTaken);
	BuildFloorScenery(Path, Random, FloorTaken);
	BuildBackdrop(Random);

	UE_LOG(LogRaceTrack, Log, TEXT("race.Track built (%s, %s): %d road / %d wall triangles, %d scenery pieces"),
		RaceTheme::Name(Theme), RaceTheme::Name(Season), Road.Triangles.Num() / 3, Walls.Triangles.Num() / 3, Props.Num());
}

UMaterialInstanceDynamic* ARaceTrackBuilder::MakeFlatMaterial(const FLinearColor& Color, float Roughness)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, GroundMaterialPath);
	if (!Base)
	{
		return nullptr;
	}
	// The pack's low-poly material mixes its colours by the mesh's vertex colours (Paint) and height masks, so set
	// them all: plain engine meshes and the water planes then come out in exactly this colour.
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
	for (const TCHAR* Parameter : { TEXT("BaseColor"), TEXT("Highlight"), TEXT("Paint") })
	{
		Material->SetVectorParameterValue(Parameter, Color);
	}
	Material->SetVectorParameterValue(TEXT("AO"), Color * 0.85f);
	Material->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	return Material;
}

void ARaceTrackBuilder::BuildGround()
{
	if (UStaticMeshComponent* Ground = AddProp(PlaneMeshPath, FVector::ZeroVector, FRotator::ZeroRotator, FVector(GroundSize / 100.0f), false))
	{
		Ground->SetMaterial(0, MakeFlatMaterial(RaceTheme::GroundColor(CurrentSeason), 1.0f));
	}
}

void ARaceTrackBuilder::BuildFences(const FRaceTrackPath& Path, FRandomStream& Random)
{
	if (Content->FencePieces.Num() == 0)
	{
		return;
	}
	const TArray<FVector2D>& Centre = Path.GetPoints();
	const float Clear = FRaceTrackPath::TrackWidth * 0.5f + FRaceTrackPath::WallWidth;

	// One fence line round the outside of the circuit: the side whose kerb loop encloses more.
	const bool bLeftIsOutside = FMath::Abs(SignedArea(OffsetLoop(Centre, Clear))) > FMath::Abs(SignedArea(OffsetLoop(Centre, -Clear)));
	const TArray<FVector2D> Loop = OffsetLoop(Centre, bLeftIsOutside ? Clear + FenceOffset : -(Clear + FenceOffset));

	// Walk the loop, a piece every FencePieceLength (skipping any that would sit too close to another stretch of road).
	const double PieceLength = Content->FencePieceLength;
	double NextAt = PieceLength * 0.5;
	double Walked = 0.0;
	for (int32 Index = 0; Index < Loop.Num(); ++Index)
	{
		const FVector2D& A = Loop[Index];
		const FVector2D& B = Loop[(Index + 1) % Loop.Num()];
		const double Length = FVector2D::Distance(A, B);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		while (Length > 0.0 && NextAt <= Walked + Length)
		{
			const FVector2D Point = A + (B - A) * ((NextAt - Walked) / Length);
			if (Path.GetDistanceToCentreline(Point) > Clear + 40.0f)
			{
				const FString& Piece = Content->FencePieces[Random.RandRange(0, Content->FencePieces.Num() - 1)];
				AddProp(Piece, FVector(Point, 0.0), FRotator(0.0f, Yaw, 0.0f), FVector::OneVector, true);
			}
			NextAt += PieceLength;
		}
		Walked += Length;
	}

	// Posts either side of the start line.
	if (!Content->StartPost.IsEmpty())
	{
		FVector2D Direction;
		const FVector2D StartLine = Path.GetPointAtDistance(Path.GetStartLineDistance(), &Direction);
		const FVector2D Across(-Direction.Y, Direction.X);
		for (const float Side : { -1.0f, 1.0f })
		{
			AddProp(Content->StartPost, FVector(StartLine + Across * Side * (Clear + 60.0f), 0.0), FRotator::ZeroRotator, FVector::OneVector, true);
		}
	}
}

void ARaceTrackBuilder::BuildNarrowingScenery(const FRaceTrackPath& Path, FRandomStream& Random)
{
	const float FullHalfWidth = FRaceTrackPath::TrackWidth * 0.5f;
	UStaticMesh* WallRock = Content->NarrowingWallRock.IsEmpty() ? nullptr : LoadMesh(Content->NarrowingWallRock);
	for (const FRaceTrackPath::FNarrowingSpan& Span : Path.GetNarrowingSpans())
	{
		const float Gap = FullHalfWidth - Span.HalfWidth; // between the narrow kerb and where the full-width kerb would be
		const float SideOffset = Span.HalfWidth + FRaceTrackPath::WallWidth + Gap * 0.5f;

		if (WallRock)
		{
			// Long rocks lined up along both narrowed kerbs, like a rocky gap (only along the narrow part, not the tapers).
			const FVector Size = WallRock->GetBoundingBox().GetSize();
			const float Length = Span.HalfLength * 2.0f;
			const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / (Size.X * 0.75f)));
			const float PieceLength = Length / Count;
			const float Depth = FMath::Max(Gap - 30.0f, 60.0f);
			for (const float Side : { -1.0f, 1.0f })
			{
				for (int32 Piece = 0; Piece < Count; ++Piece)
				{
					FVector2D Direction;
					const FVector2D Point = Path.GetPointAtDistance(Span.CentreDistance - Span.HalfLength + PieceLength * (Piece + 0.5f), &Direction);
					const FVector2D Across(-Direction.Y, Direction.X);
					const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)) + (Side < 0.0f ? 180.0f : 0.0f) + Random.FRandRange(-6.0f, 6.0f);
					const FVector Scale(PieceLength * 1.15f / Size.X, Depth / Size.Y, Random.FRandRange(0.6f, 0.85f));
					AddProp(Content->NarrowingWallRock, FVector(Point + Across * Side * SideOffset, 0.0), FRotator(0.0f, Yaw, 0.0f), Scale, true);
				}
			}
			continue;
		}

		for (float Along = -Span.HalfLength; Along <= Span.HalfLength + 1.0f; Along += 170.0f)
		{
			FVector2D Direction;
			const FVector2D Point = Path.GetPointAtDistance(Span.CentreDistance + Along, &Direction);
			const FVector2D Across(-Direction.Y, Direction.X);
			for (const float Side : { -1.0f, 1.0f })
			{
				if (const FRaceProp* Choice = PickWeighted(Content->NarrowingProps, Random))
				{
					AddProp(Choice->Mesh, FVector(Point + Across * Side * SideOffset, 0.0), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
						FVector(Random.FRandRange(Choice->ScaleMin, Choice->ScaleMax)), Choice->bCastShadow);
				}
			}
		}
	}
}

void ARaceTrackBuilder::BuildStreams(const FRaceTrackPath& Path, FRandomStream& Random, FTaken& Taken)
{
	if (Content->MaxStreams <= 0 || Content->StreamMesh.IsEmpty())
	{
		return;
	}
	const float Clear = FRaceTrackPath::TrackWidth * 0.5f + FRaceTrackPath::WallWidth + StreamTrackGap;
	int32 Placed = 0;
	for (int32 Try = 0; Try < 200 && Placed < Content->MaxStreams; ++Try)
	{
		const float Length = Random.FRandRange(StreamLengthMin, StreamLengthMax);
		const float Width = Random.FRandRange(StreamWidthMin, StreamWidthMax);
		const FVector2D Middle(Random.FRandRange(-RoomHalfX + 300.0f, RoomHalfX - 300.0f), Random.FRandRange(-RoomHalfY + 300.0f, RoomHalfY - 300.0f));
		const float Yaw = Random.FRandRange(0.0f, 180.0f);
		const FVector2D Along(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
		const FVector2D Across(-Along.Y, Along.X);

		// Open grass all the way along: inside the room, clear of the road and of anything placed already.
		bool bClear = true;
		for (float T = -Length * 0.5f; T <= Length * 0.5f + 1.0f && bClear; T += 80.0f)
		{
			const FVector2D Point = Middle + Along * T;
			bClear = FMath::Abs(Point.X) < RoomHalfX - 150.0f && FMath::Abs(Point.Y) < RoomHalfY - 150.0f &&
				Path.GetDistanceToCentreline(Point) - Clear >= Width * 0.5f && !Overlaps(Taken, Point, Width * 0.5f + 40.0f);
		}
		if (!bClear)
		{
			continue;
		}

		if (UStaticMeshComponent* Water = AddProp(Content->StreamMesh, FVector(Middle, 1.0), FRotator(0.0f, Yaw, 0.0f), FVector(Length / 100.0f, Width / 100.0f, 1.0f), false))
		{
			Water->SetMaterial(0, MakeFlatMaterial(RaceTheme::WaterColor(CurrentSeason), WaterRoughness));
		}
		for (float T = -Length * 0.5f; T <= Length * 0.5f + 1.0f; T += 80.0f)
		{
			Taken.Emplace(Middle + Along * T, Width * 0.5f + 40.0f);
		}

		// Stones and grass along the banks.
		for (float T = -Length * 0.5f; T <= Length * 0.5f; T += Random.FRandRange(90.0f, 160.0f))
		{
			for (const float Side : { -1.0f, 1.0f })
			{
				const FRaceProp* Bank = Random.FRand() < 0.6f ? PickWeighted(Content->StreamBankProps, Random) : nullptr;
				if (Bank)
				{
					const FVector2D Point = Middle + Along * T + Across * Side * (Width * 0.5f + Random.FRandRange(0.0f, 25.0f));
					AddProp(Bank->Mesh, FVector(Point, 0.0), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
						FVector(Random.FRandRange(Bank->ScaleMin, Bank->ScaleMax)), Bank->bCastShadow);
				}
			}
		}

		// Sometimes a line of stepping stones across.
		if (Content->SteppingStones.Num() > 0 && Random.FRand() < 0.6f)
		{
			const FVector2D Crossing = Middle + Along * Random.FRandRange(-Length * 0.3f, Length * 0.3f);
			for (const float Offset : { -0.3f, 0.0f, 0.3f })
			{
				const FString& Stone = Content->SteppingStones[Random.RandRange(0, Content->SteppingStones.Num() - 1)];
				AddProp(Stone, FVector(Crossing + Across * Offset * Width, 2.0), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
					FVector(Random.FRandRange(0.45f, 0.6f)), false);
			}
		}
		++Placed;
	}
	UE_LOG(LogRaceTrack, Log, TEXT("race.Track streams placed: %d"), Placed);
}

void ARaceTrackBuilder::BuildFloorScenery(const FRaceTrackPath& Path, FRandomStream& Random, FTaken& Taken)
{
	const float Clear = FRaceTrackPath::TrackWidth * 0.5f + FRaceTrackPath::WallWidth + FloorPropTrackGap;

	auto TryPlace = [&](const FRaceProp& Choice)
	{
		const FVector2D Point(Random.FRandRange(-RoomHalfX + FloorPropMargin, RoomHalfX - FloorPropMargin),
			Random.FRandRange(-RoomHalfY + FloorPropMargin, RoomHalfY - FloorPropMargin));
		const float Scale = Random.FRandRange(Choice.ScaleMin, Choice.ScaleMax);
		const float Radius = Choice.Radius * Scale;
		if (Path.GetDistanceToCentreline(Point) - Clear < Radius || Overlaps(Taken, Point, Radius))
		{
			return false;
		}
		Taken.Emplace(Point, Radius);
		AddProp(Choice.Mesh, FVector(Point, 0.0), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Scale), Choice.bCastShadow);
		return true;
	};

	for (const FRaceProp& Feature : Content->FloorFeatures)
	{
		for (int32 Try = 0; Try < 60 && !TryPlace(Feature); ++Try)
		{
		}
	}
	for (int32 Piece = 0; Piece < Content->FloorPropCount; ++Piece)
	{
		for (int32 Try = 0; Try < 40; ++Try)
		{
			const FRaceProp* Choice = PickWeighted(Content->FloorProps, Random);
			if (!Choice || TryPlace(*Choice))
			{
				break;
			}
		}
	}
}

void ARaceTrackBuilder::BuildBackdrop(FRandomStream& Random)
{
	if (Content->bUsesLevelBackdrop)
	{
		return;
	}
	FTaken Taken;

	// A lake just beyond one of the walls, where the wall projection shows the ground.
	if (!Content->LakeMesh.IsEmpty())
	{
		if (UStaticMesh* Lake = LoadMesh(Content->LakeMesh))
		{
			const float Scale = Random.FRandRange(1.0f, 1.3f);
			const float Radius = Lake->GetBoundingBox().GetSize().X * 0.5f * Scale;
			const int32 Wall = Random.RandRange(0, 3);
			const float Sign = (Wall % 2 == 0) ? 1.0f : -1.0f;
			const FVector2D Location = Wall < 2
				? FVector2D(Sign * (RoomHalfX + LakeGapToWall + Radius), Random.FRandRange(-1500.0f, 1500.0f))
				: FVector2D(Random.FRandRange(-1200.0f, 1200.0f), Sign * (RoomHalfY + LakeGapToWall + Radius));
			if (UStaticMeshComponent* Water = AddProp(Content->LakeMesh, FVector(Location, 1.0), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Scale), false))
			{
				Water->SetMaterial(0, MakeFlatMaterial(RaceTheme::WaterColor(CurrentSeason), WaterRoughness));
			}
			Taken.Emplace(Location, Radius + 100.0f);
		}
	}

	// Trees and rocks from just outside the walls outwards, bigger further out (as build_scenery.build_backdrop).
	for (int32 Piece = 0; Piece < Content->BackdropCount; ++Piece)
	{
		for (int32 Try = 0; Try < 40; ++Try)
		{
			const FRaceProp* Choice = PickWeighted(Content->BackdropProps, Random);
			if (!Choice)
			{
				return;
			}
			const FVector2D Point(Random.FRandRange(-RoomHalfX - BackdropOuterPad, RoomHalfX + BackdropOuterPad),
				Random.FRandRange(-RoomHalfY - BackdropOuterPad, RoomHalfY + BackdropOuterPad));
			if (FMath::Abs(Point.X) <= RoomHalfX + BackdropInnerPad && FMath::Abs(Point.Y) <= RoomHalfY + BackdropInnerPad)
			{
				continue;
			}
			const float Depth = FMath::Max3(float(FMath::Abs(Point.X)) - RoomHalfX, float(FMath::Abs(Point.Y)) - RoomHalfY, 0.0f);
			const float Scale = (1.0f + Depth / BackdropOuterPad * 1.6f) * Random.FRandRange(0.85f, 1.2f) * Random.FRandRange(Choice->ScaleMin, Choice->ScaleMax);
			const float Radius = Choice->Radius * Scale;
			if (Overlaps(Taken, Point, Radius))
			{
				continue;
			}
			Taken.Emplace(Point, Radius);
			AddProp(Choice->Mesh, FVector(Point, 0.0), FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Scale), Choice->bCastShadow);
			break;
		}
	}
}

UStaticMesh* ARaceTrackBuilder::LoadMesh(const FString& MeshPath)
{
	if (const TObjectPtr<UStaticMesh>* Found = PropMeshes.Find(MeshPath))
	{
		return Found->Get();
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogRaceTrack, Warning, TEXT("race.Track missing scenery mesh %s"), *MeshPath);
	}
	PropMeshes.Add(MeshPath, Mesh);
	return Mesh;
}

UStaticMeshComponent* ARaceTrackBuilder::AddProp(const FString& MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bCastShadow)
{
	UStaticMesh* Mesh = LoadMesh(RaceTheme::SeasonMesh(MeshPath, CurrentSeason));
	if (!Mesh)
	{
		return nullptr;
	}

	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(this);
	Prop->SetStaticMesh(Mesh);
	Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Prop->SetCastShadow(bCastShadow);
	for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
	{
		UMaterialInterface* Material = Mesh->GetMaterial(Slot);
		UMaterialInterface* Seasonal = RaceTheme::SeasonMaterial(Material, CurrentSeason);
		if (Seasonal != Material)
		{
			Prop->SetMaterial(Slot, Seasonal);
		}
	}
	Prop->SetupAttachment(RootComponent);
	Prop->SetRelativeTransform(FTransform(Rotation, Location, Scale));
	Prop->RegisterComponent();
	Props.Add(Prop);
	return Prop;
}

int32 ARaceTrackBuilder::HideLevelTrack(UWorld* World)
{
	int32 Hidden = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
		bool bGroundTile = false;
		const bool bFloorScenery = Actor->ActorHasTag(LevelSceneryTag) && IsLevelFloorScenery(Actor, bGroundTile);
		if (Actor->ActorHasTag(LevelTrackTag) || bFloorScenery)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			++Hidden;
		}
	}
	return Hidden;
}

void ARaceTrackBuilder::SetLevelBackdropVisible(UWorld* World, bool bVisible)
{
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
		bool bGroundTile = false;
		if (Actor->ActorHasTag(LevelSceneryTag) && !IsLevelFloorScenery(Actor, bGroundTile))
		{
			Actor->SetActorHiddenInGame(!bVisible);
		}
	}
}
