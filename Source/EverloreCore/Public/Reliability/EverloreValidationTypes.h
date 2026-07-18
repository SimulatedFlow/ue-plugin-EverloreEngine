// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EverloreValidationTypes.generated.h"

/** How bad a single validation issue is, and how the repair stage should treat it. */
UENUM(BlueprintType)
enum class EEverloreSeverity : uint8
{
	/** Can be fixed locally without the LLM (clamp / drop / truncate / dedupe / coerce). */
	AutoFixable UMETA(DisplayName = "Auto-Fixable"),
	/** Non-fatal; the content is usable but imperfect. */
	Warning     UMETA(DisplayName = "Warning"),
	/** Must be repaired by the LLM or the quest falls back to a template. */
	Error       UMETA(DisplayName = "Error")
};

/** One problem found during validation. */
USTRUCT(BlueprintType)
struct FEverloreValidationIssue
{
	GENERATED_BODY()

	/** JSON-ish path to the offending field, e.g. "objectives[2].targetId". */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Validation")
	FString Path;

	/** Machine code, e.g. "L2.UnknownNpc". */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Validation")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Validation")
	EEverloreSeverity Severity = EEverloreSeverity::Error;

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Validation")
	FString Message;

	/** Optional hint used both for auto-fix and for the LLM repair prompt. */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Validation")
	FString Suggestion;
};

/** The full result of validating one payload. */
USTRUCT(BlueprintType)
struct FEverloreValidationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Validation")
	TArray<FEverloreValidationIssue> Issues;

	void Add(EEverloreSeverity Severity, const FString& Path, FName Code, const FString& Message, const FString& Suggestion = FString());

	int32 NumErrors() const;
	int32 NumAutoFixable() const;

	bool HasErrors() const { return NumErrors() > 0; }
	bool IsClean() const { return Issues.Num() == 0; }

	/** Structured, imperative feedback fed back to the LLM during a repair pass. */
	FString ToPromptString() const;
};
