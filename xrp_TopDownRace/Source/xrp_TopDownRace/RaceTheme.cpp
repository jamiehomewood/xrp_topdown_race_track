#include "RaceTheme.h"

#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace
{
	const TCHAR* ForestRoot = TEXT("/Game/LPRiverForest/");

	FString Nature(const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/LowPolyNatureLite/Assets/Models/%s.%s"), Name, Name);
	}

	FString MeadowFence(const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/Fab/Low_Poly_Meadow_Barrier_Bundle__Fences___Walls/%s/StaticMeshes/%s.%s"), Name, Name, Name);
	}

	FString Forest(const TCHAR* Folder, const TCHAR* Name)
	{
		return FString::Printf(TEXT("/Game/LPRiverForest/Meshes/%s/%s.%s"), Folder, Name, Name);
	}

	FRaceThemeContent MakeCountryside()
	{
		// As Tools/TrackGen/build_scenery.py placed in the level.
		FRaceThemeContent Theme;
		Theme.FloorProps = {
			{ Nature(TEXT("SM_Bush_Simple")), 5, 90, 0.85f, 1.2f, true },
			{ Nature(TEXT("SM_Bush_Berries_Red")), 2, 110, 0.85f, 1.2f, true },
			{ Nature(TEXT("SM_Bush_Berries_blue")), 2, 110, 0.85f, 1.2f, true },
			{ Nature(TEXT("SM_Bush_Berries_Empty")), 2, 110, 0.85f, 1.2f, true },
			{ Nature(TEXT("SM_Grass_Array01")), 5, 85, 0.85f, 1.2f, true },
			{ Nature(TEXT("SM_Grass01")), 6, 45, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Grass03")), 4, 30, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Plant02")), 3, 85, 0.85f, 1.2f, true },
			{ Nature(TEXT("SM_Flower02_Orange")), 3, 40, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Flower02_Pink")), 3, 40, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Flower02_Yellow")), 3, 40, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Hat_Mushroom_red")), 1, 40, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Mushrooom01_brown")), 1, 35, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Stone02")), 3, 50, 0.85f, 1.2f, false },
			{ Nature(TEXT("SM_Stones02")), 2, 110, 0.85f, 1.2f, true },
			{ Nature(TEXT("SM_Rock02")), 2, 120, 0.85f, 1.2f, true },
		};
		Theme.FloorFeatures = {
			{ Nature(TEXT("SM_Tent_Blue")), 1, 230, 1.0f, 1.0f, true },
			{ Nature(TEXT("SM_Tent_Red")), 1, 230, 1.0f, 1.0f, true },
			{ Nature(TEXT("SM_Log")), 1, 140, 1.0f, 1.0f, true },
			{ Nature(TEXT("SM_Log")), 1, 140, 1.0f, 1.0f, true },
			{ Nature(TEXT("SM_Stones02")), 1, 140, 1.0f, 1.0f, true },
			{ Nature(TEXT("SM_Branch01")), 1, 140, 1.0f, 1.0f, true },
		};
		Theme.FloorPropCount = 190;
		for (const TCHAR* Piece : { TEXT("EA03_Fence_Plank_01a"), TEXT("EA03_Fence_Plank_01b"), TEXT("EA03_Fence_Plank_01c"), TEXT("EA03_Fence_Plank_01d"), TEXT("EA03_Fence_Plank_01e") })
		{
			Theme.FencePieces.Add(MeadowFence(Piece));
		}
		Theme.FencePieceLength = 150.0f;
		Theme.StartPost = MeadowFence(TEXT("EA03_Wooden_Pin_01d"));
		Theme.NarrowingProps = {
			{ Nature(TEXT("SM_Rock02")), 0.6f, 60, 0.45f, 0.65f, true },
			{ Nature(TEXT("SM_Bush_Simple")), 0.4f, 80, 0.8f, 1.05f, true },
		};
		Theme.bUsesLevelBackdrop = true;
		Theme.NearMountains = { Nature(TEXT("SM_Mountain01")), Nature(TEXT("SM_Mountain01")), Nature(TEXT("SM_Mountain01")), Nature(TEXT("SM_Hills01")), Nature(TEXT("SM_Hills02")) };
		Theme.FarMountains = { Nature(TEXT("SM_Mountain01")) };
		return Theme;
	}

	FRaceThemeContent MakeRiverForest()
	{
		// LPRiverForest models are at the same scale as the cars (trees 4-6.5 m, about 2-3 car lengths). The small
		// plants are scaled up so they still read from the room camera 1.7 m above the floor.
		FRaceThemeContent Theme;
		Theme.FloorProps = {
			{ Forest(TEXT("Plants"), TEXT("SM_LPBush01")), 5, 100, 0.9f, 1.3f, true },
			{ Forest(TEXT("Plants"), TEXT("SM_LPBush02")), 5, 95, 0.9f, 1.3f, true },
			{ Forest(TEXT("Plants"), TEXT("SM_LPGrass01")), 6, 22, 1.8f, 2.6f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPGrass02")), 5, 18, 1.8f, 2.6f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPGrass03")), 5, 18, 1.8f, 2.6f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPFlower01")), 4, 20, 2.0f, 3.0f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPFlower02")), 4, 25, 2.0f, 3.0f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPPlant01")), 3, 35, 1.5f, 2.2f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPPlant02")), 3, 45, 1.5f, 2.2f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPMushroom01")), 1, 12, 3.0f, 4.5f, false },
			{ Forest(TEXT("Plants"), TEXT("SM_LPMushroom03")), 1, 35, 1.5f, 2.5f, false },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRock02")), 3, 85, 0.7f, 1.1f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRock03")), 3, 55, 0.8f, 1.2f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRock05")), 3, 65, 0.8f, 1.2f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRock01")), 2, 125, 0.6f, 0.9f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRockPebble01")), 2, 18, 2.0f, 3.0f, false },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRockPebble02")), 2, 18, 2.0f, 3.0f, false },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRockPebble03")), 2, 18, 2.0f, 3.0f, false },
			{ Forest(TEXT("Trees"), TEXT("SM_LPStick01")), 2, 28, 1.5f, 2.2f, false },
			{ Forest(TEXT("Trees"), TEXT("SM_LPStick02")), 2, 25, 1.5f, 2.2f, false },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTreeLog01")), 1, 45, 1.0f, 1.4f, true },
		};
		Theme.FloorFeatures = {
			{ Forest(TEXT("HeroProps"), TEXT("SM_LPRockSword01")), 1, 150, 0.9f, 1.1f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRock07")), 1, 430, 0.5f, 0.7f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTreeLog02")), 1, 125, 1.0f, 1.2f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTreeLog03")), 1, 150, 1.0f, 1.2f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPLandscapeRock01")), 1, 150, 0.8f, 1.1f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPLandscapeRock02")), 1, 140, 0.8f, 1.1f, true },
			{ Forest(TEXT("HeroProps"), TEXT("SM_LPRockFace01")), 1, 110, 0.9f, 1.1f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPPineTree02")), 1, 190, 0.7f, 0.9f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTree02")), 1, 160, 0.7f, 0.9f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTree04")), 1, 130, 0.8f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTreeNaked01")), 1, 70, 0.9f, 1.1f, true },
		};
		Theme.FloorPropCount = 240;
		Theme.FencePieces = { Forest(TEXT("HeroProps"), TEXT("SM_LPFence01")) };
		Theme.FencePieceLength = 160.0f;
		Theme.StartPost = Forest(TEXT("HeroProps"), TEXT("SM_LPFence02"));
		Theme.NarrowingWallRock = Forest(TEXT("Cliffs"), TEXT("SM_LPCliffRock02"));

		Theme.bUsesLevelBackdrop = false;
		Theme.bBuildGround = true;
		Theme.BackdropProps = {
			{ Forest(TEXT("Trees"), TEXT("SM_LPPineTree01")), 6, 200, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPPineTree02")), 5, 180, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPPineTree03")), 5, 175, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTree01")), 3, 200, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTree02")), 3, 150, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTree03")), 3, 210, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTree04")), 2, 120, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTree05")), 3, 160, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTreeNaked01")), 1, 80, 1.0f, 1.0f, true },
			{ Forest(TEXT("Trees"), TEXT("SM_LPTreeNaked02")), 1, 90, 1.0f, 1.0f, true },
			{ Forest(TEXT("Plants"), TEXT("SM_LPBush01")), 3, 100, 1.0f, 1.3f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRock06")), 1, 220, 1.0f, 1.0f, true },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPLandscapeRock01")), 1, 150, 1.0f, 1.0f, true },
			{ Forest(TEXT("Cliffs"), TEXT("SM_LPCliffRock01")), 1, 470, 1.0f, 1.0f, true },
		};
		Theme.BackdropCount = 320;
		Theme.LakeMesh = Forest(TEXT("Water"), TEXT("SM_LPLake01"));

		Theme.StreamMesh = Forest(TEXT("Water"), TEXT("SM_LPRiverPlane01"));
		Theme.MaxStreams = 2;
		Theme.StreamBankProps = {
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRockPebble02")), 3, 18, 2.0f, 3.0f, false },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRock03")), 2, 55, 0.4f, 0.6f, true },
			{ Forest(TEXT("Plants"), TEXT("SM_LPGrass01")), 3, 22, 1.8f, 2.4f, false },
			{ Forest(TEXT("Rocks"), TEXT("SM_LPRockSlab01")), 1, 55, 0.5f, 0.7f, false },
		};
		Theme.SteppingStones = { Forest(TEXT("Rocks"), TEXT("SM_LPRockSlab01")), Forest(TEXT("Rocks"), TEXT("SM_LPRockSlab02")), Forest(TEXT("Rocks"), TEXT("SM_LPRockSlabCurved01")) };

		Theme.NearMountains = { Forest(TEXT("Cliffs"), TEXT("SM_LPBGMountain01")), Forest(TEXT("Cliffs"), TEXT("SM_LPBGMountain02")), Forest(TEXT("Cliffs"), TEXT("SM_LPCliffRockWall01")) };
		Theme.FarMountains = { Forest(TEXT("Cliffs"), TEXT("SM_LPBGMountain01")), Forest(TEXT("Cliffs"), TEXT("SM_LPBGMountain02")) };
		return Theme;
	}
}

const FRaceThemeContent& RaceTheme::Get(ERaceThemeKind Theme)
{
	static const FRaceThemeContent Countryside = MakeCountryside();
	static const FRaceThemeContent RiverForest = MakeRiverForest();
	return Theme == ERaceThemeKind::RiverForest ? RiverForest : Countryside;
}

const TCHAR* RaceTheme::Name(ERaceThemeKind Theme)
{
	return Theme == ERaceThemeKind::RiverForest ? TEXT("river forest") : TEXT("countryside");
}

const TCHAR* RaceTheme::Name(ERaceSeasonKind Season)
{
	switch (Season)
	{
	case ERaceSeasonKind::Autumn: return TEXT("Autumn");
	case ERaceSeasonKind::Snow: return TEXT("Snow");
	default: return TEXT("Summer");
	}
}

UMaterialInterface* RaceTheme::SeasonMaterial(UMaterialInterface* Material, ERaceSeasonKind Season)
{
	if (!Material || Season == ERaceSeasonKind::Summer)
	{
		return Material;
	}
	const FString PackageName = Material->GetOutermost()->GetName(); // /Game/LPRiverForest/Materials/Trees/MI_TreeLeaves
	if (!PackageName.StartsWith(ForestRoot))
	{
		return Material;
	}
	const FString SeasonName = Name(Season);
	const FString BaseName = FPaths::GetCleanFilename(PackageName);
	const FString VariantPackage = FString::Printf(TEXT("%s/%s/%s_%s"), *FPaths::GetPath(PackageName), *SeasonName, *BaseName, *SeasonName);

	// Remember which variants exist so missing ones aren't looked up on disk for every prop.
	static TMap<FString, bool> VariantExists;
	bool* bExists = VariantExists.Find(VariantPackage);
	if (!bExists)
	{
		bExists = &VariantExists.Add(VariantPackage, FPackageName::DoesPackageExist(VariantPackage));
	}
	if (!*bExists)
	{
		return Material;
	}
	const FString VariantObject = FString::Printf(TEXT("%s.%s_%s"), *VariantPackage, *BaseName, *SeasonName);
	UMaterialInterface* Variant = LoadObject<UMaterialInterface>(nullptr, *VariantObject);
	return Variant ? Variant : Material;
}

FString RaceTheme::SeasonMesh(const FString& MeshPath, ERaceSeasonKind Season)
{
	if (Season == ERaceSeasonKind::Snow && MeshPath.Contains(TEXT("/SM_LPPineTree0")) && !MeshPath.Contains(TEXT("Snow")))
	{
		return Forest(TEXT("Trees"), TEXT("SM_LPPineTree01Snow"));
	}
	return MeshPath;
}

FLinearColor RaceTheme::GroundColor(ERaceSeasonKind Season)
{
	switch (Season)
	{
	case ERaceSeasonKind::Autumn: return FLinearColor(0.40f, 0.23f, 0.07f);
	case ERaceSeasonKind::Snow: return FLinearColor(0.80f, 0.84f, 0.82f);
	default: return FLinearColor(0.16f, 0.30f, 0.06f);
	}
}

FLinearColor RaceTheme::WaterColor(ERaceSeasonKind Season)
{
	switch (Season)
	{
	case ERaceSeasonKind::Autumn: return FLinearColor(0.04f, 0.22f, 0.26f);
	case ERaceSeasonKind::Snow: return FLinearColor(0.50f, 0.68f, 0.78f);
	default: return FLinearColor(0.03f, 0.20f, 0.30f);
	}
}
