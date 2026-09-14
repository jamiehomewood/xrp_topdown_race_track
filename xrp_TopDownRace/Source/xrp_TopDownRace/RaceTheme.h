#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/** Scenery theme actually used for a race (Race Settings can also say "random"). */
enum class ERaceThemeKind : uint8
{
	Countryside,   // LowPolyNatureLite / meadow fences, with the trees and hills saved in the level
	RiverForest,   // LPRiverForest pack, built at runtime (with seasons)
};

/** Season for the River Forest pack's materials (same order as ERaceSeason in Race Settings). */
enum class ERaceSeasonKind : uint8
{
	Summer,
	Autumn,
	Snow,
};

/** A scenery model and how to scatter it. */
struct FRaceProp
{
	FString Mesh;            // object path
	float Weight = 1.0f;
	float Radius = 50.0f;    // footprint at scale 1 (UU)
	float ScaleMin = 1.0f;
	float ScaleMax = 1.0f;
	bool bCastShadow = false;
};

/** Everything ARaceTrackBuilder and ARaceMountainRing need to dress a track in one theme. */
struct FRaceThemeContent
{
	// Room floor (low props only, so nothing looms over the cars on the floor projection).
	TArray<FRaceProp> FloorProps;
	TArray<FRaceProp> FloorFeatures;   // bigger pieces placed first wherever they fit
	int32 FloorPropCount = 190;

	// Fence round the outside of the circuit and posts at the start line.
	TArray<FString> FencePieces;
	float FencePieceLength = 150.0f;
	FString StartPost;

	// Beside narrow stretches: either props scattered in the grass, or one long rock lined up along each kerb.
	TArray<FRaceProp> NarrowingProps;
	FString NarrowingWallRock;

	// Beyond the room walls. Countryside uses the trees, hills and ground saved in the level.
	bool bUsesLevelBackdrop = true;
	bool bBuildGround = false;
	TArray<FRaceProp> BackdropProps;
	int32 BackdropCount = 0;
	FString LakeMesh;

	// Streams across the grass on the room floor.
	FString StreamMesh;
	int32 MaxStreams = 0;
	TArray<FRaceProp> StreamBankProps;
	TArray<FString> SteppingStones;

	// Mountain ring hiding the horizon: near and far ranges.
	TArray<FString> NearMountains;
	TArray<FString> FarMountains;
};

namespace RaceTheme
{
	XRP_TOPDOWNRACE_API const FRaceThemeContent& Get(ERaceThemeKind Theme);
	XRP_TOPDOWNRACE_API const TCHAR* Name(ERaceThemeKind Theme);
	XRP_TOPDOWNRACE_API const TCHAR* Name(ERaceSeasonKind Season);

	/**
	 * The season's version of an LPRiverForest material: the pack keeps them as <folder>/<Season>/<name>_<Season>
	 * (e.g. Trees/Autumn/MI_TreeLeaves_Autumn). Returns the material itself for summer, other packs, or no variant.
	 */
	XRP_TOPDOWNRACE_API UMaterialInterface* SeasonMaterial(UMaterialInterface* Material, ERaceSeasonKind Season);

	/** Snow swaps the pine trees for the pack's snow-covered pine. */
	XRP_TOPDOWNRACE_API FString SeasonMesh(const FString& MeshPath, ERaceSeasonKind Season);

	/** Ground colour for the River Forest ground plane. */
	XRP_TOPDOWNRACE_API FLinearColor GroundColor(ERaceSeasonKind Season);

	/** Colour of streams and the lake (icy in snow). */
	XRP_TOPDOWNRACE_API FLinearColor WaterColor(ERaceSeasonKind Season);
}
