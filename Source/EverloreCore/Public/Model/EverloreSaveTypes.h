// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Model/EverloreQuestTypes.h"
#include "EverloreSaveTypes.generated.h"

/**
 * The portable Everlore save payload. This is what ExportSaveData()/ImportSaveData()
 * round-trip so a studio can nest it inside its own save system, and what the drop-in
 * UEverloreSaveGame stores. Authored quests from shipped DataTables are NOT stored here
 * (they rehydrate by id at load); only runtime-generated definitions + all runtime state.
 */
USTRUCT(BlueprintType)
struct FEverloreSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Everlore|Save")
	int32 SaveVersion = 1;

	/** Runtime-generated quest definitions (authored/table quests excluded). */
	UPROPERTY(BlueprintReadWrite, Category = "Everlore|Save")
	TArray<FEverloreQuestRecord> QuestDefs;

	/** Per-quest runtime state (the state machine). */
	UPROPERTY(BlueprintReadWrite, Category = "Everlore|Save")
	TArray<FEverloreQuestState> QuestStates;
};
