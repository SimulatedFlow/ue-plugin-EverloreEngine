// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Reliability/EverlorePipelineTypes.h"
#include "EverloreQuestGiverComponent.generated.h"

class UEverloreGuardrailConfig;

/** Fired when this giver has a guaranteed-valid quest to offer. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEverloreQuestOfferedSignature, const FEverloreQuestRecord&, Quest, EEverloreProvenance, Outcome);

/**
 * Drop this on an NPC actor to make it an AI quest giver. Component-first: designers wire
 * a guardrail config + this NPC's id, call RequestQuest, and bind OnQuestOffered — no
 * subsystem plumbing. In multiplayer, generation is gated to authority so keys stay
 * server-side; the resulting validated quest is what you replicate.
 */
UCLASS(ClassGroup = (Everlore), meta = (BlueprintSpawnableComponent))
class EVERLORECORE_API UEverloreQuestGiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** This NPC's id (must exist in the guardrail registry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest Giver")
	FName GiverNpcId;

	/** The allowed vocabulary + bounds this giver generates within. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest Giver")
	TObjectPtr<UEverloreGuardrailConfig> Guardrails;

	/** Optional default theme/hint used when RequestQuest is called with an empty theme. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest Giver")
	FString DefaultTheme;

	/** Only generate on the server (recommended for multiplayer / key safety). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Quest Giver")
	bool bAuthorityOnly = true;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Quest Giver")
	FEverloreQuestOfferedSignature OnQuestOffered;

	/** Request a fresh quest. Async; result arrives via OnQuestOffered (always valid). */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Giver")
	void RequestQuest(const FString& Theme, int32 Seed = -1);

	/** True if a guardrail config with content is assigned. */
	UFUNCTION(BlueprintPure, Category = "Everlore|Quest Giver")
	bool IsReady() const;
};
