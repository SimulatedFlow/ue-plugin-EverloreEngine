// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FEverloreQuestPayload;
struct FEverloreGuardrailSnapshot;

/**
 * Deterministic, LLM-free repair. Enforces every auto-fixable rule in place (clamp / drop
 * invalid optional elements / dedupe ids / truncate / scale to budget). It can lower
 * quality but NEVER invalidates and never invents required content — if it can't make the
 * quest valid, validation still fails and the pipeline falls back to a template.
 */
class EVERLORECORE_API FEverloreQuestAutoFix
{
public:
	static void Apply(FEverloreQuestPayload& InOut, const FEverloreGuardrailSnapshot& Guardrails);
};
