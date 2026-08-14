// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Batch/EverloreQuestBatch.h"
#include "Subsystems/EverloreBackendSubsystem.h"
#include "Reliability/EverlorePipelineTypes.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Model/EverloreQuestTableRow.h"
#include "Model/EverloreQuestTypes.h"
#include "EverloreLog.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
	/** Shared, game-thread-only accumulator for one batch run (all completions serialize there). */
	struct FBatchState
	{
		int32 Remaining = 0;
		int32 Total = 0;
		FString PackagePath;
		TArray<FEverloreQuestTableRow> Rows;
	};

	void WriteDataTable(const TSharedRef<FBatchState>& State)
	{
		const FString& PackagePath = State->PackagePath;
		const FString AssetName = FPackageName::GetShortName(PackagePath);

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			UE_LOG(LogEverlore, Error, TEXT("[Everlore.BatchQuests] could not create package %s"), *PackagePath);
			return;
		}
		Package->FullyLoad();

		UDataTable* DataTable = NewObject<UDataTable>(Package, FName(*AssetName), RF_Public | RF_Standalone);
		DataTable->RowStruct = FEverloreQuestTableRow::StaticStruct();

		int32 Index = 0;
		for (const FEverloreQuestTableRow& Row : State->Rows)
		{
			const FName RowName = !Row.Quest.QuestId.IsNone()
				? Row.Quest.QuestId
				: FName(*FString::Printf(TEXT("Quest_%d"), Index));
			DataTable->AddRow(RowName, Row);
			++Index;
		}

		FAssetRegistryModule::AssetCreated(DataTable);
		Package->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, DataTable, *FileName, SaveArgs);

		UE_LOG(LogEverlore, Warning, TEXT("=== Batch: wrote %d/%d quest rows to %s (saved=%d)"),
			State->Rows.Num(), State->Total, *PackagePath, bSaved ? 1 : 0);
	}
}

void FEverloreQuestBatch::Run(int32 Count, const FString& Theme, const FString& PackagePath)
{
	Count = FMath::Clamp(Count, 1, 200);

	UEverloreBackendSubsystem* Sub = UEverloreBackendSubsystem::Get();
	if (!Sub)
	{
		UE_LOG(LogEverlore, Error, TEXT("[Everlore.BatchQuests] backend subsystem unavailable."));
		return;
	}

	// Demo vocabulary so the tool works with no project setup (mirrors the console quest fixture).
	FEverloreGuardrailSnapshot Snap;
	Snap.Npcs = { TEXT("Aldric"), TEXT("Mira"), TEXT("Doran") };
	Snap.Items = { TEXT("HealingHerb"), TEXT("RustyKey"), TEXT("AncientRelic"), TEXT("IronOre"), TEXT("GrainSack") };
	Snap.Regions = { TEXT("OldMill"), TEXT("Thornwood"), TEXT("Riverdale"), TEXT("DeepCaverns") };
	Snap.Flags = { TEXT("MetAldric"), TEXT("MillCleared"), TEXT("RelicRecovered") };
	Snap.Factions = { TEXT("VillageGuard"), TEXT("MerchantsGuild") };

	const TArray<FName> Givers = { TEXT("Aldric"), TEXT("Mira"), TEXT("Doran") };

	const TSharedRef<FBatchState> State = MakeShared<FBatchState>();
	State->Remaining = Count;
	State->Total = Count;
	State->PackagePath = PackagePath;
	State->Rows.Reserve(Count);

	UE_LOG(LogEverlore, Warning, TEXT("[Everlore.BatchQuests] generating %d quests (theme='%s') -> %s ..."),
		Count, *Theme, *PackagePath);

	for (int32 i = 0; i < Count; ++i)
	{
		FEverloreQuestGenContext Ctx;
		Ctx.GiverNpcId = Givers[i % Givers.Num()];
		Ctx.Theme = Theme;
		Ctx.Seed = i; // spread the seeds so the pool varies

		Sub->GenerateQuest(Ctx, Snap, FEverloreQuestReadyDelegate::CreateLambda([State](const FEverloreQuestResult& R)
		{
			FEverloreQuestTableRow Row;
			Row.Quest = R.Record;
			Row.Outcome = R.Outcome;
			State->Rows.Add(Row);

			UE_LOG(LogEverlore, Verbose, TEXT("  batch %d/%d: [%s] \"%s\""),
				State->Rows.Num(), State->Total, *UEnum::GetValueAsString(R.Outcome), *R.Record.Payload.Title.ToString());

			if (--State->Remaining <= 0)
			{
				WriteDataTable(State);
			}
		}));
	}
}

void FEverloreQuestBatch::ConsoleRun(const TArray<FString>& Args)
{
	int32 Count = 5;
	if (Args.Num() > 0)
	{
		const int32 Parsed = FCString::Atoi(*Args[0]);
		if (Parsed > 0)
		{
			Count = Parsed;
		}
	}

	FString Theme;
	for (int32 i = 1; i < Args.Num(); ++i)
	{
		Theme += (Theme.IsEmpty() ? TEXT("") : TEXT(" ")) + Args[i];
	}

	// Ziel liegt seit dem 14.08.2026 IM PLUGIN, nicht mehr unter /Game: die
	// Beispiel-Inhalte sind aus dem Projekt-Content in die Plugins gewandert
	// (Silvans Vorgabe „wir wollen BeispielMaps usw in den Plugins haben").
	// Der frühere Projekt-Content-Pfad existiert nicht mehr — und beim Kaeufer
	// auf Fab hat er ohnehin nie existiert, dort gibt es nur den Plugin-Mount.
	// (Der alte Pfad wird hier bewusst nicht ausgeschrieben: die Suche nach
	//  veralteten Pfaden meldete ihn sonst dauerhaft als Falschtreffer.)
	FEverloreQuestBatch::Run(Count, Theme, TEXT("/EverloreEngine/EverloreEngine/ProjectDemo/DT_EverloreQuests"));
}
