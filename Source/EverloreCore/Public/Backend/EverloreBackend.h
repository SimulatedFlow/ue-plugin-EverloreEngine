// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Backend/EverloreBackendTypes.h"
#include "Backend/EverloreSecretProvider.h"
#include "EverloreBackend.generated.h"

/**
 * Abstract base for a swappable LLM backend.
 *
 * A backend TRANSPORTS bytes and applies constraints; it does NOT parse or validate
 * quests. Parse / validate / repair / fallback all live in the Core pipeline, so the
 * reliability guarantee is identical across every backend.
 *
 * Concrete backends live in other modules (EverloreTransport, EverloreLlamaLocal) and are
 * discovered by the registry via reflection — adding one is purely additive, with no edit
 * to Core.
 */
UCLASS(Abstract)
class EVERLORECORE_API UEverloreBackend : public UObject
{
	GENERATED_BODY()

public:
	/** Stable identifier, e.g. "Gemini", "Ollama", "LlamaCppServer", "LlamaLocal", "GenericHttp". */
	virtual FName GetBackendId() const
		PURE_VIRTUAL(UEverloreBackend::GetBackendId, return NAME_None;);

	/** Static capabilities. May be refined by Probe(). */
	virtual FEverloreBackendCapabilities GetCapabilities() const
		PURE_VIRTUAL(UEverloreBackend::GetCapabilities, return FEverloreBackendCapabilities(););

	/** Non-blocking reachability / version probe; refines capabilities. */
	virtual void Probe(FEverloreProbeDelegate OnProbed)
		PURE_VIRTUAL(UEverloreBackend::Probe, OnProbed.ExecuteIfBound(false, GetCapabilities()););

	/** THE call. Non-blocking; result is delivered on the game thread. */
	virtual FEverloreRequestHandle Generate(const FEverloreGenerationRequest& Request, FEverloreBackendCompletionDelegate OnComplete)
		PURE_VIRTUAL(UEverloreBackend::Generate, return FEverloreRequestHandle(););

	/** Optional streaming variant; backends without streaming may ignore OnDelta. */
	virtual FEverloreRequestHandle GenerateStreaming(const FEverloreGenerationRequest& Request,
		FEverloreBackendStreamDelegate OnDelta, FEverloreBackendCompletionDelegate OnComplete)
	{
		// Default: fall back to a buffered, non-streaming request.
		return Generate(Request, OnComplete);
	}

	/** Cancel an in-flight request. Safe to call after completion. */
	virtual void Cancel(const FEverloreRequestHandle& Handle)
		PURE_VIRTUAL(UEverloreBackend::Cancel, );

	/** Injected by the subsystem. Resolves API keys just-in-time; never cooked. */
	UPROPERTY(Transient)
	TScriptInterface<IEverloreSecretProvider> SecretProvider;
};
