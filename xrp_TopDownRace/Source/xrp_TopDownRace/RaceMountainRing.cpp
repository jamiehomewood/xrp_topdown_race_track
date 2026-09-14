#include "RaceMountainRing.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaceMountains, Log, All);

namespace
{
	constexpr float EyeHeight = 1700.0f; // Igloo viewpoint height: the horizon on the walls

	struct FRange
	{
		int32 Count;
		float Distance;
		float DistanceJitter;
		float PeakDegreesMin;      // peak height as an angle above the horizon, seen from the room
		float PeakDegreesMax;
		float WidthPerSpacing;     // mountain width / gap between neighbours (> 2 overlaps them into one ridge)
		float AzimuthOffset;       // in units of the gap, so the ranges' valleys don't line up
	};

	const FRange Ranges[] = {
		{ 30, 12000.0f, 1200.0f, 4.5f, 8.5f, 2.4f, 0.0f },    // near
		{ 40, 21000.0f, 2000.0f, 9.0f, 14.0f, 2.8f, 0.5f },   // far: taller, closes every gap
	};
}

ARaceMountainRing::ARaceMountainRing()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ARaceMountainRing::Build(ERaceThemeKind Theme, ERaceSeasonKind Season)
{
	if (bBuilt && Theme == BuiltTheme && Season == BuiltSeason)
	{
		return;
	}
	for (UStaticMeshComponent* Mountain : Mountains)
	{
		if (Mountain)
		{
			Mountain->DestroyComponent();
		}
	}
	Mountains.Reset();
	bBuilt = true;
	BuiltTheme = Theme;
	BuiltSeason = Season;

	const FRaceThemeContent& Content = RaceTheme::Get(Theme);
	FRandomStream Random(1717);
	for (int32 RangeIndex = 0; RangeIndex < UE_ARRAY_COUNT(Ranges); ++RangeIndex)
	{
		const FRange& Range = Ranges[RangeIndex];
		const TArray<FString>& Models = RangeIndex == 0 ? Content.NearMountains : Content.FarMountains;
		if (Models.Num() == 0)
		{
			continue;
		}
		const float Spacing = UE_TWO_PI * Range.Distance / Range.Count;
		for (int32 Index = 0; Index < Range.Count; ++Index)
		{
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Models[Random.RandRange(0, Models.Num() - 1)]);
			if (!Mesh)
			{
				continue;
			}
			const FBox Bounds = Mesh->GetBoundingBox();
			const FVector Size = Bounds.GetSize();
			if (Size.Z <= KINDA_SMALL_NUMBER || FMath::Max(Size.X, Size.Y) <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float Azimuth = (Index + Range.AzimuthOffset + Random.FRandRange(-0.25f, 0.25f)) / Range.Count * UE_TWO_PI;
			const float Distance = Range.Distance + Random.FRandRange(-Range.DistanceJitter, Range.DistanceJitter);
			const float PeakHeight = EyeHeight + Distance * FMath::Tan(FMath::DegreesToRadians(Random.FRandRange(Range.PeakDegreesMin, Range.PeakDegreesMax)));
			const float Width = Spacing * Range.WidthPerSpacing * Random.FRandRange(0.85f, 1.15f);

			// Stretch each model to the wanted width and height, base just below the ground.
			const float Across = Width / FMath::Max(Size.X, Size.Y);
			const FVector Scale(Across, Across, PeakHeight / Size.Z);
			const FVector Location(FMath::Cos(Azimuth) * Distance, FMath::Sin(Azimuth) * Distance, -Bounds.Min.Z * Scale.Z - PeakHeight * 0.04f);

			UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
			Component->SetStaticMesh(Mesh);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCastShadow(false);
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				UMaterialInterface* Material = Mesh->GetMaterial(Slot);
				UMaterialInterface* Seasonal = RaceTheme::SeasonMaterial(Material, Season);
				if (Seasonal != Material)
				{
					Component->SetMaterial(Slot, Seasonal);
				}
			}
			Component->SetupAttachment(RootComponent);
			Component->SetRelativeTransform(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Location, Scale));
			Component->RegisterComponent();
			Mountains.Add(Component);
		}
	}
	UE_LOG(LogRaceMountains, Log, TEXT("race.Mountains %s %s: %d pieces"), RaceTheme::Name(Theme), RaceTheme::Name(Season), Mountains.Num());
}
