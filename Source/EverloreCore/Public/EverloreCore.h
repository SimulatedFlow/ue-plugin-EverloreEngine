// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * EverloreCore — the reliability / game-logic layer.
 * Owns the data model, the canonical schema, the validate->repair->fallback pipeline,
 * the abstract backend + registry, and the gameplay-facing components.
 * This module is deliberately HTTP-free: the "never breaks your game" guarantee
 * cannot be reached by transport/network bugs.
 */
class FEverloreCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
