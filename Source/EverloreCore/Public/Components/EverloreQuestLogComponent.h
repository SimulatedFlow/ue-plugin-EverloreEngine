// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Model/EverloreQuestTypes.h"
#include "Model/EverloreSaveTypes.h"
#include "EverloreQuestLogComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEverloreQuestStateChangedSignature, FName, QuestId, EEverloreQuestStatus, OldStatus, EEverloreQuestStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEverloreObjectiveProgressSignature, FName, QuestId, FName, ObjectiveId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEverloreQuestIdSignature, FName, QuestId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEverloreQuestLogUpdatedSignature);

/**
 * Per-player quest log + state machine. Put it on the PlayerState (or Controller/Pawn).
 * It is the ONLY writer of quest status — the LLM can never set a quest to Completed.
 * Illegal transitions are rejected and logged. Rewards are SIGNALLED (OnQuestCompleted),
 * never applied, so the plugin never mutates an economy it does not own.
 *
 * Save/Load: ExportSaveData()/ImportSaveData() give a portable blob to nest in your own
 * save; SaveQuestLog()/LoadQuestLog() are a drop-in convenience.
 */
UCLASS(ClassGroup = (Everlore), meta = (BlueprintSpawnableComponent))
class EVERLORECORE_API UEverloreQuestLogComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEverloreQuestLogComponent();

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Quest Log")
	FEverloreQuestStateChangedSignature OnQuestStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Quest Log")
	FEverloreObjectiveProgressSignature OnObjectiveProgress;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Quest Log")
	FEverloreQuestIdSignature OnQuestAccepted;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Quest Log")
	FEverloreQuestIdSignature OnQuestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Everlore|Quest Log")
	FEverloreQuestIdSignature OnQuestFailed;

	/** Fired on clients after replicated quest data changes (refresh your UI here). */
	UPROPERTY(BlueprintAssignable, Category = "Everlore|Quest Log")
	FEverloreQuestLogUpdatedSignature OnQuestLogUpdated;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The single commit chokepoint: accept a validated quest (from generation or a table). */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log")
	bool AcceptQuest(const FEverloreQuestRecord& Quest);

	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log")
	bool AbandonQuest(FName QuestId);

	/** Advance an objective's counter (clamped to its required count). */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log")
	void NotifyObjectiveProgress(FName QuestId, FName ObjectiveId, int32 Delta = 1);

	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log")
	void SetObjectiveProgress(FName QuestId, FName ObjectiveId, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Everlore|Quest Log")
	EEverloreQuestStatus GetQuestStatus(FName QuestId) const;

	UFUNCTION(BlueprintPure, Category = "Everlore|Quest Log")
	bool GetQuestState(FName QuestId, FEverloreQuestState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Everlore|Quest Log")
	bool GetQuestRecord(FName QuestId, FEverloreQuestRecord& OutRecord) const;

	UFUNCTION(BlueprintPure, Category = "Everlore|Quest Log")
	TArray<FEverloreQuestState> GetActiveQuests() const;

	// ---- Save / Load ----

	/** Portable base64 blob of the whole log — nest this in your own save system. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log|Save")
	FString ExportSaveData() const;

	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log|Save")
	bool ImportSaveData(const FString& Blob);

	/** One-click drop-in save to a named slot. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log|Save")
	bool SaveQuestLog(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "Everlore|Quest Log|Save")
	bool LoadQuestLog(const FString& SlotName);

protected:
	/** Replicated so clients can display quests; only the authority ever writes it. */
	UPROPERTY(ReplicatedUsing = OnRep_QuestLog, SaveGame)
	TArray<FEverloreQuestRecord> QuestDefs;

	UPROPERTY(ReplicatedUsing = OnRep_QuestLog, SaveGame)
	TArray<FEverloreQuestState> QuestStates;

	UFUNCTION()
	void OnRep_QuestLog();

private:
	FEverloreQuestState* FindState(FName QuestId);
	const FEverloreQuestState* FindState(FName QuestId) const;
	const FEverloreQuestRecord* FindRecord(FName QuestId) const;

	/** Applies a state-machine transition if legal; logs + rejects otherwise. */
	bool TransitionTo(FEverloreQuestState& State, EEverloreQuestStatus NewStatus);
	void EvaluateCompletion(FName QuestId);

	/** True on the authority (server) or in standalone; mutators are gated on this. */
	bool IsAuthority() const;

	/** Fire OnQuestLogUpdated (called from both authority writes and client OnRep). */
	void MarkLogDirty();

	FEverloreSaveData BuildSaveData() const;
	void ApplySaveData(const FEverloreSaveData& Data);
};
