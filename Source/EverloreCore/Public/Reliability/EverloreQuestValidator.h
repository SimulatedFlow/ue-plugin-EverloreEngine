// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FEverloreQuestPayload;
struct FEverloreGuardrailSnapshot;
struct FEverloreValidationReport;

/**
 * Validates a parsed quest payload against a guardrail snapshot. Pure, non-mutating,
 * thread-safe (reads only the payload + POD snapshot). Layers:
 *   L1 structural  — required fields, counts, uniqueness
 *   L2 referential — every id exists in the right registry; target type matches objective
 *   L3 bounds      — reward clamps, total reward budget, objective counts
 *   L4 completable — at least one non-optional, referentially-valid objective
 *
 * Never calls check()/verify() on the (untrusted) payload — the no-crash guarantee rests
 * on defensive checks, not on exceptions (which UE compiles out).
 */
class EVERLORECORE_API FEverloreQuestValidator
{
public:
	static void Validate(const FEverloreQuestPayload& Payload, const FEverloreGuardrailSnapshot& Guardrails, FEverloreValidationReport& OutReport);
};
