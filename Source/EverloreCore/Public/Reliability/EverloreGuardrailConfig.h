// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EverloreGuardrailConfig.generated.h"

/**
 * Immutable POD snapshot of the guardrails, safe to read on a worker thread.
 * Built on the game thread from a UEverloreGuardrailConfig (and/or registry providers),
 * then handed to the validator off-thread. It touches no UObjects.
 */
struct EVERLORECORE_API FEverloreGuardrailSnapshot
{
	TSet<FName> Npcs;
	TSet<FName> Items;
	TSet<FName> Regions;
	TSet<FName> Flags;
	TSet<FName> Factions;

	int32 MaxGold = 500;
	int32 MaxExperience = 1000;
	int32 MaxItemQuantity = 99;
	int32 MaxRewardBudget = 2000;
	int32 MaxObjectivesPerQuest = 5;
	int32 MaxObjectiveCount = 99;

	bool IsNpc(FName Id) const { return !Id.IsNone() && Npcs.Contains(Id); }
	bool IsItem(FName Id) const { return !Id.IsNone() && Items.Contains(Id); }
	bool IsRegion(FName Id) const { return !Id.IsNone() && Regions.Contains(Id); }
	bool IsFlag(FName Id) const { return !Id.IsNone() && Flags.Contains(Id); }
	bool IsFaction(FName Id) const { return !Id.IsNone() && Factions.Contains(Id); }

	/** A completely empty snapshot means guardrails were never configured — a hard error. */
	bool IsEmpty() const { return Npcs.Num() == 0 && Items.Num() == 0 && Regions.Num() == 0; }
};

/**
 * Designer-authored guardrail whitelist + numeric bounds. Feeds BOTH the decode-time
 * enums and L2/L3 validation from one source, so they can never disagree. Populate by
 * hand or via the editor AssetRegistry-scan adapter (Phase e); hand entries are checked
 * against the AssetRegistry at editor time so a whitelist can't list phantom ids.
 */
UCLASS(BlueprintType)
class EVERLORECORE_API UEverloreGuardrailConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Ids")
	TArray<FName> ValidNpcIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Ids")
	TArray<FName> ValidItemIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Ids")
	TArray<FName> ValidRegionIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Ids")
	TArray<FName> ValidFlags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Ids")
	TArray<FName> ValidFactions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Bounds", meta = (ClampMin = "0"))
	int32 MaxGold = 500;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Bounds", meta = (ClampMin = "0"))
	int32 MaxExperience = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Bounds", meta = (ClampMin = "1"))
	int32 MaxItemQuantity = 99;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Bounds", meta = (ClampMin = "0"))
	int32 MaxRewardBudget = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Bounds", meta = (ClampMin = "1"))
	int32 MaxObjectivesPerQuest = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Guardrails|Bounds", meta = (ClampMin = "1"))
	int32 MaxObjectiveCount = 99;

	/** Build the immutable POD snapshot consumed by the off-thread validator. */
	FEverloreGuardrailSnapshot BuildSnapshot() const;
};
