// Copyright 2026 Silvan Teufel All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class EverloreLlamaLocal : ModuleRules
{
	public EverloreLlamaLocal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EverloreCore",
		});

		// Reserved for a future in-process GGUF backend (not implemented yet — see the header).
		// When added, link the bundled llama.cpp here (Win64 only), e.g.:
		// Link the bundled llama.cpp here (Win64 only), e.g.:
		//   string ThirdParty = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "llama.cpp");
		//   PublicIncludePaths.Add(Path.Combine(ThirdParty, "include"));
		//   PublicAdditionalLibraries.Add(Path.Combine(ThirdParty, "lib", "Win64", "llama.lib"));
		//   RuntimeDependencies.Add("$(BinaryOutputDir)/llama.dll", Path.Combine(ThirdParty, "bin", "Win64", "llama.dll"));
	}
}
