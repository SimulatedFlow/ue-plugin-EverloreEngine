// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Components/EverloreQuestLogComponent.h"
#include "SaveGame/EverloreSaveGame.h"
#include "EverloreLog.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Misc/Base64.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UEverloreQuestLogComponent::UEverloreQuestLogComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEverloreQuestLogComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEverloreQuestLogComponent, QuestDefs);
	DOREPLIFETIME(UEverloreQuestLogComponent, QuestStates);
}

void UEverloreQuestLogComponent::OnRep_QuestLog()
{
	// Client-side: replicated quest data changed — let UI refresh.
	MarkLogDirty();
}

bool UEverloreQuestLogComponent::IsAuthority() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

void UEverloreQuestLogComponent::MarkLogDirty()
{
	// Fires on the authority (single-player / listen-server host) AND on clients via OnRep,
	// so the "refresh your UI here" hook works in every net context.
	OnQuestLogUpdated.Broadcast();
}

// ---- lookups ----

FEverloreQuestState* UEverloreQuestLogComponent::FindState(FName QuestId)
{
	return QuestStates.FindByPredicate([QuestId](const FEverloreQuestState& S) { return S.QuestId == QuestId; });
}

const FEverloreQuestState* UEverloreQuestLogComponent::FindState(FName QuestId) const
{
	return QuestStates.FindByPredicate([QuestId](const FEverloreQuestState& S) { return S.QuestId == QuestId; });
}

const FEverloreQuestRecord* UEverloreQuestLogComponent::FindRecord(FName QuestId) const
{
	return QuestDefs.FindByPredicate([QuestId](const FEverloreQuestRecord& R) { return R.QuestId == QuestId; });
}

// ---- state machine ----

bool UEverloreQuestLogComponent::TransitionTo(FEverloreQuestState& State, EEverloreQuestStatus NewStatus)
{
	const EEverloreQuestStatus Old = State.Status;
	if (Old == NewStatus)
	{
		return false;
	}

	const bool bLegal =
		(Old == EEverloreQuestStatus::NotStarted && NewStatus == EEverloreQuestStatus::Active) ||
		(Old == EEverloreQuestStatus::Active && (NewStatus == EEverloreQuestStatus::Completed || NewStatus == EEverloreQuestStatus::Failed)) ||
		((Old == EEverloreQuestStatus::Completed || Old == EEverloreQuestStatus::Failed) && NewStatus == EEverloreQuestStatus::NotStarted);

	if (!bLegal)
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: illegal transition %s -> %s for quest '%s' (rejected)."),
			*UEnum::GetValueAsString(Old), *UEnum::GetValueAsString(NewStatus), *State.QuestId.ToString());
		return false;
	}

	State.Status = NewStatus;
	if (NewStatus == EEverloreQuestStatus::Completed || NewStatus == EEverloreQuestStatus::Failed)
	{
		State.ClosedAt = FDateTime::UtcNow();
	}
	OnQuestStateChanged.Broadcast(State.QuestId, Old, NewStatus);
	return true;
}

bool UEverloreQuestLogComponent::AcceptQuest(const FEverloreQuestRecord& Quest)
{
	if (!Quest.IsValidId())
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: AcceptQuest with no QuestId."));
		return false;
	}
	if (!IsAuthority())
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: AcceptQuest ignored on a client — quest state is server-authoritative."));
		return false;
	}

	if (const FEverloreQuestState* Existing = FindState(Quest.QuestId))
	{
		if (Existing->Status == EEverloreQuestStatus::Active)
		{
			return false; // already in progress
		}
	}

	// Store / replace the definition.
	QuestDefs.RemoveAll([&Quest](const FEverloreQuestRecord& R) { return R.QuestId == Quest.QuestId; });
	QuestDefs.Add(Quest);

	// Build a fresh state with one progress entry per objective.
	FEverloreQuestState NewState;
	NewState.QuestId = Quest.QuestId;
	NewState.Status = EEverloreQuestStatus::NotStarted;
	NewState.ActiveObjectiveIndex = 0;
	NewState.AcceptedAt = FDateTime::UtcNow();
	for (const FEverloreObjective& Obj : Quest.Payload.Objectives)
	{
		FEverloreObjectiveProgress Progress;
		Progress.ObjectiveId = Obj.ObjectiveId;
		Progress.CurrentCount = 0;
		Progress.bComplete = false;
		NewState.Objectives.Add(Progress);
	}

	QuestStates.RemoveAll([&Quest](const FEverloreQuestState& S) { return S.QuestId == Quest.QuestId; });
	QuestStates.Add(NewState);

	// NotStarted -> Active (no more array mutations, so the pointer is stable).
	if (FEverloreQuestState* State = FindState(Quest.QuestId))
	{
		TransitionTo(*State, EEverloreQuestStatus::Active);
	}
	OnQuestAccepted.Broadcast(Quest.QuestId);
	MarkLogDirty();
	return true;
}

bool UEverloreQuestLogComponent::AbandonQuest(FName QuestId)
{
	if (!IsAuthority())
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: AbandonQuest ignored on a client — quest state is server-authoritative."));
		return false;
	}
	if (FEverloreQuestState* State = FindState(QuestId))
	{
		if (State->Status == EEverloreQuestStatus::Active && TransitionTo(*State, EEverloreQuestStatus::Failed))
		{
			OnQuestFailed.Broadcast(QuestId);
			MarkLogDirty();
			return true;
		}
	}
	return false;
}

void UEverloreQuestLogComponent::NotifyObjectiveProgress(FName QuestId, FName ObjectiveId, int32 Delta)
{
	const FEverloreQuestState* State = FindState(QuestId);
	if (!State || State->Status != EEverloreQuestStatus::Active)
	{
		return;
	}
	int32 Current = 0;
	if (const FEverloreObjectiveProgress* Progress = State->Objectives.FindByPredicate(
		[ObjectiveId](const FEverloreObjectiveProgress& P) { return P.ObjectiveId == ObjectiveId; }))
	{
		Current = Progress->CurrentCount;
	}
	SetObjectiveProgress(QuestId, ObjectiveId, Current + Delta);
}

void UEverloreQuestLogComponent::SetObjectiveProgress(FName QuestId, FName ObjectiveId, int32 Count)
{
	if (!IsAuthority())
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: objective progress ignored on a client — quest state is server-authoritative."));
		return;
	}
	FEverloreQuestState* State = FindState(QuestId);
	if (!State || State->Status != EEverloreQuestStatus::Active)
	{
		return;
	}
	const FEverloreQuestRecord* Record = FindRecord(QuestId);
	if (!Record)
	{
		return;
	}

	// Required count from the definition.
	int32 Required = 1;
	for (const FEverloreObjective& Obj : Record->Payload.Objectives)
	{
		if (Obj.ObjectiveId == ObjectiveId)
		{
			Required = FMath::Max(1, Obj.RequiredCount);
			break;
		}
	}

	FEverloreObjectiveProgress* Progress = State->Objectives.FindByPredicate(
		[ObjectiveId](const FEverloreObjectiveProgress& P) { return P.ObjectiveId == ObjectiveId; });
	if (!Progress)
	{
		return;
	}

	Progress->CurrentCount = FMath::Clamp(Count, 0, Required);
	Progress->bComplete = Progress->CurrentCount >= Required;
	OnObjectiveProgress.Broadcast(QuestId, ObjectiveId);

	EvaluateCompletion(QuestId);
	MarkLogDirty();
}

void UEverloreQuestLogComponent::EvaluateCompletion(FName QuestId)
{
	FEverloreQuestState* State = FindState(QuestId);
	const FEverloreQuestRecord* Record = FindRecord(QuestId);
	if (!State || !Record || State->Status != EEverloreQuestStatus::Active)
	{
		return;
	}

	for (const FEverloreObjective& Obj : Record->Payload.Objectives)
	{
		if (Obj.bOptional)
		{
			continue;
		}
		const FEverloreObjectiveProgress* Progress = State->Objectives.FindByPredicate(
			[&Obj](const FEverloreObjectiveProgress& P) { return P.ObjectiveId == Obj.ObjectiveId; });
		if (!Progress || !Progress->bComplete)
		{
			return; // a required objective is still open
		}
	}

	if (TransitionTo(*State, EEverloreQuestStatus::Completed))
	{
		OnQuestCompleted.Broadcast(QuestId); // rewards are signalled, not applied
	}
}

// ---- queries ----

EEverloreQuestStatus UEverloreQuestLogComponent::GetQuestStatus(FName QuestId) const
{
	const FEverloreQuestState* State = FindState(QuestId);
	return State ? State->Status : EEverloreQuestStatus::NotStarted;
}

bool UEverloreQuestLogComponent::GetQuestState(FName QuestId, FEverloreQuestState& OutState) const
{
	if (const FEverloreQuestState* State = FindState(QuestId))
	{
		OutState = *State;
		return true;
	}
	return false;
}

bool UEverloreQuestLogComponent::GetQuestRecord(FName QuestId, FEverloreQuestRecord& OutRecord) const
{
	if (const FEverloreQuestRecord* Record = FindRecord(QuestId))
	{
		OutRecord = *Record;
		return true;
	}
	return false;
}

TArray<FEverloreQuestState> UEverloreQuestLogComponent::GetActiveQuests() const
{
	return QuestStates.FilterByPredicate([](const FEverloreQuestState& S) { return S.Status == EEverloreQuestStatus::Active; });
}

// ---- save / load ----

FEverloreSaveData UEverloreQuestLogComponent::BuildSaveData() const
{
	FEverloreSaveData Data;
	Data.SaveVersion = 1;
	Data.QuestDefs = QuestDefs;
	Data.QuestStates = QuestStates;
	return Data;
}

void UEverloreQuestLogComponent::ApplySaveData(const FEverloreSaveData& Data)
{
	QuestDefs = Data.QuestDefs;
	QuestStates = Data.QuestStates;
	MarkLogDirty();
}

FString UEverloreQuestLogComponent::ExportSaveData() const
{
	FEverloreSaveData Data = BuildSaveData();

	TArray<uint8> Bytes;
	FMemoryWriter MemWriter(Bytes, /*bIsPersistent*/ true);
	FObjectAndNameAsStringProxyArchive Ar(MemWriter, /*bLoadIfFindFails*/ false);
	FEverloreSaveData::StaticStruct()->SerializeItem(Ar, &Data, nullptr);

	return FBase64::Encode(Bytes);
}

bool UEverloreQuestLogComponent::ImportSaveData(const FString& Blob)
{
	if (!IsAuthority())
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: ImportSaveData ignored on a client — quest state is server-authoritative."));
		return false;
	}

	TArray<uint8> Bytes;
	if (!FBase64::Decode(Blob, Bytes))
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: ImportSaveData got invalid base64."));
		return false;
	}

	FEverloreSaveData Data;
	FMemoryReader MemReader(Bytes, /*bIsPersistent*/ true);
	// bLoadIfFindFails=false: an imported blob may be untrusted (shared/downloaded saves); never
	// let embedded object-path strings force arbitrary asset loads. Unresolved refs null out.
	FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bLoadIfFindFails*/ false);
	FEverloreSaveData::StaticStruct()->SerializeItem(Ar, &Data, nullptr);

	if (MemReader.IsError())
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: ImportSaveData blob is corrupt or truncated (archive error); not applied."));
		return false;
	}
	if (Data.SaveVersion > 1)
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestLog: save blob version %d is newer than supported (1); loading best-effort."), Data.SaveVersion);
	}

	ApplySaveData(Data);
	return true;
}

bool UEverloreQuestLogComponent::SaveQuestLog(const FString& SlotName)
{
	UEverloreSaveGame* SaveObj = Cast<UEverloreSaveGame>(UGameplayStatics::CreateSaveGameObject(UEverloreSaveGame::StaticClass()));
	if (!SaveObj)
	{
		return false;
	}
	// Store the whole-struct blob: USaveGame only persists SaveGame-tagged members, and our
	// nested quest structs aren't tagged, so route through the working Export path.
	SaveObj->Blob = ExportSaveData();
	return UGameplayStatics::SaveGameToSlot(SaveObj, SlotName, 0);
}

bool UEverloreQuestLogComponent::LoadQuestLog(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return false;
	}
	UEverloreSaveGame* SaveObj = Cast<UEverloreSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!SaveObj)
	{
		return false;
	}
	return ImportSaveData(SaveObj->Blob); // authority-gated + validated inside
}
