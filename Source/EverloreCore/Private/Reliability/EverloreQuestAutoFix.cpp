// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Reliability/EverloreQuestAutoFix.h"
#include "Reliability/EverloreQuestRules.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Model/EverloreQuestTypes.h"

void FEverloreQuestAutoFix::Apply(FEverloreQuestPayload& P, const FEverloreGuardrailSnapshot& G)
{
	// Objectives: clamp counts, drop invalid OPTIONAL objectives, drop bad prerequisites.
	for (int32 i = P.Objectives.Num() - 1; i >= 0; --i)
	{
		FEverloreObjective& O = P.Objectives[i];
		O.RequiredCount = FMath::Clamp(O.RequiredCount, 1, FMath::Max(1, G.MaxObjectiveCount));

		FString Expected;
		if (!EverloreRules::IsObjectiveTargetValid(O, G, Expected) && O.bOptional)
		{
			P.Objectives.RemoveAt(i);
			continue;
		}

		for (int32 c = O.Prerequisites.Num() - 1; c >= 0; --c)
		{
			const FEverloreCondition& Cond = O.Prerequisites[c];
			const bool bBad =
				(Cond.Type == EEverloreConditionType::HasFlag && !G.IsFlag(Cond.TargetId)) ||
				(Cond.Type == EEverloreConditionType::HasItem && !G.IsItem(Cond.TargetId));
			if (bBad)
			{
				O.Prerequisites.RemoveAt(c);
			}
		}
	}

	// Assign / dedupe objective ids. Replacement ids are guaranteed not to collide with an
	// already-present distinct id (a plain "obj_<i>" could — review finding).
	TSet<FName> Seen;
	int32 NextSuffix = 0;
	for (int32 i = 0; i < P.Objectives.Num(); ++i)
	{
		FName& Id = P.Objectives[i].ObjectiveId;
		if (Id.IsNone() || Seen.Contains(Id))
		{
			FName Candidate;
			do
			{
				Candidate = FName(*FString::Printf(TEXT("obj_%d"), NextSuffix++));
			} while (Seen.Contains(Candidate));
			Id = Candidate;
		}
		Seen.Add(Id);
	}

	// Truncate to the objective cap.
	if (P.Objectives.Num() > G.MaxObjectivesPerQuest)
	{
		P.Objectives.SetNum(G.MaxObjectivesPerQuest);
	}

	// Rewards: drop invalid-target, clamp amounts.
	for (int32 i = P.Rewards.Num() - 1; i >= 0; --i)
	{
		FEverloreReward& R = P.Rewards[i];
		switch (R.Type)
		{
		case EEverloreRewardType::Item:
			if (!G.IsItem(R.TargetId)) { P.Rewards.RemoveAt(i); continue; }
			R.Amount = FMath::Clamp(R.Amount, 0, FMath::Max(0, G.MaxItemQuantity));
			break;
		case EEverloreRewardType::Gold:
			R.Amount = FMath::Clamp(R.Amount, 0, FMath::Max(0, G.MaxGold));
			break;
		case EEverloreRewardType::Experience:
			R.Amount = FMath::Clamp(R.Amount, 0, FMath::Max(0, G.MaxExperience));
			break;
		case EEverloreRewardType::Reputation:
			if (!G.IsFaction(R.TargetId)) { P.Rewards.RemoveAt(i); continue; }
			break;
		case EEverloreRewardType::Flag:
			if (!G.IsFlag(R.TargetId)) { P.Rewards.RemoveAt(i); continue; }
			break;
		default:
			break;
		}
	}

	// Reward budget: drop from the end until within budget.
	while (P.Rewards.Num() > 0 && EverloreRules::TotalRewardValue(P.Rewards) > G.MaxRewardBudget)
	{
		P.Rewards.Pop();
	}

	// Prerequisite flags: drop unknown.
	P.PrerequisiteFlags.RemoveAll([&G](const FName& F) { return !G.IsFlag(F); });
}
