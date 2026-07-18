// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FEverloreCharacterContext;
struct FEverloreGenerationResponse;
struct FEverloreChatIntent;

/**
 * Single source of truth for the roleplay-chat contract. Composes the in-character system
 * prompt from a character's persona + memory, builds the constrained response schema that
 * co-emits { reply, intent } (so the model can only pick a whitelisted effect), and parses
 * a backend response back into a spoken reply + a validated intent.
 *
 * ParseReply is intentionally total: it ALWAYS yields a usable reply string (falling back
 * to raw text if the JSON is malformed) and never surfaces an off-whitelist intent, so a
 * chat turn can never leave the game in an ambiguous state.
 */
class EVERLORECORE_API FEverloreChatSchema
{
public:
	/** In-character system instruction built from persona + disposition + facts + memory. */
	static FString SystemPrompt(const FEverloreCharacterContext& Character, bool bIntentsEnabled);

	/** Gemini/JSON responseSchema for { reply, intent, intent_argument }. Empty when intents are off. */
	static FString BuildResponseSchema();

	/**
	 * Turn a backend response into a spoken reply + a whitelisted intent.
	 * bIntentsEnabled selects the structured (JSON) vs free-text path. Always sets OutReply.
	 */
	static void ParseReply(const FEverloreGenerationResponse& Response, bool bIntentsEnabled,
		FString& OutReply, FEverloreChatIntent& OutIntent);
};
