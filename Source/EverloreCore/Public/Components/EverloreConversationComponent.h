// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Model/EverloreChatTypes.h"
#include "Model/EverloreQuestTypes.h" // FEverloreQuestRecord, EEverloreProvenance
#include "EverloreConversationComponent.generated.h"

class UEverloreGuardrailConfig;
class UEverloreCharacterComponent;
struct FEverloreGenerationResponse;

// ---- Blueprint-facing (dynamic multicast) events ----

/** Streamed reply chunk; bDone=true on the final chunk (only fires when the backend streams). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEverloreChatDeltaSignature, const FString&, DeltaText, bool, bDone);
/** The complete, in-character spoken reply for this turn. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreChatReplySignature, const FString&, Reply);
/** A validated, whitelisted intent the reply requested (may be None). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreChatIntentSignature, const FEverloreChatIntent&, Intent);
/** A guaranteed-valid quest spawned by an OfferQuest intent (via the quest pipeline). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEverloreChatQuestSignature, const FEverloreQuestRecord&, Quest, EEverloreProvenance, Outcome);
/** A transport/config error for this turn (never corrupts game state). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreChatErrorSignature, const FString&, Error);

// ---- Native (non-dynamic) per-exchange callbacks, used by the async BP node ----

DECLARE_DELEGATE_TwoParams(FEverloreChatOnDelta, const FString& /*Delta*/, bool /*bDone*/);
DECLARE_DELEGATE_OneParam(FEverloreChatOnReply, const FString& /*Reply*/);
DECLARE_DELEGATE_OneParam(FEverloreChatOnIntent, const FEverloreChatIntent& /*Intent*/);
DECLARE_DELEGATE_OneParam(FEverloreChatOnError, const FString& /*Error*/);

/** Bundle of native callbacks for one chat exchange (async node path). */
struct FEverloreChatExchangeCallbacks
{
	FEverloreChatOnDelta OnDelta;
	FEverloreChatOnReply OnReply;
	FEverloreChatOnIntent OnIntent;
	FEverloreChatOnError OnError;
};

/**
 * Drop this on an NPC to give it free-form roleplay chat. Component-first: it reads the
 * NPC's persona + memory from a UEverloreCharacterComponent, keeps a rolling transcript,
 * and (optionally) lets the model co-emit a whitelisted structured intent alongside its
 * spoken reply. The one game-state intent, OfferQuest, is routed through the SAME quest
 * reliability pipeline, so a chat can never spawn an invalid quest or corrupt state.
 *
 * In multiplayer, generation is gated to authority (bAuthorityOnly) so API keys stay
 * server-side; the reply/quest are what you replicate to the client.
 */
UCLASS(ClassGroup = (Everlore), meta = (BlueprintSpawnableComponent))
class EVERLORECORE_API UEverloreConversationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEverloreConversationComponent();

	/** Source of persona + memory. If null, a UEverloreCharacterComponent on the owner is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation")
	TObjectPtr<UEverloreCharacterComponent> CharacterSource;

	/** Allowed vocabulary + bounds for OfferQuest intents. If null, OfferQuest is ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation")
	TObjectPtr<UEverloreGuardrailConfig> Guardrails;

	/** Speaker id stamped on player turns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation")
	FName PlayerSpeakerId = TEXT("Player");

	/** Let the model co-emit a whitelisted structured intent (constrained JSON). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation")
	bool bEnableIntents = true;

	/** Prefer token streaming when the backend supports it. Ignored while intents are on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation")
	bool bStreaming = true;

	/** Only run generation on the server (recommended for multiplayer / key safety). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation")
	bool bAuthorityOnly = true;

	/** Roleplay creativity. Higher than quest generation, which favours determinism. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature = 0.85f;

	/** Turns to keep verbatim before older ones are summarized into MemoryBlob (0 = never). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Conversation", meta = (ClampMin = "0"))
	int32 MaxHistoryTurns = 12;

	// ---- Events ----

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Conversation")
	FEverloreChatDeltaSignature OnReplyDelta;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Conversation")
	FEverloreChatReplySignature OnReplyReceived;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Conversation")
	FEverloreChatIntentSignature OnIntent;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Conversation")
	FEverloreChatQuestSignature OnQuestOffered;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Conversation")
	FEverloreChatErrorSignature OnChatError;

	// ---- API ----

	/** Send a player message. Async; results arrive via the events above. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Conversation")
	void SendMessage(const FString& PlayerText);

	/** Clear the transcript (does not erase the character's long-term MemoryBlob). */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Conversation")
	void ResetConversation();

	/** Cancel the in-flight reply, if any. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Conversation")
	void CancelReply();

	/** True while a reply is being generated (one exchange at a time). */
	UFUNCTION(BlueprintPure, Category = "Everlore|Conversation")
	bool IsBusy() const { return bExchangeActive; }

	/** True if a character (persona) can be resolved. */
	UFUNCTION(BlueprintPure, Category = "Everlore|Conversation")
	bool IsReady() const;

	/** Current rolling transcript (recent turns kept verbatim). */
	UFUNCTION(BlueprintPure, Category = "Everlore|Conversation")
	const TArray<FEverloreConversationTurn>& GetHistory() const { return History; }

	/** Native entry used by the "Talk To NPC (Async)" node. Same flow as SendMessage. */
	void Ask(const FString& PlayerText, const FEverloreChatExchangeCallbacks& Callbacks);

protected:
	/** Rolling transcript. Older turns are compressed into the character's MemoryBlob. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Everlore|Conversation")
	TArray<FEverloreConversationTurn> History;

	//~ Begin UActorComponent — tear down any in-flight exchange so bound async nodes complete.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent

private:
	UEverloreCharacterComponent* ResolveCharacter() const;

	void FinalizeExchange(uint32 ExchangeId, const FString& PlayerText, const FEverloreGenerationResponse& Response, FEverloreChatExchangeCallbacks Callbacks);
	void ApplyIntent(const FEverloreChatIntent& Intent);
	void MaybeSummarize();

	bool bExchangeActive = false;
	bool bSummarizing = false;
	FEverloreRequestHandle ActiveHandle;

	/** Bumped per exchange and on cancel/reset; a completion whose id != this is stale and ignored. */
	uint32 ExchangeGeneration = 0;

	/** Bumped only on ResetConversation; invalidates an in-flight background summary. */
	uint32 ConversationEpoch = 0;

	/** Callbacks of the current exchange, so a destroy/cancel can deliver a terminal signal. */
	FEverloreChatExchangeCallbacks PendingCallbacks;
};
