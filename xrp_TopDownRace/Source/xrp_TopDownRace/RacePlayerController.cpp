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
		TEXT("Test aid (no controller needed): players take their cars and drive by themselves, logging position, distance moved, FPS, slipstream and spin each second. ")
		TEXT("1 = full throttle straight, 2 = steering 60% and steering the other way when stuck against a wall, ")
		TEXT("3 = driven by the computer driver (checks taking over a car), 4 = doughnut: throttle + handbrake + full lock."));

	TAutoConsoleVariable<int32> CVarRaceAutoDriveCars(
		TEXT("race.AutoDriveCars"), 4,
		TEXT("How many players race.AutoDrive brings in (1-4)."));

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

void ARacePlayerController::ApplyRoomAudioListener()
{
	// The view target is a camera far above the track looking straight down, which would make 3D panning
	// meaningless. The room's speakers surround the floor, so listen from the floor centre, level, facing the
	// configured front wall. Cars are then panned to the speakers on the side of the room they are on.
	const FVector RoomCentre(0.0f, 0.0f, 50.0f);
	SetAudioListenerOverride(nullptr, RoomCentre, FRotator(0.0f, GetDefault<URaceInputSettings>()->AudioFrontYaw, 0.0f));
	bRoomListenerApplied = true;
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
	if (!bRoomListenerApplied)
	{
		ApplyRoomAudioListener();
	}

	const int32 Slot = GetSlotIndex();
	ARaceCarPawn* Car = GameMode->GetCarForSlot(Slot);
	if (!Car)
	{
		return;
	}

	const URaceInputSettings* Settings = GetDefault<URaceInputSettings>();
	float Throttle = 0.0f;
	float Brake = 0.0f;
	float Steer = 0.0f;
	bool bHandbrake = false;
	bool bJoinPressed = false;

	if (Settings->bGamepadInputEnabled)
	{
		Throttle = FMath::Max(GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis),
			IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom) ? 1.0f : 0.0f);
		Brake = FMath::Max(GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis),
			IsInputKeyDown(EKeys::Gamepad_FaceButton_Left) ? 1.0f : 0.0f);
		bHandbrake = IsInputKeyDown(EKeys::Gamepad_FaceButton_Right) || IsInputKeyDown(EKeys::Gamepad_RightShoulder);

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
	if (Settings->bKeyboardInputEnabled && Slot == 0)
	{
		if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) { Throttle = 1.0f; }
		if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) { Brake = 1.0f; }
		if (IsInputKeyDown(EKeys::SpaceBar)) { bHandbrake = true; }
		Steer += (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) ? 1.0f : 0.0f;
		Steer -= (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) ? 1.0f : 0.0f;

		for (const FKey& Key : KeyboardJoinKeys())
		{
			bJoinPressed |= IsInputKeyDown(Key);
		}
	}
	Steer = FMath::Clamp(Steer, -1.0f, 1.0f);

	const int32 AutoDriveMode = CVarRaceAutoDrive.GetValueOnGameThread();
	if (AutoDriveMode > 0)
	{
		bJoinPressed = !Car->IsPlayerControlled() && Slot < CVarRaceAutoDriveCars.GetValueOnGameThread();
		Throttle = 1.0f;
		Brake = 0.0f;
		Steer = 0.0f;
		bHandbrake = false;
		if (AutoDriveMode == 2)
		{
			// Behave like a player: if the car has barely moved for half a second, steer the other way and keep
			// turning that way long enough to come round (about 190 degrees at standstill steering).
			AutoDriveProbeTimer += DeltaTime;
			AutoDriveFlipHold = FMath::Max(0.0f, AutoDriveFlipHold - DeltaTime);
			if (AutoDriveProbeTimer >= 0.5f)
			{
				const FVector ProbeLocation = Car->GetActorLocation();
				if (AutoDriveFlipHold <= 0.0f && FVector::Dist2D(ProbeLocation, AutoDriveProbeLocation) < 20.0f)
				{
					AutoDriveSteerSign = -AutoDriveSteerSign;
					AutoDriveFlipHold = 3.0f;
					++AutoDriveEscapes;
				}
				AutoDriveProbeLocation = ProbeLocation;
				AutoDriveProbeTimer = 0.0f;
			}
			Steer = 0.6f * AutoDriveSteerSign;
		}
		else if (AutoDriveMode == 3)
		{
			GameMode->ComputeComputerDriverInput(Slot, DeltaTime, Throttle, Brake, Steer, bHandbrake);
		}
		else if (AutoDriveMode >= 4)
		{
			Throttle = 0.7f;
			Steer = 1.0f;
			bHandbrake = true;
		}

		AutoDriveLogTimer += DeltaTime;
		++AutoDriveFrames;
		if (Car->IsPlayerControlled() && AutoDriveLogTimer >= 1.0f)
		{
			const FVector Location = Car->GetActorLocation();
			const float Moved = bAutoDriveHasLastLocation ? FVector::Dist2D(Location, AutoDriveLastLocation) : 0.0f;
			UE_LOG(LogTemp, Log, TEXT("race.AutoDrive slot %d at (%.0f, %.0f) yaw %.0f speed %.0f moved %.0f fps %.1f escapes %d draft %.2f spin %.0f"),
				Slot, Location.X, Location.Y, Car->GetActorRotation().Yaw, Car->GetSpeed(), Moved,
				AutoDriveFrames / AutoDriveLogTimer, AutoDriveEscapes, Car->GetDraftFactor(), Car->GetSpinRate());
			AutoDriveLastLocation = Location;
			bAutoDriveHasLastLocation = true;
			AutoDriveLogTimer = 0.0f;
			AutoDriveFrames = 0;
		}
	}

	if (!Car->IsPlayerControlled())
	{
		// Arcade join: a fresh press takes this slot's car over from the computer, whatever the race is doing.
		if (bJoinPressed && !bJoinButtonHeld)
		{
			GameMode->JoinRace(this);
			IdleTime = 0.0f;
		}
		bJoinButtonHeld = bJoinPressed;
		return;
	}

	// Hand the car back to the computer if its player walks away.
	const bool bAnyInput = Throttle > 0.05f || Brake > 0.05f || FMath::Abs(Steer) > 0.05f || bHandbrake || bJoinPressed;
	IdleTime = bAnyInput ? 0.0f : IdleTime + DeltaTime;
	if (Settings->IdleReleaseSeconds > 0.0f && IdleTime >= Settings->IdleReleaseSeconds)
	{
		IdleTime = 0.0f;
		bJoinButtonHeld = true;
		GameMode->ReleaseCar(Slot);
		return;
	}

	Car->SetDriveInput(Throttle, Brake, Steer, bHandbrake);
}
