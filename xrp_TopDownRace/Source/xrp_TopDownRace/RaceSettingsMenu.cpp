#include "RaceSettingsMenu.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RaceCarPawn.h"
#include "RaceInputSettings.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRaceMenu, Log, All);

namespace
{
	/**
	 * Project default of each setting, captured the first time a menu is built in this process. Class defaults
	 * live as long as the process (in the editor, across play sessions), so a later menu must not mistake values
	 * changed by an earlier one for the defaults.
	 */
	TMap<FString, double>& ProjectDefaults()
	{
		static TMap<FString, double> Defaults;
		return Defaults;
	}
}

void FRaceSettingsMenu::Initialise()
{
	if (Items.Num() > 0)
	{
		return;
	}
	UClass* Race = URaceInputSettings::StaticClass();
	UClass* Car = ARaceCarPawn::StaticClass();

	AddValue(TEXT("LAPS"), Race, TEXT("RaceLaps"), 1, 20, 1, EFormat::Integer);
	AddToggle(TEXT("NEW TRACK EACH RACE"), Race, TEXT("bNewTrackEachRace"));
	AddToggle(TEXT("NARROW SECTIONS"), Race, TEXT("bNarrowTrackSections"));
	AddValue(TEXT("TRACK GRID"), Race, TEXT("TrackGrid"), 0, 0, 1, EFormat::Integer); // range comes from the enum
	AddToggle(TEXT("SLIPSTREAM"), Race, TEXT("bDraftingEnabled"));

	// Computer drivers (pace is rolled per car at the start of each race).
	AddValue(TEXT("CPU PACE MIN"), Race, TEXT("CpuPaceMin"), 0.5, 1.0, 0.02, EFormat::Percent);
	AddValue(TEXT("CPU PACE MAX"), Race, TEXT("CpuPaceMax"), 0.5, 1.0, 0.02, EFormat::Percent);
	AddValue(TEXT("CPU MISTAKE EVERY (MIN)"), Race, TEXT("CpuMistakeGapMin"), 1.0, 20.0, 0.5, EFormat::Seconds);
	AddValue(TEXT("CPU MISTAKE EVERY (MAX)"), Race, TEXT("CpuMistakeGapMax"), 1.0, 30.0, 0.5, EFormat::Seconds);
	AddValue(TEXT("CPU SPIN CHANCE"), Race, TEXT("CpuSpinChance"), 0.0, 1.0, 0.05, EFormat::Percent);
	AddToggle(TEXT("CPU EASE OFF WHEN AHEAD"), Race, TEXT("bCpuEaseOffWhenAhead"));
	AddValue(TEXT("CPU EASE OFF PACE"), Race, TEXT("CpuEaseOffPace"), 0.5, 1.0, 0.05, EFormat::Percent);

	// Car handling (every car, straight away).
	AddValue(TEXT("TOP SPEED"), Car, TEXT("MaxSpeed"), 800, 3000, 100, EFormat::Integer);
	AddValue(TEXT("ACCELERATION"), Car, TEXT("Acceleration"), 500, 4000, 100, EFormat::Integer);
	AddValue(TEXT("STEERING"), Car, TEXT("TurnRate"), 120, 400, 10, EFormat::Integer);
	AddValue(TEXT("GRIP"), Car, TEXT("Grip"), 2.0, 20.0, 0.5, EFormat::Decimal);
	AddValue(TEXT("HANDBRAKE GRIP"), Car, TEXT("HandbrakeGrip"), 0.1, 3.0, 0.1, EFormat::Decimal);
	AddValue(TEXT("WALL BOUNCE"), Car, TEXT("WallBounce"), 0.0, 1.0, 0.05, EFormat::Percent);
	AddValue(TEXT("CAR BOUNCE"), Car, TEXT("CarBounce"), 0.0, 1.0, 0.05, EFormat::Percent);

	AddValue(TEXT("ENGINE VOLUME"), Race, TEXT("EngineVolume"), 0.0, 2.0, 0.1, EFormat::Percent);
	AddValue(TEXT("BEEP VOLUME"), Race, TEXT("SignalVolume"), 0.0, 2.0, 0.1, EFormat::Percent);
	AddValue(TEXT("IDLE PLAYER BACK TO CPU"), Race, TEXT("IdleReleaseSeconds"), 0.0, 300.0, 10.0, EFormat::Seconds);

	AddAction(TEXT("RESTART RACE"), ERaceMenuAction::RestartRace);
	AddAction(TEXT("RESET ALL TO DEFAULTS"), ERaceMenuAction::ResetDefaults);
	AddAction(TEXT("CLOSE"), ERaceMenuAction::Close);

	Load();
}

bool FRaceSettingsMenu::AddProperty(FItem& Item, const TCHAR* PropertyName)
{
	Item.Property = FindFProperty<FProperty>(Item.Class, PropertyName);
	if (!Item.Property || !(CastField<FNumericProperty>(Item.Property) || CastField<FBoolProperty>(Item.Property) || CastField<FEnumProperty>(Item.Property)))
	{
		UE_LOG(LogRaceMenu, Warning, TEXT("race.Menu: %s has no number, on/off or choice property '%s'; row '%s' left out."),
			*Item.Class->GetName(), PropertyName, *Item.Label);
		return false;
	}
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Item.Property))
	{
		// Step through the enum's entries (NumEnums includes the hidden _MAX entry).
		Item.Min = 0.0;
		Item.Max = double(FMath::Max(0, EnumProperty->GetEnum()->NumEnums() - 2));
		Item.Step = 1.0;
	}
	Item.DefaultValue = ProjectDefaults().FindOrAdd(SaveKey(Item), GetValue(Item));
	Items.Add(Item);
	return true;
}

void FRaceSettingsMenu::AddValue(const TCHAR* Label, UClass* Class, const TCHAR* PropertyName, double Min, double Max, double Step, EFormat Format)
{
	FItem Item;
	Item.Label = Label;
	Item.Class = Class;
	Item.Min = Min;   // (choice properties get their range from the enum in AddProperty)
	Item.Max = Max;
	Item.Step = Step;
	Item.Format = Format;
	AddProperty(Item, PropertyName);
}

void FRaceSettingsMenu::AddToggle(const TCHAR* Label, UClass* Class, const TCHAR* PropertyName)
{
	FItem Item;
	Item.Label = Label;
	Item.Class = Class;
	AddProperty(Item, PropertyName);
}

void FRaceSettingsMenu::AddAction(const TCHAR* Label, ERaceMenuAction Action)
{
	FItem Item;
	Item.Label = Label;
	Item.Action = Action;
	Items.Add(Item);
}

double FRaceSettingsMenu::GetValue(const FItem& Item)
{
	UObject* Defaults = Item.Class ? Item.Class->GetDefaultObject() : nullptr;
	if (!Defaults)
	{
		return 0.0;
	}
	if (const FBoolProperty* Bool = CastField<FBoolProperty>(Item.Property))
	{
		return Bool->GetPropertyValue_InContainer(Defaults) ? 1.0 : 0.0;
	}
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Item.Property))
	{
		return double(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Defaults)));
	}
	if (const FNumericProperty* Number = CastField<FNumericProperty>(Item.Property))
	{
		const void* Value = Number->ContainerPtrToValuePtr<void>(Defaults);
		return Number->IsFloatingPoint() ? Number->GetFloatingPointPropertyValue(Value) : double(Number->GetSignedIntPropertyValue(Value));
	}
	return 0.0;
}

void FRaceSettingsMenu::SetValue(const FItem& Item, double Value)
{
	UObject* Defaults = Item.Class ? Item.Class->GetDefaultObject() : nullptr;
	if (!Defaults)
	{
		return;
	}
	if (const FBoolProperty* Bool = CastField<FBoolProperty>(Item.Property))
	{
		Bool->SetPropertyValue_InContainer(Defaults, Value > 0.5);
	}
	else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Item.Property))
	{
		EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(EnumProperty->ContainerPtrToValuePtr<void>(Defaults), int64(FMath::RoundToDouble(Value)));
	}
	else if (const FNumericProperty* Number = CastField<FNumericProperty>(Item.Property))
	{
		void* Target = Number->ContainerPtrToValuePtr<void>(Defaults);
		if (Number->IsFloatingPoint())
		{
			Number->SetFloatingPointPropertyValue(Target, Value);
		}
		else
		{
			Number->SetIntPropertyValue(Target, int64(FMath::RoundToDouble(Value)));
		}
	}
}

FString FRaceSettingsMenu::FormatValue(const FItem& Item)
{
	if (Item.Action != ERaceMenuAction::None)
	{
		return FString();
	}
	const double Value = GetValue(Item);
	if (CastField<FBoolProperty>(Item.Property))
	{
		return Value > 0.5 ? TEXT("ON") : TEXT("OFF");
	}
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Item.Property))
	{
		return EnumProperty->GetEnum()->GetDisplayNameTextByValue(int64(FMath::RoundToDouble(Value))).ToString().ToUpper();
	}
	switch (Item.Format)
	{
	case EFormat::Integer: return FString::Printf(TEXT("%d"), FMath::RoundToInt(Value));
	case EFormat::Percent: return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0));
	case EFormat::Seconds: return Value <= 0.0 ? FString(TEXT("NEVER")) : FString::Printf(TEXT("%s S"), *FString::SanitizeFloat(FMath::RoundToDouble(Value * 10.0) / 10.0, 0));
	default: return FString::Printf(TEXT("%.1f"), Value);
	}
}

FString FRaceSettingsMenu::SaveKey(const FItem& Item)
{
	return FString::Printf(TEXT("%s.%s"), *Item.Class->GetName(), *Item.Property->GetName());
}

FString FRaceSettingsMenu::SavePath()
{
	return FPaths::ProjectSavedDir() / TEXT("RaceSettings.ini");
}

void FRaceSettingsMenu::Open(int32 SlotIndex)
{
	OwnerSlot = SlotIndex;
	Selected = 0;
}

void FRaceSettingsMenu::Close()
{
	if (IsOpen())
	{
		OwnerSlot = INDEX_NONE;
		Save();
	}
}

FRaceSettingsMenu::FNavigateResult FRaceSettingsMenu::Navigate(int32 Rows, int32 Steps, bool bConfirm)
{
	FNavigateResult Result;
	if (!IsOpen() || Items.Num() == 0)
	{
		return Result;
	}
	if (Rows != 0)
	{
		Selected = ((Selected + Rows) % Items.Num() + Items.Num()) % Items.Num();
	}

	const FItem& Item = Items[Selected];
	if (Item.Action != ERaceMenuAction::None)
	{
		if (bConfirm)
		{
			Result.Action = Item.Action;
			if (Item.Action == ERaceMenuAction::ResetDefaults)
			{
				for (const FItem& Each : Items)
				{
					if (Each.Property)
					{
						SetValue(Each, Each.DefaultValue);
					}
				}
				Result.bValuesChanged = true;
				UE_LOG(LogRaceMenu, Log, TEXT("race.Menu everything reset to the project defaults"));
			}
		}
		return Result;
	}

	const double Old = GetValue(Item);
	double New = Old;
	if (CastField<FBoolProperty>(Item.Property))
	{
		if (Steps != 0 || bConfirm)
		{
			New = Old > 0.5 ? 0.0 : 1.0;
		}
	}
	else if (Steps != 0)
	{
		// Step, then snap onto the Min + n * Step grid so repeated presses land on round numbers.
		New = FMath::Clamp(Old + Steps * Item.Step, Item.Min, Item.Max);
		New = FMath::Clamp(Item.Min + FMath::RoundToDouble((New - Item.Min) / Item.Step) * Item.Step, Item.Min, Item.Max);
	}
	if (!FMath::IsNearlyEqual(New, Old, 1.0e-6))
	{
		SetValue(Item, New);
		Result.bValuesChanged = true;
		UE_LOG(LogRaceMenu, Log, TEXT("race.Menu %s = %s"), *Item.Label, *FormatValue(Item));
	}
	return Result;
}

void FRaceSettingsMenu::ApplyCarValues(UObject* Car) const
{
	for (const FItem& Item : Items)
	{
		if (Item.Property && Car && Car->IsA(Item.Class) && Item.Class->IsChildOf(AActor::StaticClass()))
		{
			Item.Property->CopyCompleteValue_InContainer(Car, Item.Class->GetDefaultObject());
		}
	}
}

void FRaceSettingsMenu::GetText(FString& OutTitle, TArray<FString>& OutRows, int32& OutSelectedRow, FString& OutHint) const
{
	OutRows.Reset();
	OutSelectedRow = INDEX_NONE;
	if (Items.Num() == 0)
	{
		return;
	}
	OutTitle = FString::Printf(TEXT("SETTINGS   %d / %d"), Selected + 1, Items.Num());

	// A window of rows that keeps the selection in the middle where it can.
	const int32 First = FMath::Clamp(Selected - VisibleRows / 2, 0, FMath::Max(0, Items.Num() - VisibleRows));
	for (int32 Index = First; Index < FMath::Min(First + VisibleRows, Items.Num()); ++Index)
	{
		const FItem& Item = Items[Index];
		const bool bAction = Item.Action != ERaceMenuAction::None;
		if (Index == Selected)
		{
			OutRows.Add(bAction ? FString::Printf(TEXT(">  %s  <"), *Item.Label) : FString::Printf(TEXT("%s:   <  %s  >"), *Item.Label, *FormatValue(Item)));
			OutSelectedRow = OutRows.Num() - 1;
		}
		else
		{
			OutRows.Add(bAction ? Item.Label : FString::Printf(TEXT("%s:   %s"), *Item.Label, *FormatValue(Item)));
		}
	}
	OutHint = Items[Selected].Action != ERaceMenuAction::None
		? TEXT("UP/DOWN: SELECT     A / ENTER: OK     B / TAB: CLOSE")
		: TEXT("UP/DOWN: SELECT     LEFT/RIGHT: CHANGE     B / TAB: CLOSE");
}

void FRaceSettingsMenu::Save() const
{
	TArray<FString> Lines;
	Lines.Add(TEXT("; Settings changed in the in-game menu on this machine (only the ones that differ from Config/DefaultGame.ini)."));
	Lines.Add(TEXT("; Delete this file, or use RESET ALL TO DEFAULTS in the menu, to go back to the project defaults."));
	for (const FItem& Item : Items)
	{
		if (Item.Property && !FMath::IsNearlyEqual(GetValue(Item), Item.DefaultValue, 1.0e-4))
		{
			Lines.Add(FString::Printf(TEXT("%s=%s"), *SaveKey(Item), *FString::SanitizeFloat(GetValue(Item))));
		}
	}
	const bool bSaved = FFileHelper::SaveStringArrayToFile(Lines, *SavePath());
	UE_LOG(LogRaceMenu, Log, TEXT("race.Menu closed; %s %d changed setting(s) to %s"), bSaved ? TEXT("saved") : TEXT("FAILED to save"), Lines.Num() - 2, *SavePath());
}

void FRaceSettingsMenu::Load()
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *SavePath()))
	{
		return;
	}
	for (const FString& Line : Lines)
	{
		FString Key;
		FString Value;
		if (Line.StartsWith(TEXT(";")) || !Line.Split(TEXT("="), &Key, &Value))
		{
			continue;
		}
		Key.TrimStartAndEndInline();
		for (const FItem& Item : Items)
		{
			if (Item.Property && SaveKey(Item) == Key)
			{
				SetValue(Item, FMath::Clamp(FCString::Atod(*Value), Item.Min, Item.Max));
				UE_LOG(LogRaceMenu, Log, TEXT("race.Menu loaded %s = %s (from %s)"), *Item.Label, *FormatValue(Item), *SavePath());
			}
		}
	}
}
