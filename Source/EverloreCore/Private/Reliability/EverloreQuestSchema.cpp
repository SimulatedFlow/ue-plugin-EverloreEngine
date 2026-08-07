// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Reliability/EverloreQuestSchema.h"
#include "Reliability/EverloreEnumUtils.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Model/EverloreQuestTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "EverloreJsonSchema.h" // shared Simple/EnumString/ArrayOf/SetRequired (unity-build safe)

namespace
{
	FString JoinIds(const TSet<FName>& Ids, int32 Max = 40)
	{
		TArray<FString> Names;
		for (const FName& Id : Ids)
		{
			Names.Add(Id.ToString());
			if (Names.Num() >= Max)
			{
				break;
			}
		}
		return FString::Join(Names, TEXT(", "));
	}
}

using namespace Everlore::Json;

FString FEverloreQuestSchema::BuildResponseSchema()
{
	// objective item
	TSharedRef<FJsonObject> ObjProps = MakeShared<FJsonObject>();
	ObjProps->SetObjectField(TEXT("type"), EnumString(EverloreEnum::Names<EEverloreObjectiveType>()));
	ObjProps->SetObjectField(TEXT("targetId"), Simple(TEXT("string")));
	ObjProps->SetObjectField(TEXT("requiredCount"), Simple(TEXT("integer")));
	ObjProps->SetObjectField(TEXT("description"), Simple(TEXT("string")));
	ObjProps->SetObjectField(TEXT("optional"), Simple(TEXT("boolean")));
	TSharedRef<FJsonObject> ObjItem = MakeShared<FJsonObject>();
	ObjItem->SetStringField(TEXT("type"), TEXT("object"));
	ObjItem->SetObjectField(TEXT("properties"), ObjProps);
	SetRequired(ObjItem, { TEXT("type"), TEXT("targetId"), TEXT("requiredCount") });

	// reward item
	TSharedRef<FJsonObject> RewProps = MakeShared<FJsonObject>();
	RewProps->SetObjectField(TEXT("type"), EnumString(EverloreEnum::Names<EEverloreRewardType>()));
	RewProps->SetObjectField(TEXT("targetId"), Simple(TEXT("string")));
	RewProps->SetObjectField(TEXT("amount"), Simple(TEXT("integer")));
	TSharedRef<FJsonObject> RewItem = MakeShared<FJsonObject>();
	RewItem->SetStringField(TEXT("type"), TEXT("object"));
	RewItem->SetObjectField(TEXT("properties"), RewProps);
	SetRequired(RewItem, { TEXT("type"), TEXT("amount") });

	// root
	TSharedRef<FJsonObject> RootProps = MakeShared<FJsonObject>();
	RootProps->SetObjectField(TEXT("type"), EnumString(EverloreEnum::Names<EEverloreQuestType>()));
	RootProps->SetObjectField(TEXT("title"), Simple(TEXT("string")));
	RootProps->SetObjectField(TEXT("summary"), Simple(TEXT("string")));
	RootProps->SetObjectField(TEXT("objectives"), ArrayOf(ObjItem));
	RootProps->SetObjectField(TEXT("rewards"), ArrayOf(RewItem));
	RootProps->SetObjectField(TEXT("prerequisiteFlags"), ArrayOf(Simple(TEXT("string"))));

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("type"), TEXT("object"));
	Root->SetObjectField(TEXT("properties"), RootProps);
	SetRequired(Root, { TEXT("type"), TEXT("title"), TEXT("objectives") });

	// Encourage a natural field order.
	TArray<TSharedPtr<FJsonValue>> Ordering;
	for (const TCHAR* Key : { TEXT("type"), TEXT("title"), TEXT("summary"), TEXT("objectives"), TEXT("rewards"), TEXT("prerequisiteFlags") })
	{
		Ordering.Add(MakeShared<FJsonValueString>(Key));
	}
	Root->SetArrayField(TEXT("propertyOrdering"), Ordering);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

FString FEverloreQuestSchema::SystemPrompt()
{
	return TEXT(
		"You are a quest designer for a video game. Produce exactly one quest as JSON that "
		"matches the provided schema. Use ONLY ids from the allowed lists in the user message. "
		"Objective targets must match their type: Kill/TalkTo/Escort/Deliver target an NPC, "
		"Collect targets an item, ReachLocation targets a region. Keep rewards modest. "
		"Do not invent ids that are not listed. Output JSON only.");
}

FString FEverloreQuestSchema::BuildPrompt(const FEverloreGuardrailSnapshot& Guardrails, FName GiverNpcId, const FString& Theme)
{
	FString Prompt;
	Prompt += FString::Printf(TEXT("The quest giver is the NPC '%s'.\n"), *GiverNpcId.ToString());
	if (!Theme.IsEmpty())
	{
		Prompt += FString::Printf(TEXT("Theme / hint: %s\n"), *Theme);
	}
	Prompt += TEXT("\nAllowed ids you may reference:\n");
	Prompt += FString::Printf(TEXT("- NPCs: %s\n"), *JoinIds(Guardrails.Npcs));
	Prompt += FString::Printf(TEXT("- Items: %s\n"), *JoinIds(Guardrails.Items));
	Prompt += FString::Printf(TEXT("- Regions: %s\n"), *JoinIds(Guardrails.Regions));
	if (Guardrails.Flags.Num() > 0)
	{
		Prompt += FString::Printf(TEXT("- Story flags: %s\n"), *JoinIds(Guardrails.Flags));
	}
	Prompt += FString::Printf(TEXT("\nReward limits: gold <= %d, xp <= %d, item qty <= %d, at most %d objectives.\n"),
		Guardrails.MaxGold, Guardrails.MaxExperience, Guardrails.MaxItemQuantity, Guardrails.MaxObjectivesPerQuest);
	Prompt += TEXT("Create one coherent, completable quest now.");
	return Prompt;
}
