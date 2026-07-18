// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Subsystems/EverloreBackendSubsystem.h"
#include "Backend/EverloreBackend.h"
#include "Backend/EverloreSecretProvider.h"
#include "EverloreSettings.h"
#include "EverloreLog.h"
#include "Reliability/EverloreQuestPipeline.h"
#include "Reliability/EverloreQuestSchema.h"
#include "Reliability/EverloreChatSchema.h"
#include "Reliability/EverloreGuardrailConfig.h"
#include "Model/EverloreQuestTypes.h"
#include "Model/EverloreChatTypes.h"
#include "Model/EverloreDialogueTypes.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

UEverloreBackendSubsystem* UEverloreBackendSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UEverloreBackendSubsystem>() : nullptr;
}

void UEverloreBackendSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SecretProvider = NewObject<UEverloreDefaultSecretProvider>(this, TEXT("EverloreSecretProvider"));

	const UEverloreSettings* Settings = GetDefault<UEverloreSettings>();
	ActiveBackendId = Settings->ActiveBackendId;

	// Wire known env-var names so the server / dev path can resolve keys (re-synced per dispatch
	// in PumpQueue so a mid-session Project Settings edit takes effect without an editor restart).
	SyncSecretConfig();

#if !UE_BUILD_SHIPPING
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Everlore.TestQuest"),
		TEXT("Generate a test quest via the active backend and log the validated result. Optional args = theme."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEverloreBackendSubsystem::ConsoleTestQuest),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Everlore.TestChat"),
		TEXT("Roleplay one chat turn with a demo NPC (Aldric), log the reply + parsed intent, and if the NPC offers a quest, route it through the quest pipeline. Optional args = the player's line."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEverloreBackendSubsystem::ConsoleTestChat),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Everlore.Backends"),
		TEXT("List all backends discovered by the registry and show the active one + readiness."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEverloreBackendSubsystem::ConsoleListBackends),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Everlore.SetBackend"),
		TEXT("Switch the active backend for this session, with an optional model override. Args: <BackendId> [Model]  (e.g. 'Everlore.SetBackend Ollama hermes3:8b')."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEverloreBackendSubsystem::ConsoleSetBackend),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Everlore.SetKey"),
		TEXT("Set a runtime API key for a backend this SESSION (editor testing — try your key in PIE without an OS env var + restart). Never ship a key in a client build. Args: <BackendId> <Key>."),
		FConsoleCommandWithArgsDelegate::CreateUObject(this, &UEverloreBackendSubsystem::ConsoleSetKey),
		ECVF_Default);
#endif

	UE_LOG(LogEverlore, Log, TEXT("EverloreBackendSubsystem initialized. Active backend: %s"), *ActiveBackendId.ToString());
}

void UEverloreBackendSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("Everlore.TestQuest"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("Everlore.TestChat"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("Everlore.Backends"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("Everlore.SetBackend"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("Everlore.SetKey"));
#endif

	Queue.Reset();
	InFlight.Reset();
	BackendInstances.Reset();
	BackendClasses.Reset();
	SecretProvider = nullptr;
	bRegistryBuilt = false;

	Super::Deinitialize();
}

void UEverloreBackendSubsystem::RebuildRegistry()
{
	BackendClasses.Reset();

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UEverloreBackend::StaticClass()))
		{
			continue;
		}
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		const UEverloreBackend* CDO = Class->GetDefaultObject<UEverloreBackend>();
		if (!CDO)
		{
			continue;
		}

		const FName Id = CDO->GetBackendId();
		if (Id.IsNone())
		{
			continue;
		}

		if (BackendClasses.Contains(Id))
		{
			UE_LOG(LogEverlore, Warning, TEXT("Duplicate backend id '%s' (%s vs %s) — keeping first."),
				*Id.ToString(), *BackendClasses[Id]->GetName(), *Class->GetName());
			continue;
		}

		BackendClasses.Add(Id, Class);
		UE_LOG(LogEverlore, Verbose, TEXT("Registered backend '%s' (%s)."), *Id.ToString(), *Class->GetName());
	}

	bRegistryBuilt = true;
}

TArray<FName> UEverloreBackendSubsystem::ListAvailableBackends()
{
	if (!bRegistryBuilt)
	{
		RebuildRegistry();
	}
	TArray<FName> Ids;
	BackendClasses.GetKeys(Ids);
	return Ids;
}

UEverloreBackend* UEverloreBackendSubsystem::GetOrCreateBackend(FName BackendId)
{
	if (BackendId.IsNone())
	{
		return nullptr;
	}
	if (!bRegistryBuilt)
	{
		RebuildRegistry();
	}

	if (const TObjectPtr<UEverloreBackend>* Existing = BackendInstances.Find(BackendId))
	{
		return *Existing;
	}

	const TSubclassOf<UEverloreBackend>* Class = BackendClasses.Find(BackendId);
	if (!Class || !*Class)
	{
		UE_LOG(LogEverlore, Warning, TEXT("Unknown backend id '%s'."), *BackendId.ToString());
		return nullptr;
	}

	UEverloreBackend* Instance = NewObject<UEverloreBackend>(this, *Class);
	Instance->SecretProvider.SetObject(SecretProvider);
	Instance->SecretProvider.SetInterface(Cast<IEverloreSecretProvider>(SecretProvider));
	BackendInstances.Add(BackendId, Instance);

	UE_LOG(LogEverlore, Log, TEXT("Instantiated backend '%s'."), *BackendId.ToString());
	return Instance;
}

UEverloreBackend* UEverloreBackendSubsystem::GetActiveBackend()
{
	return GetOrCreateBackend(ActiveBackendId);
}

bool UEverloreBackendSubsystem::SetActiveBackend(FName BackendId)
{
	if (!bRegistryBuilt)
	{
		RebuildRegistry();
	}
	if (!BackendClasses.Contains(BackendId))
	{
		UE_LOG(LogEverlore, Warning, TEXT("SetActiveBackend: unknown backend '%s'."), *BackendId.ToString());
		return false;
	}
	ActiveBackendId = BackendId;
	UE_LOG(LogEverlore, Log, TEXT("Active backend set to '%s'."), *BackendId.ToString());
	return true;
}

bool UEverloreBackendSubsystem::IsActiveBackendReady()
{
	UEverloreBackend* Backend = GetActiveBackend();
	if (!Backend)
	{
		return false;
	}
	// A backend that needs a key is only "ready" once a key actually resolves — otherwise
	// callers get silent template-fallback quests and think the plugin is broken.
	if (Backend->GetCapabilities().bRequiresSecret)
	{
		SyncSecretConfig();
		return SecretProvider && SecretProvider->HasSecret(ActiveBackendId);
	}
	return true;
}

void UEverloreBackendSubsystem::SetPlayerApiKey(FName BackendId, const FString& ApiKey)
{
	if (SecretProvider)
	{
		SecretProvider->SetPlayerProvidedSecret(BackendId, ApiKey);
	}
}

void UEverloreBackendSubsystem::ClearPlayerApiKey(FName BackendId)
{
	if (SecretProvider)
	{
		SecretProvider->ClearPlayerProvidedSecret(BackendId);
	}
}

bool UEverloreBackendSubsystem::HasPlayerApiKey(FName BackendId) const
{
	return SecretProvider && SecretProvider->HasPlayerProvidedSecret(BackendId);
}

void UEverloreBackendSubsystem::SetDeveloperApiKey(FName BackendId, const FString& ApiKey)
{
	if (SecretProvider)
	{
		SecretProvider->SetRuntimeSecret(BackendId, ApiKey);
	}
}

bool UEverloreBackendSubsystem::HasKeyForActiveBackend()
{
	UEverloreBackend* Backend = GetActiveBackend();
	if (!Backend)
	{
		return false;
	}
	if (!Backend->GetCapabilities().bRequiresSecret)
	{
		return true;
	}
	SyncSecretConfig();
	return SecretProvider && SecretProvider->HasSecret(ActiveBackendId);
}

int32 UEverloreBackendSubsystem::GetMaxConcurrency() const
{
	const UEverloreSettings* Settings = GetDefault<UEverloreSettings>();

	if (const TObjectPtr<UEverloreBackend>* Active = BackendInstances.Find(ActiveBackendId))
	{
		if (*Active && (*Active)->GetCapabilities().bIsLocalInference)
		{
			return FMath::Max(1, Settings->MaxConcurrentLocalRequests);
		}
	}
	return FMath::Max(1, Settings->MaxConcurrentRequests);
}

FEverloreRequestHandle UEverloreBackendSubsystem::SubmitRaw(const FEverloreGenerationRequest& Request, FEverloreBackendCompletionDelegate OnComplete)
{
	FPendingRequest Pending;
	Pending.Id = NextRequestId++;
	Pending.Request = Request;
	Pending.OnComplete = OnComplete;

	FEverloreRequestHandle Handle;
	Handle.Id = Pending.Id;

	Queue.Add(MoveTemp(Pending));
	PumpQueue();
	return Handle;
}

FEverloreRequestHandle UEverloreBackendSubsystem::SubmitRawStreaming(const FEverloreGenerationRequest& Request,
	FEverloreBackendStreamDelegate OnDelta, FEverloreBackendCompletionDelegate OnComplete)
{
	FPendingRequest Pending;
	Pending.Id = NextRequestId++;
	Pending.Request = Request;
	Pending.OnComplete = OnComplete;
	Pending.OnDelta = OnDelta;
	Pending.bStreaming = true;

	FEverloreRequestHandle Handle;
	Handle.Id = Pending.Id;

	Queue.Add(MoveTemp(Pending));
	PumpQueue();
	return Handle;
}

void UEverloreBackendSubsystem::SyncSecretConfig()
{
	if (!SecretProvider)
	{
		return;
	}
	const UEverloreSettings* Settings = GetDefault<UEverloreSettings>();
	SecretProvider->SetEnvVarName(TEXT("Gemini"), Settings->GeminiApiKeyEnvVar);
	SecretProvider->SetEnvVarName(TEXT("GenericHttp"), Settings->GenericHttpApiKeyEnvVar);
}

void UEverloreBackendSubsystem::PumpQueue()
{
	SyncSecretConfig();
	const int32 MaxConcurrency = GetMaxConcurrency();

	while (Queue.Num() > 0 && InFlight.Num() < MaxConcurrency)
	{
		FPendingRequest Pending = Queue[0];
		Queue.RemoveAt(0);

		UEverloreBackend* Backend = GetActiveBackend();
		if (!Backend)
		{
			// No backend configured — this is a CONFIG error (the only kind the caller
			// must handle). Complete immediately with a clear message.
			FEverloreGenerationResponse Error;
			Error.Status = EEverloreBackendStatus::ConnectionError;
			Error.ErrorMessage = NSLOCTEXT("Everlore", "NoBackend", "No active Everlore backend is configured.");
			Pending.OnComplete.ExecuteIfBound(Error);
			continue;
		}

		FInFlightRequest Entry;
		Entry.OnComplete = Pending.OnComplete;
		Entry.Backend = Backend;
		InFlight.Add(Pending.Id, Entry);

		FEverloreBackendCompletionDelegate Wrapper =
			FEverloreBackendCompletionDelegate::CreateUObject(this, &UEverloreBackendSubsystem::HandleRawComplete, Pending.Id);

		const FEverloreRequestHandle BackendHandle = Pending.bStreaming
			? Backend->GenerateStreaming(Pending.Request, Pending.OnDelta, Wrapper)
			: Backend->Generate(Pending.Request, Wrapper);

		// Guard against synchronous completion (entry already consumed by HandleRawComplete).
		if (FInFlightRequest* Live = InFlight.Find(Pending.Id))
		{
			Live->BackendHandle = BackendHandle;
		}
	}
}

void UEverloreBackendSubsystem::HandleRawComplete(const FEverloreGenerationResponse& Response, int64 OurId)
{
	FInFlightRequest Entry;
	if (!InFlight.RemoveAndCopyValue(OurId, Entry))
	{
		return; // already cancelled / removed
	}

	// Free the slot and pump BEFORE calling the user, so the callback can submit more.
	PumpQueue();
	Entry.OnComplete.ExecuteIfBound(Response);
}

void UEverloreBackendSubsystem::CancelRaw(const FEverloreRequestHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}

	// Still queued?
	for (int32 i = 0; i < Queue.Num(); ++i)
	{
		if (Queue[i].Id == Handle.Id)
		{
			FEverloreGenerationResponse Cancelled;
			Cancelled.Status = EEverloreBackendStatus::Cancelled;
			FEverloreBackendCompletionDelegate OnComplete = Queue[i].OnComplete;
			Queue.RemoveAt(i);
			OnComplete.ExecuteIfBound(Cancelled);
			return;
		}
	}

	// In flight? Copy handle + backend out FIRST — a reentrant synchronous cancel-completion
	// can remove the InFlight entry and dangle both the reference and the pointer (review).
	FEverloreRequestHandle BackendHandle;
	UEverloreBackend* Backend = nullptr;
	if (const FInFlightRequest* Entry = InFlight.Find(Handle.Id))
	{
		BackendHandle = Entry->BackendHandle;
		Backend = Entry->Backend.Get();
	}
	if (Backend)
	{
		Backend->Cancel(BackendHandle);
	}
}

void UEverloreBackendSubsystem::GenerateQuest(const FEverloreQuestGenContext& Ctx, const FEverloreGuardrailSnapshot& Snapshot, FEverloreQuestReadyDelegate OnReady)
{
	FEverloreGenerationRequest Req;
	Req.SystemPrompt = FEverloreQuestSchema::SystemPrompt();
	Req.Messages.Add(FEverloreChatMessage(EEverloreChatRole::User, FEverloreQuestSchema::BuildPrompt(Snapshot, Ctx.GiverNpcId, Ctx.Theme)));
	Req.ResponseSchemaJson = FEverloreQuestSchema::BuildResponseSchema();
	Req.Sampling.Seed = Ctx.Seed;
	Req.Priority = EEverloreRequestPriority::Interactive;

	const FString BackendId = ActiveBackendId.ToString();
	const FString Model = GetDefault<UEverloreSettings>()->GeminiModel;

	// The reliability pipeline runs in the completion (all CPU, on the game thread).
	SubmitRaw(Req, FEverloreBackendCompletionDelegate::CreateLambda(
		[Ctx, Snapshot, OnReady, BackendId, Model](const FEverloreGenerationResponse& Response)
		{
			const FEverloreQuestResult Result = FEverloreQuestPipeline::BuildResult(Response, Ctx, Snapshot, BackendId, Model);
			OnReady.ExecuteIfBound(Result);
		}));
}

void UEverloreBackendSubsystem::ConsoleTestQuest(const TArray<FString>& Args)
{
	// A tiny demo guardrail set so the command works with no project setup.
	FEverloreGuardrailSnapshot Snap;
	Snap.Npcs = { TEXT("Aldric"), TEXT("Bandit_Leader"), TEXT("Merchant_Bo") };
	Snap.Items = { TEXT("Ale"), TEXT("Healing_Herb"), TEXT("Iron_Sword") };
	Snap.Regions = { TEXT("Blackwood"), TEXT("Old_Mill") };
	Snap.Flags = { TEXT("Met_Aldric") };
	Snap.Factions = { TEXT("Village") };

	FEverloreQuestGenContext Ctx;
	Ctx.GiverNpcId = TEXT("Aldric");
	Ctx.Theme = FString::Join(Args, TEXT(" "));
	Ctx.Seed = -1;

	UE_LOG(LogEverlore, Warning, TEXT("[Everlore.TestQuest] backend='%s' theme='%s' — generating..."),
		*ActiveBackendId.ToString(), *Ctx.Theme);

	GenerateQuest(Ctx, Snap, FEverloreQuestReadyDelegate::CreateLambda(
		[](const FEverloreQuestResult& R)
		{
			UE_LOG(LogEverlore, Warning, TEXT("=== Quest [%s] \"%s\" ==="),
				*UEnum::GetValueAsString(R.Outcome), *R.Record.Payload.Title.ToString());
			UE_LOG(LogEverlore, Warning, TEXT("Summary: %s"), *R.Record.Payload.Summary.ToString());
			for (const FEverloreObjective& O : R.Record.Payload.Objectives)
			{
				UE_LOG(LogEverlore, Warning, TEXT("  objective [%s] target=%s x%d%s"),
					*UEnum::GetValueAsString(O.Type), *O.TargetId.ToString(), O.RequiredCount,
					O.bOptional ? TEXT(" (optional)") : TEXT(""));
			}
			for (const FEverloreReward& Rw : R.Record.Payload.Rewards)
			{
				UE_LOG(LogEverlore, Warning, TEXT("  reward [%s] %s x%d"),
					*UEnum::GetValueAsString(Rw.Type), *Rw.TargetId.ToString(), Rw.Amount);
			}
		}));
}

void UEverloreBackendSubsystem::ConsoleTestChat(const TArray<FString>& Args)
{
	// A demo NPC persona so the command works with no project setup (mirrors the quest fixture).
	FEverloreCharacterContext Persona;
	Persona.CharacterId = TEXT("Aldric");
	Persona.DisplayName = FText::FromString(TEXT("Aldric the Miller"));
	Persona.Persona = FText::FromString(TEXT("Aldric is a weary, good-hearted miller in the village of Riverdale. His mill by the old millpond is overrun by giant rats and he fears for the winter grain. He speaks plainly, with rural warmth, and is quick to trust anyone willing to help. He knows the woods of Thornwood well."));
	Persona.Disposition = FText::FromString(TEXT("Anxious but hopeful; grateful toward the player."));

	const FString PlayerMsg = Args.Num() > 0 ? FString::Join(Args, TEXT(" "))
		: TEXT("Well met, Aldric. I'm an adventurer looking for work - is there anything troubling you that I could help with?");

	FEverloreGenerationRequest Req;
	Req.SystemPrompt = FEverloreChatSchema::SystemPrompt(Persona, /*bIntentsEnabled=*/true);
	Req.Messages.Add(FEverloreChatMessage(EEverloreChatRole::User, PlayerMsg));
	Req.ResponseSchemaJson = FEverloreChatSchema::BuildResponseSchema();
	Req.Sampling.Temperature = 0.85f;
	Req.Priority = EEverloreRequestPriority::Interactive;

	UE_LOG(LogEverlore, Warning, TEXT("[Everlore.TestChat] backend='%s' player=\"%s\" - generating..."),
		*ActiveBackendId.ToString(), *PlayerMsg);

	SubmitRaw(Req, FEverloreBackendCompletionDelegate::CreateWeakLambda(this, [this](const FEverloreGenerationResponse& Response)
	{
		if (!Response.IsSuccess())
		{
			UE_LOG(LogEverlore, Error, TEXT("=== Chat FAILED (status %d, HTTP %d): %s"),
				(int32)Response.Status, Response.HttpStatusCode, *Response.ErrorMessage.ToString());
			return;
		}

		FString Reply;
		FEverloreChatIntent Intent;
		FEverloreChatSchema::ParseReply(Response, /*bIntentsEnabled=*/true, Reply, Intent);

		UE_LOG(LogEverlore, Warning, TEXT("=== Chat reply: \"%s\""), *Reply);
		UE_LOG(LogEverlore, Warning, TEXT("=== Chat intent: %s  arg=\"%s\""),
			*UEnum::GetValueAsString(Intent.Type), *Intent.Argument);

		if (Intent.Type != EEverloreChatIntentType::OfferQuest)
		{
			UE_LOG(LogEverlore, Warning, TEXT("=== (no quest offered this turn)"));
			return;
		}

		// The NPC decided to offer a quest: route the free-text theme through the SAME quest
		// reliability pipeline, proving a chat can only ever spawn a guaranteed-valid quest.
		FEverloreGuardrailSnapshot Snap;
		Snap.Npcs = { TEXT("Aldric"), TEXT("Mira"), TEXT("Doran") };
		Snap.Items = { TEXT("HealingHerb"), TEXT("RustyKey"), TEXT("AncientRelic"), TEXT("IronOre"), TEXT("GrainSack") };
		Snap.Regions = { TEXT("OldMill"), TEXT("Thornwood"), TEXT("Riverdale"), TEXT("DeepCaverns") };
		Snap.Flags = { TEXT("MetAldric"), TEXT("MillCleared"), TEXT("RelicRecovered") };
		Snap.Factions = { TEXT("VillageGuard"), TEXT("MerchantsGuild") };

		FEverloreQuestGenContext Ctx;
		Ctx.GiverNpcId = TEXT("Aldric");
		Ctx.Theme = Intent.Argument;
		Ctx.Seed = -1;

		UE_LOG(LogEverlore, Warning, TEXT("=== Chat OfferQuest -> routing through quest pipeline (theme=\"%s\")..."), *Intent.Argument);
		GenerateQuest(Ctx, Snap, FEverloreQuestReadyDelegate::CreateLambda([](const FEverloreQuestResult& R)
		{
			UE_LOG(LogEverlore, Warning, TEXT("=== Chat-spawned Quest [%s] \"%s\" (%d objectives, %d rewards)"),
				*UEnum::GetValueAsString(R.Outcome), *R.Record.Payload.Title.ToString(),
				R.Record.Payload.Objectives.Num(), R.Record.Payload.Rewards.Num());
		}));
	}));
}

void UEverloreBackendSubsystem::ConsoleListBackends(const TArray<FString>& Args)
{
	const TArray<FName> Ids = ListAvailableBackends();
	UE_LOG(LogEverlore, Warning, TEXT("=== Everlore backends: %d discovered ==="), Ids.Num());
	for (const FName& Id : Ids)
	{
		UEverloreBackend* B = GetOrCreateBackend(Id);
		const FEverloreBackendCapabilities Caps = B ? B->GetCapabilities() : FEverloreBackendCapabilities();
		UE_LOG(LogEverlore, Warning, TEXT("  - %s%s | jsonSchema=%d local=%d needsKey=%d streaming=%d"),
			*Id.ToString(),
			Id == ActiveBackendId ? TEXT(" (ACTIVE)") : TEXT(""),
			Caps.bSupportsJsonSchema ? 1 : 0, Caps.bIsLocalInference ? 1 : 0,
			Caps.bRequiresSecret ? 1 : 0, Caps.bSupportsStreaming ? 1 : 0);
	}
	UE_LOG(LogEverlore, Warning, TEXT("=== Active backend '%s' ready: %d ==="),
		*ActiveBackendId.ToString(), IsActiveBackendReady() ? 1 : 0);
}

void UEverloreBackendSubsystem::ConsoleSetBackend(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		UE_LOG(LogEverlore, Warning, TEXT("[Everlore.SetBackend] usage: Everlore.SetBackend <BackendId> [Model]. Available: %s"),
			*FString::JoinBy(ListAvailableBackends(), TEXT(", "), [](const FName& N) { return N.ToString(); }));
		return;
	}

	const FName Id(*Args[0]);
	const bool bSwitched = SetActiveBackend(Id);

	if (bSwitched && Args.Num() > 1)
	{
		const FString Model = Args[1];
		UEverloreSettings* Settings = GetMutableDefault<UEverloreSettings>();
		if (Id == TEXT("Ollama"))           { Settings->OllamaModel = Model; }
		else if (Id == TEXT("LlamaServer")) { Settings->LlamaServerModel = Model; }
		else if (Id == TEXT("GenericHttp")) { Settings->GenericHttpModel = Model; }
		else if (Id == TEXT("Gemini"))      { Settings->GeminiModel = Model; }
		UE_LOG(LogEverlore, Warning, TEXT("[Everlore.SetBackend] model override for '%s' = '%s'"), *Id.ToString(), *Model);
	}

	UE_LOG(LogEverlore, Warning, TEXT("[Everlore.SetBackend] active='%s' (switched=%d) ready=%d"),
		*GetActiveBackendId().ToString(), bSwitched ? 1 : 0, IsActiveBackendReady() ? 1 : 0);
}

void UEverloreBackendSubsystem::ConsoleSetKey(const TArray<FString>& Args)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogEverlore, Warning, TEXT("[Everlore.SetKey] usage: Everlore.SetKey <BackendId> <Key>"));
		return;
	}
	SetDeveloperApiKey(FName(*Args[0]), Args[1]);
	// Log the length only — never the key itself.
	UE_LOG(LogEverlore, Warning, TEXT("[Everlore.SetKey] runtime key set for '%s' (length %d). Active backend ready: %d"),
		*Args[0], Args[1].Len(), IsActiveBackendReady() ? 1 : 0);
}
