// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Backends/EverloreGeminiBackend.h"
#include "EverloreSettings.h"
#include "EverloreLog.h"
#include "Util/EverloreJsonUtils.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

FEverloreBackendCapabilities UEverloreGeminiBackend::GetCapabilities() const
{
	FEverloreBackendCapabilities Caps;
	Caps.bSupportsJsonSchema = true;
	Caps.bSupportsFunctionCalling = true;
	Caps.bSupportsStreaming = false; // buffered for now; streaming added later
	Caps.bRequiresSecret = true;
	Caps.bIsLocalInference = false;
	Caps.PreferredDialect = EEverloreSchemaDialect::GeminiResponseSchema;
	Caps.MaxContextTokens = 1048576;
	return Caps;
}

void UEverloreGeminiBackend::Probe(FEverloreProbeDelegate OnProbed)
{
	const FEverloreBackendCapabilities Caps = GetCapabilities();
	if (!SecretProvider.GetInterface())
	{
		OnProbed.ExecuteIfBound(false, Caps);
		return;
	}

	SecretProvider->ResolveSecret(GetBackendId(),
		FEverloreSecretResultDelegate::CreateLambda([OnProbed, Caps](bool bFound, const FString& /*Secret*/, EEverloreSecretSource /*Source*/)
		{
			// A resolvable key is our cheap "reachable" signal for now.
			OnProbed.ExecuteIfBound(bFound, Caps);
		}));
}

FEverloreRequestHandle UEverloreGeminiBackend::Generate(const FEverloreGenerationRequest& Request, FEverloreBackendCompletionDelegate OnComplete)
{
	const int64 Id = NextId++;
	FEverloreRequestHandle Handle;
	Handle.Id = Id;

	if (!SecretProvider.GetInterface())
	{
		FEverloreGenerationResponse R;
		R.Status = EEverloreBackendStatus::AuthError;
		R.ErrorMessage = NSLOCTEXT("Everlore", "GeminiNoProvider", "No secret provider is set for the Gemini backend.");
		OnComplete.ExecuteIfBound(R);
		return Handle;
	}

	// Resolve the key just-in-time (never stored on the object).
	TWeakObjectPtr<UEverloreGeminiBackend> WeakThis(this);
	SecretProvider->ResolveSecret(GetBackendId(),
		FEverloreSecretResultDelegate::CreateLambda([WeakThis, Request, Id, OnComplete](bool bFound, const FString& Secret, EEverloreSecretSource /*Source*/)
		{
			UEverloreGeminiBackend* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			if (Self->CancelledDuringResolve.Remove(Id) > 0)
			{
				// Cancelled while the key was resolving — bail before starting HTTP and
				// release the caller's concurrency slot with a Cancelled result.
				FEverloreGenerationResponse R;
				R.Status = EEverloreBackendStatus::Cancelled;
				OnComplete.ExecuteIfBound(R);
				return;
			}
			if (!bFound || Secret.IsEmpty())
			{
				FEverloreGenerationResponse R;
				R.Status = EEverloreBackendStatus::AuthError;
				R.ErrorMessage = NSLOCTEXT("Everlore", "GeminiNoKey",
					"No Gemini API key available. Set one via the developer runtime API, the player's key input, or the EVERLORE_GEMINI_KEY environment variable.");
				OnComplete.ExecuteIfBound(R);
				return;
			}
			Self->StartHttp(Request, Secret, Id, OnComplete);
		}));

	return Handle;
}

void UEverloreGeminiBackend::StartHttp(const FEverloreGenerationRequest& Request, const FString& ApiKey, int64 Id, FEverloreBackendCompletionDelegate OnComplete)
{
	const UEverloreSettings* Settings = GetDefault<UEverloreSettings>();

	const FString Url = FString::Printf(TEXT("%s/models/%s:generateContent"),
		*Settings->GeminiEndpoint, *Settings->GeminiModel);

	const float Timeout = Request.TimeoutSecondsOverride > 0.f ? Request.TimeoutSecondsOverride : Settings->DefaultTimeoutSeconds;

	FHttpRequestRef HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(Url);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("x-goog-api-key"), ApiKey); // key in header, not URL
	HttpRequest->SetContentAsString(BuildRequestBody(Request));
	HttpRequest->SetTimeout(Timeout);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UEverloreGeminiBackend::HandleHttp, Id, OnComplete);

	InFlight.Add(Id, HttpRequest);
	HttpRequest->ProcessRequest();
}

FString UEverloreGeminiBackend::BuildRequestBody(const FEverloreGenerationRequest& Request) const
{
	const UEverloreSettings* Settings = GetDefault<UEverloreSettings>();
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	// Collect a system instruction from SystemPrompt + any System-role messages.
	FString SystemText = Request.SystemPrompt;
	for (const FEverloreChatMessage& Msg : Request.Messages)
	{
		if (Msg.Role == EEverloreChatRole::System)
		{
			SystemText = SystemText.IsEmpty() ? Msg.Content : (SystemText + TEXT("\n") + Msg.Content);
		}
	}
	if (!SystemText.IsEmpty())
	{
		const TSharedRef<FJsonObject> Part = MakeShared<FJsonObject>();
		Part->SetStringField(TEXT("text"), SystemText);
		TArray<TSharedPtr<FJsonValue>> Parts{ MakeShared<FJsonValueObject>(Part) };

		const TSharedRef<FJsonObject> SysInstr = MakeShared<FJsonObject>();
		SysInstr->SetArrayField(TEXT("parts"), Parts);
		Root->SetObjectField(TEXT("system_instruction"), SysInstr);
	}

	// contents: user / assistant turns.
	TArray<TSharedPtr<FJsonValue>> Contents;
	for (const FEverloreChatMessage& Msg : Request.Messages)
	{
		if (Msg.Role == EEverloreChatRole::System)
		{
			continue;
		}
		const TSharedRef<FJsonObject> Part = MakeShared<FJsonObject>();
		Part->SetStringField(TEXT("text"), Msg.Content);
		TArray<TSharedPtr<FJsonValue>> Parts{ MakeShared<FJsonValueObject>(Part) };

		const TSharedRef<FJsonObject> Content = MakeShared<FJsonObject>();
		Content->SetStringField(TEXT("role"), Msg.Role == EEverloreChatRole::Assistant ? TEXT("model") : TEXT("user"));
		Content->SetArrayField(TEXT("parts"), Parts);
		Contents.Add(MakeShared<FJsonValueObject>(Content));
	}
	Root->SetArrayField(TEXT("contents"), Contents);

	// generationConfig
	const TSharedRef<FJsonObject> GenConfig = MakeShared<FJsonObject>();
	GenConfig->SetNumberField(TEXT("temperature"), Request.Sampling.Temperature);
	GenConfig->SetNumberField(TEXT("topP"), Request.Sampling.TopP);
	GenConfig->SetNumberField(TEXT("maxOutputTokens"), Request.Sampling.MaxOutputTokens);
	if (Request.Sampling.Seed >= 0)
	{
		GenConfig->SetNumberField(TEXT("seed"), Request.Sampling.Seed);
	}

	// Disable/limit hidden "thinking" so structured generation doesn't burn the output
	// budget on reasoning and truncate (see EVERLORE_GEMINI thinking notes).
	if (Settings->GeminiThinkingBudget >= 0)
	{
		const TSharedRef<FJsonObject> Thinking = MakeShared<FJsonObject>();
		Thinking->SetNumberField(TEXT("thinkingBudget"), Settings->GeminiThinkingBudget);
		GenConfig->SetObjectField(TEXT("thinkingConfig"), Thinking);
	}

	if (!Request.ResponseSchemaJson.IsEmpty())
	{
		GenConfig->SetStringField(TEXT("responseMimeType"), TEXT("application/json"));

		TSharedPtr<FJsonObject> SchemaObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Request.ResponseSchemaJson);
		if (FJsonSerializer::Deserialize(Reader, SchemaObj) && SchemaObj.IsValid())
		{
			GenConfig->SetObjectField(TEXT("responseSchema"), SchemaObj);
		}
		else
		{
			UE_LOG(LogEverlore, Warning, TEXT("Gemini: ResponseSchemaJson did not parse; sending unconstrained."));
		}
	}
	Root->SetObjectField(TEXT("generationConfig"), GenConfig);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

void UEverloreGeminiBackend::HandleHttp(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully,
	int64 Id, FEverloreBackendCompletionDelegate OnComplete)
{
	InFlight.Remove(Id);

	FEverloreGenerationResponse R;
	R.Attempt = 1;

	if (!bConnectedSuccessfully || !HttpResponse.IsValid())
	{
		R.Status = EEverloreBackendStatus::ConnectionError;
		R.ErrorMessage = NSLOCTEXT("Everlore", "GeminiConnFail", "Failed to reach the Gemini endpoint (connection error or timeout).");
		OnComplete.ExecuteIfBound(R);
		return;
	}

	R.HttpStatusCode = HttpResponse->GetResponseCode();
	R.RawBody = HttpResponse->GetContentAsString();

	if (R.HttpStatusCode == 401 || R.HttpStatusCode == 403)
	{
		R.Status = EEverloreBackendStatus::AuthError;
		R.ErrorMessage = NSLOCTEXT("Everlore", "GeminiAuth", "Gemini rejected the API key (401/403).");
		OnComplete.ExecuteIfBound(R);
		return;
	}
	if (R.HttpStatusCode == 429)
	{
		R.Status = EEverloreBackendStatus::RateLimited;
		OnComplete.ExecuteIfBound(R);
		return;
	}
	if (R.HttpStatusCode >= 500)
	{
		R.Status = EEverloreBackendStatus::ServerError;
		OnComplete.ExecuteIfBound(R);
		return;
	}
	if (R.HttpStatusCode != 200)
	{
		R.Status = EEverloreBackendStatus::MalformedTransport;
		R.ErrorMessage = FText::FromString(FString::Printf(TEXT("Gemini HTTP %d"), R.HttpStatusCode));
		OnComplete.ExecuteIfBound(R);
		return;
	}

	// Parse the 200 body.
	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(R.RawBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		R.Status = EEverloreBackendStatus::MalformedTransport;
		R.ErrorMessage = NSLOCTEXT("Everlore", "GeminiBadJson", "Gemini returned an unparseable body.");
		OnComplete.ExecuteIfBound(R);
		return;
	}

	// Safety / prompt block?
	const TSharedPtr<FJsonObject>* PromptFeedback;
	if (RootObj->TryGetObjectField(TEXT("promptFeedback"), PromptFeedback))
	{
		FString BlockReason;
		if ((*PromptFeedback)->TryGetStringField(TEXT("blockReason"), BlockReason) && !BlockReason.IsEmpty())
		{
			R.Status = EEverloreBackendStatus::SafetyBlocked;
			R.ErrorMessage = FText::FromString(FString::Printf(TEXT("Gemini blocked the prompt: %s"), *BlockReason));
			OnComplete.ExecuteIfBound(R);
			return;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Candidates;
	if (!RootObj->TryGetArrayField(TEXT("candidates"), Candidates) || Candidates->Num() == 0)
	{
		R.Status = EEverloreBackendStatus::SafetyBlocked;
		R.ErrorMessage = NSLOCTEXT("Everlore", "GeminiNoCand", "Gemini returned no candidates.");
		OnComplete.ExecuteIfBound(R);
		return;
	}

	const TSharedPtr<FJsonObject> Cand0 = (*Candidates)[0]->AsObject();
	FString FinishReason;
	if (Cand0.IsValid())
	{
		Cand0->TryGetStringField(TEXT("finishReason"), FinishReason);
	}

	// Extract candidate text (concatenate parts).
	FString Text;
	const TSharedPtr<FJsonObject>* ContentObj;
	if (Cand0.IsValid() && Cand0->TryGetObjectField(TEXT("content"), ContentObj))
	{
		const TArray<TSharedPtr<FJsonValue>>* Parts;
		if ((*ContentObj)->TryGetArrayField(TEXT("parts"), Parts))
		{
			for (const TSharedPtr<FJsonValue>& PartVal : *Parts)
			{
				const TSharedPtr<FJsonObject> PartObj = PartVal->AsObject();
				FString PartText;
				if (PartObj.IsValid() && PartObj->TryGetStringField(TEXT("text"), PartText))
				{
					Text += PartText;
				}
			}
		}
	}

	R.Text = Text;
	R.FinishReason = FinishReason == TEXT("STOP") ? EEverloreFinishReason::Stop
		: FinishReason == TEXT("MAX_TOKENS") ? EEverloreFinishReason::MaxTokens
		: FinishReason == TEXT("SAFETY") ? EEverloreFinishReason::Safety
		: EEverloreFinishReason::Other;

	// If the model was constrained to JSON the text IS the object; otherwise scan it out.
	R.ExtractedJson = Text.TrimStartAndEnd().StartsWith(TEXT("{"))
		? Text.TrimStartAndEnd()
		: FEverloreJsonUtils::ExtractFirstJsonObject(Text);
	R.bConstrainedDecodingApplied = !R.ExtractedJson.IsEmpty();

	if (R.FinishReason == EEverloreFinishReason::MaxTokens)
	{
		// Truncated output is a transport failure — never partial-parse it.
		R.Status = EEverloreBackendStatus::Truncated;
		R.ErrorMessage = NSLOCTEXT("Everlore", "GeminiTruncated", "Gemini hit the output token limit (truncated).");
		OnComplete.ExecuteIfBound(R);
		return;
	}

	R.Status = EEverloreBackendStatus::Success;
	OnComplete.ExecuteIfBound(R);
}

void UEverloreGeminiBackend::Cancel(const FEverloreRequestHandle& Handle)
{
	if (const FHttpRequestPtr* Req = InFlight.Find(Handle.Id))
	{
		if (Req->IsValid())
		{
			(*Req)->CancelRequest();
		}
		InFlight.Remove(Handle.Id);
	}
	else
	{
		// No HTTP request yet — cancelled during the async secret-resolution window.
		// Mark it so the resolution callback bails with a Cancelled response.
		CancelledDuringResolve.Add(Handle.Id);
	}
}
