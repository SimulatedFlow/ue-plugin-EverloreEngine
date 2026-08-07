// Copyright 2026 Silvan Teufel All Rights Reserved.

#include "Backends/EverloreHttpBackendBase.h"
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

namespace
{
	/** Append the non-system turns as {role, content}; roles map assistant->assistant, else user. */
	void AppendChatMessages(TArray<TSharedPtr<FJsonValue>>& Out, const FEverloreGenerationRequest& Request)
	{
		for (const FEverloreChatMessage& Msg : Request.Messages)
		{
			if (Msg.Role == EEverloreChatRole::System)
			{
				continue;
			}
			const TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
			M->SetStringField(TEXT("role"), Msg.Role == EEverloreChatRole::Assistant ? TEXT("assistant") : TEXT("user"));
			M->SetStringField(TEXT("content"), Msg.Content);
			Out.Add(MakeShared<FJsonValueObject>(M));
		}
	}

	/** Collect SystemPrompt + any System-role messages into one system string. */
	FString CollectSystem(const FEverloreGenerationRequest& Request)
	{
		FString SystemText = Request.SystemPrompt;
		for (const FEverloreChatMessage& Msg : Request.Messages)
		{
			if (Msg.Role == EEverloreChatRole::System)
			{
				SystemText = SystemText.IsEmpty() ? Msg.Content : (SystemText + TEXT("\n") + Msg.Content);
			}
		}
		return SystemText;
	}

	FString SerializeJson(const TSharedRef<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}
}

void UEverloreHttpBackendBase::FillExtractedJson(FEverloreGenerationResponse& Out)
{
	const FString Trimmed = Out.Text.TrimStartAndEnd();
	Out.ExtractedJson = Trimmed.StartsWith(TEXT("{")) ? Trimmed : FEverloreJsonUtils::ExtractFirstJsonObject(Out.Text);
	Out.bConstrainedDecodingApplied = !Out.ExtractedJson.IsEmpty();
}

// ---------------------------------------------------------------------------------------
//  OpenAI-compatible ( /v1/chat/completions )
// ---------------------------------------------------------------------------------------

FString UEverloreHttpBackendBase::BuildOpenAiBody(const FEverloreGenerationRequest& Request, const FString& Model)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), Model);

	TArray<TSharedPtr<FJsonValue>> Messages;
	const FString SystemText = CollectSystem(Request);
	if (!SystemText.IsEmpty())
	{
		const TSharedRef<FJsonObject> Sys = MakeShared<FJsonObject>();
		Sys->SetStringField(TEXT("role"), TEXT("system"));
		Sys->SetStringField(TEXT("content"), SystemText);
		Messages.Add(MakeShared<FJsonValueObject>(Sys));
	}
	AppendChatMessages(Messages, Request);
	Root->SetArrayField(TEXT("messages"), Messages);

	Root->SetNumberField(TEXT("temperature"), Request.Sampling.Temperature);
	Root->SetNumberField(TEXT("max_tokens"), Request.Sampling.MaxOutputTokens);
	if (Request.Sampling.Seed >= 0)
	{
		Root->SetNumberField(TEXT("seed"), Request.Sampling.Seed);
	}

	if (!Request.ResponseSchemaJson.IsEmpty())
	{
		// json_object is the broadly-supported constraint across OpenAI-compatible servers;
		// the reliability pipeline validates/repairs the shape regardless, so we don't depend
		// on full json_schema support that many servers lack.
		const TSharedRef<FJsonObject> RF = MakeShared<FJsonObject>();
		RF->SetStringField(TEXT("type"), TEXT("json_object"));
		Root->SetObjectField(TEXT("response_format"), RF);
	}

	return SerializeJson(Root);
}

void UEverloreHttpBackendBase::ParseOpenAiBody(const FString& Body, FEverloreGenerationResponse& Out)
{
	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		Out.Status = EEverloreBackendStatus::MalformedTransport;
		Out.ErrorMessage = NSLOCTEXT("Everlore", "OpenAiBadJson", "Backend returned an unparseable body.");
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Choices;
	if (!RootObj->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
	{
		Out.Status = EEverloreBackendStatus::SafetyBlocked;
		Out.ErrorMessage = NSLOCTEXT("Everlore", "OpenAiNoChoice", "Backend returned no choices.");
		return;
	}

	const TSharedPtr<FJsonObject> Choice0 = (*Choices)[0]->AsObject();
	FString FinishReason;
	if (Choice0.IsValid())
	{
		Choice0->TryGetStringField(TEXT("finish_reason"), FinishReason);
		const TSharedPtr<FJsonObject>* MessageObj;
		if (Choice0->TryGetObjectField(TEXT("message"), MessageObj))
		{
			(*MessageObj)->TryGetStringField(TEXT("content"), Out.Text);
		}
	}

	Out.FinishReason = FinishReason == TEXT("stop") ? EEverloreFinishReason::Stop
		: FinishReason == TEXT("length") ? EEverloreFinishReason::MaxTokens
		: EEverloreFinishReason::Other;

	FillExtractedJson(Out);
	Out.Status = EEverloreBackendStatus::Success;
}

// ---------------------------------------------------------------------------------------
//  Ollama ( /api/chat )
// ---------------------------------------------------------------------------------------

FString UEverloreHttpBackendBase::BuildOllamaBody(const FEverloreGenerationRequest& Request, const FString& Model)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), Model);

	TArray<TSharedPtr<FJsonValue>> Messages;
	const FString SystemText = CollectSystem(Request);
	if (!SystemText.IsEmpty())
	{
		const TSharedRef<FJsonObject> Sys = MakeShared<FJsonObject>();
		Sys->SetStringField(TEXT("role"), TEXT("system"));
		Sys->SetStringField(TEXT("content"), SystemText);
		Messages.Add(MakeShared<FJsonValueObject>(Sys));
	}
	AppendChatMessages(Messages, Request);
	Root->SetArrayField(TEXT("messages"), Messages);

	Root->SetBoolField(TEXT("stream"), false);

	const TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
	Options->SetNumberField(TEXT("temperature"), Request.Sampling.Temperature);
	Options->SetNumberField(TEXT("num_predict"), Request.Sampling.MaxOutputTokens);
	if (Request.Sampling.Seed >= 0)
	{
		Options->SetNumberField(TEXT("seed"), Request.Sampling.Seed);
	}
	Root->SetObjectField(TEXT("options"), Options);

	// Ollama structured outputs: `format` accepts a full JSON schema object.
	if (!Request.ResponseSchemaJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> SchemaObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Request.ResponseSchemaJson);
		if (FJsonSerializer::Deserialize(Reader, SchemaObj) && SchemaObj.IsValid())
		{
			Root->SetObjectField(TEXT("format"), SchemaObj);
		}
	}

	return SerializeJson(Root);
}

void UEverloreHttpBackendBase::ParseOllamaBody(const FString& Body, FEverloreGenerationResponse& Out)
{
	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		Out.Status = EEverloreBackendStatus::MalformedTransport;
		Out.ErrorMessage = NSLOCTEXT("Everlore", "OllamaBadJson", "Ollama returned an unparseable body.");
		return;
	}

	const TSharedPtr<FJsonObject>* MessageObj;
	if (!RootObj->TryGetObjectField(TEXT("message"), MessageObj))
	{
		// A 200 with no assistant message is an error / unexpected body (e.g. {"error":...}),
		// not a successful generation — parity with the OpenAI/Gemini no-container guards.
		Out.Status = EEverloreBackendStatus::MalformedTransport;
		Out.ErrorMessage = NSLOCTEXT("Everlore", "OllamaNoMessage", "Ollama returned no assistant message.");
		return;
	}
	(*MessageObj)->TryGetStringField(TEXT("content"), Out.Text);

	FString DoneReason;
	RootObj->TryGetStringField(TEXT("done_reason"), DoneReason);
	Out.FinishReason = DoneReason == TEXT("length") ? EEverloreFinishReason::MaxTokens
		: DoneReason == TEXT("stop") ? EEverloreFinishReason::Stop
		: EEverloreFinishReason::Other;

	FillExtractedJson(Out);
	Out.Status = EEverloreBackendStatus::Success;
}

// ---------------------------------------------------------------------------------------
//  Request lifecycle (mirrors the Gemini backend)
// ---------------------------------------------------------------------------------------

void UEverloreHttpBackendBase::Probe(FEverloreProbeDelegate OnProbed)
{
	const FEverloreBackendCapabilities Caps = GetCapabilities();
	if (!RequiresSecret())
	{
		// A local endpoint is our cheap "reachable enough" signal without a network round-trip.
		OnProbed.ExecuteIfBound(true, Caps);
		return;
	}
	if (!SecretProvider.GetInterface())
	{
		OnProbed.ExecuteIfBound(false, Caps);
		return;
	}
	SecretProvider->ResolveSecret(GetBackendId(),
		FEverloreSecretResultDelegate::CreateLambda([OnProbed, Caps](bool bFound, const FString&, EEverloreSecretSource)
		{
			OnProbed.ExecuteIfBound(bFound, Caps);
		}));
}

FEverloreRequestHandle UEverloreHttpBackendBase::Generate(const FEverloreGenerationRequest& Request, FEverloreBackendCompletionDelegate OnComplete)
{
	const int64 Id = NextId++;
	FEverloreRequestHandle Handle;
	Handle.Id = Id;

	if (!RequiresSecret())
	{
		StartHttp(Request, FString(), Id, OnComplete);
		return Handle;
	}

	if (!SecretProvider.GetInterface())
	{
		FEverloreGenerationResponse R;
		R.Status = EEverloreBackendStatus::AuthError;
		R.ErrorMessage = NSLOCTEXT("Everlore", "HttpNoProvider", "No secret provider is set for this backend.");
		OnComplete.ExecuteIfBound(R);
		return Handle;
	}

	TWeakObjectPtr<UEverloreHttpBackendBase> WeakThis(this);
	SecretProvider->ResolveSecret(GetBackendId(),
		FEverloreSecretResultDelegate::CreateLambda([WeakThis, Request, Id, OnComplete](bool bFound, const FString& Secret, EEverloreSecretSource)
		{
			UEverloreHttpBackendBase* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			if (Self->CancelledDuringResolve.Remove(Id) > 0)
			{
				FEverloreGenerationResponse R;
				R.Status = EEverloreBackendStatus::Cancelled;
				OnComplete.ExecuteIfBound(R);
				return;
			}
			if (!bFound || Secret.IsEmpty())
			{
				FEverloreGenerationResponse R;
				R.Status = EEverloreBackendStatus::AuthError;
				R.ErrorMessage = NSLOCTEXT("Everlore", "HttpNoKey", "No API key available for this backend (runtime key, player key, or environment variable).");
				OnComplete.ExecuteIfBound(R);
				return;
			}
			Self->StartHttp(Request, Secret, Id, OnComplete);
		}));

	return Handle;
}

void UEverloreHttpBackendBase::StartHttp(const FEverloreGenerationRequest& Request, const FString& ApiKey, int64 Id, FEverloreBackendCompletionDelegate OnComplete)
{
	const UEverloreSettings* Settings = GetDefault<UEverloreSettings>();
	const float Timeout = Request.TimeoutSecondsOverride > 0.f ? Request.TimeoutSecondsOverride : Settings->DefaultTimeoutSeconds;

	FHttpRequestRef HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetURL(GetEndpointUrl());
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!ApiKey.IsEmpty())
	{
		HttpRequest->SetHeader(AuthHeaderName(), AuthHeaderValue(ApiKey));
	}
	HttpRequest->SetContentAsString(BuildRequestBody(Request));
	HttpRequest->SetTimeout(Timeout);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UEverloreHttpBackendBase::HandleHttp, Id, OnComplete);

	InFlight.Add(Id, HttpRequest);
	HttpRequest->ProcessRequest();
}

void UEverloreHttpBackendBase::HandleHttp(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully,
	int64 Id, FEverloreBackendCompletionDelegate OnComplete)
{
	InFlight.Remove(Id);

	FEverloreGenerationResponse R;
	R.Attempt = 1;

	if (!bConnectedSuccessfully || !HttpResponse.IsValid())
	{
		R.Status = EEverloreBackendStatus::ConnectionError;
		R.ErrorMessage = FText::FromString(FString::Printf(TEXT("Failed to reach %s (connection error, timeout, or server not running)."), *GetEndpointUrl()));
		OnComplete.ExecuteIfBound(R);
		return;
	}

	R.HttpStatusCode = HttpResponse->GetResponseCode();
	R.RawBody = HttpResponse->GetContentAsString();

	if (R.HttpStatusCode == 401 || R.HttpStatusCode == 403)
	{
		R.Status = EEverloreBackendStatus::AuthError;
		R.ErrorMessage = NSLOCTEXT("Everlore", "HttpAuth", "Backend rejected the request (401/403).");
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
		R.ErrorMessage = FText::FromString(FString::Printf(TEXT("Backend HTTP %d"), R.HttpStatusCode));
		OnComplete.ExecuteIfBound(R);
		return;
	}

	ParseSuccessBody(R.RawBody, R);

	if (R.FinishReason == EEverloreFinishReason::MaxTokens)
	{
		// Truncated output is a transport failure — never partial-parse it.
		R.Status = EEverloreBackendStatus::Truncated;
		R.ErrorMessage = NSLOCTEXT("Everlore", "HttpTruncated", "Backend hit the output token limit (truncated).");
	}

	OnComplete.ExecuteIfBound(R);
}

void UEverloreHttpBackendBase::Cancel(const FEverloreRequestHandle& Handle)
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
		CancelledDuringResolve.Add(Handle.Id);
	}
}
