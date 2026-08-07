// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Backend/EverloreBackendTypes.h" // EEverloreChatRole
#include "EverloreChatTypes.generated.h"

/**
 * One persisted turn of a roleplay conversation. Speaker-tagged and SaveGame-friendly,
 * unlike the transport-only FEverloreChatMessage. The ConversationComponent keeps a
 * rolling window of these and sends it to the backend; older turns are compressed into
 * the character's MemoryBlob so context stays bounded.
 */
USTRUCT(BlueprintType)
struct FEverloreConversationTurn
{
	GENERATED_BODY()

	/** Who spoke: the NPC's CharacterId, or the player speaker id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Everlore|Chat")
	FName SpeakerId;

	/** Role from the model's perspective (User = player, Assistant = NPC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Everlore|Chat")
	EEverloreChatRole Role = EEverloreChatRole::User;

	/** What was said. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Everlore|Chat")
	FString Text;

	FEverloreConversationTurn() = default;
	FEverloreConversationTurn(FName InSpeaker, EEverloreChatRole InRole, const FString& InText)
		: SpeakerId(InSpeaker), Role(InRole), Text(InText) {}
};

/**
 * The closed whitelist of structured effects a roleplay reply may request. This enum IS
 * the safety boundary: the response schema constrains the model to exactly these values,
 * and each maps to a bounded, safe effect. The only one that touches game state
 * (OfferQuest) is routed through the SAME quest reliability pipeline (validate / repair /
 * fallback), so a chat can never put the game into an invalid state.
 */
UENUM(BlueprintType)
enum class EEverloreChatIntentType : uint8
{
	None            UMETA(DisplayName = "None"),             // pure conversation, no effect
	OfferQuest      UMETA(DisplayName = "Offer Quest"),      // Argument = theme; routed via GenerateQuest
	RememberFact    UMETA(DisplayName = "Remember Fact"),    // Argument = free-text fact -> MemoryBlob
	SetDisposition  UMETA(DisplayName = "Set Disposition"),  // Argument = new disposition text
	EndConversation UMETA(DisplayName = "End Conversation")  // NPC wishes to end the chat
};

/**
 * A validated structured intent extracted from a reply. Guaranteed to be one of the
 * whitelisted types; its effect is applied by the ConversationComponent. Free-text only
 * (no engine ids) — the one id-bearing effect, a spawned quest, is produced by the quest
 * pipeline downstream, never trusted from the chat model directly.
 */
USTRUCT(BlueprintType)
struct FEverloreChatIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Chat")
	EEverloreChatIntentType Type = EEverloreChatIntentType::None;

	/** Free-text argument: theme (OfferQuest), fact (RememberFact) or mood (SetDisposition). */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Chat")
	FString Argument;

	bool IsActionable() const { return Type != EEverloreChatIntentType::None; }
};
