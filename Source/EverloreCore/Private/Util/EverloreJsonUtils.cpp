// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Util/EverloreJsonUtils.h"

FString FEverloreJsonUtils::ExtractFirstJsonObject(const FString& In)
{
	int32 Depth = 0;
	int32 StartIdx = INDEX_NONE;
	bool bInString = false;
	bool bEscaped = false;

	for (int32 i = 0; i < In.Len(); ++i)
	{
		const TCHAR C = In[i];

		if (bInString)
		{
			if (bEscaped)
			{
				bEscaped = false;
			}
			else if (C == TEXT('\\'))
			{
				bEscaped = true;
			}
			else if (C == TEXT('"'))
			{
				bInString = false;
			}
			continue;
		}

		if (C == TEXT('"'))
		{
			bInString = true;
		}
		else if (C == TEXT('{'))
		{
			if (Depth == 0)
			{
				StartIdx = i;
			}
			++Depth;
		}
		else if (C == TEXT('}'))
		{
			if (Depth > 0)
			{
				--Depth;
				if (Depth == 0 && StartIdx != INDEX_NONE)
				{
					return In.Mid(StartIdx, i - StartIdx + 1);
				}
			}
		}
	}

	return FString();
}
