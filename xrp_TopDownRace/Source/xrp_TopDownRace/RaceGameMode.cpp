#include "RaceGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RaceCarPawn.h"
#include "RacePlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogRace, Log, All);

ARaceGameMode::ARaceGameMode()
{
	PlayerControllerClass = ARacePlayerController::StaticClass();
	DefaultPawnClass = nullptr;

	const TCHAR* CarsRoot = TEXT("/Game/Fab/Mobile_Optimize-Free_Low_Poly_Cars");
	for (const TCHAR* Name : { TEXT("Sport_Car_39"), TEXT("N_Muscle_Car_10"), TEXT("Hatchback_Car_15"), TEXT("Police_Car_N_4") })
	{
		CarMeshes.Add(TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(TEXT("%s/%s/StaticMeshes/%s.%s"), CarsRoot, Name, Name, Name))));
	}

	// 2 x 2 grid behind the start line, facing the race direction (+X).
	const float StartLineX = 300.0f;
	const float StraightY = -1950.0f;
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Lane = 0; Lane < 2; ++Lane)
		{
			const FVector Location(StartLineX - 450.0f - Row * 650.0f, StraightY + (Lane == 0 ? -170.0f : 170.0f), 0.0f);
			GridSlots.Add(FTransform(FRotator::ZeroRotator, Location));
		}
	}
}

void ARaceGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// No default pawn: cars stay parked (inactive) until their player presses a button.
}

void ARaceGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureCars();

	// One shared top-down view; the Igloo Manager handles the room projection.
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->SetForceDisableSplitscreen(true);
	}

	for (int32 ControllerId = 0; ControllerId < MaxPlayers; ++ControllerId)
	{
		if (!UGameplayStatics::GetPlayerControllerFromID(this, ControllerId))
		{
			UGameplayStatics::CreatePlayer(this, ControllerId, true);
		}
	}
	UE_LOG(LogRace, Log, TEXT("Race ready: %d car slots, press any button on a controller to join."), Cars.Num());
}

void ARaceGameMode::EnsureCars()
{
	if (Cars.Num() > 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Slot = 0; Slot < MaxPlayers; ++Slot)
	{
		const FTransform SlotTransform = GridSlots.IsValidIndex(Slot) ? GridSlots[Slot] : FTransform::Identity;
		ARaceCarPawn* Car = GetWorld()->SpawnActor<ARaceCarPawn>(ARaceCarPawn::StaticClass(), SlotTransform, Params);
		if (!Car)
		{
			continue;
		}
		if (CarMeshes.Num() > 0)
		{
			Car->SetCarMesh(CarMeshes[Slot % CarMeshes.Num()].LoadSynchronous());
		}
		Car->Deactivate();
		Cars.Add(Car);
	}
}

ARaceCarPawn* ARaceGameMode::GetCarForSlot(int32 SlotIndex)
{
	EnsureCars();
	return Cars.IsValidIndex(SlotIndex) ? Cars[SlotIndex].Get() : nullptr;
}

void ARaceGameMode::JoinRace(ARacePlayerController* Player)
{
	const int32 Slot = Player ? Player->GetSlotIndex() : INDEX_NONE;
	ARaceCarPawn* Car = GetCarForSlot(Slot);
	if (!Car || Car->IsActive())
	{
		return;
	}
	Car->Activate(GridSlots.IsValidIndex(Slot) ? GridSlots[Slot] : FTransform::Identity);
	UE_LOG(LogRace, Log, TEXT("Player %d joined the race."), Slot + 1);
}

AActor* ARaceGameMode::GetTrackCamera()
{
	if (TrackCamera)
	{
		return TrackCamera;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TrackCameraTag))
		{
			TrackCamera = *It;
			return TrackCamera;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
		FVector(0.0f, 0.0f, FallbackCameraHeight), FRotator(-90.0f, 0.0f, 0.0f), Params);
	if (Camera)
	{
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
		Camera->Tags.Add(TrackCameraTag);
	}
	TrackCamera = Camera;
	return TrackCamera;
}
