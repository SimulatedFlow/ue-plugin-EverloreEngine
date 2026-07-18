// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Reliability/EverloreChatSchema.h"
#include "Model/EverloreChatTypes.h"
#include "Model/EverloreDialogueTypes.h" // FEverloreCharacterContext
#include "Backend/EverloreBackendTypes.h" // FEverloreGenerationResponse
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

namespace
{
	/** The canonical wire strings for the intent enum — must match the schema's enum values. */
	const TCHAR* IntentToString(EEverloreChatIntentType Type)
	{
		switch (Type)
		{
		case EEverloreChatIntentType::OfferQuest:      return TEXT("offer_quest");
		case EEverloreChatIntentType::RememberFact:    return TEXT("remember_fact");
		case EEverloreChatIntentType::SetDisposition:  return TEXT("set_disposition");
		case EEverloreChatIntentType::EndConversation: return TEXT("end_conversation");
		default:                                       return TEXT("none");
		}
	}

	EEverloreChatIntentType IntentFromString(const FString& In)
	{
		const FString S = In.ToLower();
		if (S == TEXT("offer_quest"))      return EEverloreChatIntentType::OfferQuest;
		if (S == TEXT("remember_fact"))    return EEverloreChatIntentType::RememberFact;
		if (S == TEXT("set_disposition"))  return EEverloreChatIntentType::SetDisposition;
		if (S == TEXT("end_conversation")) return EEverloreChatIntentType::EndConversation;
		return EEverloreChatIntentType::None;
	}

	TSharedRef<FJsonObject> Simple(const FString& Type)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("type"), Type);
		return O;
	}
}

FString FEverloreChatSchema::SystemPrompt(const FEverloreCharacterContext& C, bool bIntentsEnabled)
{
	const FString Name = C.DisplayName.IsEmpty() ? C.CharacterId.ToString() : C.DisplayName.ToString();

	FString P;
	P += FString::Printf(TEXT("You are %s, a character in a video game. Stay fully in character. Never break the fourth wall, mention being an AI, or describe your own actions in stage directions.\n"),
		Name.IsEmpty() ? TEXT("an NPC") : *Name);

	if (!C.Persona.IsEmpty())
	{
		P += FString::Printf(TEXT("Persona: %s\n"), *C.Persona.ToString());
	}
	if (!C.Disposition.IsEmpty())
	{
		P += FString::Printf(TEXT("Your current disposition toward the player: %s\n"), *C.Disposition.ToString());
	}
	if (C.KnownFacts.Num() > 0)
	{
		TArray<FString> Facts;
		for (const FName& F : C.KnownFacts)
		{
			Facts.Add(F.ToString());
		}
		P += FString::Printf(TEXT("Established facts you know: %s\n"), *FString::Join(Facts, TEXT(", ")));
	}
	if (!C.MemoryBlob.IsEmpty())
	{
		P += FString::Printf(TEXT("What you remember from earlier:\n%s\n"), *C.MemoryBlob);
	}

	P += TEXT("Reply in first person, in character, concise and natural (1-4 sentences).\n");

	if (bIntentsEnabled)
	{
		P += TEXT(
			"Respond ONLY as JSON matching the provided schema. Put your spoken line in \"reply\". "
			"Set \"intent\" to exactly one of: none, offer_quest, remember_fact, set_disposition, end_conversation. "
			"Use \"none\" unless the conversation clearly warrants that effect. When it does: for offer_quest put a "
			"one-line quest theme in \"intent_argument\"; for remember_fact put the single fact to remember; for "
			"set_disposition put your new attitude toward the player. Never invent intents outside that list.");
	}
	else
	{
		P += TEXT("Reply with plain spoken text only — no JSON, no narration tags.");
	}
	return P;
}

FString FEverloreChatSchema::BuildResponseSchema()
{
	TArray<TSharedPtr<FJsonValue>> IntentEnum;
	for (EEverloreChatIntentType T : { EEverloreChatIntentType::None, EEverloreChatIntentType::OfferQuest,
		EEverloreChatIntentType::RememberFact, EEverloreChatIntentType::SetDisposition, EEverloreChatIntentType::EndConversation })
	{
		IntentEnum.Add(MakeShared<FJsonValueString>(IntentToString(T)));
	}
	TSharedRef<FJsonObject> IntentField = MakeShared<FJsonObject>();
	IntentField->SetStringField(TEXT("type"), TEXT("string"));
	IntentField->SetArrayField(TEXT("enum"), IntentEnum);

	TSharedRef<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetObjectField(TEXT("reply"), Simple(TEXT("string")));
	Props->SetObjectField(TEXT("intent"), IntentField);
	Props->SetObjectField(TEXT("intent_argument"), Simple(TEXT("string")));

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("type"), TEXT("object"));
	Root->SetObjectField(TEXT("properties"), Props);

	TArray<TSharedPtr<FJsonValue>> Required{ MakeShared<FJsonValueString>(TEXT("reply")), MakeShared<FJsonValueString>(TEXT("intent")) };
	Root->SetArrayField(TEXT("required"), Required);

	TArray<TSharedPtr<FJsonValue>> Ordering;
	for (const TCHAR* Key : { TEXT("reply"), TEXT("intent"), TEXT("intent_argument") })
	{
		Ordering.Add(MakeShared<FJsonValueString>(Key));
	}
	Root->SetArrayField(TEXT("propertyOrdering"), Ordering);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

void FEverloreChatSchema::ParseReply(const FEverloreGenerationResponse& Response, bool bIntentsEnabled,
	FString& OutReply, FEverloreChatIntent& OutIntent)
{
	OutIntent = FEverloreChatIntent();

	if (!bIntentsEnabled)
	{
		OutReply = Response.Text.TrimStartAndEnd();
		if (OutReply.IsEmpty())
		{
			OutReply = TEXT("...");
		}
		return;
	}

	// Structured path: the constrained model returns a JSON object. Prefer the escape-aware
	// extraction; fall back to the raw text.
	const FString Json = !Response.ExtractedJson.IsEmpty() ? Response.ExtractedJson : Response.Text;

	TSharedPtr<FJsonObject> Obj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
	{
		FString Reply;
		Obj->TryGetStringField(TEXT("reply"), Reply);
		OutReply = Reply.TrimStartAndEnd();

		FString IntentStr;
		Obj->TryGetStringField(TEXT("intent"), IntentStr);
		OutIntent.Type = IntentFromString(IntentStr);

		FString Arg;
		Obj->TryGetStringField(TEXT("intent_argument"), Arg);
		OutIntent.Argument = Arg.TrimStartAndEnd();

		if (OutReply.IsEmpty())
		{
			OutReply = TEXT("...");
		}
		return;
	}

	// Constrained decoding should make this unreachable, but never fail the turn: treat the
	// whole text as the spoken line and emit no effect.
	OutReply = Response.Text.TrimStartAndEnd();
	if (OutReply.IsEmpty())
	{
		OutReply = TEXT("...");
	}
}
