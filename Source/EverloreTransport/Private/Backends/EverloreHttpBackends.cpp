// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Backends/EverloreHttpBackends.h"
#include "EverloreSettings.h"

// ---- Ollama ----

FEverloreBackendCapabilities UEverloreOllamaBackend::GetCapabilities() const
{
	FEverloreBackendCapabilities Caps;
	Caps.bSupportsJsonSchema = true; // Ollama `format` structured outputs
	Caps.bRequiresSecret = false;
	Caps.bIsLocalInference = true;
	Caps.PreferredDialect = EEverloreSchemaDialect::OllamaFormat;
	Caps.MaxContextTokens = 8192;
	return Caps;
}

FString UEverloreOllamaBackend::GetEndpointUrl() const
{
	return GetDefault<UEverloreSettings>()->OllamaEndpoint / TEXT("api/chat");
}

FString UEverloreOllamaBackend::BuildRequestBody(const FEverloreGenerationRequest& Request) const
{
	return BuildOllamaBody(Request, GetDefault<UEverloreSettings>()->OllamaModel);
}

// ---- llama.cpp server (OpenAI-compatible) ----

FEverloreBackendCapabilities UEverloreLlamaServerBackend::GetCapabilities() const
{
	FEverloreBackendCapabilities Caps;
	Caps.bSupportsJsonSchema = true;
	Caps.bRequiresSecret = false;
	Caps.bIsLocalInference = true;
	Caps.PreferredDialect = EEverloreSchemaDialect::LlamaJsonSchema;
	Caps.MaxContextTokens = 4096;
	return Caps;
}

FString UEverloreLlamaServerBackend::GetEndpointUrl() const
{
	return GetDefault<UEverloreSettings>()->LlamaServerEndpoint / TEXT("v1/chat/completions");
}

FString UEverloreLlamaServerBackend::BuildRequestBody(const FEverloreGenerationRequest& Request) const
{
	return BuildOpenAiBody(Request, GetDefault<UEverloreSettings>()->LlamaServerModel);
}

// ---- Generic OpenAI-compatible HTTP ----

FEverloreBackendCapabilities UEverloreGenericHttpBackend::GetCapabilities() const
{
	FEverloreBackendCapabilities Caps;
	Caps.bSupportsJsonSchema = true;
	Caps.bRequiresSecret = RequiresSecret();
	// Derive locality from the endpoint (same live-settings pattern as bRequiresSecret) so a
	// local-default GenericHttp gets the concurrency-of-one profile instead of over-subscribing
	// a single-GPU server, while genuine cloud endpoints keep the higher cloud concurrency.
	const FString Endpoint = GetDefault<UEverloreSettings>()->GenericHttpEndpoint;
	Caps.bIsLocalInference = Endpoint.Contains(TEXT("localhost")) || Endpoint.Contains(TEXT("127.0.0.1")) || Endpoint.Contains(TEXT("[::1]"));
	Caps.PreferredDialect = EEverloreSchemaDialect::PortableSubset;
	Caps.MaxContextTokens = 0;
	return Caps;
}

FString UEverloreGenericHttpBackend::GetEndpointUrl() const
{
	return GetDefault<UEverloreSettings>()->GenericHttpEndpoint;
}

FString UEverloreGenericHttpBackend::BuildRequestBody(const FEverloreGenerationRequest& Request) const
{
	return BuildOpenAiBody(Request, GetDefault<UEverloreSettings>()->GenericHttpModel);
}

bool UEverloreGenericHttpBackend::RequiresSecret() const
{
	// Needs a key only if the developer named an env var to read one from.
	return !GetDefault<UEverloreSettings>()->GenericHttpApiKeyEnvVar.IsEmpty();
}
