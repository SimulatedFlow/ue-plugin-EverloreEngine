// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Reliability/EverlorePipelineTypes.h"

struct FEverloreGenerationResponse;
struct FEverloreGuardrailSnapshot;

/**
 * The S2–S8 core: turns a raw backend response into a guaranteed-valid committed record.
 *   parse -> validate -> auto-fix -> re-validate -> (fallback) -> commit
 * Exactly three outcomes, none of which is "committed-invalid":
 *   LlmValidated | LlmRepaired | TemplateFallback.
 * (Async LLM re-prompt repair layers on top of this in the subsystem.)
 */
class EVERLORECORE_API FEverloreQuestPipeline
{
public:
	static FEverloreQuestResult BuildResult(const FEverloreGenerationResponse& Response,
		const FEverloreQuestGenContext& Ctx, const FEverloreGuardrailSnapshot& Snapshot,
		const FString& BackendId, const FString& Model);

private:
	static FEverloreQuestResult Commit(const FEverloreQuestPayload& Payload, EEverloreProvenance Source,
		const FEverloreQuestGenContext& Ctx, const FString& BackendId, const FString& Model,
		const FEverloreValidationReport& Report);
};
