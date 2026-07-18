// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Backends/EverloreHttpBackendBase.h"
#include "EverloreHttpBackends.generated.h"

/**
 * Ollama backend (local, no key). Talks to /api/chat with `format` structured outputs.
 * Configure the endpoint + model in Project Settings > Everlore Engine > Ollama.
 */
UCLASS()
class EVERLORETRANSPORT_API UEverloreOllamaBackend : public UEverloreHttpBackendBase
{
	GENERATED_BODY()

public:
	virtual FName GetBackendId() const override { return TEXT("Ollama"); }
	virtual FEverloreBackendCapabilities GetCapabilities() const override;

protected:
	virtual FString GetEndpointUrl() const override;
	virtual FString BuildRequestBody(const FEverloreGenerationRequest& Request) const override;
	virtual void ParseSuccessBody(const FString& Body, FEverloreGenerationResponse& Out) const override { ParseOllamaBody(Body, Out); }
	virtual bool RequiresSecret() const override { return false; }
};

/**
 * llama.cpp server backend (local, no key). Talks to the OpenAI-compatible
 * /v1/chat/completions endpoint. Configure it under Everlore Engine > LlamaServer.
 */
UCLASS()
class EVERLORETRANSPORT_API UEverloreLlamaServerBackend : public UEverloreHttpBackendBase
{
	GENERATED_BODY()

public:
	virtual FName GetBackendId() const override { return TEXT("LlamaServer"); }
	virtual FEverloreBackendCapabilities GetCapabilities() const override;

protected:
	virtual FString GetEndpointUrl() const override;
	virtual FString BuildRequestBody(const FEverloreGenerationRequest& Request) const override;
	virtual void ParseSuccessBody(const FString& Body, FEverloreGenerationResponse& Out) const override { ParseOpenAiBody(Body, Out); }
	virtual bool RequiresSecret() const override { return false; }
};

/**
 * Generic OpenAI-compatible HTTP backend (OpenRouter, LocalAI, vLLM, Together, …).
 * Point it at any /v1/chat/completions URL under Everlore Engine > GenericHttp; if that
 * endpoint needs a key, set the env-var NAME to read it from — the key itself is never cooked.
 */
UCLASS()
class EVERLORETRANSPORT_API UEverloreGenericHttpBackend : public UEverloreHttpBackendBase
{
	GENERATED_BODY()

public:
	virtual FName GetBackendId() const override { return TEXT("GenericHttp"); }
	virtual FEverloreBackendCapabilities GetCapabilities() const override;

protected:
	virtual FString GetEndpointUrl() const override;
	virtual FString BuildRequestBody(const FEverloreGenerationRequest& Request) const override;
	virtual void ParseSuccessBody(const FString& Body, FEverloreGenerationResponse& Out) const override { ParseOpenAiBody(Body, Out); }
	virtual bool RequiresSecret() const override;
};
