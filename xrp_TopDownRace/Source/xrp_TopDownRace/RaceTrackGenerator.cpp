#include "RaceTrackGenerator.h"

#include "Algo/Reverse.h"
#include "Math/RandomStream.h"
#include "RaceTrackPath.h"

namespace
{
	// The centreline stays inside this box: 30 cm further out on every side than the original track, which leaves
	// the outer kerb about 36 cm and the fence about 27 cm from the room walls (and clear of the corner panels).
	constexpr double CentreLimitX = 1750.0;
	constexpr double CentreLimitY = 2250.0;

	constexpr int32 Columns = 2;                 // 3.5 m across fits two cells at most
	constexpr double MinCellSize = 1100.0;       // >= two corner radii, plus a strip of grass in narrow gaps
	constexpr double MinLapLength = 9000.0;
	constexpr double MinCentreGap = 930.0;       // road + both kerb walls + a little grass between separate stretches
	constexpr double ClearanceAlongGap = 1600.0; // ...for points at least this far apart along the lap
	constexpr double GridRunUp = 1220.0;         // straight road behind the start line for the 2 x 2 grid
	constexpr double MinStartStraight = 1500.0;
	constexpr float CellChance = 0.72f;

	/**
	 * Share of layouts kept by corner count. Plain ovals (4 corners) and L-shapes (6) are the outlines random cells
	 * make most often but the least interesting to race, so most are thrown away in favour of twistier ones.
	 */
	float KeepChance(int32 NumCorners)
	{
		return NumCorners <= 4 ? 0.1f : (NumCorners <= 6 ? 0.4f : 1.0f);
	}
	constexpr int32 MaxAttempts = 500;

	// Narrow stretches on straights: about two car widths, so cars have to queue or squeeze through.
	constexpr double NarrowWidthMin = 300.0;
	constexpr double NarrowWidthMax = 420.0;
	constexpr double NarrowLengthMin = 450.0;
	constexpr double NarrowLengthMax = 900.0;
	constexpr double NarrowStraightMargin = 60.0;  // full-width road kept at each end of the straight
	constexpr double GridClearAhead = 250.0;       // road past the start line kept full width

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

bool RaceTrackGenerator::Generate(FRandomStream& Random, FRaceTrackLayout& OutLayout, bool bNarrowSections, int32 ForcedRows)
{
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		// Grid: 2 columns and 2-4 rows (4.5 m fits four), with random cut positions; every cell >= MinCellSize.
		const float RowRoll = Random.FRand();
		const int32 Rows = (ForcedRows >= 2 && ForcedRows <= 4) ? ForcedRows : (RowRoll < 0.35f ? 4 : (RowRoll < 0.8f ? 3 : 2));
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
		if (NumCorners < 4 || Random.FRand() > KeepChance(NumCorners))
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
		const double StartAlong = TangentA + RandomRange(Random, GridRunUp + 60.0, Straight - 150.0); // along the start side
		Layout.StartLine = A + Direction * StartAlong;

		// Narrow stretches on random straights (none, one or two), away from the start grid.
		if (bNarrowSections)
		{
			const float NarrowRoll = Random.FRand();
			const int32 Wanted = NarrowRoll < 0.15f ? 0 : (NarrowRoll < 0.7f ? 1 : 2);
			TArray<int32> Sides;
			for (int32 Index = 0; Index < NumCorners; ++Index)
			{
				Sides.Insert(Index, Random.RandRange(0, Sides.Num()));
			}
			for (int32 CandidateSide : Sides)
			{
				if (Layout.Narrowings.Num() >= Wanted)
				{
					break;
				}
				const FVector2D& From = Corners[CandidateSide];
				const FVector2D& To = Corners[(CandidateSide + 1) % NumCorners];
				const FVector2D Along = (To - From).GetSafeNormal();
				const double StraightStart = TangentLength(Corners[(CandidateSide + NumCorners - 1) % NumCorners], From, To);
				const double StraightEnd = FVector2D::Distance(From, To) - TangentLength(From, To, Corners[(CandidateSide + 2) % NumCorners]);
				const double Length = RandomRange(Random, NarrowLengthMin, NarrowLengthMax);
				const double Reach = Length * 0.5 + FRaceTrackPath::NarrowingTaper + NarrowStraightMargin;
				if (StraightEnd - StraightStart < 2.0 * Reach)
				{
					continue;
				}
				const double Centre = RandomRange(Random, StraightStart + Reach, StraightEnd - Reach);
				if (CandidateSide == Side && Centre + Reach > StartAlong - GridRunUp - 130.0 && Centre - Reach < StartAlong + GridClearAhead)
				{
					continue; // would squeeze the start grid
				}
				FRaceTrackNarrowing& Narrowing = Layout.Narrowings.AddDefaulted_GetRef();
				Narrowing.Centre = From + Along * Centre;
				Narrowing.Length = float(Length);
				Narrowing.Width = float(RandomRange(Random, NarrowWidthMin, NarrowWidthMax));
			}
		}

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
		if (Layout.Narrowings.Num() > 0)
		{
			Layout.Name += FString::Printf(TEXT(" +%d narrow"), Layout.Narrowings.Num());
		}

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
	Path.Build(Corners, Layout.StartLine, Layout.Narrowings);
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

	// Narrow stretches: sensible width, on a straight (tapers included), clear of the grid and of each other.
	const TArray<FRaceTrackPath::FNarrowingSpan>& Spans = Path.GetNarrowingSpans();
	for (int32 Index = 0; Index < Spans.Num(); ++Index)
	{
		const FRaceTrackPath::FNarrowingSpan& Span = Spans[Index];
		const FRaceTrackNarrowing& Narrowing = Layout.Narrowings[Index];
		if (Narrowing.Width < NarrowWidthMin - 1.0 || Narrowing.Width > FRaceTrackPath::TrackWidth)
		{
			return FString::Printf(TEXT("narrowing %d is %.0f wide"), Index, Narrowing.Width);
		}
		if (FVector2D::Distance(Path.GetPointAtDistance(Span.CentreDistance), Narrowing.Centre) > 5.0)
		{
			return FString::Printf(TEXT("narrowing %d is off the road"), Index);
		}
		const float Reach = Span.HalfLength + FRaceTrackPath::NarrowingTaper;
		FVector2D AtCentre;
		FVector2D BeforeTaper;
		FVector2D AfterTaper;
		Path.GetPointAtDistance(Span.CentreDistance, &AtCentre);
		Path.GetPointAtDistance(Span.CentreDistance - Reach - 30.0f, &BeforeTaper);
		Path.GetPointAtDistance(Span.CentreDistance + Reach + 30.0f, &AfterTaper);
		if ((AtCentre | BeforeTaper) < 0.999 || (AtCentre | AfterTaper) < 0.999)
		{
			return FString::Printf(TEXT("narrowing %d isn't on a straight"), Index);
		}
		const float FromLine = Path.WrapDistance(Span.CentreDistance - StartDistance + LapLength * 0.5f) - LapLength * 0.5f;
		if (FromLine + Reach > -float(GridRunUp + 130.0) && FromLine - Reach < float(GridClearAhead))
		{
			return FString::Printf(TEXT("narrowing %d squeezes the start grid"), Index);
		}
		for (int32 Other = 0; Other < Index; ++Other)
		{
			const float Gap = FMath::Abs(Path.WrapDistance(Span.CentreDistance - Spans[Other].CentreDistance + LapLength * 0.5f) - LapLength * 0.5f);
			if (Gap < Reach + Spans[Other].HalfLength + FRaceTrackPath::NarrowingTaper + 100.0f)
			{
				return FString::Printf(TEXT("narrowings %d and %d overlap"), Other, Index);
			}
		}
	}
	return FString();
}
