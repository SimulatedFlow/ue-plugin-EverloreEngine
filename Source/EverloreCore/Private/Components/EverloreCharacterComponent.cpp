// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Components/EverloreCharacterComponent.h"

void UEverloreCharacterComponent::RememberFact(FName Fact)
{
	if (!Fact.IsNone())
	{
		Character.KnownFacts.AddUnique(Fact);
	}
}

void UEverloreCharacterComponent::AppendMemory(const FString& Text)
{
	if (Text.IsEmpty())
	{
		return;
	}
	if (!Character.MemoryBlob.IsEmpty())
	{
		Character.MemoryBlob += TEXT("\n");
	}
	Character.MemoryBlob += Text;

	// Keep long-term memory bounded so it can't grow the per-turn chat system prompt without
	// limit (it is re-injected on every turn). Evict oldest content, snapping the cut to a line
	// boundary so a partial line is never kept.
	constexpr int32 MaxChars = 8000; // ~2k tokens of persistent memory
	if (Character.MemoryBlob.Len() > MaxChars)
	{
		Character.MemoryBlob.RightChopInline(Character.MemoryBlob.Len() - MaxChars);
		int32 NewlineIdx = INDEX_NONE;
		if (Character.MemoryBlob.FindChar(TEXT('\n'), NewlineIdx))
		{
			Character.MemoryBlob.RightChopInline(NewlineIdx + 1);
		}
	}
}
