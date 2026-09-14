#include "RaceTrackGenerator.h"

#include "Algo/Reverse.h"
#include "Math/RandomStream.h"
#include "RaceTrackPath.h"

namespace
{
	// The centreline stays inside this box, which leaves the same walkway round the room edge as the original track.
	constexpr double CentreLimitX = 1450.0;
	constexpr double CentreLimitY = 1950.0;

	constexpr int32 Columns = 2;                 // 2.9 m across fits two cells at most
	constexpr double MinCellSize = 1100.0;       // >= two corner radii, plus a strip of grass in narrow gaps
	constexpr double MinLapLength = 9000.0;
	constexpr double MinCentreGap = 930.0;       // road + both kerb walls + a little grass between separate stretches
	constexpr double ClearanceAlongGap = 1600.0; // ...for points at least this far apart along the lap
	constexpr double GridRunUp = 1220.0;         // straight road behind the start line for the 2 x 2 grid
	constexpr double MinStartStraight = 1500.0;
	constexpr float CellChance = 0.72f;
	constexpr float PlainRectangleChance = 0.25f; // keep only some 4-corner (oval) layouts, they're the least interesting
	constexpr int32 MaxAttempts = 500;

	/** How far the rounded corner at Corner eats into the straights either side of it. */
	double TangentLength(const FVector2D& Previous, const FVector2D& Corner, const FVector2D& Next)
	{
		const FVector2D In = (Corner - Previous).GetSafeNormal();
		const FVector2D Out = (Next - Corner).GetSafeNormal();
		const double Theta = FMath::Acos(FMath::Clamp(In | Out, -1.0, 1.0));
		return FRaceTrackPath::CornerRadius * FMath::Tan(Theta / 2.0);
	}

	double RandomRange(FRandomStream& Random, double Min, double Max)
	{
		return Min + (Max - Min) * double(Random.FRand());
	}
}

FRaceTrackLayout RaceTrackGenerator::Classic()
{
	FRaceTrackLayout Layout;
	Layout.ControlPoints = FRaceTrackPath::ClassicControlPoints();
	Layout.StartLine = FRaceTrackPath::ClassicStartLine();
	Layout.Name = TEXT("classic");
	return Layout;
}

bool RaceTrackGenerator::Generate(FRandomStream& Random, FRaceTrackLayout& OutLayout)
{
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Grid: 2 columns, and 3 rows (most layouts) or 2, with random cut positions; every cell >= MinCellSize.
		const int32 Rows = Random.FRand() < 0.7f ? 3 : 2;
		TArray<double> XCuts = { -CentreLimitX, RandomRange(Random, -CentreLimitX + MinCellSize, CentreLimitX - MinCellSize), CentreLimitX };
		TArray<double> YCuts = { -CentreLimitY };
		for (int32 Row = 1; Row < Rows; ++Row)
		{
			YCuts.Add(RandomRange(Random, YCuts.Last() + MinCellSize, CentreLimitY - (Rows - Row) * MinCellSize));
		}
		YCuts.Add(CentreLimitY);

		// Random cells.
		const int32 NumCells = Columns * Rows;
		TArray<bool> Cells;
		Cells.SetNum(NumCells);
		int32 Count = 0;
		for (int32 Index = 0; Index < NumCells; ++Index)
		{
			Cells[Index] = Random.FRand() < CellChance;
			Count += Cells[Index] ? 1 : 0;
		}
		if (Count < 2)
		{
			continue;
		}
		auto Cell = [&Cells, Rows](int32 Col, int32 Row)
		{
			return Col >= 0 && Col < Columns && Row >= 0 && Row < Rows && Cells[Col + Row * Columns];
		};

		// All in one piece.
		TArray<bool> Seen;
		Seen.SetNumZeroed(NumCells);
		TArray<int32> Stack = { Cells.IndexOfByKey(true) };
		Seen[Stack[0]] = true;
		int32 Reached = 0;
		while (Stack.Num() > 0)
		{
			const int32 Index = Stack.Pop();
			++Reached;
			const int32 Col = Index % Columns;
			const int32 Row = Index / Columns;
			for (const FIntPoint& Step : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
			{
				if (Cell(Col + Step.X, Row + Step.Y) && !Seen[(Col + Step.X) + (Row + Step.Y) * Columns])
				{
					Seen[(Col + Step.X) + (Row + Step.Y) * Columns] = true;
					Stack.Add((Col + Step.X) + (Row + Step.Y) * Columns);
				}
			}
		}
		if (Reached != Count)
		{
			continue;
		}

		// Outline, anticlockwise: each cell edge with no cell across it, the inside on its left. Cells touching only
		// at a corner would give a vertex two ways out (a figure of eight), which is rejected.
		const int32 VertexColumns = Columns + 1;
		auto Key = [VertexColumns](int32 I, int32 J) { return I + J * VertexColumns; };
		TMap<int32, int32> NextVertex;
		bool bPinched = false;
		auto AddEdge = [&NextVertex, &bPinched](int32 From, int32 To)
		{
			bPinched |= NextVertex.Contains(From);
			NextVertex.Add(From, To);
		};
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Col = 0; Col < Columns; ++Col)
			{
				if (!Cell(Col, Row))
				{
					continue;
				}
				if (!Cell(Col, Row - 1)) { AddEdge(Key(Col, Row), Key(Col + 1, Row)); }
				if (!Cell(Col + 1, Row)) { AddEdge(Key(Col + 1, Row), Key(Col + 1, Row + 1)); }
				if (!Cell(Col, Row + 1)) { AddEdge(Key(Col + 1, Row + 1), Key(Col, Row + 1)); }
				if (!Cell(Col - 1, Row)) { AddEdge(Key(Col, Row + 1), Key(Col, Row)); }
			}
		}
		if (bPinched || NextVertex.Num() == 0)
		{
			continue;
		}

		TArray<FVector2D> Outline;
		const int32 StartKey = NextVertex.CreateConstIterator()->Key;
		int32 Current = StartKey;
		do
		{
			Outline.Add(FVector2D(XCuts[Current % VertexColumns], YCuts[Current / VertexColumns]));
			const int32* Next = NextVertex.Find(Current);
			if (!Next)
			{
				break;
			}
			Current = *Next;
		}
		while (Current != StartKey && Outline.Num() <= NextVertex.Num());
		if (Current != StartKey || Outline.Num() != NextVertex.Num())
		{
			continue;
		}

		// Keep only the corners (drop vertices part-way along a straight edge).
		TArray<FVector2D> Corners;
		for (int32 Index = 0; Index < Outline.Num(); ++Index)
		{
			const FVector2D& Previous = Outline[(Index + Outline.Num() - 1) % Outline.Num()];
			const FVector2D& Corner = Outline[Index];
			const FVector2D& Next = Outline[(Index + 1) % Outline.Num()];
			if (((Corner - Previous).GetSafeNormal() | (Next - Corner).GetSafeNormal()) < 0.999)
			{
				Corners.Add(Corner);
			}
		}
		const bool bClockwise = Random.FRand() < 0.5f;
		if (bClockwise)
		{
			Algo::Reverse(Corners);
		}
		const int32 NumCorners = Corners.Num();
		if (NumCorners < 4 || (NumCorners == 4 && Random.FRand() > PlainRectangleChance))
		{
			continue;
		}

		// Start line on a random straight long enough for the grid behind it.
		TArray<int32> StartSides;
		for (int32 Index = 0; Index < NumCorners; ++Index)
		{
			const FVector2D& A = Corners[Index];
			const FVector2D& B = Corners[(Index + 1) % NumCorners];
			const double Straight = FVector2D::Distance(A, B) - TangentLength(Corners[(Index + NumCorners - 1) % NumCorners], A, B)
				- TangentLength(A, B, Corners[(Index + 2) % NumCorners]);
			if (Straight >= MinStartStraight)
			{
				StartSides.Add(Index);
			}
		}
		if (StartSides.Num() == 0)
		{
			continue;
		}
		const int32 Side = StartSides[Random.RandRange(0, StartSides.Num() - 1)];
		const FVector2D& A = Corners[Side];
		const FVector2D& B = Corners[(Side + 1) % NumCorners];
		const FVector2D Direction = (B - A).GetSafeNormal();
		const double TangentA = TangentLength(Corners[(Side + NumCorners - 1) % NumCorners], A, B);
		const double Straight = FVector2D::Distance(A, B) - TangentA - TangentLength(A, B, Corners[(Side + 2) % NumCorners]);

		FRaceTrackLayout Layout;
		Layout.ControlPoints = Corners;
		Layout.StartLine = A + Direction * (TangentA + RandomRange(Random, GridRunUp + 60.0, Straight - 150.0));

		// Name: the cell pattern, top row first ('#' = road round it), and the direction.
		FString Shape;
		for (int32 Row = Rows - 1; Row >= 0; --Row)
		{
			for (int32 Col = 0; Col < Columns; ++Col)
			{
				Shape += Cell(Col, Row) ? TEXT("#") : TEXT(".");
			}
			if (Row > 0)
			{
				Shape += TEXT("/");
			}
		}
		Layout.Name = FString::Printf(TEXT("%s %s"), *Shape, bClockwise ? TEXT("clockwise") : TEXT("anticlockwise"));

		if (Validate(Layout).IsEmpty())
		{
			OutLayout = MoveTemp(Layout);
			return true;
		}
	}
	return false;
}

FString RaceTrackGenerator::Validate(const FRaceTrackLayout& Layout)
{
	const TArray<FVector2D>& Corners = Layout.ControlPoints;
	const int32 NumCorners = Corners.Num();
	if (NumCorners < 4)
	{
		return TEXT("fewer than 4 corners");
	}
	for (int32 Index = 0; Index < NumCorners; ++Index)
	{
		const FVector2D& A = Corners[Index];
		const FVector2D& B = Corners[(Index + 1) % NumCorners];
		const double Needed = TangentLength(Corners[(Index + NumCorners - 1) % NumCorners], A, B) + TangentLength(A, B, Corners[(Index + 2) % NumCorners]);
		if (FVector2D::Distance(A, B) < Needed + 1.0)
		{
			return FString::Printf(TEXT("straight %d is too short for its corners (%.0f < %.0f)"), Index, FVector2D::Distance(A, B), Needed);
		}
	}

	FRaceTrackPath Path;
	Path.Build(Corners, Layout.StartLine);
	for (const FVector2D& Point : Path.GetPoints())
	{
		if (FMath::Abs(Point.X) > CentreLimitX + 1.0 || FMath::Abs(Point.Y) > CentreLimitY + 1.0)
		{
			return FString::Printf(TEXT("leaves the room floor at (%.0f, %.0f)"), Point.X, Point.Y);
		}
	}

	const float LapLength = Path.GetLapLength();
	if (LapLength < MinLapLength)
	{
		return FString::Printf(TEXT("lap too short (%.0f)"), LapLength);
	}

	// Separate stretches of road (far apart along the lap) must not come close to each other.
	const int32 NumSamples = FMath::CeilToInt(LapLength / 100.0f);
	const double SampleSpacing = LapLength / NumSamples;
	TArray<FVector2D> Samples;
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		Samples.Add(Path.GetPointAtDistance(float(Index * SampleSpacing)));
	}
	for (int32 First = 0; First < NumSamples; ++First)
	{
		for (int32 Second = First + 1; Second < NumSamples; ++Second)
		{
			const double AlongGap = FMath::Min(Second - First, NumSamples - (Second - First)) * SampleSpacing;
			const double Gap = FVector2D::Distance(Samples[First], Samples[Second]);
			if (AlongGap > ClearanceAlongGap && Gap < MinCentreGap)
			{
				return FString::Printf(TEXT("road passes too close to itself (%.0f apart at (%.0f, %.0f))"), Gap, Samples[First].X, Samples[First].Y);
			}
		}
	}

	const float StartDistance = Path.GetStartLineDistance();
	if (FVector2D::Distance(Path.GetPointAtDistance(StartDistance), Layout.StartLine) > 5.0)
	{
		return TEXT("start line is off the road");
	}
	FVector2D AtLine;
	FVector2D BehindGrid;
	FVector2D PastLine;
	Path.GetPointAtDistance(StartDistance, &AtLine);
	Path.GetPointAtDistance(StartDistance - float(GridRunUp), &BehindGrid);
	Path.GetPointAtDistance(StartDistance + 100.0f, &PastLine);
	if ((AtLine | BehindGrid) < 0.999 || (AtLine | PastLine) < 0.999)
	{
		return TEXT("start grid isn't on a straight");
	}
	return FString();
}
