// Copyright 2026 Simulated Flow All Rights Reserved.

using UnrealBuildTool;

public class EverloreEditor : ModuleRules
{
	public EverloreEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags", // FEverloreQuestPayload.Tags — batch tool copies quest records
			"EverloreCore",
			"EverloreTransport",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"ToolMenus",
			"Blutility",
			"UMGEditor",
			"AssetRegistry",
			"Slate",
			"SlateCore",
		});
	}
}
