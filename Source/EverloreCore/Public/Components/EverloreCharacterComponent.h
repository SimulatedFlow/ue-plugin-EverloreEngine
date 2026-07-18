// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Model/EverloreDialogueTypes.h"
#include "EverloreCharacterComponent.generated.h"

/**
 * Holds a persistent NPC identity (persona + memory) shared by quest-giving and roleplay
 * chat. Drop it on any NPC actor. Memory (KnownFacts / MemoryBlob) is SaveGame-tagged, so
 * the NPC "remembers" the player across sessions.
 */
UCLASS(ClassGroup = (Everlore), meta = (BlueprintSpawnableComponent))
class EVERLORECORE_API UEverloreCharacterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Everlore|Character")
	FEverloreCharacterContext Character;

	/** Add a durable fact this NPC knows (deduped). */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Character")
	void RememberFact(FName Fact);

	/** Append a line to the rolling long-term memory blob. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Character")
	void AppendMemory(const FString& Text);

	UFUNCTION(BlueprintPure, Category = "Everlore|Character")
	FText GetPersona() const { return Character.Persona; }

	UFUNCTION(BlueprintPure, Category = "Everlore|Character")
	FName GetCharacterId() const { return Character.CharacterId; }
};
