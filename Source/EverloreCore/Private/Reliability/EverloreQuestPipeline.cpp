// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Reliability/EverloreQuestPipeline.h"
#include "Reliability/EverloreQuestJson.h"
#include "Reliability/EverloreQuestValidator.h"
#include "Reliability/EverloreQuestAutoFix.h"
#include "Reliability/EverloreQuestFallback.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Backend/EverloreBackendTypes.h"
#include "Model/EverloreQuestTypes.h"
#include "EverloreLog.h"
#include "Misc/Guid.h"

FEverloreQuestResult FEverloreQuestPipeline::Commit(const FEverloreQuestPayload& Payload, EEverloreProvenance Source,
	const FEverloreQuestGenContext& Ctx, const FString& BackendId, const FString& Model, const FEverloreValidationReport& Report)
{
	FEverloreQuestResult Res;
	Res.Record.QuestId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Short));
	Res.Record.SchemaVersion = 1;
	Res.Record.Provenance.Source = Source;
	Res.Record.Provenance.BackendId = BackendId;
	Res.Record.Provenance.Model = Model;
	Res.Record.Provenance.Seed = Ctx.Seed;
	Res.Record.Provenance.GeneratedAt = FDateTime::UtcNow();
	Res.Record.Payload = Payload;
	// Engine stamps objective identity (objectiveId is not part of the LLM schema). This
	// guarantees non-None, unique, stable ids so per-objective progress tracking works.
	for (int32 i = 0; i < Res.Record.Payload.Objectives.Num(); ++i)
	{
		Res.Record.Payload.Objectives[i].ObjectiveId = FName(*FString::Printf(TEXT("obj_%d"), i));
	}
	Res.Outcome = Source;
	Res.bUsedFallback = (Source == EEverloreProvenance::TemplateFallback);
	Res.Report = Report;
	return Res;
}

FEverloreQuestResult FEverloreQuestPipeline::BuildResult(const FEverloreGenerationResponse& Response,
	const FEverloreQuestGenContext& Ctx, const FEverloreGuardrailSnapshot& Snapshot,
	const FString& BackendId, const FString& Model)
{
	FEverloreQuestPayload Payload;
	bool bParsed = false;
	if (Response.Status == EEverloreBackendStatus::Success && !Response.ExtractedJson.IsEmpty())
	{
		bParsed = FEverloreQuestJson::ParseQuestPayload(Response.ExtractedJson, Payload);
	}

	if (bParsed)
	{
		Payload.GiverNpcId = Ctx.GiverNpcId; // engine injects the giver

		FEverloreValidationReport R1;
		FEverloreQuestValidator::Validate(Payload, Snapshot, R1);
		// Commit as-is ONLY when there is nothing to correct — no errors AND no auto-fixable
		// issues. Auto-fixable includes every value/budget clamp and cap/dedupe, so committing
		// on "!HasErrors()" alone would ship out-of-bounds rewards etc. (review finding).
		if (!R1.HasErrors() && R1.NumAutoFixable() == 0)
		{
			UE_LOG(LogEverlore, Verbose, TEXT("Quest validated cleanly (%d informational notes)."), R1.Issues.Num());
			return Commit(Payload, EEverloreProvenance::LlmValidated, Ctx, BackendId, Model, R1);
		}

		// S5 deterministic auto-fix (errors and/or auto-fixable bound/dedupe issues), re-validate.
		FEverloreQuestAutoFix::Apply(Payload, Snapshot);
		FEverloreValidationReport R2;
		FEverloreQuestValidator::Validate(Payload, Snapshot, R2);
		if (!R2.HasErrors())
		{
			UE_LOG(LogEverlore, Verbose, TEXT("Quest auto-repaired (%d errors, %d auto-fixes)."), R1.NumErrors(), R1.NumAutoFixable());
			return Commit(Payload, EEverloreProvenance::LlmRepaired, Ctx, BackendId, Model, R2);
		}
		UE_LOG(LogEverlore, Warning, TEXT("Quest still has %d errors after auto-fix — using fallback."), R2.NumErrors());
	}
	else
	{
		UE_LOG(LogEverlore, Warning, TEXT("Quest response unusable (status=%d) — using fallback."), (int32)Response.Status);
	}

	// S7 fallback — valid by construction, given the entry points enforce >=1 NPC.
	FEverloreQuestPayload Fb = FEverloreQuestFallback::BuildFallback(Snapshot, Ctx.GiverNpcId, Ctx.Seed);
	FEverloreValidationReport Rf;
	FEverloreQuestValidator::Validate(Fb, Snapshot, Rf);
	if (Rf.HasErrors())
	{
		// Should be impossible once callers guarantee an NPC giver exists. Log loudly so a
		// misconfiguration (no NPC ids) is never silently shipped as if it were valid.
		UE_LOG(LogEverlore, Error, TEXT("Everlore fallback FAILED validation (%d errors) — the guardrail config has no usable NPC giver. Quest is NOT valid; fix the guardrail config."), Rf.NumErrors());
	}
	return Commit(Fb, EEverloreProvenance::TemplateFallback, Ctx, BackendId, Model, Rf);
}
