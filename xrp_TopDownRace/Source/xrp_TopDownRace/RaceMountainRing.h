#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceMountainRing.generated.h"

class UStaticMeshComponent;

/**
 * Low-poly mountain ranges all the way round the room, overlapping into one ridge so the flat horizon line never
 * shows on the wall projection. The horizon itself sits at the Igloo eye height (the cameras must stay there for
 * the floor to be true to scale), so the ridge rises a few degrees above it: a near range and a taller far range.
 */
UCLASS()
class XRP_TOPDOWNRACE_API ARaceMountainRing : public AActor
{
	GENERATED_BODY()

public:
	ARaceMountainRing();

	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Mountains;
};
