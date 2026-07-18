// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FEverloreQuestPayload;

/**
 * Hand-rolled, defensive JSON -> quest payload parsing. We control every field mapping so
 * that malformed / hallucinated / adversarial output can never crash us: we use only
 * bool-returning JSON APIs and tolerate missing/extra fields (defaults fill the gaps).
 */
class EVERLORECORE_API FEverloreQuestJson
{
public:
	/** Returns false only if the string is not a JSON object at all. */
	static bool ParseQuestPayload(const FString& Json, FEverloreQuestPayload& OutPayload);
};
