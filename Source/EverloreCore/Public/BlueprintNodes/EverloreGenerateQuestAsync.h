// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Reliability/EverlorePipelineTypes.h"
#include "EverloreGenerateQuestAsync.generated.h"

class UEverloreGuardrailConfig;

/** BP output pin: fired with a guaranteed-valid quest. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEverloreQuestPin, const FEverloreQuestRecord&, Quest, EEverloreProvenance, Outcome);
/** BP error pin: fired ONLY for configuration problems (missing subsystem / guardrails). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreQuestErrorPin, const FString&, Error);

/**
 * "Generate Quest (Async)" — the primary Blueprint entry point. The happy path always
 * produces a valid quest, so a BP author never has to handle a "no quest" branch. The
 * On Repaired / On Fallback pins make the reliability guarantee visible in the graph
 * itself; On Error only fires for a config mistake.
 */
UCLASS()
class EVERLORECORE_API UEverloreGenerateQuestAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Generated cleanly and validated as-is. */
	UPROPERTY(BlueprintAssignable)
	FEverloreQuestPin OnCompleted;

	/** Generated, but needed repair to become valid (still guaranteed valid). */
	UPROPERTY(BlueprintAssignable)
	FEverloreQuestPin OnRepaired;

	/** The LLM output was unusable; a deterministic template was used (still valid). */
	UPROPERTY(BlueprintAssignable)
	FEverloreQuestPin OnFallback;

	/** Only a configuration error (no backend/guardrails) — never "the LLM misbehaved". */
	UPROPERTY(BlueprintAssignable)
	FEverloreQuestErrorPin OnError;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Generate Quest (Async)", AdvancedDisplay = "Seed,bServerOnly"), Category = "Everlore")
	static UEverloreGenerateQuestAsync* GenerateQuest(UObject* WorldContextObject, UEverloreGuardrailConfig* Guardrails, FName GiverNpcId, const FString& Theme, int32 Seed = -1, bool bServerOnly = true);

	virtual void Activate() override;

private:
	void HandleReady(const FEverloreQuestResult& Result);

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;

	UPROPERTY()
	TObjectPtr<UEverloreGuardrailConfig> GuardrailsConfig;

	FEverloreQuestGenContext Ctx;

	/** If true, refuse to generate on a network client (use with a shared developer key). */
	bool bServerOnly = false;
};
