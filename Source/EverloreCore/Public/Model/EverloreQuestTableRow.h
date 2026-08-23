// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Model/EverloreQuestTypes.h"
#include "EverloreQuestTableRow.generated.h"

/**
 * One row of a baked quest DataTable. The editor batch tool generates quests through the
 * full reliability pipeline (so every row is guaranteed-valid) and writes them here, letting
 * a studio curate a pool offline and ship deterministic, hand-reviewed content — no runtime
 * LLM calls, no keys, no latency.
 */
USTRUCT(BlueprintType)
struct FEverloreQuestTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The committed, guaranteed-valid quest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore")
	FEverloreQuestRecord Quest;

	/** How it was produced (AI-validated / repaired / template fallback). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore")
	EEverloreProvenance Outcome = EEverloreProvenance::Authored;
};
