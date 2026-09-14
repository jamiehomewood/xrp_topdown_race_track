#include "RaceHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RaceGameMode.h"

void ARaceHUD::DrawHUD()
{
	Super::DrawHUD();

	const ARaceGameMode* GameMode = GetWorld()->GetAuthGameMode<ARaceGameMode>();
	const ULocalPlayer* LocalPlayer = PlayerOwner ? PlayerOwner->GetLocalPlayer() : nullptr;
	// All players share one view, so only the first player's HUD draws.
	if (!Canvas || !GameMode || !GameMode->IsSettingsMenuOpen() || !LocalPlayer || LocalPlayer->GetControllerId() != 0)
	{
		return;
	}

	FString Title;
	FString Hint;
	TArray<FString> Rows;
	int32 SelectedRow = INDEX_NONE;
	GameMode->GetSettingsMenuText(Title, Rows, SelectedRow, Hint);

	UFont* Font = GEngine->GetLargeFont();
	const float Scale = FMath::Max(Canvas->ClipY / 900.0f, 0.5f);
	const float RowHeight = 34.0f * Scale;
	const float Width = 820.0f * Scale;
	const float Height = RowHeight * (Rows.Num() + 3);
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float TextLeft = Left + 28.0f * Scale;

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f), Left, Top, Width, Height);
	DrawText(FString::Printf(TEXT("PLAYER %d:  %s"), GameMode->GetSettingsMenuOwner() + 1, *Title), FLinearColor::White, TextLeft, Top + 8.0f * Scale, Font, Scale * 1.1f);
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		const FLinearColor Color = Row == SelectedRow ? FLinearColor(1.0f, 0.82f, 0.0f) : FLinearColor(0.85f, 0.85f, 0.85f);
		DrawText(Rows[Row], Color, TextLeft, Top + RowHeight * (Row + 1.3f), Font, Scale);
	}
	DrawText(Hint, FLinearColor(0.6f, 0.6f, 0.6f), TextLeft, Top + RowHeight * (Rows.Num() + 1.7f), Font, Scale * 0.8f);
}
