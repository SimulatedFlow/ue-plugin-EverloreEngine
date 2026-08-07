// Copyright 2026 Simulated Flow All Rights Reserved.

#include "Components/EverloreConversationComponent.h"
#include "Components/EverloreCharacterComponent.h"
#include "Subsystems/EverloreBackendSubsystem.h"
#include "Backend/EverloreBackend.h"
#include "Reliability/EverloreChatSchema.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Reliability/EverlorePipelineTypes.h"
#include "Model/EverloreDialogueTypes.h"
#include "EverloreLog.h"
#include "GameFramework/Actor.h"

UEverloreConversationComponent::UEverloreConversationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEverloreConversationComponent::IsReady() const
{
	return ResolveCharacter() != nullptr;
}

UEverloreCharacterComponent* UEverloreConversationComponent::ResolveCharacter() const
{
	if (CharacterSource)
	{
		return CharacterSource;
	}
	if (const AActor* Owner = GetOwner())
	{
		return Owner->FindComponentByClass<UEverloreCharacterComponent>();
	}
	return nullptr;
}

void UEverloreConversationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Give any in-flight exchange a terminal signal so a bound "Talk To NPC (Async)" node
	// reaches SetReadyToDestroy instead of leaking, rooted, for the session's lifetime.
	CancelReply();
	Super::EndPlay(EndPlayReason);
}

void UEverloreConversationComponent::ResetConversation()
{
	// Invalidate any in-flight background summary (its captured epoch will no longer match),
	// then tear down any in-flight exchange, then clear the transcript.
	++ConversationEpoch;
	bSummarizing = false;
	CancelReply();
	History.Reset();
}

void UEverloreConversationComponent::CancelReply()
{
	if (!bExchangeActive)
	{
		return;
	}

	// Supersede the exchange first so its (possibly synchronous) completion is dropped by the
	// generation guard in FinalizeExchange rather than mutating state or firing a spurious error.
	++ExchangeGeneration;
	bExchangeActive = false;

	FEverloreChatExchangeCallbacks Callbacks = PendingCallbacks;
	PendingCallbacks = FEverloreChatExchangeCallbacks();

	if (UEverloreBackendSubsystem* Sub = UEverloreBackendSubsystem::Get())
	{
		Sub->CancelRaw(ActiveHandle);
	}

	// Terminal signal so async nodes tear down (treated as a clean, expected cancel, not a fault).
	Callbacks.OnError.ExecuteIfBound(TEXT("Reply cancelled."));
}

void UEverloreConversationComponent::SendMessage(const FString& PlayerText)
{
	FEverloreChatExchangeCallbacks Cb;
	Cb.OnDelta  = FEverloreChatOnDelta::CreateWeakLambda(this,  [this](const FString& D, bool bDone) { OnReplyDelta.Broadcast(D, bDone); });
	Cb.OnReply  = FEverloreChatOnReply::CreateWeakLambda(this,  [this](const FString& R)             { OnReplyReceived.Broadcast(R); });
	Cb.OnIntent = FEverloreChatOnIntent::CreateWeakLambda(this, [this](const FEverloreChatIntent& I) { OnIntent.Broadcast(I); });
	Cb.OnError  = FEverloreChatOnError::CreateWeakLambda(this,  [this](const FString& E)             { OnChatError.Broadcast(E); });
	Ask(PlayerText, Cb);
}

void UEverloreConversationComponent::Ask(const FString& PlayerText, const FEverloreChatExchangeCallbacks& Callbacks)
{
	if (PlayerText.TrimStartAndEnd().IsEmpty())
	{
		Callbacks.OnError.ExecuteIfBound(TEXT("Empty message."));
		return;
	}
	if (bExchangeActive)
	{
		Callbacks.OnError.ExecuteIfBound(TEXT("A reply is already being generated."));
		return;
	}
	if (bAuthorityOnly)
	{
		const AActor* Owner = GetOwner();
		if (Owner && !Owner->HasAuthority())
		{
			UE_LOG(LogEverlore, Verbose, TEXT("Conversation: Ask ignored on non-authority."));
			Callbacks.OnError.ExecuteIfBound(TEXT("Chat generation is server-only here (bAuthorityOnly)."));
			return;
		}
	}

	UEverloreBackendSubsystem* Sub = UEverloreBackendSubsystem::Get();
	if (!Sub)
	{
		Callbacks.OnError.ExecuteIfBound(TEXT("Everlore backend subsystem is unavailable."));
		return;
	}

	// Persona (an empty context is fine — the NPC just speaks generically).
	FEverloreCharacterContext Persona;
	if (const UEverloreCharacterComponent* Char = ResolveCharacter())
	{
		Persona = Char->Character;
	}

	// Compose from the COMMITTED history plus the new player line. The player turn is only
	// committed to History on success (in FinalizeExchange), so a failed/cancelled turn never
	// leaves a dangling User turn that would break role alternation on the next request.
	FEverloreGenerationRequest Req;
	Req.SystemPrompt = FEverloreChatSchema::SystemPrompt(Persona, bEnableIntents);
	for (const FEverloreConversationTurn& Turn : History)
	{
		Req.Messages.Add(FEverloreChatMessage(Turn.Role, Turn.Text));
	}
	Req.Messages.Add(FEverloreChatMessage(EEverloreChatRole::User, PlayerText));
	Req.ResponseSchemaJson = bEnableIntents ? FEverloreChatSchema::BuildResponseSchema() : FString();
	Req.Sampling.Temperature = Temperature;
	Req.Priority = EEverloreRequestPriority::Interactive;

	// Streaming only makes sense for free-text (structured intent output must be buffered so we
	// never show raw JSON), and only if the active backend actually streams.
	UEverloreBackend* Backend = Sub->GetActiveBackend();
	const bool bWantStream = bStreaming && !bEnableIntents && Backend && Backend->GetCapabilities().bSupportsStreaming;
	Req.bStream = bWantStream;

	const uint32 ThisExchange = ++ExchangeGeneration;
	bExchangeActive = true;
	PendingCallbacks = Callbacks;

	FEverloreBackendCompletionDelegate OnComplete = FEverloreBackendCompletionDelegate::CreateWeakLambda(this,
		[this, Callbacks, ThisExchange, PlayerText](const FEverloreGenerationResponse& Response)
		{
			FinalizeExchange(ThisExchange, PlayerText, Response, Callbacks);
		});

	if (bWantStream)
	{
		FEverloreBackendStreamDelegate OnDelta = FEverloreBackendStreamDelegate::CreateWeakLambda(this,
			[this, Callbacks, ThisExchange](const FString& Delta, bool bDone)
			{
				if (ThisExchange != ExchangeGeneration)
				{
					return; // a superseded/cancelled exchange's late delta — drop it
				}
				Callbacks.OnDelta.ExecuteIfBound(Delta, bDone);
			});
		ActiveHandle = Sub->SubmitRawStreaming(Req, OnDelta, OnComplete);
	}
	else
	{
		ActiveHandle = Sub->SubmitRaw(Req, OnComplete);
	}
}

void UEverloreConversationComponent::FinalizeExchange(uint32 ExchangeId, const FString& PlayerText, const FEverloreGenerationResponse& Response, FEverloreChatExchangeCallbacks Callbacks)
{
	// Drop the completion of any exchange that was cancelled or superseded (or fired after a
	// reset): it must not clear a newer exchange's busy flag, mutate History, or run effects.
	if (ExchangeId != ExchangeGeneration)
	{
		return;
	}
	bExchangeActive = false;
	PendingCallbacks = FEverloreChatExchangeCallbacks();

	if (!Response.IsSuccess())
	{
		// Nothing was committed to History yet, so there is no dangling turn to roll back.
		const FString Err = Response.ErrorMessage.IsEmpty()
			? FString::Printf(TEXT("Chat backend error (status %d, HTTP %d)."), (int32)Response.Status, Response.HttpStatusCode)
			: Response.ErrorMessage.ToString();
		UE_LOG(LogEverlore, Warning, TEXT("Conversation: %s"), *Err);
		Callbacks.OnError.ExecuteIfBound(Err);
		return;
	}

	FString Reply;
	FEverloreChatIntent Intent;
	FEverloreChatSchema::ParseReply(Response, bEnableIntents, Reply, Intent);

	// Commit BOTH turns atomically now that we have a valid reply — keeps History strictly
	// alternating (User, Assistant, ...) and stores the clean spoken line, never raw JSON.
	History.Add(FEverloreConversationTurn(PlayerSpeakerId, EEverloreChatRole::User, PlayerText));

	FName SpeakerId = TEXT("NPC");
	if (const UEverloreCharacterComponent* Char = ResolveCharacter())
	{
		if (!Char->Character.CharacterId.IsNone())
		{
			SpeakerId = Char->Character.CharacterId;
		}
	}
	History.Add(FEverloreConversationTurn(SpeakerId, EEverloreChatRole::Assistant, Reply));

	// Intent first (so a listener applies the effect together with the line), then the reply
	// as the terminal signal of the exchange.
	if (Intent.IsActionable())
	{
		Callbacks.OnIntent.ExecuteIfBound(Intent);
		ApplyIntent(Intent);
	}
	Callbacks.OnReply.ExecuteIfBound(Reply);

	MaybeSummarize();
}

void UEverloreConversationComponent::ApplyIntent(const FEverloreChatIntent& Intent)
{
	UEverloreCharacterComponent* Char = ResolveCharacter();

	switch (Intent.Type)
	{
	case EEverloreChatIntentType::RememberFact:
		if (Char && !Intent.Argument.IsEmpty())
		{
			Char->AppendMemory(Intent.Argument);
		}
		break;

	case EEverloreChatIntentType::SetDisposition:
		if (Char && !Intent.Argument.IsEmpty())
		{
			Char->Character.Disposition = FText::FromString(Intent.Argument);
		}
		break;

	case EEverloreChatIntentType::OfferQuest:
	{
		UEverloreBackendSubsystem* Sub = UEverloreBackendSubsystem::Get();
		if (!Sub || !Guardrails)
		{
			UE_LOG(LogEverlore, Verbose, TEXT("Conversation: OfferQuest ignored (no subsystem or guardrail config)."));
			break;
		}
		const FEverloreGuardrailSnapshot Snapshot = Guardrails->BuildSnapshot();
		if (Snapshot.Npcs.Num() == 0)
		{
			UE_LOG(LogEverlore, Warning, TEXT("Conversation: OfferQuest ignored (guardrail config has no NPC ids)."));
			break;
		}

		FEverloreQuestGenContext Ctx;
		Ctx.GiverNpcId = (Char && !Char->Character.CharacterId.IsNone()) ? Char->Character.CharacterId : NAME_None;
		// This NPC must be a known giver; if not listed, fall back to the first valid NPC.
		if (!Snapshot.IsNpc(Ctx.GiverNpcId))
		{
			for (const FName& N : Snapshot.Npcs)
			{
				Ctx.GiverNpcId = N;
				break;
			}
		}
		Ctx.Theme = Intent.Argument;
		Ctx.Seed = -1;

		TWeakObjectPtr<UEverloreConversationComponent> Weak(this);
		Sub->GenerateQuest(Ctx, Snapshot, FEverloreQuestReadyDelegate::CreateLambda(
			[Weak](const FEverloreQuestResult& Result)
			{
				if (UEverloreConversationComponent* Self = Weak.Get())
				{
					Self->OnQuestOffered.Broadcast(Result.Record, Result.Outcome);
				}
			}));
		break;
	}

	case EEverloreChatIntentType::EndConversation:
	case EEverloreChatIntentType::None:
	default:
		break;
	}
}

void UEverloreConversationComponent::MaybeSummarize()
{
	if (MaxHistoryTurns <= 0 || bSummarizing)
	{
		return;
	}
	if (History.Num() <= MaxHistoryTurns)
	{
		return;
	}

	const int32 DropCount = History.Num() - MaxHistoryTurns;

	UEverloreBackendSubsystem* Sub = UEverloreBackendSubsystem::Get();
	UEverloreCharacterComponent* Char = ResolveCharacter();
	if (!Sub || !Char)
	{
		// Can't summarize; hard-trim so context stays bounded (lossy).
		History.RemoveAt(0, DropCount);
		return;
	}

	FString Excerpt;
	for (int32 i = 0; i < DropCount; ++i)
	{
		const FEverloreConversationTurn& T = History[i];
		const TCHAR* Who = (T.Role == EEverloreChatRole::User) ? TEXT("Player") : TEXT("You");
		Excerpt += FString::Printf(TEXT("%s: %s\n"), Who, *T.Text);
	}

	FEverloreGenerationRequest Req;
	Req.SystemPrompt = TEXT("You compress a roleplay transcript excerpt into durable memory. Reply with 1-2 concise sentences, first person, capturing only what this character should remember later. Plain text only.");
	Req.Messages.Add(FEverloreChatMessage(EEverloreChatRole::User, Excerpt));
	Req.Sampling.Temperature = 0.3f;
	Req.Sampling.MaxOutputTokens = 256;
	Req.Priority = EEverloreRequestPriority::Background;

	bSummarizing = true;
	const uint32 Epoch = ConversationEpoch;
	Sub->SubmitRaw(Req, FEverloreBackendCompletionDelegate::CreateWeakLambda(this,
		[this, DropCount, Epoch](const FEverloreGenerationResponse& Response)
		{
			// If the conversation was reset while this summary was in flight, the summarized
			// turns are gone and the front of History now holds unrelated fresh turns. Bail
			// WITHOUT touching bSummarizing (a new conversation may own it) or History.
			if (Epoch != ConversationEpoch)
			{
				return;
			}
			bSummarizing = false;

			if (Response.IsSuccess() && !Response.Text.TrimStartAndEnd().IsEmpty())
			{
				if (UEverloreCharacterComponent* C = ResolveCharacter())
				{
					C->AppendMemory(Response.Text.TrimStartAndEnd());
				}
			}

			// Only the front turns are ever removed and the conversation was not reset, so the
			// oldest DropCount turns are exactly the summarized ones even if newer turns were
			// appended meanwhile.
			const int32 SafeDrop = FMath::Min(DropCount, History.Num());
			if (SafeDrop > 0)
			{
				History.RemoveAt(0, SafeDrop);
			}
		}));
}
