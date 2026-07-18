// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Reliability/EverloreGuardrailConfig.h"

FEverloreGuardrailSnapshot UEverloreGuardrailConfig::BuildSnapshot() const
{
	FEverloreGuardrailSnapshot Snap;
	Snap.Npcs.Append(ValidNpcIds);
	Snap.Items.Append(ValidItemIds);
	Snap.Regions.Append(ValidRegionIds);
	Snap.Flags.Append(ValidFlags);
	Snap.Factions.Append(ValidFactions);

	Snap.MaxGold = MaxGold;
	Snap.MaxExperience = MaxExperience;
	Snap.MaxItemQuantity = MaxItemQuantity;
	Snap.MaxRewardBudget = MaxRewardBudget;
	Snap.MaxObjectivesPerQuest = MaxObjectivesPerQuest;
	Snap.MaxObjectiveCount = MaxObjectiveCount;
	return Snap;
}
