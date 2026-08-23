// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Model/EverloreQuestTypes.h"
#include "Reliability/EverloreGuardrailConfig.h"

/**
 * Single source of truth for the semantic quest rules shared by the validator and the
 * auto-fixer, so the two can never disagree about what makes a quest valid.
 */
namespace EverloreRules
{
	inline int32 RewardValue(const FEverloreReward& R)
	{
		switch (R.Type)
		{
		case EEverloreRewardType::Gold:
		case EEverloreRewardType::Experience: return FMath::Max(0, R.Amount);
		case EEverloreRewardType::Item:       return FMath::Max(0, R.Amount) * 100;
		case EEverloreRewardType::Reputation: return FMath::Max(0, R.Amount) * 10;
		default:                              return 0;
		}
	}

	inline int32 TotalRewardValue(const TArray<FEverloreReward>& Rewards)
	{
		int32 Total = 0;
		for (const FEverloreReward& R : Rewards)
		{
			Total += RewardValue(R);
		}
		return Total;
	}

	/** True if the objective's target is valid for its type; OutExpected describes the kind. */
	inline bool IsObjectiveTargetValid(const FEverloreObjective& Obj, const FEverloreGuardrailSnapshot& G, FString& OutExpected)
	{
		switch (Obj.Type)
		{
		case EEverloreObjectiveType::Kill:
		case EEverloreObjectiveType::TalkTo:
		case EEverloreObjectiveType::Escort:
		case EEverloreObjectiveType::Deliver:
			OutExpected = TEXT("an existing NPC");
			return G.IsNpc(Obj.TargetId);
		case EEverloreObjectiveType::Collect:
			OutExpected = TEXT("an existing item");
			return G.IsItem(Obj.TargetId);
		case EEverloreObjectiveType::ReachLocation:
			OutExpected = TEXT("an existing region");
			return G.IsRegion(Obj.TargetId);
		case EEverloreObjectiveType::Interact:
			OutExpected = TEXT("an existing item or region");
			return G.IsItem(Obj.TargetId) || G.IsRegion(Obj.TargetId);
		case EEverloreObjectiveType::Survive:
			OutExpected = TEXT("(no target)");
			return true;
		default:
			OutExpected = TEXT("a valid target");
			return false;
		}
	}
}
