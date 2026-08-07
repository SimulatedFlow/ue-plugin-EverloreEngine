// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Reliability/EverloreQuestFallback.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Model/EverloreQuestTypes.h"

FEverloreQuestPayload FEverloreQuestFallback::BuildFallback(const FEverloreGuardrailSnapshot& G, FName GiverNpcId, int32 /*Seed*/)
{
	FEverloreQuestPayload P;
	P.Type = EEverloreQuestType::TalkTo;

	// A valid giver is guaranteed by construction: use the requested one if it is a known
	// NPC, otherwise the first NPC in the registry.
	FName Giver = GiverNpcId;
	if (!G.IsNpc(Giver))
	{
		for (const FName& Npc : G.Npcs)
		{
			Giver = Npc;
			break;
		}
	}
	P.GiverNpcId = Giver;

	P.Title = FText::FromString(TEXT("A Quiet Word"));
	P.Summary = FText::FromString(TEXT("The quest giver has something to say."));

	FEverloreObjective Talk;
	Talk.ObjectiveId = TEXT("obj_0");
	Talk.Type = EEverloreObjectiveType::TalkTo;
	Talk.TargetId = Giver;
	Talk.RequiredCount = 1;
	Talk.bOptional = false;
	Talk.Description = FText::FromString(TEXT("Speak with the quest giver."));
	P.Objectives.Add(Talk);

	// A small, budget-safe reward if the game supports gold.
	if (G.MaxGold > 0)
	{
		FEverloreReward Gold;
		Gold.Type = EEverloreRewardType::Gold;
		Gold.Amount = FMath::Min(25, G.MaxGold);
		P.Rewards.Add(Gold);
	}

	return P;
}
