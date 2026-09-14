#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceTheme.h"
#include "RaceMountainRing.generated.h"

class UStaticMeshComponent;

/**
 * Low-poly mountain ranges all the way round the room, overlapping into one ridge so the flat horizon line never
 * shows on the wall projection. The horizon itself sits at the Igloo eye height (the cameras must stay there for
 * the floor to be true to scale), so the ridge rises a few degrees above it: a near range and a taller far range,
 * with models (and season) from the scenery theme.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceMountainRing : public AActor
{
	GENERATED_BODY()

public:
	ARaceMountainRing();

	/** (Re)builds the ranges for a theme and season; does nothing if they're already built. */
	void Build(ERaceThemeKind Theme, ERaceSeasonKind Season);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Mountains;

	bool bBuilt = false;
	ERaceThemeKind BuiltTheme = ERaceThemeKind::Countryside;
	ERaceSeasonKind BuiltSeason = ERaceSeasonKind::Summer;
};
