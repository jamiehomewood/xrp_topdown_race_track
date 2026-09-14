#include "RaceTrackPath.h"

namespace
{
	// track_geom.py: CONTROL_POINTS (race order), CORNER_RADIUS, ARC_SEGMENTS, START_LINE_X.
	const FVector2D ControlPoints[] = {
		{ -1450.0, -1950.0 }, { 1450.0, -1950.0 }, { 1450.0, 1950.0 }, { -1450.0, 1950.0 },
		{ -1450.0, 850.0 }, { -50.0, 850.0 }, { -50.0, -650.0 }, { -1450.0, -650.0 },
	};
	constexpr double CornerRadius = 500.0;
	constexpr int32 ArcSegments = 10;
	constexpr double StartLineX = 300.0;
}

FRaceTrackPath::FRaceTrackPath()
{
	// Same corner filleting as track_geom.centreline().
	const int32 NumControl = UE_ARRAY_COUNT(ControlPoints);
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
	StartLineDistance = GetDistanceAlong(FVector2D(StartLineX, ControlPoints[0].Y));
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

FVector2D FRaceTrackPath::GetPointAtDistance(float Distance, FVector2D* OutDirection) const
{
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
