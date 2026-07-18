// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EverloreCore.h"
#include "EverloreLog.h"

#define LOCTEXT_NAMESPACE "FEverloreCoreModule"

void FEverloreCoreModule::StartupModule()
{
	UE_LOG(LogEverlore, Log, TEXT("EverloreCore started."));

	// Data-egress disclosure (see README "Data handling / privacy"). Cloud backends send
	// prompts + player chat to a third party; local backends keep data on-device.
	UE_LOG(LogEverlore, Warning, TEXT("Everlore: a CLOUD backend (Gemini, or a remote GenericHTTP endpoint) transmits your prompts and any player-typed chat to that third-party service. Local backends (Ollama, llama.cpp server) keep data on-device. Disclose this and obtain player consent as required in your game."));
}

void FEverloreCoreModule::ShutdownModule()
{
	UE_LOG(LogEverlore, Log, TEXT("EverloreCore shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEverloreCoreModule, EverloreCore)
