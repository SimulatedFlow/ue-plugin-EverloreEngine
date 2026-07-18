// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Reliability/EverloreQuestValidator.h"
#include "Reliability/EverloreValidationTypes.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Model/EverloreQuestTypes.h"
#include "Reliability/EverloreQuestRules.h"

void FEverloreQuestValidator::Validate(const FEverloreQuestPayload& Payload, const FEverloreGuardrailSnapshot& Guardrails, FEverloreValidationReport& OutReport)
{
	// A never-configured guardrail set can't prove anything — treat as a hard error so the
	// pipeline falls back to a content-free template rather than trusting the payload.
	if (Guardrails.IsEmpty())
	{
		OutReport.Add(EEverloreSeverity::Error, TEXT("$"), TEXT("Config.EmptyGuardrails"),
			TEXT("Guardrail registry is empty; cannot validate references."),
			TEXT("Assign a UEverloreGuardrailConfig with NPC/item/region ids."));
		return;
	}

	// ---- L1 structural ----
	if (Payload.Title.IsEmptyOrWhitespace())
	{
		OutReport.Add(EEverloreSeverity::Error, TEXT("title"), TEXT("L1.MissingTitle"),
			TEXT("Quest has no title."), TEXT("Provide a short title."));
	}
	if (Payload.Summary.IsEmptyOrWhitespace())
	{
		OutReport.Add(EEverloreSeverity::Warning, TEXT("summary"), TEXT("L1.MissingSummary"),
			TEXT("Quest has no summary."));
	}
	if (Payload.Objectives.Num() == 0)
	{
		OutReport.Add(EEverloreSeverity::Error, TEXT("objectives"), TEXT("L1.NoObjectives"),
			TEXT("Quest has no objectives."), TEXT("Add at least one non-optional objective."));
	}
	if (Payload.Objectives.Num() > Guardrails.MaxObjectivesPerQuest)
	{
		OutReport.Add(EEverloreSeverity::AutoFixable, TEXT("objectives"), TEXT("L1.TooManyObjectives"),
			FString::Printf(TEXT("Quest has %d objectives; max is %d."), Payload.Objectives.Num(), Guardrails.MaxObjectivesPerQuest),
			TEXT("Extra objectives will be dropped."));
	}

	// Duplicate objective ids.
	TSet<FName> SeenObjectiveIds;
	for (int32 i = 0; i < Payload.Objectives.Num(); ++i)
	{
		const FName Id = Payload.Objectives[i].ObjectiveId;
		if (!Id.IsNone())
		{
			bool bAlready = false;
			SeenObjectiveIds.Add(Id, &bAlready);
			if (bAlready)
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, FString::Printf(TEXT("objectives[%d].objectiveId"), i),
					TEXT("L1.DuplicateObjectiveId"), FString::Printf(TEXT("Duplicate objective id '%s'."), *Id.ToString()),
					TEXT("A fresh id will be assigned."));
			}
		}
	}

	// ---- L2 referential ----
	if (!Guardrails.IsNpc(Payload.GiverNpcId))
	{
		OutReport.Add(EEverloreSeverity::Error, TEXT("giverNpcId"), TEXT("L2.UnknownGiver"),
			FString::Printf(TEXT("giverNpcId '%s' is not a known NPC."), *Payload.GiverNpcId.ToString()),
			TEXT("Use one of the allowed NPC ids."));
	}

	int32 ValidNonOptionalObjectives = 0;
	for (int32 i = 0; i < Payload.Objectives.Num(); ++i)
	{
		const FEverloreObjective& Obj = Payload.Objectives[i];
		const FString ObjPath = FString::Printf(TEXT("objectives[%d]"), i);

		FString Expected;
		const bool bTargetOk = EverloreRules::IsObjectiveTargetValid(Obj, Guardrails, Expected);
		if (!bTargetOk)
		{
			const EEverloreSeverity Sev = Obj.bOptional ? EEverloreSeverity::AutoFixable : EEverloreSeverity::Error;
			OutReport.Add(Sev, ObjPath + TEXT(".targetId"), TEXT("L2.BadObjectiveTarget"),
				FString::Printf(TEXT("Objective %d (%s) target '%s' must be %s."),
					i, *UEnum::GetValueAsString(Obj.Type), *Obj.TargetId.ToString(), *Expected),
				Obj.bOptional ? TEXT("Optional objective will be dropped.") : TEXT("Use a valid target of the right kind."));
		}
		else if (!Obj.bOptional)
		{
			++ValidNonOptionalObjectives;
		}

		// ---- L3 bounds (objective count) ----
		if (Obj.RequiredCount < 1 || Obj.RequiredCount > Guardrails.MaxObjectiveCount)
		{
			OutReport.Add(EEverloreSeverity::AutoFixable, ObjPath + TEXT(".requiredCount"), TEXT("L3.CountOutOfRange"),
				FString::Printf(TEXT("requiredCount %d out of range [1, %d]."), Obj.RequiredCount, Guardrails.MaxObjectiveCount),
				TEXT("Will be clamped."));
		}

		// Objective prerequisites reference existing flags/items.
		for (int32 c = 0; c < Obj.Prerequisites.Num(); ++c)
		{
			const FEverloreCondition& Cond = Obj.Prerequisites[c];
			const FString CondPath = FString::Printf(TEXT("%s.prerequisites[%d].targetId"), *ObjPath, c);
			if (Cond.Type == EEverloreConditionType::HasFlag && !Guardrails.IsFlag(Cond.TargetId))
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, CondPath, TEXT("L2.UnknownFlag"),
					FString::Printf(TEXT("Condition flag '%s' does not exist."), *Cond.TargetId.ToString()),
					TEXT("Invalid condition will be dropped."));
			}
			else if (Cond.Type == EEverloreConditionType::HasItem && !Guardrails.IsItem(Cond.TargetId))
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, CondPath, TEXT("L2.UnknownItem"),
					FString::Printf(TEXT("Condition item '%s' does not exist."), *Cond.TargetId.ToString()),
					TEXT("Invalid condition will be dropped."));
			}
		}
	}

	// ---- L2/L3 rewards ----
	int32 TotalRewardValue = 0;
	for (int32 i = 0; i < Payload.Rewards.Num(); ++i)
	{
		const FEverloreReward& Reward = Payload.Rewards[i];
		const FString RewardPath = FString::Printf(TEXT("rewards[%d]"), i);

		switch (Reward.Type)
		{
		case EEverloreRewardType::Item:
			if (!Guardrails.IsItem(Reward.TargetId))
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, RewardPath + TEXT(".targetId"), TEXT("L2.UnknownRewardItem"),
					FString::Printf(TEXT("Reward item '%s' does not exist."), *Reward.TargetId.ToString()),
					TEXT("Invalid reward will be dropped."));
			}
			if (Reward.Amount > Guardrails.MaxItemQuantity)
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, RewardPath + TEXT(".amount"), TEXT("L3.ItemQtyTooHigh"),
					FString::Printf(TEXT("Item amount %d exceeds max %d."), Reward.Amount, Guardrails.MaxItemQuantity),
					TEXT("Will be clamped."));
			}
			break;
		case EEverloreRewardType::Gold:
			if (Reward.Amount > Guardrails.MaxGold)
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, RewardPath + TEXT(".amount"), TEXT("L3.GoldTooHigh"),
					FString::Printf(TEXT("Gold %d exceeds max %d."), Reward.Amount, Guardrails.MaxGold), TEXT("Will be clamped."));
			}
			break;
		case EEverloreRewardType::Experience:
			if (Reward.Amount > Guardrails.MaxExperience)
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, RewardPath + TEXT(".amount"), TEXT("L3.XpTooHigh"),
					FString::Printf(TEXT("XP %d exceeds max %d."), Reward.Amount, Guardrails.MaxExperience), TEXT("Will be clamped."));
			}
			break;
		case EEverloreRewardType::Reputation:
			if (!Guardrails.IsFaction(Reward.TargetId))
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, RewardPath + TEXT(".targetId"), TEXT("L2.UnknownFaction"),
					FString::Printf(TEXT("Reputation faction '%s' does not exist."), *Reward.TargetId.ToString()),
					TEXT("Invalid reward will be dropped."));
			}
			break;
		case EEverloreRewardType::Flag:
			if (!Guardrails.IsFlag(Reward.TargetId))
			{
				OutReport.Add(EEverloreSeverity::AutoFixable, RewardPath + TEXT(".targetId"), TEXT("L2.UnknownRewardFlag"),
					FString::Printf(TEXT("Reward flag '%s' does not exist."), *Reward.TargetId.ToString()),
					TEXT("Invalid reward will be dropped."));
			}
			break;
		default:
			break;
		}

		TotalRewardValue += EverloreRules::RewardValue(Reward);
	}

	if (TotalRewardValue > Guardrails.MaxRewardBudget)
	{
		OutReport.Add(EEverloreSeverity::AutoFixable, TEXT("rewards"), TEXT("L3.RewardBudgetExceeded"),
			FString::Printf(TEXT("Total reward value %d exceeds budget %d."), TotalRewardValue, Guardrails.MaxRewardBudget),
			TEXT("Rewards will be scaled down."));
	}

	// PrerequisiteFlags exist.
	for (int32 i = 0; i < Payload.PrerequisiteFlags.Num(); ++i)
	{
		if (!Guardrails.IsFlag(Payload.PrerequisiteFlags[i]))
		{
			OutReport.Add(EEverloreSeverity::AutoFixable, FString::Printf(TEXT("prerequisiteFlags[%d]"), i),
				TEXT("L2.UnknownPrereqFlag"),
				FString::Printf(TEXT("Prerequisite flag '%s' does not exist."), *Payload.PrerequisiteFlags[i].ToString()),
				TEXT("Invalid prerequisite will be dropped."));
		}
	}

	// ---- L4 completable ----
	if (Payload.Objectives.Num() > 0 && ValidNonOptionalObjectives == 0)
	{
		OutReport.Add(EEverloreSeverity::Error, TEXT("objectives"), TEXT("L4.NotCompletable"),
			TEXT("Quest has no valid, non-optional objective, so it can never be completed."),
			TEXT("Add at least one required objective with a valid target."));
	}
}
