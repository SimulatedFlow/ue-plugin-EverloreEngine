// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Backend/EverloreSecretProvider.h"
#include "EverloreLog.h"
#include "HAL/PlatformMisc.h"

void UEverloreDefaultSecretProvider::SetRuntimeSecret(FName BackendId, const FString& Secret)
{
	RuntimeSecrets.Add(BackendId, Secret);
}

void UEverloreDefaultSecretProvider::ClearRuntimeSecret(FName BackendId)
{
	RuntimeSecrets.Remove(BackendId);
}

void UEverloreDefaultSecretProvider::SetPlayerProvidedSecret(FName BackendId, const FString& Secret)
{
	PlayerSecrets.Add(BackendId, Secret);
}

void UEverloreDefaultSecretProvider::ClearPlayerProvidedSecret(FName BackendId)
{
	PlayerSecrets.Remove(BackendId);
}

bool UEverloreDefaultSecretProvider::HasPlayerProvidedSecret(FName BackendId) const
{
	return PlayerSecrets.Contains(BackendId);
}

void UEverloreDefaultSecretProvider::SetEnvVarName(FName BackendId, const FString& EnvVarName)
{
	EnvVarNames.Add(BackendId, EnvVarName);
}

void UEverloreDefaultSecretProvider::SetEditorTestSecret(FName BackendId, const FString& Secret)
{
#if WITH_EDITOR
	EditorTestSecrets.Add(BackendId, Secret);
#endif
}

bool UEverloreDefaultSecretProvider::HasSecret(FName BackendId) const
{
	if (RuntimeSecrets.Contains(BackendId) || PlayerSecrets.Contains(BackendId))
	{
		return true;
	}
	if (const FString* EnvName = EnvVarNames.Find(BackendId))
	{
		if (!EnvName->IsEmpty() && !FPlatformMisc::GetEnvironmentVariable(**EnvName).IsEmpty())
		{
			return true;
		}
	}
#if WITH_EDITOR
	if (EditorTestSecrets.Contains(BackendId))
	{
		return true;
	}
#endif
	return false;
}

void UEverloreDefaultSecretProvider::ResolveSecret(FName BackendId, FEverloreSecretResultDelegate OnResolved)
{
	// 1. Runtime-set (developer).
	if (const FString* Runtime = RuntimeSecrets.Find(BackendId))
	{
		OnResolved.ExecuteIfBound(true, *Runtime, EEverloreSecretSource::RuntimeSet);
		return;
	}

	// 2. Player-provided (BYOK).
	if (const FString* Player = PlayerSecrets.Find(BackendId))
	{
		OnResolved.ExecuteIfBound(true, *Player, EEverloreSecretSource::PlayerProvided);
		return;
	}

	// 3. Environment variable (dedicated-server path).
	if (const FString* EnvName = EnvVarNames.Find(BackendId))
	{
		const FString Value = FPlatformMisc::GetEnvironmentVariable(**EnvName);
		if (!Value.IsEmpty())
		{
			OnResolved.ExecuteIfBound(true, Value, EEverloreSecretSource::EnvironmentVariable);
			return;
		}
	}

#if WITH_EDITOR
	// 4. Editor-only test field.
	if (const FString* EditorKey = EditorTestSecrets.Find(BackendId))
	{
		OnResolved.ExecuteIfBound(true, *EditorKey, EEverloreSecretSource::EditorTest);
		return;
	}
#endif

	UE_LOG(LogEverlore, Verbose, TEXT("No secret found for backend '%s'."), *BackendId.ToString());
	OnResolved.ExecuteIfBound(false, FString(), EEverloreSecretSource::None);
}
