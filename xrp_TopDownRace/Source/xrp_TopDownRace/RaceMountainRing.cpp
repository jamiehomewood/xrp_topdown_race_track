#include "RaceMountainRing.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
		bool bUseHills;
	};

	const FRange Ranges[] = {
		{ 30, 12000.0f, 1200.0f, 4.5f, 8.5f, 2.4f, 0.0f, true },    // near: hills and mountains
		{ 40, 21000.0f, 2000.0f, 9.0f, 14.0f, 2.8f, 0.5f, false },  // far: taller mountains, closes every gap
	};

	const TCHAR* MountainMesh = TEXT("/Game/LowPolyNatureLite/Assets/Models/SM_Mountain01.SM_Mountain01");
	const TCHAR* HillMeshes[] = {
		TEXT("/Game/LowPolyNatureLite/Assets/Models/SM_Hills01.SM_Hills01"),
		TEXT("/Game/LowPolyNatureLite/Assets/Models/SM_Hills02.SM_Hills02"),
	};
}

ARaceMountainRing::ARaceMountainRing()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ARaceMountainRing::BeginPlay()
{
	Super::BeginPlay();

	UStaticMesh* Mountain = LoadObject<UStaticMesh>(nullptr, MountainMesh);
	TArray<UStaticMesh*> Hills;
	for (const TCHAR* Path : HillMeshes)
	{
		if (UStaticMesh* Hill = LoadObject<UStaticMesh>(nullptr, Path))
		{
			Hills.Add(Hill);
		}
	}
	if (!Mountain)
	{
		UE_LOG(LogRaceMountains, Warning, TEXT("race.Mountains missing %s"), MountainMesh);
		return;
	}

	FRandomStream Random(1717);
	for (const FRange& Range : Ranges)
	{
		const float Spacing = UE_TWO_PI * Range.Distance / Range.Count;
		for (int32 Index = 0; Index < Range.Count; ++Index)
		{
			UStaticMesh* Mesh = (Range.bUseHills && Hills.Num() > 0 && Random.FRand() < 0.4f) ? Hills[Random.RandRange(0, Hills.Num() - 1)] : Mountain;
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
			const FVector Scale(Width / FMath::Max(Size.X, Size.Y), Width / FMath::Max(Size.X, Size.Y), PeakHeight / Size.Z);
			const FVector Location(FMath::Cos(Azimuth) * Distance, FMath::Sin(Azimuth) * Distance, -Bounds.Min.Z * Scale.Z - PeakHeight * 0.04f);

			UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
			Component->SetStaticMesh(Mesh);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCastShadow(false);
			Component->SetupAttachment(RootComponent);
			Component->SetRelativeTransform(FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), Location, Scale));
			Component->RegisterComponent();
			Mountains.Add(Component);
		}
	}
	UE_LOG(LogRaceMountains, Log, TEXT("race.Mountains %d ranges, %d pieces (mountain model %.0f x %.0f x %.0f UU)"),
		int32(UE_ARRAY_COUNT(Ranges)), Mountains.Num(), Mountain->GetBoundingBox().GetSize().X, Mountain->GetBoundingBox().GetSize().Y, Mountain->GetBoundingBox().GetSize().Z);
}
