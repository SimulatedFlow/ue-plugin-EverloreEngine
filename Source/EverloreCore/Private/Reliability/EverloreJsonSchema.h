// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Shared building blocks for the Everlore response JSON schemas.
//
// These were previously duplicated as identical helpers inside the anonymous
// namespace of several .cpp files (EverloreQuestSchema.cpp, EverloreChatSchema.cpp).
// Under Unreal's unity/jumbo build those files are concatenated into one translation
// unit, so the duplicated definitions collided ("Simple already has a body", C2084)
// and broke the whole plugin build. Defining them ONCE here as inline functions in a
// named namespace gives exactly one definition and is unity-build safe.
namespace Everlore::Json
{
	/** { "type": <Type> } */
	inline TSharedRef<FJsonObject> Simple(const FString& Type)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("type"), Type);
		return O;
	}

	/** { "type": "string", "enum": [<Values...>] } */
	inline TSharedRef<FJsonObject> EnumString(const TArray<FString>& Values)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("type"), TEXT("string"));
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& V : Values)
		{
			Arr.Add(MakeShared<FJsonValueString>(V));
		}
		O->SetArrayField(TEXT("enum"), Arr);
		return O;
	}

	/** { "type": "array", "items": <Items> } */
	inline TSharedRef<FJsonObject> ArrayOf(TSharedRef<FJsonObject> Items)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("type"), TEXT("array"));
		O->SetObjectField(TEXT("items"), Items);
		return O;
	}

	/** Sets the JSON-schema "required": [<Names...>] array on O. */
	inline void SetRequired(TSharedRef<FJsonObject> O, const TArray<FString>& Names)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& N : Names)
		{
			Arr.Add(MakeShared<FJsonValueString>(N));
		}
		O->SetArrayField(TEXT("required"), Arr);
	}
}
