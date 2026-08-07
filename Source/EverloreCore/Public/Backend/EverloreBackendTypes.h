// Copyright 2026 Silvan Teufel All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EverloreBackendTypes.generated.h"

/**
 * Transport-level outcome of a single backend request.
 * NOTE: this is about the *transport*, not about quest validity. Whether the
 * returned content is a usable quest is decided later by the Core pipeline.
 */
UENUM(BlueprintType)
enum class EEverloreBackendStatus : uint8
{
	Success             UMETA(DisplayName = "Success"),
	Cancelled           UMETA(DisplayName = "Cancelled"),
	Timeout             UMETA(DisplayName = "Timeout"),
	ConnectionError     UMETA(DisplayName = "Connection Error"),
	AuthError           UMETA(DisplayName = "Auth Error"),        // 401/403 — do NOT transport-retry
	RateLimited         UMETA(DisplayName = "Rate Limited"),      // 429    — retry with backoff
	ServerError         UMETA(DisplayName = "Server Error"),      // 5xx    — retry with backoff
	Truncated           UMETA(DisplayName = "Truncated"),         // finishReason=MAX_TOKENS — never partial-parse
	SafetyBlocked       UMETA(DisplayName = "Safety Blocked"),    // empty candidate / safety block
	MalformedTransport  UMETA(DisplayName = "Malformed Transport")// non-JSON where JSON expected
};

/** Why the model stopped generating (normalized across providers). */
UENUM(BlueprintType)
enum class EEverloreFinishReason : uint8
{
	Unknown    UMETA(DisplayName = "Unknown"),
	Stop       UMETA(DisplayName = "Stop"),        // natural / stop token
	MaxTokens  UMETA(DisplayName = "Max Tokens"),  // hit the output cap => treat as Truncated
	Safety     UMETA(DisplayName = "Safety"),
	Other      UMETA(DisplayName = "Other")
};

/** Coarse scheduling priority used by the request queue. */
UENUM(BlueprintType)
enum class EEverloreRequestPriority : uint8
{
	Interactive UMETA(DisplayName = "Interactive"), // player is waiting (runtime quest / chat reply)
	Background  UMETA(DisplayName = "Background"),   // prewarm / speculative
	Batch       UMETA(DisplayName = "Batch")         // editor batch generation
};

/** Which structured-output dialect a backend natively speaks. */
UENUM(BlueprintType)
enum class EEverloreSchemaDialect : uint8
{
	None                 UMETA(DisplayName = "None"),                  // no constrained decoding; rely on validation+repair
	PortableSubset       UMETA(DisplayName = "Portable JSON Subset"),  // plain JSON-schema subset
	GeminiResponseSchema UMETA(DisplayName = "Gemini responseSchema"),
	OllamaFormat         UMETA(DisplayName = "Ollama format schema"),
	LlamaJsonSchema      UMETA(DisplayName = "llama.cpp json_schema"),
	LlamaGbnf            UMETA(DisplayName = "llama.cpp GBNF grammar")
};

/** Role of a single message in a (possibly multi-turn) request. */
UENUM(BlueprintType)
enum class EEverloreChatRole : uint8
{
	System    UMETA(DisplayName = "System"),
	User      UMETA(DisplayName = "User"),
	Assistant UMETA(DisplayName = "Assistant")
};

/**
 * What a backend can do. The reliability layer reads these to adapt (which
 * constraint artifact to send, whether to stream, whether a key is required, etc.).
 */
USTRUCT(BlueprintType)
struct FEverloreBackendCapabilities
{
	GENERATED_BODY()

	/** llama.cpp GBNF grammar-constrained sampling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	bool bSupportsGrammar = false;

	/** Gemini responseSchema / Ollama format / llama json_schema. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	bool bSupportsJsonSchema = false;

	/** Tool / function calling (an alternative route to structured output). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	bool bSupportsFunctionCalling = false;

	/** Incremental token streaming (mainly valuable for roleplay chat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	bool bSupportsStreaming = false;

	/** Needs an API key. In multiplayer these are hard-gated to server authority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	bool bRequiresSecret = false;

	/** Runs on localhost / in-process => scheduled with a concurrency-of-one profile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	bool bIsLocalInference = false;

	/** Preferred constraint dialect for this backend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	EEverloreSchemaDialect PreferredDialect = EEverloreSchemaDialect::PortableSubset;

	/** Model context window in tokens (0 = unknown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Backend")
	int32 MaxContextTokens = 0;
};

/** Sampling knobs. Defaults favour reliability (low temperature). */
USTRUCT(BlueprintType)
struct FEverloreSamplingParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Sampling", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float Temperature = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Sampling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TopP = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Sampling", meta = (ClampMin = "1"))
	int32 MaxOutputTokens = 2048;

	/** Deterministic seed; -1 = provider default / random. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Sampling")
	int32 Seed = -1;
};

/** One message in a request. Single-shot generation is just one User message. */
USTRUCT(BlueprintType)
struct FEverloreChatMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore")
	EEverloreChatRole Role = EEverloreChatRole::User;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore")
	FString Content;

	FEverloreChatMessage() = default;
	FEverloreChatMessage(EEverloreChatRole InRole, const FString& InContent)
		: Role(InRole), Content(InContent) {}
};

/**
 * Opaque handle to an in-flight request, used for cancellation.
 * The subsystem maps Id -> internal state; the handle itself is a lightweight value.
 */
USTRUCT(BlueprintType)
struct FEverloreRequestHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Everlore")
	int64 Id = 0;

	bool IsValid() const { return Id != 0; }
};

/**
 * Everything a backend needs to run one generation.
 * The prompt is FULLY composed by Core (task + guardrail enums + few-shot); the
 * constraint artifacts (ResponseSchemaJson / GrammarGbnf) are derived by Core from
 * the single canonical schema. The backend only chooses which artifact fits its dialect.
 */
USTRUCT(BlueprintType)
struct FEverloreGenerationRequest
{
	GENERATED_BODY()

	/** Persona / instructions (system role). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	FString SystemPrompt;

	/** Conversation turns. For a one-shot quest this is a single User message. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	TArray<FEverloreChatMessage> Messages;

	/** JSON-schema artifact for structured backends ("" = unconstrained). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	FString ResponseSchemaJson;

	/** GBNF grammar artifact for llama.cpp ("" = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	FString GrammarGbnf;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	FEverloreSamplingParams Sampling;

	/** Per-request timeout override in seconds (<= 0 = use settings default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	float TimeoutSecondsOverride = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	EEverloreRequestPriority Priority = EEverloreRequestPriority::Interactive;

	/** Request incremental streaming (chat). Ignored if the backend can't stream. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Everlore|Request")
	bool bStream = false;

	/** Local-only trace id (no telemetry leaves the machine). */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Request")
	FGuid CorrelationId;
};

/**
 * Raw result of a backend request. The backend returns bytes + best-effort
 * extracted JSON + a classified status; it does NOT interpret quests.
 */
USTRUCT(BlueprintType)
struct FEverloreGenerationResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	EEverloreBackendStatus Status = EEverloreBackendStatus::MalformedTransport;

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	int32 HttpStatusCode = 0;

	/** Exactly as received (kept for the repair prompt's context). */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	FString RawBody;

	/** String-literal-&-escape-aware extraction of the JSON object, if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	FString ExtractedJson;

	/** Free assistant text (roleplay chat path). */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	FString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	EEverloreFinishReason FinishReason = EEverloreFinishReason::Unknown;

	/** Did the backend actually apply constrained decoding? (a flag, never a trust basis) */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	bool bConstrainedDecodingApplied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	double LatencyMs = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	int32 Attempt = 0;

	/** User-facing error message when Status != Success. */
	UPROPERTY(BlueprintReadOnly, Category = "Everlore|Response")
	FText ErrorMessage;

	bool IsSuccess() const { return Status == EEverloreBackendStatus::Success; }
};

// ---- Native (non-dynamic) delegates — backends are native C++ and on the hot path ----

/** Fired once when a Generate() request completes (on the game thread). */
DECLARE_DELEGATE_OneParam(FEverloreBackendCompletionDelegate, const FEverloreGenerationResponse& /*Response*/);

/** Fired for each streamed text delta; bDone=true on the final chunk. */
DECLARE_DELEGATE_TwoParams(FEverloreBackendStreamDelegate, const FString& /*DeltaText*/, bool /*bDone*/);

/** Fired when Probe() finishes refining a backend's capabilities. */
DECLARE_DELEGATE_TwoParams(FEverloreProbeDelegate, bool /*bReachable*/, const FEverloreBackendCapabilities& /*Refined*/);
