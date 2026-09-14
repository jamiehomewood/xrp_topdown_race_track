#pragma once

#include "CoreMinimal.h"

/**
 * The track centreline in race direction: lap timing, race positions, the computer drivers' line, the start grid,
 * and the road ARaceTrackBuilder builds. Made from a closed control polygon whose corners are rounded with
 * CornerRadius (the same filleting as Tools/TrackGen/track_geom.py, which built the level's original track).
 */
struct XRP_TOPDOWNRACE_API FRaceTrackPath
{
	static constexpr double CornerRadius = 500.0;
	static constexpr int32 ArcSegments = 10;          // low-poly corners
	static constexpr float TrackWidth = 700.0f;
	static constexpr float WallWidth = 40.0f;
	static constexpr float WallHeight = 60.0f;

	/** The original track. */
	FRaceTrackPath();

	/** Rebuilds from a control polygon in race order, with the start line at the centreline point nearest StartLine. */
	void Build(const TArray<FVector2D>& ControlPoints, const FVector2D& StartLine);

	/** The original track's control polygon and start line (track_geom.CONTROL_POINTS, START_LINE_X). */
	static TArray<FVector2D> ClassicControlPoints();
	static FVector2D ClassicStartLine();

	/** Distance along the lap (0..LapLength) of the centreline point closest to Location. */
	float GetDistanceAlong(const FVector2D& Location) const;

	/** Shortest distance from Location to the centreline. */
	float GetDistanceToCentreline(const FVector2D& Location) const;

	/** Centreline point at a distance along the lap (wraps), and the unit direction of travel there. */
	FVector2D GetPointAtDistance(float Distance, FVector2D* OutDirection = nullptr) const;

	/** Wraps any distance into 0..LapLength. */
	float WrapDistance(float Distance) const;

	float GetLapLength() const { return LapLength; }

	/** Distance along the lap of the start/finish line. */
	float GetStartLineDistance() const { return StartLineDistance; }

	/** The centreline as a closed polyline (no repeated end point). */
	const TArray<FVector2D>& GetPoints() const { return Points; }

private:
	TArray<FVector2D> Points;
	TArray<float> CumulativeDistance;
	float LapLength = 0.0f;
	float StartLineDistance = 0.0f;
};
