#pragma once

#include "CoreMinimal.h"

/**
 * A 5 x 7 dot-matrix font, as on LED timing boards: used by the position boards and for the winner's block letters.
 * Covers A-Z, 0-9, space and . : / + - ! ( ) (lower case draws as upper case; anything else is blank).
 */
namespace RaceDotFont
{
	constexpr int32 GlyphWidth = 5;
	constexpr int32 GlyphHeight = 7;
	constexpr int32 Advance = 6;   // glyph plus one blank column

	/** True if dot (X, Y) of the character is lit; Y = 0 is the top row. */
	XRP_TOPDOWNRACE_API bool IsLit(TCHAR Character, int32 X, int32 Y);

	/** Width in dots of a line of text (no gap after the last character). */
	XRP_TOPDOWNRACE_API int32 TextWidth(const FString& Text, int32 Scale = 1);
}
