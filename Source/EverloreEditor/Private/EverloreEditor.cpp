// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "EverloreEditor.h"
#include "EverloreLog.h"
#include "Batch/EverloreQuestBatch.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FEverloreEditorModule"

void FEverloreEditorModule::StartupModule()
{
	UE_LOG(LogEverlore, Log, TEXT("EverloreEditor started."));

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Everlore.BatchQuests"),
		TEXT("Generate <count> guaranteed-valid quests through the pipeline and bake them into a DataTable at /EverloreEngine/EverloreEngine/ProjectDemo/DT_EverloreQuests. Args: <count> [theme]."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FEverloreQuestBatch::ConsoleRun),
		ECVF_Default);
}

void FEverloreEditorModule::ShutdownModule()
{
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("Everlore.BatchQuests"));
	UE_LOG(LogEverlore, Log, TEXT("EverloreEditor shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEverloreEditorModule, EverloreEditor)
