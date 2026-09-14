#pragma once

#include "CoreMinimal.h"

/**
 * The track centreline in race direction, for lap timing, race positions and the test autopilot.
 * Mirrors Tools/TrackGen/track_geom.py (same control polygon, corner radius, arc segments and start line):
 * keep the two in sync if the track shape changes.
 */
struct XRP_TOPDOWNRACE_API FRaceTrackPath
{
	FRaceTrackPath();

	/** Distance along the lap (0..LapLength) of the centreline point closest to Location. */
	float GetDistanceAlong(const FVector2D& Location) const;

	/** Centreline point at a distance along the lap (wraps), and the unit direction of travel there. */
	FVector2D GetPointAtDistance(float Distance, FVector2D* OutDirection = nullptr) const;

	/** Wraps any distance into 0..LapLength. */
	float WrapDistance(float Distance) const;

	float GetLapLength() const { return LapLength; }

	/** Distance along the lap of the start/finish line. */
	float GetStartLineDistance() const { return StartLineDistance; }

private:
	TArray<FVector2D> Points;
	TArray<float> CumulativeDistance;
	float LapLength = 0.0f;
	float StartLineDistance = 0.0f;
};
