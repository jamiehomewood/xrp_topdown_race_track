#include "RaceStartLights.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RaceSignalSynth.h"

namespace
{
	// Emissive values are well above 1 so the lit lights read as glowing under the fixed exposure.
	const FLinearColor LitColor(14.0f, 0.2f, 0.05f);
	const FLinearColor DarkColor(0.06f, 0.0f, 0.0f);
	const FLinearColor HousingColor(0.012f, 0.012f, 0.014f);

	// Unlit emissive material made by Tools/TrackGen/build_track.py; the engine shape material is a lit fallback.
	const TCHAR* LightMaterialPath = TEXT("/Game/RaceTrack/Materials/M_RaceLight.M_RaceLight");
	const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	// Engine basic shapes are 100 UU across, centred on their pivot; the cylinder's axis is Z.
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
}

ARaceStartLights::ARaceStartLights()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SignalSound = CreateDefaultSubobject<URaceSignalSynth>(TEXT("SignalSound"));
	SignalSound->SetupAttachment(RootComponent);
	SignalSound->bAutoActivate = false;
	SignalSound->bAllowSpatialization = false;
}

void ARaceStartLights::BeginPlay()
{
	Super::BeginPlay();
	SignalSound->Start();
}

UMaterialInterface* ARaceStartLights::GetBaseMaterial()
{
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, LightMaterialPath);
		if (!BaseMaterial)
		{
			BaseMaterial = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath);
		}
	}
	return BaseMaterial;
}

UStaticMeshComponent* ARaceStartLights::AddMesh(UStaticMesh* Mesh, const FTransform& RelativeTransform, UMaterialInterface* Material)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetupAttachment(RootComponent);
	Component->SetRelativeTransform(RelativeTransform);
	Component->SetMaterial(0, Material);
	Component->RegisterComponent();
	Meshes.Add(Component);
	return Component;
}

void ARaceStartLights::AddBoard(const FTransform& BoardTransform, float LightRadius, float Spacing)
{
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, CylinderPath);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	UMaterialInterface* Material = GetBaseMaterial();
	if (!Cylinder || !Cube || !Material)
	{
		return;
	}

	// Dark housing slab just behind the lights.
	const float Width = Spacing * (NumLights - 1) + LightRadius * 3.0f;
	const float Height = LightRadius * 3.0f;
	const float Thickness = LightRadius * 0.4f;
	UMaterialInstanceDynamic* HousingMaterial = UMaterialInstanceDynamic::Create(Material, this);
	HousingMaterial->SetVectorParameterValue(TEXT("Color"), HousingColor);
	const FTransform HousingLocal(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -Thickness * 0.5f - 6.0f),
		FVector(Height / 100.0f, Width / 100.0f, Thickness / 100.0f));
	AddMesh(Cube, HousingLocal * BoardTransform, HousingMaterial);

	for (int32 Index = 0; Index < NumLights; ++Index)
	{
		UMaterialInstanceDynamic* LightMaterial = UMaterialInstanceDynamic::Create(Material, this);
		LightMaterial->SetVectorParameterValue(TEXT("Color"), Index < LitCount ? LitColor : DarkColor);
		const FTransform LightLocal(FRotator::ZeroRotator, FVector(0.0f, (Index - (NumLights - 1) * 0.5f) * Spacing, 0.0f),
			FVector(LightRadius * 2.0f / 100.0f, LightRadius * 2.0f / 100.0f, 0.1f));
		AddMesh(Cylinder, LightLocal * BoardTransform, LightMaterial);
		LightMaterials.Add(LightMaterial);
	}
}

bool ARaceStartLights::SetLitCount(int32 Count)
{
	Count = FMath::Clamp(Count, 0, NumLights);
	if (Count == LitCount)
	{
		return false;
	}
	LitCount = Count;
	for (int32 Index = 0; Index < LightMaterials.Num(); ++Index)
	{
		if (LightMaterials[Index])
		{
			LightMaterials[Index]->SetVectorParameterValue(TEXT("Color"), (Index % NumLights) < Count ? LitColor : DarkColor);
		}
	}
	return true;
}
