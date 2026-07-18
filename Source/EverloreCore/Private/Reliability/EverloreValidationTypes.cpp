// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Reliability/EverloreValidationTypes.h"

void FEverloreValidationReport::Add(EEverloreSeverity Severity, const FString& Path, FName Code, const FString& Message, const FString& Suggestion)
{
	FEverloreValidationIssue Issue;
	Issue.Severity = Severity;
	Issue.Path = Path;
	Issue.Code = Code;
	Issue.Message = Message;
	Issue.Suggestion = Suggestion;
	Issues.Add(MoveTemp(Issue));
}

int32 FEverloreValidationReport::NumErrors() const
{
	int32 Count = 0;
	for (const FEverloreValidationIssue& Issue : Issues)
	{
		if (Issue.Severity == EEverloreSeverity::Error)
		{
			++Count;
		}
	}
	return Count;
}

int32 FEverloreValidationReport::NumAutoFixable() const
{
	int32 Count = 0;
	for (const FEverloreValidationIssue& Issue : Issues)
	{
		if (Issue.Severity == EEverloreSeverity::AutoFixable)
		{
			++Count;
		}
	}
	return Count;
}

FString FEverloreValidationReport::ToPromptString() const
{
	FString Out;
	for (const FEverloreValidationIssue& Issue : Issues)
	{
		if (Issue.Severity == EEverloreSeverity::Warning)
		{
			continue; // repair only needs errors + auto-fixables it couldn't resolve
		}
		Out += FString::Printf(TEXT("- %s: %s"), *Issue.Path, *Issue.Message);
		if (!Issue.Suggestion.IsEmpty())
		{
			Out += FString::Printf(TEXT(" (%s)"), *Issue.Suggestion);
		}
		Out += TEXT("\n");
	}
	return Out;
}
