#pragma once

#include "CoreMinimal.h"
#include "RaceTrackPath.h"

/** A circuit: its control polygon in race order (corners get rounded), where the start line goes, and narrow stretches. */
struct FRaceTrackLayout
{
	TArray<FVector2D> ControlPoints;
	FVector2D StartLine = FVector2D::ZeroVector;
	TArray<FRaceTrackNarrowing> Narrowings;
	FString Name;
};

/**
 * Random circuits for the room. A layout is the outline of a random group of cells on a 2-column, 2-4 row grid whose cut
 * positions are random too (the original track is one of these: 6 cells with a side cell missing), so the road never
 * crosses itself and every corner is a rounded right angle. The direction of travel and the start straight are
 * random. Every layout is checked before use: it stays inside the room floor's walkway, every straight is long
 * enough for its corners, neighbouring pieces of road have grass between their walls, the lap isn't too short, and
 * the start line sits on a straight with room for the grid behind it.
 */
namespace RaceTrackGenerator
{
	/** The original hand-made track. */
	XRP_TOPDOWNRACE_API FRaceTrackLayout Classic();

	/**
	 * A random valid layout, usually with one or two narrow stretches if bNarrowSections, on a grid of Rows rows
	 * (2-4; anything else = a random mix). False only if none was found (use Classic then).
	 */
	XRP_TOPDOWNRACE_API bool Generate(FRandomStream& Random, FRaceTrackLayout& OutLayout, bool bNarrowSections = true, int32 Rows = 0);

	/** Why a layout can't be raced on, or an empty string if it's fine. */
	XRP_TOPDOWNRACE_API FString Validate(const FRaceTrackLayout& Layout);
}
