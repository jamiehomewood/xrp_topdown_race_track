#include "RaceGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "RaceCarPawn.h"
#include "RaceInputSettings.h"
#include "RacePlayerController.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRace, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarRaceIglooCameraReport(
		TEXT("race.IglooCameraReport"), 0,
		TEXT("Log each Igloo capture camera's direction, capture mode and sampled image brightness 8 seconds into play."));

	/** Capture cameras whose view points more upward than this (forward Z) count as the ceiling camera. */
	constexpr float CeilingForwardZ = 0.7f;
}

ARaceGameMode::ARaceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

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

	// The desktop window is only a monitor; spend the GPU on the Igloo capture cameras instead.
	if (IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		ScreenPercentage->Set(GetDefault<URaceInputSettings>()->DesktopViewScreenPercentage, ECVF_SetByGameSetting);
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

void ARaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Igloo spawns its cameras during its own BeginPlay and may recreate them (e.g. on a resolution change).
	IglooScanTimer -= DeltaSeconds;
	if (IglooScanTimer <= 0.0f)
	{
		IglooScanTimer = 2.0f;
		RefreshIglooCameras();
	}

	// Re-assert every frame in case Igloo's blueprint turns per-frame capture back on.
	for (const TWeakObjectPtr<USceneCaptureComponent2D>& Capture : DisabledCeilingCaptures)
	{
		if (Capture.IsValid())
		{
			Capture->bCaptureEveryFrame = false;
			Capture->bCaptureOnMovement = false;
		}
	}

	if (!bIglooReported && CVarRaceIglooCameraReport.GetValueOnGameThread() > 0)
	{
		IglooReportTimer += DeltaSeconds;
		if (IglooReportTimer >= 8.0f)
		{
			bIglooReported = true;
			ReportIglooCameras();
		}
	}
}

void ARaceGameMode::RefreshIglooCameras()
{
	const bool bRenderCeiling = GetDefault<URaceInputSettings>()->bRenderIglooCeilingCamera;
	bool bFoundNew = false;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->GetClass()->GetName().StartsWith(TEXT("IglooPanoCamera")))
		{
			continue;
		}
		USceneCaptureComponent2D* Capture = It->FindComponentByClass<USceneCaptureComponent2D>();
		if (!Capture || KnownIglooCaptures.Contains(Capture))
		{
			continue;
		}
		KnownIglooCaptures.Add(Capture);
		bFoundNew = true;

		const FVector Forward = Capture->GetForwardVector();
		const bool bCeiling = Forward.Z > CeilingForwardZ;
		UE_LOG(LogRace, Log, TEXT("Igloo camera %s looks (%.2f, %.2f, %.2f), capture every frame %d%s"),
			*It->GetName(), Forward.X, Forward.Y, Forward.Z, Capture->bCaptureEveryFrame ? 1 : 0,
			bCeiling ? (bRenderCeiling ? TEXT(" [ceiling, rendering]") : TEXT(" [ceiling, disabled]")) : TEXT(""));

		if (bCeiling && !bRenderCeiling)
		{
			DisableCapture(Capture);
			DisabledCeilingCaptures.Add(Capture);
		}
	}

	if (bFoundNew && DisabledCeilingCaptures.Num() == 0 && !bRenderCeiling && !bWarnedNoCeilingCamera)
	{
		bWarnedNoCeilingCamera = true;
		UE_LOG(LogRace, Warning, TEXT("No upward-facing Igloo camera found among %d; nothing to disable."), KnownIglooCaptures.Num());
	}
}

void ARaceGameMode::DisableCapture(USceneCaptureComponent2D* Capture)
{
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;

	// If Igloo still triggers captures manually, render nothing: no primitives, no sky or fog.
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->ShowOnlyActors.Reset();
	Capture->ShowOnlyComponents.Reset();
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetCloud(false);

	if (Capture->TextureTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, Capture->TextureTarget, FLinearColor::Black);
	}
}

void ARaceGameMode::ReportIglooCameras()
{
	for (const TWeakObjectPtr<USceneCaptureComponent2D>& Weak : KnownIglooCaptures)
	{
		USceneCaptureComponent2D* Capture = Weak.Get();
		if (!Capture)
		{
			continue;
		}
		const FVector Forward = Capture->GetForwardVector();
		UTextureRenderTarget2D* Target = Capture->TextureTarget;
		if (!Target)
		{
			UE_LOG(LogRace, Log, TEXT("Igloo camera report: looks (%.2f, %.2f, %.2f), no render target"), Forward.X, Forward.Y, Forward.Z);
			continue;
		}

		// Average a 5 x 5 grid of pixels (enough to tell a black tile from a rendered one).
		FLinearColor Sum = FLinearColor::Black;
		const int32 Grid = 5;
		for (int32 GX = 0; GX < Grid; ++GX)
		{
			for (int32 GY = 0; GY < Grid; ++GY)
			{
				const int32 X = (Target->SizeX - 1) * (GX + 0.5f) / Grid;
				const int32 Y = (Target->SizeY - 1) * (GY + 0.5f) / Grid;
				Sum += FLinearColor(UKismetRenderingLibrary::ReadRenderTargetPixel(this, Target, X, Y));
			}
		}
		Sum /= float(Grid * Grid);
		UE_LOG(LogRace, Log, TEXT("Igloo camera report: looks (%.2f, %.2f, %.2f), target %dx%d, capture every frame %d, mean colour (%.2f, %.2f, %.2f)"),
			Forward.X, Forward.Y, Forward.Z, Target->SizeX, Target->SizeY, Capture->bCaptureEveryFrame ? 1 : 0, Sum.R, Sum.G, Sum.B);
	}
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
