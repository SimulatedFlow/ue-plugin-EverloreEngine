// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Backend/EverloreBackend.h"
#include "HttpFwd.h"
#include "EverloreHttpBackendBase.generated.h"

struct FEverloreGenerationResponse;

/**
 * Shared async-HTTP machinery for chat/completion backends that talk to an HTTP endpoint
 * (Ollama, llama.cpp server, any OpenAI-compatible service). Subclasses supply only the
 * URL, the request body and the response parsing; this base owns the non-blocking request
 * lifecycle, just-in-time (optional) secret resolution, cancellation and game-thread
 * completion — identical reliability to the Gemini backend. It TRANSPORTS bytes; it never
 * parses or validates a quest.
 */
UCLASS(Abstract)
class EVERLORETRANSPORT_API UEverloreHttpBackendBase : public UEverloreBackend
{
	GENERATED_BODY()

public:
	virtual void Probe(FEverloreProbeDelegate OnProbed) override;
	virtual FEverloreRequestHandle Generate(const FEverloreGenerationRequest& Request, FEverloreBackendCompletionDelegate OnComplete) override;
	virtual void Cancel(const FEverloreRequestHandle& Handle) override;

protected:
	/** Full URL to POST to, e.g. "http://localhost:11434/api/chat". */
	virtual FString GetEndpointUrl() const
		PURE_VIRTUAL(UEverloreHttpBackendBase::GetEndpointUrl, return FString(););

	/** Serialize the request into this backend's wire format. */
	virtual FString BuildRequestBody(const FEverloreGenerationRequest& Request) const
		PURE_VIRTUAL(UEverloreHttpBackendBase::BuildRequestBody, return FString(););

	/** Parse a 200 body: set Text / ExtractedJson / FinishReason, and Status=Success on success. */
	virtual void ParseSuccessBody(const FString& Body, FEverloreGenerationResponse& Out) const
		PURE_VIRTUAL(UEverloreHttpBackendBase::ParseSuccessBody, );

	/** Must a key be resolved before the request runs? Local backends return false. */
	virtual bool RequiresSecret() const { return false; }

	/** Header name carrying the key when one is present (default = OpenAI bearer scheme). */
	virtual FString AuthHeaderName() const { return TEXT("Authorization"); }
	virtual FString AuthHeaderValue(const FString& Key) const { return TEXT("Bearer ") + Key; }

	// ---- Shared wire-format helpers ----

	/** OpenAI-compatible /v1/chat/completions body (llama.cpp server, GenericHttp). */
	static FString BuildOpenAiBody(const FEverloreGenerationRequest& Request, const FString& Model);
	/** Parse OpenAI-compatible { choices:[{ message:{ content } }] }. */
	static void ParseOpenAiBody(const FString& Body, FEverloreGenerationResponse& Out);

	/** Ollama /api/chat body. */
	static FString BuildOllamaBody(const FEverloreGenerationRequest& Request, const FString& Model);
	/** Parse Ollama { message:{ content }, done }. */
	static void ParseOllamaBody(const FString& Body, FEverloreGenerationResponse& Out);

	/** Escape-aware extraction of the assistant text into ExtractedJson (shared by parsers). */
	static void FillExtractedJson(FEverloreGenerationResponse& Out);

private:
	void StartHttp(const FEverloreGenerationRequest& Request, const FString& ApiKey, int64 Id, FEverloreBackendCompletionDelegate OnComplete);
	void HandleHttp(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully,
		int64 Id, FEverloreBackendCompletionDelegate OnComplete);

	int64 NextId = 1;
	TMap<int64, FHttpRequestPtr> InFlight;

	/** Ids cancelled while their key was still resolving (no HTTP request yet). */
	TSet<int64> CancelledDuringResolve;
};
