#include "RacePlayerController.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "RaceCarPawn.h"
#include "RaceGameMode.h"
#include "RaceInputSettings.h"

namespace
{
	TAutoConsoleVariable<int32> CVarRaceAutoDrive(
		TEXT("race.AutoDrive"), 0,
		TEXT("Test aid (no controller needed): every player joins and drives full throttle, logging car position each second."));

	const TArray<FKey>& GamepadJoinKeys()
	{
		static const TArray<FKey> Keys = {
			EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right,
			EKeys::Gamepad_FaceButton_Left, EKeys::Gamepad_FaceButton_Top,
			EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_RightShoulder,
			EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_RightTrigger,
			EKeys::Gamepad_Special_Left, EKeys::Gamepad_Special_Right,
			EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down,
			EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right,
			EKeys::Gamepad_LeftThumbstick, EKeys::Gamepad_RightThumbstick,
		};
		return Keys;
	}

	const TArray<FKey>& KeyboardJoinKeys()
	{
		static const TArray<FKey> Keys = { EKeys::Enter, EKeys::SpaceBar, EKeys::W, EKeys::Up };
		return Keys;
	}
}

ARacePlayerController::ARacePlayerController()
{
	// The view stays on the shared top-down track camera instead of following a pawn.
	bAutoManageActiveCameraTarget = false;
}

int32 ARacePlayerController::GetSlotIndex() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetControllerId() : INDEX_NONE;
}

void ARacePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	ARaceGameMode* GameMode = GetWorld()->GetAuthGameMode<ARaceGameMode>();
	if (!GameMode)
	{
		return;
	}

	if (AActor* TrackCamera = GameMode->GetTrackCamera(); TrackCamera && GetViewTarget() != TrackCamera)
	{
		SetViewTarget(TrackCamera);
	}

	ARaceCarPawn* Car = GameMode->GetCarForSlot(GetSlotIndex());
	if (!Car)
	{
		return;
	}

	const URaceInputSettings* Settings = GetDefault<URaceInputSettings>();
	float Throttle = 0.0f;
	float Brake = 0.0f;
	float Steer = 0.0f;
	bool bJoinPressed = false;

	if (Settings->bGamepadInputEnabled)
	{
		Throttle = FMath::Max(GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis),
			IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom) ? 1.0f : 0.0f);
		Brake = FMath::Max(GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis),
			IsInputKeyDown(EKeys::Gamepad_FaceButton_Left) ? 1.0f : 0.0f);

		const float StickX = GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		if (FMath::Abs(StickX) > Settings->StickDeadZone)
		{
			Steer += FMath::Sign(StickX) * (FMath::Abs(StickX) - Settings->StickDeadZone) / (1.0f - Settings->StickDeadZone);
		}
		Steer += (IsInputKeyDown(EKeys::Gamepad_DPad_Right) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::Gamepad_DPad_Left) ? 1.0f : 0.0f);

		for (const FKey& Key : GamepadJoinKeys())
		{
			bJoinPressed |= IsInputKeyDown(Key);
		}
	}

	// Keyboard input only ever reaches the primary local player.
	if (Settings->bKeyboardInputEnabled && GetSlotIndex() == 0)
	{
		if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) { Throttle = 1.0f; }
		if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) { Brake = 1.0f; }
		Steer += (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) ? 1.0f : 0.0f;
		Steer -= (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) ? 1.0f : 0.0f;

		for (const FKey& Key : KeyboardJoinKeys())
		{
			bJoinPressed |= IsInputKeyDown(Key);
		}
	}

	const bool bAutoDrive = CVarRaceAutoDrive.GetValueOnGameThread() > 0;
	if (bAutoDrive)
	{
		bJoinPressed = !Car->IsActive();
		Throttle = 1.0f;
		AutoDriveLogTimer += DeltaTime;
		if (Car->IsActive() && AutoDriveLogTimer >= 1.0f)
		{
			AutoDriveLogTimer = 0.0f;
			const FVector Location = Car->GetActorLocation();
			UE_LOG(LogTemp, Log, TEXT("race.AutoDrive slot %d at (%.0f, %.0f) yaw %.0f speed %.0f"),
				GetSlotIndex(), Location.X, Location.Y, Car->GetActorRotation().Yaw, Car->GetSpeed());
		}
	}

	if (!Car->IsActive())
	{
		// Arcade join: a fresh press on an idle controller brings its car onto the grid.
		if (bJoinPressed && !bJoinButtonHeld)
		{
			GameMode->JoinRace(this);
		}
		bJoinButtonHeld = bJoinPressed;
		return;
	}

	Car->SetDriveInput(Throttle, Brake, Steer);
}
