// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Editor batch tool: generate N guaranteed-valid quests through the reliability pipeline and
 * bake them into a UDataTable of FEverloreQuestTableRow for deterministic shipping. Studios
 * generate a pool offline, curate/hand-edit the rows, and ship them with no runtime LLM calls.
 * Driven by the console command "Everlore.BatchQuests <count> [theme]".
 */
class FEverloreQuestBatch
{
public:
	/** Generate Count quests (async) and, when all complete, write + save the DataTable. */
	static void Run(int32 Count, const FString& Theme, const FString& PackagePath);

	/** Console entry: Args = <count> [theme...]. */
	static void ConsoleRun(const TArray<FString>& Args);
};
