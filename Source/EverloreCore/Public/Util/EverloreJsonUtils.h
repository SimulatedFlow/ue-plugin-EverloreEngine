// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Small JSON helpers shared across the plugin. */
class EVERLORECORE_API FEverloreJsonUtils
{
public:
	/**
	 * Extract the first balanced top-level JSON object ({...}) from arbitrary text,
	 * respecting string literals and escapes so a brace inside a string never
	 * mis-terminates the scan. Returns an empty string if none is found.
	 * Used for unconstrained backends and for repair-on-garbage; constrained
	 * backends usually return pure JSON already.
	 */
	static FString ExtractFirstJsonObject(const FString& In);
};
