// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Components/EverloreQuestGiverComponent.h"
#include "Subsystems/EverloreBackendSubsystem.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "EverloreLog.h"
#include "GameFramework/Actor.h"

bool UEverloreQuestGiverComponent::IsReady() const
{
	return Guardrails != nullptr;
}

void UEverloreQuestGiverComponent::RequestQuest(const FString& Theme, int32 Seed)
{
	if (bAuthorityOnly)
	{
		const AActor* Owner = GetOwner();
		if (Owner && !Owner->HasAuthority())
		{
			UE_LOG(LogEverlore, Verbose, TEXT("QuestGiver '%s': RequestQuest ignored on non-authority."), *GiverNpcId.ToString());
			return;
		}
	}

	UEverloreBackendSubsystem* Sub = UEverloreBackendSubsystem::Get();
	if (!Sub || !Guardrails)
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestGiver '%s': not ready (missing subsystem or guardrail config)."), *GiverNpcId.ToString());
		return;
	}

	const FEverloreGuardrailSnapshot Snapshot = Guardrails->BuildSnapshot();
	if (Snapshot.Npcs.Num() == 0)
	{
		UE_LOG(LogEverlore, Warning, TEXT("QuestGiver '%s': guardrail config has no NPC ids; a quest giver must be an NPC."), *GiverNpcId.ToString());
		return;
	}

	FEverloreQuestGenContext Context;
	Context.GiverNpcId = GiverNpcId;
	Context.Theme = Theme.IsEmpty() ? DefaultTheme : Theme;
	Context.Seed = Seed;

	TWeakObjectPtr<UEverloreQuestGiverComponent> WeakThis(this);
	Sub->GenerateQuest(Context, Snapshot, FEverloreQuestReadyDelegate::CreateLambda(
		[WeakThis](const FEverloreQuestResult& Result)
		{
			if (UEverloreQuestGiverComponent* Self = WeakThis.Get())
			{
				Self->OnQuestOffered.Broadcast(Result.Record, Result.Outcome);
			}
		}));
}
