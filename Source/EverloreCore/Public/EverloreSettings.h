// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EverloreSettings.generated.h"

/**
 * Project-wide Everlore configuration. Non-secret only.
 * Stored in the plugin-owned Config/DefaultEverloreEngine.ini (config = EverloreEngine).
 * API KEYS ARE NEVER STORED HERE — only the *name* of the environment variable to read.
 */
UCLASS(config = EverloreEngine, defaultconfig, meta = (DisplayName = "Everlore Engine"))
class EVERLORECORE_API UEverloreSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Default backend id. Shipped ids: Gemini, Ollama, LlamaServer, GenericHttp. A typo = no active backend (generation always falls back to safe templates). Switch at runtime with the subsystem SetActiveBackend or the Everlore.SetBackend console command. */
	UPROPERTY(config, EditAnywhere, Category = "Backend")
	FName ActiveBackendId = TEXT("Gemini");

	/** Max concurrent requests for a cloud / remote backend. */
	UPROPERTY(config, EditAnywhere, Category = "Backend", meta = (ClampMin = "1"))
	int32 MaxConcurrentRequests = 4;

	/** Max concurrent requests for a local backend (GPU contention — keep low). */
	UPROPERTY(config, EditAnywhere, Category = "Backend", meta = (ClampMin = "1"))
	int32 MaxConcurrentLocalRequests = 1;

	/** Default request timeout in seconds. */
	UPROPERTY(config, EditAnywhere, Category = "Backend", meta = (ClampMin = "1.0"))
	float DefaultTimeoutSeconds = 30.f;

	// ---- Google Gemini ----
	UPROPERTY(config, EditAnywhere, Category = "Gemini")
	FString GeminiEndpoint = TEXT("https://generativelanguage.googleapis.com/v1beta");

	/** Auto-updating alias so the plugin never breaks when a pinned model is retired. */
	UPROPERTY(config, EditAnywhere, Category = "Gemini")
	FString GeminiModel = TEXT("gemini-flash-latest");

	/**
	 * Gemini "thinking" budget in tokens. 0 disables thinking — recommended for structured
	 * quest generation (reliable JSON, faster, cheaper: newer flash models otherwise burn
	 * the whole output budget on hidden reasoning and truncate). -1 = model default; raise
	 * it only for open-ended roleplay where reasoning helps.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Gemini")
	int32 GeminiThinkingBudget = 0;

	/** Environment variable that holds the Gemini API key (server / dev path). */
	UPROPERTY(config, EditAnywhere, Category = "Gemini")
	FString GeminiApiKeyEnvVar = TEXT("EVERLORE_GEMINI_KEY");

	// ---- Ollama (local, no key) ----
	/** Base URL of your running Ollama server (see https://ollama.com). No API key needed. */
	UPROPERTY(config, EditAnywhere, Category = "Ollama")
	FString OllamaEndpoint = TEXT("http://localhost:11434");

	/** An Ollama model you have pulled (e.g. run `ollama pull llama3.1:8b`). */
	UPROPERTY(config, EditAnywhere, Category = "Ollama")
	FString OllamaModel = TEXT("llama3.1:8b");

	// ---- llama.cpp server (local, OpenAI-compatible, no key) ----
	/** Base URL of your llama.cpp `llama-server` (its /v1/chat/completions endpoint is used). */
	UPROPERTY(config, EditAnywhere, Category = "LlamaServer")
	FString LlamaServerEndpoint = TEXT("http://localhost:8080");

	/** Sent as the OpenAI "model" field; llama.cpp server ignores the value (uses its loaded model). */
	UPROPERTY(config, EditAnywhere, Category = "LlamaServer")
	FString LlamaServerModel = TEXT("local-model");

	// ---- Generic OpenAI-compatible HTTP backend (OpenRouter, LocalAI, vLLM, …) ----
	/** Full chat-completions URL, e.g. "https://openrouter.ai/api/v1/chat/completions". */
	UPROPERTY(config, EditAnywhere, Category = "GenericHttp")
	FString GenericHttpEndpoint = TEXT("http://localhost:8000/v1/chat/completions");

	UPROPERTY(config, EditAnywhere, Category = "GenericHttp")
	FString GenericHttpModel = TEXT("gpt-4o-mini");

	/** Env-var NAME holding the bearer key, if the endpoint needs one ("" = no auth). Never cooked. */
	UPROPERTY(config, EditAnywhere, Category = "GenericHttp")
	FString GenericHttpApiKeyEnvVar = TEXT("");
};
