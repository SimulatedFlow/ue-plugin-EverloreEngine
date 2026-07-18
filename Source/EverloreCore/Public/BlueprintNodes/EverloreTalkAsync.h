// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Model/EverloreChatTypes.h"
#include "EverloreTalkAsync.generated.h"

class UEverloreConversationComponent;

/** BP output pin: a streamed reply chunk (bDone=true on the last). Only fires when the backend streams. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEverloreTalkDeltaPin, const FString&, DeltaText, bool, bDone);
/** BP output pin: the complete in-character reply for this turn. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreTalkReplyPin, const FString&, Reply);
/** BP output pin: a validated whitelisted intent the reply requested (a spawned quest arrives on the component's OnQuestOffered). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreTalkIntentPin, const FEverloreChatIntent&, Intent);
/** BP output pin: a transport/config error for this turn (never corrupts game state). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreTalkErrorPin, const FString&, Error);

/**
 * "Talk To NPC (Async)" — the Blueprint entry point for roleplay chat. Drives a
 * UEverloreConversationComponent through one exchange: On Delta streams chunks (when the
 * backend supports it), On Reply delivers the finished line, On Intent surfaces a
 * whitelisted effect the NPC decided on. On Error only fires for transport/config problems.
 */
UCLASS()
class EVERLORECORE_API UEverloreTalkAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEverloreTalkDeltaPin OnDelta;

	UPROPERTY(BlueprintAssignable)
	FEverloreTalkReplyPin OnReply;

	UPROPERTY(BlueprintAssignable)
	FEverloreTalkIntentPin OnIntent;

	UPROPERTY(BlueprintAssignable)
	FEverloreTalkErrorPin OnError;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Talk To NPC (Async)"), Category = "Everlore")
	static UEverloreTalkAsync* TalkToNpc(UObject* WorldContextObject, UEverloreConversationComponent* Conversation, const FString& Message);

	virtual void Activate() override;

private:
	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;

	UPROPERTY()
	TObjectPtr<UEverloreConversationComponent> Conversation;

	FString Message;
};
