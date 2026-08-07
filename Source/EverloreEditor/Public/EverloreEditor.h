// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/**
 * EverloreEditor — editor-only batch tooling.
 * Editor Utility Widget review panel + commandlet that generate quests through the
 * same pipeline and bake them into UDataTables for deterministic shipping.
 * Type "Editor" => automatically stripped from cooked / shipping / dedicated-server builds.
 */
class FEverloreEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
