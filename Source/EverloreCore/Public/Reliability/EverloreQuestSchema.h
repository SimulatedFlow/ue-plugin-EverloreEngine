// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FEverloreGuardrailSnapshot;

/**
 * Single source of truth for the quest generation contract: builds the Gemini/JSON
 * responseSchema and composes the prompt (task + allowed vocabulary) from ONE place,
 * so the schema, the prompt and the validator can't drift apart.
 *
 * The quest giver is NOT part of the schema — the engine injects it, removing a whole
 * class of invalid-giver output.
 */
class EVERLORECORE_API FEverloreQuestSchema
{
public:
	/** JSON-schema artifact (portable subset) fed to a structured backend. */
	static FString BuildResponseSchema();

	/** System instruction that pins the model to schema-only JSON output. */
	static FString SystemPrompt();

	/** User prompt: task + the allowed id vocabulary from the guardrail snapshot. */
	static FString BuildPrompt(const FEverloreGuardrailSnapshot& Guardrails, FName GiverNpcId, const FString& Theme);
};
