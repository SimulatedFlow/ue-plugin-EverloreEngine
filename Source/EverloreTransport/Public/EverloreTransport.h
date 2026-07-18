// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * EverloreTransport — concrete backends and HTTP/JSON transport.
 * Transports bytes and applies constraints; it never parses quests.
 * Parse / validate / repair / fallback all live in EverloreCore.
 */
class FEverloreTransportModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
