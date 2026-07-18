// Copyright 2026 Simulated Flow All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/Class.h"

/** Reflection helpers for turning UENUMs into schema enums and parsing them back. */
namespace EverloreEnum
{
	/** Human enumerator names of a UENUM (skips the hidden _MAX entry). */
	template <typename TEnum>
	TArray<FString> Names()
	{
		TArray<FString> Out;
		if (const UEnum* E = StaticEnum<TEnum>())
		{
			for (int32 i = 0; i < E->NumEnums(); ++i)
			{
				const FString Name = E->GetNameStringByIndex(i);
				if (!Name.Contains(TEXT("_MAX")))
				{
					Out.Add(Name);
				}
			}
		}
		return Out;
	}

	/** Case-insensitive parse of an enumerator name; returns Default if unknown. */
	template <typename TEnum>
	TEnum Parse(const FString& S, TEnum Default)
	{
		if (const UEnum* E = StaticEnum<TEnum>())
		{
			if (!S.IsEmpty())
			{
				for (int32 i = 0; i < E->NumEnums(); ++i)
				{
					const FString Name = E->GetNameStringByIndex(i);
					if (Name.Contains(TEXT("_MAX")))
					{
						continue;
					}
					if (Name.Equals(S, ESearchCase::IgnoreCase))
					{
						return static_cast<TEnum>(E->GetValueByIndex(i));
					}
				}
			}
		}
		return Default;
	}
}
