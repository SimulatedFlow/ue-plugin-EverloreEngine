// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EverloreSecretProvider.generated.h"

/** Where a resolved secret came from (for logging and the client/server key policy). */
UENUM(BlueprintType)
enum class EEverloreSecretSource : uint8
{
	None                UMETA(DisplayName = "None"),
	RuntimeSet          UMETA(DisplayName = "Runtime (developer)"),
	PlayerProvided      UMETA(DisplayName = "Player-provided (BYOK)"),
	EnvironmentVariable UMETA(DisplayName = "Environment variable (server)"),
	EditorTest          UMETA(DisplayName = "Editor test field")
};

/** Result of an async secret resolution. */
DECLARE_DELEGATE_ThreeParams(FEverloreSecretResultDelegate, bool /*bFound*/, const FString& /*Secret*/, EEverloreSecretSource /*Source*/);

UINTERFACE(MinimalAPI, BlueprintType)
class UEverloreSecretProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Resolves the API key for a backend without ever cooking or shipping it.
 * Backends receive a provider and ask for their key just-in-time.
 */
class IEverloreSecretProvider
{
	GENERATED_BODY()

public:
	virtual void ResolveSecret(FName BackendId, FEverloreSecretResultDelegate OnResolved) = 0;
};

/**
 * Default provider. Resolves a backend's key in strict precedence order:
 *   1. Runtime-set (developer pushes it from their own secure store)
 *   2. Player-provided (BYOK — the end player entered their own key at runtime)
 *   3. Environment variable (the dedicated-server path; key lives only in the server process)
 *   4. Editor test field (transient, never cooked)
 *
 * No key is ever a cooked UPROPERTY(Config) value. The player-provided key is held in
 * memory here; optional local persistence (the player's own machine only) is layered on
 * top in Phase (b) via a USaveGame — it is never included in the shipped build.
 */
UCLASS()
class EVERLORECORE_API UEverloreDefaultSecretProvider : public UObject, public IEverloreSecretProvider
{
	GENERATED_BODY()

public:
	/** (1) Developer sets a key at runtime from their own secure store. Highest precedence. */
	void SetRuntimeSecret(FName BackendId, const FString& Secret);
	void ClearRuntimeSecret(FName BackendId);

	/** (2) Player-BYOK: shipped game calls this when the player enters their own key. */
	void SetPlayerProvidedSecret(FName BackendId, const FString& Secret);
	void ClearPlayerProvidedSecret(FName BackendId);
	bool HasPlayerProvidedSecret(FName BackendId) const;

	/** (3) Name of the environment variable to consult (e.g. "EVERLORE_GEMINI_KEY"). */
	void SetEnvVarName(FName BackendId, const FString& EnvVarName);

	/** (4) Editor-only test key. Compiled out of shipping builds. */
	void SetEditorTestSecret(FName BackendId, const FString& Secret);

	/** Synchronous best-effort: does a key resolve for this backend right now (any tier)? */
	bool HasSecret(FName BackendId) const;

	//~ Begin IEverloreSecretProvider
	virtual void ResolveSecret(FName BackendId, FEverloreSecretResultDelegate OnResolved) override;
	//~ End IEverloreSecretProvider

private:
	TMap<FName, FString> RuntimeSecrets;
	TMap<FName, FString> PlayerSecrets;
	TMap<FName, FString> EnvVarNames;
#if WITH_EDITOR
	TMap<FName, FString> EditorTestSecrets;
#endif
};
