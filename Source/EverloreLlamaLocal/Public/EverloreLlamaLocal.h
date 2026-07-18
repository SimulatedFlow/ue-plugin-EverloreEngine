// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * EverloreLlamaLocal — reserved module for a FUTURE in-process GGUF backend (bundled
 * llama.cpp, Win64). It is currently an inert placeholder: it registers no backend and
 * ships no third-party binaries. For on-device inference today, run a local server and use
 * the "Ollama" or "LlamaServer" HTTP backends. (Win64-gated so it never bloats other platforms.)
 */
class FEverloreLlamaLocalModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
