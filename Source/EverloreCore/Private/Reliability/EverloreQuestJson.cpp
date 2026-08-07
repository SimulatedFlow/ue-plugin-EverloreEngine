// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Reliability/EverloreQuestJson.h"
#include "Reliability/EverloreEnumUtils.h"
#include "Model/EverloreQuestTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

namespace
{
	FString GetStr(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		FString V;
		O->TryGetStringField(Field, V);
		return V;
	}

	FName GetName(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const FString V = GetStr(O, Field);
		return V.IsEmpty() ? NAME_None : FName(*V);
	}

	int32 GetInt(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, int32 Default)
	{
		// Accept numbers or numeric strings; never throws.
		double Num = 0.0;
		if (O->TryGetNumberField(Field, Num))
		{
			return FMath::RoundToInt(Num);
		}
		FString S;
		if (O->TryGetStringField(Field, S) && S.IsNumeric())
		{
			return FCString::Atoi(*S);
		}
		return Default;
	}

	bool GetBool(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, bool Default)
	{
		bool B = Default;
		O->TryGetBoolField(Field, B);
		return B;
	}
}

bool FEverloreQuestJson::ParseQuestPayload(const FString& Json, FEverloreQuestPayload& Out)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	Out = FEverloreQuestPayload();
	Out.Type = EverloreEnum::Parse<EEverloreQuestType>(GetStr(Root, TEXT("type")), EEverloreQuestType::Fetch);
	Out.Title = FText::FromString(GetStr(Root, TEXT("title")));
	Out.Summary = FText::FromString(GetStr(Root, TEXT("summary")));
	// giverNpcId is injected by the engine, but accept it if present.
	Out.GiverNpcId = GetName(Root, TEXT("giverNpcId"));

	// objectives
	const TArray<TSharedPtr<FJsonValue>>* Objectives;
	if (Root->TryGetArrayField(TEXT("objectives"), Objectives))
	{
		for (const TSharedPtr<FJsonValue>& ObjVal : *Objectives)
		{
			const TSharedPtr<FJsonObject> ObjObj = ObjVal->AsObject();
			if (!ObjObj.IsValid())
			{
				continue;
			}
			FEverloreObjective Obj;
			Obj.ObjectiveId = GetName(ObjObj, TEXT("objectiveId"));
			Obj.Type = EverloreEnum::Parse<EEverloreObjectiveType>(GetStr(ObjObj, TEXT("type")), EEverloreObjectiveType::TalkTo);
			Obj.TargetId = GetName(ObjObj, TEXT("targetId"));
			Obj.RequiredCount = GetInt(ObjObj, TEXT("requiredCount"), 1);
			Obj.Description = FText::FromString(GetStr(ObjObj, TEXT("description")));
			Obj.bOptional = GetBool(ObjObj, TEXT("optional"), false);

			const TArray<TSharedPtr<FJsonValue>>* Prereqs;
			if (ObjObj->TryGetArrayField(TEXT("prerequisites"), Prereqs))
			{
				for (const TSharedPtr<FJsonValue>& PVal : *Prereqs)
				{
					const TSharedPtr<FJsonObject> PObj = PVal->AsObject();
					if (!PObj.IsValid())
					{
						continue;
					}
					FEverloreCondition Cond;
					Cond.Type = EverloreEnum::Parse<EEverloreConditionType>(GetStr(PObj, TEXT("type")), EEverloreConditionType::None);
					Cond.TargetId = GetName(PObj, TEXT("targetId"));
					Cond.Op = EverloreEnum::Parse<EEverloreComparison>(GetStr(PObj, TEXT("op")), EEverloreComparison::GreaterEqual);
					Cond.Value = GetInt(PObj, TEXT("value"), 0);
					Obj.Prerequisites.Add(Cond);
				}
			}
			Out.Objectives.Add(Obj);
		}
	}

	// rewards
	const TArray<TSharedPtr<FJsonValue>>* Rewards;
	if (Root->TryGetArrayField(TEXT("rewards"), Rewards))
	{
		for (const TSharedPtr<FJsonValue>& RewVal : *Rewards)
		{
			const TSharedPtr<FJsonObject> RewObj = RewVal->AsObject();
			if (!RewObj.IsValid())
			{
				continue;
			}
			FEverloreReward Reward;
			Reward.Type = EverloreEnum::Parse<EEverloreRewardType>(GetStr(RewObj, TEXT("type")), EEverloreRewardType::Gold);
			Reward.TargetId = GetName(RewObj, TEXT("targetId"));
			Reward.Amount = GetInt(RewObj, TEXT("amount"), 0);
			Out.Rewards.Add(Reward);
		}
	}

	// prerequisiteFlags
	const TArray<TSharedPtr<FJsonValue>>* Flags;
	if (Root->TryGetArrayField(TEXT("prerequisiteFlags"), Flags))
	{
		for (const TSharedPtr<FJsonValue>& FVal : *Flags)
		{
			FString F;
			if (FVal->TryGetString(F) && !F.IsEmpty())
			{
				Out.PrerequisiteFlags.Add(FName(*F));
			}
		}
	}

	return true;
}
