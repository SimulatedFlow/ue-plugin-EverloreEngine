// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Backend/EverloreBackend.h"
#include "HttpFwd.h"
#include "EverloreGeminiBackend.generated.h"

/**
 * Google Gemini backend (first reference / test path).
 * Uses generateContent with responseMimeType=application/json + responseSchema for
 * structured output. Async HTTP; completion is delivered on the game thread.
 * Requires an API key (never hardcoded) resolved via the injected secret provider.
 */
UCLASS()
class EVERLORETRANSPORT_API UEverloreGeminiBackend : public UEverloreBackend
{
	GENERATED_BODY()

public:
	virtual FName GetBackendId() const override { return TEXT("Gemini"); }
	virtual FEverloreBackendCapabilities GetCapabilities() const override;
	virtual void Probe(FEverloreProbeDelegate OnProbed) override;
	virtual FEverloreRequestHandle Generate(const FEverloreGenerationRequest& Request, FEverloreBackendCompletionDelegate OnComplete) override;
	virtual void Cancel(const FEverloreRequestHandle& Handle) override;

private:
	FString BuildRequestBody(const FEverloreGenerationRequest& Request) const;

	void StartHttp(const FEverloreGenerationRequest& Request, const FString& ApiKey, int64 Id, FEverloreBackendCompletionDelegate OnComplete);

	void HandleHttp(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully,
		int64 Id, FEverloreBackendCompletionDelegate OnComplete);

	int64 NextId = 1;
	TMap<int64, FHttpRequestPtr> InFlight;

	/** Ids cancelled while their API key was still resolving (no HTTP request yet). */
	TSet<int64> CancelledDuringResolve;
};
