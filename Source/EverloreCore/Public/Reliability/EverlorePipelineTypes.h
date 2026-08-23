// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Model/EverloreQuestTypes.h"
#include "Reliability/EverloreValidationTypes.h"
#include "EverlorePipelineTypes.generated.h"

/** Inputs to one quest generation. */
USTRUCT(BlueprintType)
struct FEverloreQuestGenContext
{
	GENERATED_BODY()

	/** The NPC that will offer the quest (engine-injected; never chosen by the LLM). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore")
	FName GiverNpcId;

	/** Optional free-text hint / theme for the generator. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore")
	FString Theme;

	/** Deterministic seed; -1 = random. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore")
	int32 Seed = -1;
};

/** The always-valid outcome of the pipeline. */
USTRUCT(BlueprintType)
struct FEverloreQuestResult
{
	GENERATED_BODY()

	/** The committed, guaranteed-valid quest. */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore")
	FEverloreQuestRecord Record;

	/** How it came to be (drives the demo's provenance badge). */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore")
	EEverloreProvenance Outcome = EEverloreProvenance::TemplateFallback;

	UPROPERTY(BlueprintReadOnly, Category = "Everlore")
	bool bUsedFallback = false;

	/** Residual (non-error) issues, for diagnostics. */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore")
	FEverloreValidationReport Report;
};

DECLARE_DELEGATE_OneParam(FEverloreQuestReadyDelegate, const FEverloreQuestResult& /*Result*/);
