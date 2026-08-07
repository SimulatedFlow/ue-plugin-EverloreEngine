// Copyright 2026 Simulated Flow All Rights Reserved.

#include "EverloreLlamaLocal.h"
#include "EverloreLog.h"

#define LOCTEXT_NAMESPACE "FEverloreLlamaLocalModule"

void FEverloreLlamaLocalModule::StartupModule()
{
	UE_LOG(LogEverlore, Log, TEXT("EverloreLlamaLocal started."));
}

void FEverloreLlamaLocalModule::ShutdownModule()
{
	UE_LOG(LogEverlore, Log, TEXT("EverloreLlamaLocal shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEverloreLlamaLocalModule, EverloreLlamaLocal)
