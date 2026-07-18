// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "Backend/EverloreBackendTypes.h"
#include "Templates/SubclassOf.h"    // full TSubclassOf definition (Casts.h only forward-declares it) — the TMap<..., TSubclassOf<UEverloreBackend>> below needs it under non-unity / installed builds
#include "Backend/EverloreBackend.h"
#include "Reliability/EverlorePipelineTypes.h"
#include "EverloreBackendSubsystem.generated.h"

class UEverloreDefaultSecretProvider;
struct FEverloreGuardrailSnapshot;

/**
 * Process-wide brain. Exists in the editor, PIE, packaged game, dedicated server and
 * commandlets. Owns:
 *   - the backend REGISTRY (reflection-discovered UEverloreBackend subclasses, lazy),
 *   - the instantiated + GC-rooted backend objects,
 *   - the default secret provider,
 *   - a concurrency-capped request SCHEDULER.
 *
 * It is deliberately NOT a component: the editor-batch / commandlet path has no world
 * or actor, and there must be exactly one instance per process. The gameplay-facing
 * components (Phase d+) are the front door and delegate here.
 *
 * Phase (b): raw request routing (SubmitRaw). The validate->repair->fallback pipeline
 * wraps this in Phase (c)/(d).
 */
UCLASS()
class EVERLORECORE_API UEverloreBackendSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	/** Convenience accessor. */
	static UEverloreBackendSubsystem* Get();

	// ---- Registry ----

	/** Ids of all backends discovered via reflection. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend")
	TArray<FName> ListAvailableBackends();

	/** Select the active backend by id. Returns false if unknown. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend")
	bool SetActiveBackend(FName BackendId);

	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend")
	FName GetActiveBackendId() const { return ActiveBackendId; }

	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend")
	bool IsActiveBackendReady();

	/** Get (or lazily create) a backend instance by id. May return nullptr. */
	UEverloreBackend* GetOrCreateBackend(FName BackendId);

	/** The currently-active backend instance, or nullptr. */
	UEverloreBackend* GetActiveBackend();

	UEverloreDefaultSecretProvider* GetSecretProvider() const { return SecretProvider; }

	// ---- API keys / BYOK (Blueprint-callable so a BP-only game can wire a key UI) ----

	/** Player enters THEIR own key at runtime. Stored locally in memory only — never cooked or shipped. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend|Keys")
	void SetPlayerApiKey(FName BackendId, const FString& ApiKey);

	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend|Keys")
	void ClearPlayerApiKey(FName BackendId);

	UFUNCTION(BlueprintPure, Category = "Everlore|Backend|Keys")
	bool HasPlayerApiKey(FName BackendId) const;

	/**
	 * Developer pushes a key at runtime from their OWN secure store (highest precedence).
	 * Use server-side only — never include a developer key in a shipped CLIENT build.
	 */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend|Keys")
	void SetDeveloperApiKey(FName BackendId, const FString& ApiKey);

	/** True if the active backend needs no key, or a key resolves for it. Gate generation on this. */
	UFUNCTION(BlueprintCallable, Category = "Everlore|Backend|Keys")
	bool HasKeyForActiveBackend();

	// ---- Raw scheduling (Phase b) ----

	/** Queue a raw generation request against the active backend. Non-blocking. */
	FEverloreRequestHandle SubmitRaw(const FEverloreGenerationRequest& Request, FEverloreBackendCompletionDelegate OnComplete);

	/**
	 * Streaming variant: OnDelta fires per chunk on the game thread (bDone=true on the last),
	 * OnComplete fires once with the full buffered result. Backends without streaming fall back
	 * to a single delta at completion (base UEverloreBackend::GenerateStreaming), so callers can
	 * always wire OnDelta without checking capabilities first.
	 */
	FEverloreRequestHandle SubmitRawStreaming(const FEverloreGenerationRequest& Request,
		FEverloreBackendStreamDelegate OnDelta, FEverloreBackendCompletionDelegate OnComplete);

	/** Cancel a queued or in-flight raw request. */
	void CancelRaw(const FEverloreRequestHandle& Handle);

	// ---- Quest pipeline ----

	/** Generate a guaranteed-valid quest through the full reliability pipeline. */
	void GenerateQuest(const FEverloreQuestGenContext& Ctx, const FEverloreGuardrailSnapshot& Snapshot, FEverloreQuestReadyDelegate OnReady);

private:
	/** Mirror the secret env-var NAMES from live settings so a mid-session config edit takes effect. */
	void SyncSecretConfig();
	void RebuildRegistry();
	void ConsoleTestQuest(const TArray<FString>& Args);
	void ConsoleTestChat(const TArray<FString>& Args);
	void ConsoleListBackends(const TArray<FString>& Args);
	void ConsoleSetBackend(const TArray<FString>& Args);
	void ConsoleSetKey(const TArray<FString>& Args);
	void PumpQueue();
	int32 GetMaxConcurrency() const;
	void HandleRawComplete(const FEverloreGenerationResponse& Response, int64 OurId);

	struct FPendingRequest
	{
		int64 Id = 0;
		FEverloreGenerationRequest Request;
		FEverloreBackendCompletionDelegate OnComplete;
		FEverloreBackendStreamDelegate OnDelta;
		bool bStreaming = false;
	};

	struct FInFlightRequest
	{
		FEverloreBackendCompletionDelegate OnComplete;
		TWeakObjectPtr<UEverloreBackend> Backend;
		FEverloreRequestHandle BackendHandle;
	};

	UPROPERTY(Transient)
	TObjectPtr<UEverloreDefaultSecretProvider> SecretProvider;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UEverloreBackend>> BackendInstances;

	/** Reflection-discovered classes keyed by backend id. */
	TMap<FName, TSubclassOf<UEverloreBackend>> BackendClasses;

	bool bRegistryBuilt = false;
	FName ActiveBackendId = NAME_None;

	TArray<FPendingRequest> Queue;
	TMap<int64, FInFlightRequest> InFlight;
	int64 NextRequestId = 1;
};
