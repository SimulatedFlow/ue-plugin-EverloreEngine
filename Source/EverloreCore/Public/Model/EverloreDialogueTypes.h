// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Model/EverloreQuestTypes.h" // FEverloreProvenance
#include "EverloreDialogueTypes.generated.h"

/**
 * Dialogue data model. A FLAT graph — nodes in an array, edges by FName — so it maps
 * cleanly onto the portable constrained-decoding schema (no $ref / recursion). L4
 * validation proves the graph is navigable (reachable terminal, no dead-ends).
 * The same two-schema split as quests: Payload (LLM) vs Record (engine-stamped).
 */

UENUM(BlueprintType)
enum class EEverloreDialogueNodeType : uint8
{
	Line       UMETA(DisplayName = "Line"),        // NPC says something, then continues
	Choice     UMETA(DisplayName = "Choice"),      // player picks an option
	QuestOffer UMETA(DisplayName = "Quest Offer"), // offers OfferQuestId
	End        UMETA(DisplayName = "End")          // terminal
};

/** A selectable option leaving a Choice node. */
USTRUCT(BlueprintType)
struct FEverloreDialogueOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName OptionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FText Text;

	/** Node this option leads to (must reference an existing NodeId). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName NextNodeId;

	/** Optional gate: only shown if this story flag is set (must exist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName ConditionFlag;

	/** Optional whitelisted effect tag fired on selection (game decides what it does). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName EffectTag;

	/** Optional quest to grant on selection (must exist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName GrantQuestId;
};

/** A node in the flat dialogue graph. */
USTRUCT(BlueprintType)
struct FEverloreDialogueNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName NodeId;

	/** Speaker (must be an Npc, or PLAYER). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	EEverloreDialogueNodeType Type = EEverloreDialogueNodeType::Line;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FText Line;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	bool bTerminal = false;

	/** For Line nodes: the single next node. For Choice nodes: use Options instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName NextNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	TArray<FEverloreDialogueOption> Options;

	/** For QuestOffer nodes: the quest offered here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName OfferQuestId;
};

/** LLM-authored dialogue content. */
USTRUCT(BlueprintType)
struct FEverloreDialoguePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FText Persona;

	/** Entry point (must reference an existing NodeId). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FName EntryNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	TArray<FEverloreDialogueNode> Nodes;
};

/** Persisted dialogue: engine-stamped identity + payload. */
USTRUCT(BlueprintType)
struct FEverloreDialogueRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Dialogue")
	FName DialogueId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Dialogue")
	int32 SchemaVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Everlore|Dialogue")
	FEverloreProvenance Provenance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Dialogue")
	FEverloreDialoguePayload Payload;
};

/**
 * Persistent per-NPC identity used by BOTH structured dialogue and roleplay chat.
 * MemoryBlob / KnownFacts survive across sessions (saved), so an NPC "remembers" the player.
 */
USTRUCT(BlueprintType)
struct FEverloreCharacterContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Character")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Character")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Character")
	FText Persona;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Character")
	FText Disposition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Everlore|Character")
	TArray<FName> KnownFacts;

	/** Rolling long-term memory summary (saved). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Everlore|Character")
	FString MemoryBlob;
};
