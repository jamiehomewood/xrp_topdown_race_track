#include "RaceTrackBuilder.h"

#include "Algo/Reverse.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

	// Scenery, as in Tools/TrackGen/build_scenery.py: only low props on the floor so nothing hides the cars.
	const TCHAR* NatureFolder = TEXT("/Game/LowPolyNatureLite/Assets/Models/");
	const TCHAR* FenceFolder = TEXT("/Game/Fab/Low_Poly_Meadow_Barrier_Bundle__Fences___Walls/");
	constexpr float FenceOffset = 90.0f;       // fence line beyond the outer kerb wall
	constexpr float FencePieceLength = 150.0f;
	constexpr float FloorPropMargin = 60.0f;   // from the room edge
	constexpr float FloorPropTrackGap = 130.0f;
	constexpr int32 FloorPropCount = 190;

	struct FPropChoice
	{
		const TCHAR* Name;
		float Weight;
		float Radius; // footprint in UU
	};

	const FPropChoice FloorProps[] = {
		{ TEXT("SM_Bush_Simple"), 5, 90 }, { TEXT("SM_Bush_Berries_Red"), 2, 110 }, { TEXT("SM_Bush_Berries_blue"), 2, 110 },
		{ TEXT("SM_Bush_Berries_Empty"), 2, 110 }, { TEXT("SM_Grass_Array01"), 5, 85 }, { TEXT("SM_Grass01"), 6, 45 },
		{ TEXT("SM_Grass03"), 4, 30 }, { TEXT("SM_Plant02"), 3, 85 }, { TEXT("SM_Flower02_Orange"), 3, 40 },
		{ TEXT("SM_Flower02_Pink"), 3, 40 }, { TEXT("SM_Flower02_Yellow"), 3, 40 }, { TEXT("SM_Hat_Mushroom_red"), 1, 40 },
		{ TEXT("SM_Mushrooom01_brown"), 1, 35 }, { TEXT("SM_Stone02"), 3, 50 }, { TEXT("SM_Stones02"), 2, 110 },
		{ TEXT("SM_Rock02"), 2, 120 },
	};

	// Bigger features, placed first wherever they fit.
	const FPropChoice FloorFeatures[] = {
		{ TEXT("SM_Tent_Blue"), 1, 230 }, { TEXT("SM_Tent_Red"), 1, 230 }, { TEXT("SM_Log"), 1, 140 },
		{ TEXT("SM_Log"), 1, 140 }, { TEXT("SM_Stones02"), 1, 140 }, { TEXT("SM_Branch01"), 1, 140 },
	};

	const TCHAR* FencePieces[] = {
		TEXT("EA03_Fence_Plank_01a"), TEXT("EA03_Fence_Plank_01b"), TEXT("EA03_Fence_Plank_01c"),
		TEXT("EA03_Fence_Plank_01d"), TEXT("EA03_Fence_Plank_01e"),
	};

	FString NatureMesh(const TCHAR* Name)
	{
		return FString::Printf(TEXT("%s%s.%s"), NatureFolder, Name, Name);
	}

	FString FenceMesh(const TCHAR* Name)
	{
		return FString::Printf(TEXT("%s%s/StaticMeshes/%s.%s"), FenceFolder, Name, Name, Name);
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

void ARaceTrackBuilder::Build(const FRaceTrackPath& Path, int32 ScenerySeed)
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

	for (UStaticMeshComponent* Prop : Props)
	{
		if (Prop)
		{
			Prop->DestroyComponent();
		}
	}
	Props.Reset();
	FRandomStream Random(ScenerySeed);
	BuildFences(Path, Random);
	BuildNarrowingScenery(Path, Random);
	BuildFloorScenery(Path, Random);

	UE_LOG(LogRaceTrack, Log, TEXT("race.Track built: %d road / %d wall triangles, %d scenery pieces"),
		Road.Triangles.Num() / 3, Walls.Triangles.Num() / 3, Props.Num());
}

void ARaceTrackBuilder::BuildFences(const FRaceTrackPath& Path, FRandomStream& Random)
{
	const TArray<FVector2D>& Centre = Path.GetPoints();
	const float Clear = FRaceTrackPath::TrackWidth * 0.5f + FRaceTrackPath::WallWidth;

	// One fence line round the outside of the circuit: the side whose kerb loop encloses more.
	const bool bLeftIsOutside = FMath::Abs(SignedArea(OffsetLoop(Centre, Clear))) > FMath::Abs(SignedArea(OffsetLoop(Centre, -Clear)));
	const TArray<FVector2D> Loop = OffsetLoop(Centre, bLeftIsOutside ? Clear + FenceOffset : -(Clear + FenceOffset));

	// Walk the loop, a piece every FencePieceLength (skipping any that would sit too close to another stretch of road).
	double NextAt = FencePieceLength * 0.5;
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
				AddProp(FenceMesh(FencePieces[Random.RandRange(0, UE_ARRAY_COUNT(FencePieces) - 1)]), Point, Yaw, 1.0f, true);
			}
			NextAt += FencePieceLength;
		}
		Walked += Length;
	}

	// Posts either side of the start line.
	FVector2D Direction;
	const FVector2D StartLine = Path.GetPointAtDistance(Path.GetStartLineDistance(), &Direction);
	const FVector2D Across(-Direction.Y, Direction.X);
	for (const float Side : { -1.0f, 1.0f })
	{
		AddProp(FenceMesh(TEXT("EA03_Wooden_Pin_01d")), StartLine + Across * Side * (Clear + 60.0f), 0.0f, 1.0f, true);
	}
}

void ARaceTrackBuilder::BuildNarrowingScenery(const FRaceTrackPath& Path, FRandomStream& Random)
{
	// Rocks and bushes in the grass beside a narrowed kerb, so the squeeze looks like it's there for a reason.
	const float FullHalfWidth = FRaceTrackPath::TrackWidth * 0.5f;
	for (const FRaceTrackPath::FNarrowingSpan& Span : Path.GetNarrowingSpans())
	{
		const float Gap = FullHalfWidth - Span.HalfWidth; // between the narrow kerb and where the full-width kerb would be
		for (float Along = -Span.HalfLength; Along <= Span.HalfLength + 1.0f; Along += 170.0f)
		{
			FVector2D Direction;
			const FVector2D Point = Path.GetPointAtDistance(Span.CentreDistance + Along, &Direction);
			const FVector2D Across(-Direction.Y, Direction.X);
			for (const float Side : { -1.0f, 1.0f })
			{
				const bool bRock = Random.FRand() < 0.6f;
				const FVector2D Location = Point + Across * Side * (Span.HalfWidth + FRaceTrackPath::WallWidth + Gap * 0.5f);
				AddProp(NatureMesh(bRock ? TEXT("SM_Rock02") : TEXT("SM_Bush_Simple")), Location, Random.FRandRange(0.0f, 360.0f),
					bRock ? Random.FRandRange(0.45f, 0.65f) : Random.FRandRange(0.8f, 1.05f), true);
			}
		}
	}
}

void ARaceTrackBuilder::BuildFloorScenery(const FRaceTrackPath& Path, FRandomStream& Random)
{
	const float Clear = FRaceTrackPath::TrackWidth * 0.5f + FRaceTrackPath::WallWidth + FloorPropTrackGap;
	TArray<TPair<FVector2D, float>> Taken;

	auto TryPlace = [&](const FPropChoice& Choice, bool bRandomScale)
	{
		const FVector2D Point(Random.FRandRange(-RoomHalfX + FloorPropMargin, RoomHalfX - FloorPropMargin),
			Random.FRandRange(-RoomHalfY + FloorPropMargin, RoomHalfY - FloorPropMargin));
		const float Scale = bRandomScale ? Random.FRandRange(0.85f, 1.2f) : 1.0f;
		const float Radius = Choice.Radius * Scale;
		if (Path.GetDistanceToCentreline(Point) - Clear < Radius)
		{
			return false;
		}
		for (const TPair<FVector2D, float>& Other : Taken)
		{
			if (FVector2D::Distance(Point, Other.Key) < Radius + Other.Value + 20.0f)
			{
				return false;
			}
		}
		Taken.Emplace(Point, Radius);
		AddProp(NatureMesh(Choice.Name), Point, Random.FRandRange(0.0f, 360.0f), Scale, Choice.Radius > 60.0f);
		return true;
	};

	for (const FPropChoice& Feature : FloorFeatures)
	{
		for (int32 Try = 0; Try < 60 && !TryPlace(Feature, false); ++Try)
		{
		}
	}

	float TotalWeight = 0.0f;
	for (const FPropChoice& Choice : FloorProps)
	{
		TotalWeight += Choice.Weight;
	}
	for (int32 Piece = 0; Piece < FloorPropCount; ++Piece)
	{
		for (int32 Try = 0; Try < 40; ++Try)
		{
			float Pick = Random.FRandRange(0.0f, TotalWeight);
			const FPropChoice* Choice = &FloorProps[UE_ARRAY_COUNT(FloorProps) - 1];
			for (const FPropChoice& Candidate : FloorProps)
			{
				Pick -= Candidate.Weight;
				if (Pick <= 0.0f)
				{
					Choice = &Candidate;
					break;
				}
			}
			if (TryPlace(*Choice, true))
			{
				break;
			}
		}
	}
}

void ARaceTrackBuilder::AddProp(const FString& MeshPath, const FVector2D& Location, float Yaw, float Scale, bool bCastShadow)
{
	UStaticMesh* Mesh = nullptr;
	if (const TObjectPtr<UStaticMesh>* Found = PropMeshes.Find(MeshPath))
	{
		Mesh = Found->Get();
	}
	else
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
		if (!Mesh)
		{
			UE_LOG(LogRaceTrack, Warning, TEXT("race.Track missing scenery mesh %s"), *MeshPath);
		}
		PropMeshes.Add(MeshPath, Mesh);
	}
	if (!Mesh)
	{
		return;
	}

	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(this);
	Prop->SetStaticMesh(Mesh);
	Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Prop->SetCastShadow(bCastShadow);
	Prop->SetupAttachment(RootComponent);
	Prop->SetRelativeTransform(FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Location, 0.0), FVector(Scale)));
	Prop->RegisterComponent();
	Props.Add(Prop);
}

int32 ARaceTrackBuilder::HideLevelTrack(UWorld* World)
{
	int32 Hidden = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
		const FVector Location = Actor->GetActorLocation();
		const UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		// Floor scenery: tagged scenery standing on the room floor, except the big ground tile centred there.
		const bool bFloorScenery = Actor->ActorHasTag(LevelSceneryTag) && FMath::Abs(Location.X) < RoomHalfX + 100.0f &&
			FMath::Abs(Location.Y) < RoomHalfY + 100.0f && !(Mesh && Mesh->GetName() == TEXT("SM_Tile_Grass"));
		if (Actor->ActorHasTag(LevelTrackTag) || bFloorScenery)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			++Hidden;
		}
	}
	return Hidden;
}
