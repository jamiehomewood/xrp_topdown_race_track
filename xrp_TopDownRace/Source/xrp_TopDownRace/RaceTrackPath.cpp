#include "RaceTrackPath.h"

TArray<FVector2D> FRaceTrackPath::ClassicControlPoints()
{
	return {
		{ -1450.0, -1950.0 }, { 1450.0, -1950.0 }, { 1450.0, 1950.0 }, { -1450.0, 1950.0 },
		{ -1450.0, 850.0 }, { -50.0, 850.0 }, { -50.0, -650.0 }, { -1450.0, -650.0 },
	};
}

FVector2D FRaceTrackPath::ClassicStartLine()
{
	return FVector2D(300.0, -1950.0);
}

FRaceTrackPath::FRaceTrackPath()
{
	Build(ClassicControlPoints(), ClassicStartLine());
}

void FRaceTrackPath::Build(const TArray<FVector2D>& ControlPoints, const FVector2D& StartLine, const TArray<FRaceTrackNarrowing>& Narrowings)
{
	Points.Reset();
	CumulativeDistance.Reset();
	NarrowingSpans.Reset();
	LapLength = 0.0f;
	StartLineDistance = 0.0f;

	// Same corner filleting as track_geom.centreline().
	const int32 NumControl = ControlPoints.Num();
	if (NumControl < 3)
	{
		return;
	}
	for (int32 Index = 0; Index < NumControl; ++Index)
	{
		const FVector2D& Previous = ControlPoints[(Index + NumControl - 1) % NumControl];
		const FVector2D& Corner = ControlPoints[Index];
		const FVector2D& Next = ControlPoints[(Index + 1) % NumControl];
		const FVector2D In = (Corner - Previous).GetSafeNormal();
		const FVector2D Out = (Next - Corner).GetSafeNormal();
		const double Cross = In.X * Out.Y - In.Y * Out.X;
		const double Theta = FMath::Acos(FMath::Clamp(In | Out, -1.0, 1.0));
		const FVector2D TangentStart = Corner - In * CornerRadius * FMath::Tan(Theta / 2.0);
		const double Side = Cross > 0.0 ? 1.0 : -1.0;
		const FVector2D Centre(TangentStart.X - In.Y * CornerRadius * Side, TangentStart.Y + In.X * CornerRadius * Side);
		const double StartAngle = FMath::Atan2(TangentStart.Y - Centre.Y, TangentStart.X - Centre.X);
		for (int32 Step = 0; Step <= ArcSegments; ++Step)
		{
			const double Angle = StartAngle + Side * Theta * Step / ArcSegments;
			Points.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * CornerRadius);
		}
	}

	CumulativeDistance.SetNum(Points.Num());
	double Total = 0.0;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		CumulativeDistance[Index] = float(Total);
		Total += FVector2D::Distance(Points[Index], Points[(Index + 1) % Points.Num()]);
	}
	LapLength = float(Total);
	StartLineDistance = GetDistanceAlong(StartLine);

	for (const FRaceTrackNarrowing& Narrowing : Narrowings)
	{
		FNarrowingSpan& Span = NarrowingSpans.AddDefaulted_GetRef();
		Span.CentreDistance = GetDistanceAlong(Narrowing.Centre);
		Span.HalfLength = Narrowing.Length * 0.5f;
		Span.HalfWidth = FMath::Min(Narrowing.Width, TrackWidth) * 0.5f;
	}
}

float FRaceTrackPath::GetHalfWidthAt(float Distance) const
{
	const float FullHalfWidth = TrackWidth * 0.5f;
	float HalfWidth = FullHalfWidth;
	for (const FNarrowingSpan& Span : NarrowingSpans)
	{
		// Distance from the narrowing's centre, either way round the lap.
		const float Offset = FMath::Abs(WrapDistance(Distance - Span.CentreDistance + LapLength * 0.5f) - LapLength * 0.5f);
		if (Offset < Span.HalfLength + NarrowingTaper)
		{
			const float Alpha = FMath::Clamp((Offset - Span.HalfLength) / NarrowingTaper, 0.0f, 1.0f);
			HalfWidth = FMath::Min(HalfWidth, FMath::Lerp(Span.HalfWidth, FullHalfWidth, Alpha));
		}
	}
	return HalfWidth;
}

TArray<float> FRaceTrackPath::GetWidthBreakDistances() const
{
	TArray<float> Breaks;
	for (const FNarrowingSpan& Span : NarrowingSpans)
	{
		for (const float Offset : { -Span.HalfLength - NarrowingTaper, -Span.HalfLength, Span.HalfLength, Span.HalfLength + NarrowingTaper })
		{
			Breaks.Add(WrapDistance(Span.CentreDistance + Offset));
		}
	}
	Breaks.Sort();
	return Breaks;
}

float FRaceTrackPath::WrapDistance(float Distance) const
{
	return LapLength > 0.0f ? FMath::Fmod(FMath::Fmod(Distance, LapLength) + LapLength, LapLength) : 0.0f;
}

float FRaceTrackPath::GetDistanceAlong(const FVector2D& Location) const
{
	double BestDistanceSquared = TNumericLimits<double>::Max();
	float BestAlong = 0.0f;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector2D& A = Points[Index];
		const FVector2D& B = Points[(Index + 1) % Points.Num()];
		const FVector2D Segment = B - A;
		const double LengthSquared = Segment.SizeSquared();
		const double T = LengthSquared > 0.0 ? FMath::Clamp(((Location - A) | Segment) / LengthSquared, 0.0, 1.0) : 0.0;
		const double DistanceSquared = FVector2D::DistSquared(Location, A + Segment * T);
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestAlong = CumulativeDistance[Index] + float(FMath::Sqrt(LengthSquared) * T);
		}
	}
	return BestAlong;
}

float FRaceTrackPath::GetDistanceToCentreline(const FVector2D& Location) const
{
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector2D& A = Points[Index];
		const FVector2D Segment = Points[(Index + 1) % Points.Num()] - A;
		const double LengthSquared = Segment.SizeSquared();
		const double T = LengthSquared > 0.0 ? FMath::Clamp(((Location - A) | Segment) / LengthSquared, 0.0, 1.0) : 0.0;
		BestDistanceSquared = FMath::Min(BestDistanceSquared, FVector2D::DistSquared(Location, A + Segment * T));
	}
	return float(FMath::Sqrt(BestDistanceSquared));
}

FVector2D FRaceTrackPath::GetPointAtDistance(float Distance, FVector2D* OutDirection) const
{
	if (Points.Num() < 2)
	{
		return FVector2D::ZeroVector;
	}
	const float Wrapped = WrapDistance(Distance);
	int32 Index = Points.Num() - 1;
	for (int32 Candidate = 0; Candidate < Points.Num() - 1; ++Candidate)
	{
		if (Wrapped < CumulativeDistance[Candidate + 1])
		{
			Index = Candidate;
			break;
		}
	}
	const FVector2D& A = Points[Index];
	const FVector2D& B = Points[(Index + 1) % Points.Num()];
	const double SegmentLength = FVector2D::Distance(A, B);
	const double T = SegmentLength > 0.0 ? (Wrapped - CumulativeDistance[Index]) / SegmentLength : 0.0;
	if (OutDirection)
	{
		*OutDirection = (B - A).GetSafeNormal();
	}
	return A + (B - A) * FMath::Clamp(T, 0.0, 1.0);
}
