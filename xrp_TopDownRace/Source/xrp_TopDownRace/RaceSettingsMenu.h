#pragma once

#include "CoreMinimal.h"

class FProperty;

/** What a menu row does when chosen. */
enum class ERaceMenuAction : uint8
{
	None,
	RestartRace,
	ResetDefaults,
	Close,
};

/**
 * The in-game settings menu's model: Race Settings and car handling values edited in place on their class
 * defaults (so the game picks them up straight away), plus a few actions. Values that differ from the project
 * defaults (Config/DefaultGame.ini) are saved to Saved/RaceSettings.ini on this machine and loaded at start, so the
 * menu never changes the files in the repository.
 */
class FRaceSettingsMenu
{
public:
	struct FNavigateResult
	{
		bool bValuesChanged = false;
		ERaceMenuAction Action = ERaceMenuAction::None;
	};

	static constexpr int32 VisibleRows = 11;

	/** Builds the rows, remembers the project defaults, then applies this machine's saved values. */
	void Initialise();

	bool IsOpen() const { return OwnerSlot != INDEX_NONE; }
	int32 GetOwnerSlot() const { return OwnerSlot; }
	void Open(int32 SlotIndex);

	/** Closes and saves. */
	void Close();

	/** Rows moves the selection (wrapping), Steps changes the selected value, bConfirm toggles or runs an action. */
	FNavigateResult Navigate(int32 Rows, int32 Steps, bool bConfirm);

	/** Copies the car handling values from the car class defaults onto a car already in the world. */
	void ApplyCarValues(UObject* Car) const;

	/** Menu text: title, the visible window of rows, which of those is selected, and a key hint. */
	void GetText(FString& OutTitle, TArray<FString>& OutRows, int32& OutSelectedRow, FString& OutHint) const;

private:
	enum class EFormat : uint8 { Integer, Decimal, Percent, Seconds };

	struct FItem
	{
		FString Label;
		UClass* Class = nullptr;
		FProperty* Property = nullptr;
		double Min = 0.0;
		double Max = 1.0;
		double Step = 1.0;
		EFormat Format = EFormat::Decimal;
		ERaceMenuAction Action = ERaceMenuAction::None;
		double DefaultValue = 0.0;
	};

	void AddValue(const TCHAR* Label, UClass* Class, const TCHAR* PropertyName, double Min, double Max, double Step, EFormat Format);
	void AddToggle(const TCHAR* Label, UClass* Class, const TCHAR* PropertyName);
	void AddAction(const TCHAR* Label, ERaceMenuAction Action);
	bool AddProperty(FItem& Item, const TCHAR* PropertyName);

	static double GetValue(const FItem& Item);
	static void SetValue(const FItem& Item, double Value);
	static FString FormatValue(const FItem& Item);
	static FString SaveKey(const FItem& Item);
	static FString SavePath();
	void Save() const;
	void Load();

	TArray<FItem> Items;
	int32 Selected = 0;
	int32 OwnerSlot = INDEX_NONE;
};
