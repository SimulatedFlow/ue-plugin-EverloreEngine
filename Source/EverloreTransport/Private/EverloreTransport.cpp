// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "EverloreTransport.h"
#include "EverloreLog.h"

#define LOCTEXT_NAMESPACE "FEverloreTransportModule"

void FEverloreTransportModule::StartupModule()
{
	UE_LOG(LogEverlore, Log, TEXT("EverloreTransport started."));
}

void FEverloreTransportModule::ShutdownModule()
{
	UE_LOG(LogEverlore, Log, TEXT("EverloreTransport shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEverloreTransportModule, EverloreTransport)
