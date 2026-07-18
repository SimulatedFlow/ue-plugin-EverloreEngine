// Copyright 2026 Simulated Flow All Rights Reserved.

#include "BlueprintNodes/EverloreGenerateQuestAsync.h"
#include "Subsystems/EverloreBackendSubsystem.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UEverloreGenerateQuestAsync* UEverloreGenerateQuestAsync::GenerateQuest(UObject* WorldContextObject, UEverloreGuardrailConfig* Guardrails, FName GiverNpcId, const FString& Theme, int32 Seed, bool bServerOnly)
{
	UEverloreGenerateQuestAsync* Node = NewObject<UEverloreGenerateQuestAsync>();
	Node->WorldContextObject = WorldContextObject;
	Node->GuardrailsConfig = Guardrails;
	Node->Ctx.GiverNpcId = GiverNpcId;
	Node->Ctx.Theme = Theme;
	Node->Ctx.Seed = Seed;
	Node->bServerOnly = bServerOnly;
	Node->RegisterWithGameInstance(WorldContextObject);
	return Node;
}

void UEverloreGenerateQuestAsync::Activate()
{
	UEverloreBackendSubsystem* Sub = UEverloreBackendSubsystem::Get();
	if (!Sub)
	{
		OnError.Broadcast(TEXT("Everlore backend subsystem is unavailable."));
		SetReadyToDestroy();
		return;
	}
	if (bServerOnly)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (World && World->GetNetMode() == NM_Client)
		{
			OnError.Broadcast(TEXT("Server-only generation was requested on a client. Generate on the server, or let the player use their own key (BYOK)."));
			SetReadyToDestroy();
			return;
		}
	}
	if (!GuardrailsConfig)
	{
		OnError.Broadcast(TEXT("No guardrail config provided; cannot guarantee a valid quest."));
		SetReadyToDestroy();
		return;
	}

	const FEverloreGuardrailSnapshot Snapshot = GuardrailsConfig->BuildSnapshot();
	if (Snapshot.Npcs.Num() == 0)
	{
		// A quest giver must be an existing NPC; without one, even the fallback can't be valid.
		OnError.Broadcast(TEXT("Guardrail config has no NPC ids; a quest giver must be an NPC."));
		SetReadyToDestroy();
		return;
	}

	TWeakObjectPtr<UEverloreGenerateQuestAsync> WeakThis(this);
	Sub->GenerateQuest(Ctx, Snapshot, FEverloreQuestReadyDelegate::CreateLambda(
		[WeakThis](const FEverloreQuestResult& Result)
		{
			if (UEverloreGenerateQuestAsync* Self = WeakThis.Get())
			{
				Self->HandleReady(Result);
			}
		}));
}

void UEverloreGenerateQuestAsync::HandleReady(const FEverloreQuestResult& Result)
{
	switch (Result.Outcome)
	{
	case EEverloreProvenance::LlmValidated:
		OnCompleted.Broadcast(Result.Record, Result.Outcome);
		break;
	case EEverloreProvenance::LlmRepaired:
		OnRepaired.Broadcast(Result.Record, Result.Outcome);
		break;
	default:
		OnFallback.Broadcast(Result.Record, Result.Outcome);
		break;
	}
	SetReadyToDestroy();
}
