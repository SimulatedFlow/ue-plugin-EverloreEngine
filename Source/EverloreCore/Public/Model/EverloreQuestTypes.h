// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "EverloreQuestTypes.generated.h"

/**
 * Quest data model. The central design rule (see architecture §6.0):
 *   PAYLOAD  — what the LLM authors (creative/semantic content only)
 *   RECORD   — Payload + identity/version/provenance stamped by the ENGINE at commit
 *   STATE    — mutable per-player progress; only gameplay code ever writes it
 * The LLM never sees an id, a status, or a version field, so it cannot corrupt saves.
 *
 * Polymorphism is expressed with enum discriminators (flat structs, no FInstancedStruct),
 * because the constrained-decoding schema must stay a flat, closed, portable subset.
 */

UENUM(BlueprintType)
enum class EEverloreQuestType : uint8
{
	Fetch    UMETA(DisplayName = "Fetch"),
	Kill     UMETA(DisplayName = "Kill"),
	Escort   UMETA(DisplayName = "Escort"),
	Deliver  UMETA(DisplayName = "Deliver"),
	TalkTo   UMETA(DisplayName = "Talk To"),
	Explore  UMETA(DisplayName = "Explore"),
	Defend   UMETA(DisplayName = "Defend"),
	Collect  UMETA(DisplayName = "Collect")
};

UENUM(BlueprintType)
enum class EEverloreObjectiveType : uint8
{
	Collect       UMETA(DisplayName = "Collect"),        // TargetId must be an Item
	Kill          UMETA(DisplayName = "Kill"),           // TargetId must be an Npc
	ReachLocation UMETA(DisplayName = "Reach Location"), // TargetId must be a Region
	TalkTo        UMETA(DisplayName = "Talk To"),        // TargetId must be an Npc
	Interact      UMETA(DisplayName = "Interact"),       // TargetId must be an Item/Region
	Escort        UMETA(DisplayName = "Escort"),         // TargetId must be an Npc
	Survive       UMETA(DisplayName = "Survive"),        // duration in RequiredCount
	Deliver       UMETA(DisplayName = "Deliver")         // TargetId must be an Npc
};

UENUM(BlueprintType)
enum class EEverloreConditionType : uint8
{
	None          UMETA(DisplayName = "None"),
	HasFlag       UMETA(DisplayName = "Has Flag"),
	HasItem       UMETA(DisplayName = "Has Item"),
	MinLevel      UMETA(DisplayName = "Min Level"),
	MinReputation UMETA(DisplayName = "Min Reputation")
};

UENUM(BlueprintType)
enum class EEverloreComparison : uint8
{
	Equal        UMETA(DisplayName = "=="),
	NotEqual     UMETA(DisplayName = "!="),
	Greater      UMETA(DisplayName = ">"),
	GreaterEqual UMETA(DisplayName = ">="),
	Less         UMETA(DisplayName = "<"),
	LessEqual    UMETA(DisplayName = "<=")
};

UENUM(BlueprintType)
enum class EEverloreRewardType : uint8
{
	Gold       UMETA(DisplayName = "Gold"),
	Experience UMETA(DisplayName = "Experience"),
	Item       UMETA(DisplayName = "Item"),       // TargetId must be an Item
	Reputation UMETA(DisplayName = "Reputation"), // TargetId is a faction
	Flag       UMETA(DisplayName = "Flag")        // TargetId is a story flag
};

UENUM(BlueprintType)
enum class EEverloreQuestStatus : uint8
{
	NotStarted UMETA(DisplayName = "Not Started"),
	Active     UMETA(DisplayName = "Active"),
	Completed  UMETA(DisplayName = "Completed"),
	Failed     UMETA(DisplayName = "Failed")
};

/** How a committed record came to be — drives the demo's provenance badge. */
UENUM(BlueprintType)
enum class EEverloreProvenance : uint8
{
	Authored         UMETA(DisplayName = "Authored"),          // hand-made / DataTable
	LlmValidated     UMETA(DisplayName = "AI-Validated"),      // generated, passed validation as-is
	LlmRepaired      UMETA(DisplayName = "AI-Repaired"),       // generated, needed repair
	TemplateFallback UMETA(DisplayName = "Template (Game-Safe)")// deterministic fallback
};

/** A gate on an objective or quest (only existing flags/items referenced). */
USTRUCT(BlueprintType)
struct FEverloreCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	EEverloreConditionType Type = EEverloreConditionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	EEverloreComparison Op = EEverloreComparison::GreaterEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	int32 Value = 0;
};

/** A reward. Amounts are clamped by the validator against the reward budget. */
USTRUCT(BlueprintType)
struct FEverloreReward
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	EEverloreRewardType Type = EEverloreRewardType::Gold;

	/** For Item/Reputation/Flag rewards: the referenced id (must exist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	int32 Amount = 0;
};

/** One step of a quest. */
USTRUCT(BlueprintType)
struct FEverloreObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FName ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	EEverloreObjectiveType Type = EEverloreObjectiveType::TalkTo;

	/** The Npc / Item / Region this objective targets (validated against the registry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	bool bOptional = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	TArray<FEverloreCondition> Prerequisites;
};

/**
 * The LLM-authored content of a quest. NO id, status or version — those belong to the engine.
 */
USTRUCT(BlueprintType)
struct FEverloreQuestPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	EEverloreQuestType Type = EEverloreQuestType::Fetch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FText Summary;

	/** The NPC that offers this quest (must exist in the Npc registry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FName GiverNpcId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	TArray<FEverloreObjective> Objectives;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	TArray<FEverloreReward> Rewards;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	TArray<FName> PrerequisiteFlags;
};

/** Where a record came from — stamped by the engine, never by the LLM. */
USTRUCT(BlueprintType)
struct FEverloreProvenance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	EEverloreProvenance Source = EEverloreProvenance::Authored;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FString BackendId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FString Model;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FDateTime GeneratedAt;
};

/** The persisted definition of a quest: engine-stamped identity + the LLM payload. */
USTRUCT(BlueprintType)
struct FEverloreQuestRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Quest")
	int32 SchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Quest")
	FEverloreProvenance Provenance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest")
	FEverloreQuestPayload Payload;

	bool IsValidId() const { return !QuestId.IsNone(); }
};

/** Runtime progress of a single objective (saved). */
USTRUCT(BlueprintType)
struct FEverloreObjectiveProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	FName ObjectiveId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	int32 CurrentCount = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	bool bComplete = false;
};

/**
 * Mutable per-player quest state (the state machine). Only UEverloreQuestLogComponent
 * writes Status — never the LLM. Saved via UPROPERTY(SaveGame).
 */
USTRUCT(BlueprintType)
struct FEverloreQuestState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	FName QuestId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	EEverloreQuestStatus Status = EEverloreQuestStatus::NotStarted;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	int32 ActiveObjectiveIndex = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	TArray<FEverloreObjectiveProgress> Objectives;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	FDateTime AcceptedAt;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Everlore|Quest")
	FDateTime ClosedAt;
};
