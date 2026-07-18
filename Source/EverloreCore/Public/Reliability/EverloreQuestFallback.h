// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FEverloreQuestPayload;
struct FEverloreGuardrailSnapshot;

/**
 * Deterministic, guaranteed-valid fallback. This is why "guaranteed valid & game-safe"
 * is literally true: it is a total function whose output is valid by construction — a
 * single "talk to the quest giver" objective, filled only from the live registry.
 * (Phase e replaces/augments this with a hand-authored template pool.)
 */
class EVERLORECORE_API FEverloreQuestFallback
{
public:
	static FEverloreQuestPayload BuildFallback(const FEverloreGuardrailSnapshot& Guardrails, FName GiverNpcId, int32 Seed);
};
