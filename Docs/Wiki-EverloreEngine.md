<!--
  Everlore Engine — Wiki documentation (English).
  Paste the body below into the Wiki.js editor at https://wiki.teufel-engineering.com/e/en/EverLoreEngine
  (Wiki.js uses Markdown; headings auto-generate the page's table of contents.)
-->

# Everlore Engine — AI Quest & Dialogue Director

**Schema-enforced, validated, game-safe AI quests, dialogue and NPC roleplay for Unreal Engine 5.8.**

Everlore Engine turns any LLM into a *reliable* game system. Bring your own backend — Google
Gemini, a local Ollama or llama.cpp server, or any OpenAI-compatible endpoint — and everything the
model returns is **schema-constrained, validated, auto-repaired, and (if still unusable) replaced by
a deterministic template**. A generated quest can never reference an id you didn't allow, exceed the
reward budget you set, or become uncompletable. Your game only ever sees a **guaranteed-valid**
result.

> The reliability layer is the whole point. A small local 8-billion-parameter model and Google
> Gemini both produce a guaranteed-valid quest through the same pipeline — one may pass clean, the
> other may be auto-repaired — but your game logic never has to handle "the AI returned garbage".

## Feature overview

- **Guaranteed-valid quests** — LLM output is parsed, validated against your rules, auto-repaired,
  and falls back to deterministic templates. Provenance is reported so you can badge the result.
- **NPC roleplay chat** — free-form conversation with persona + rolling/summarized memory, plus a
  closed set of **whitelisted intents** (offer quest, remember fact, set disposition, end). The one
  intent that touches game state (offer quest) routes through the same quest reliability pipeline.
- **Bring your own backend** — Gemini (cloud), Ollama (local), llama.cpp server (local), or any
  OpenAI-compatible HTTP endpoint. Swap at runtime.
- **BYOK (bring your own key)** — players (or developers) supply their own API key at runtime. Keys
  are never cooked into the build, never logged, never saved.
- **Component-first** — drop-in `ActorComponent`s and async Blueprint nodes; no C++ required.
- **Multiplayer-ready** — quest state replicates; generation is gated to server authority so keys
  stay server-side.
- **Editor batch tool** — generate a pool of quests offline and bake them into a DataTable for
  deterministic, hand-curated shipping content (no runtime LLM calls).

---

## Requirements

- **Unreal Engine 5.8**
- One backend:
  - **Google Gemini** (cloud) — a Google AI Studio API key, or
  - **Ollama** (local) — [ollama.com](https://ollama.com), no key, or
  - **llama.cpp server** (`llama-server`, local) — no key, or
  - **Any OpenAI-compatible endpoint** — OpenRouter, LocalAI, vLLM, Together, …

The plugin ships **no** API keys and **no** third-party inference binaries. You provide the key (or
run a local server) at runtime.

## Installation

1. Copy the `EverloreEngine` folder into your project's `Plugins/` directory.
2. Enable **Everlore Engine** in *Edit → Plugins*, then restart the editor.
3. Open *Project Settings → Plugins → Everlore Engine* and pick your backend + model.
4. Provide a key if your backend needs one (see [API keys / BYOK](#api-keys--byok)).

---

## Quick start — quests (5 minutes)

1. **Define your allowed vocabulary.** Create a Data Asset of type **Everlore Guardrail Config**
   (*Add → Miscellaneous → Data Asset → EverloreGuardrailConfig*). Fill in `Valid Npc Ids`,
   `Valid Item Ids`, `Valid Region Ids` (and optionally `Valid Flags` / `Valid Factions`) with your
   game's own ids, and set the reward/objective bounds (`Max Gold`, `Max Objectives Per Quest`, …).
   **The LLM may only ever use these ids.**
2. **Make an NPC a quest giver.** Add an **Everlore Quest Giver Component** to an NPC actor. Set
   `Giver Npc Id` (must be in the config's `Valid Npc Ids`), assign your `Guardrails` asset, and an
   optional `Default Theme`.
3. **Give the player a log.** Add an **Everlore Quest Log Component** to your player pawn or
   PlayerState.
4. **Generate + accept.** In a Blueprint graph, call **Generate Quest (Async)** (inputs: your
   `Guardrails`, the `Giver Npc Id`, and a `Theme`). On any success pin (`On Completed`,
   `On Repaired`, `On Fallback`), pass the `Quest` output to the quest log's **Accept Quest**.
   Alternatively, call `Request Quest` on the giver component and bind its `On Quest Offered` event.
5. **Set a key** (Gemini only) and press Play.

**Try it headlessly first:** with a key set, open the console and run
`Everlore.TestQuest a haunted mill`, then check the log for `=== Quest`.

## Quick start — roleplay chat

1. Add an **Everlore Character Component** to your NPC and fill its `Character` struct — `Display
   Name`, `Persona` (free-text personality/backstory), `Disposition`.
2. Add an **Everlore Conversation Component** to the same NPC. Assign a `Guardrails` asset (needed
   only if you want the NPC to be able to *offer quests* from chat).
3. Drive a turn: call **Talk To NPC (Async)** (or the component's `Send Message`) with the player's
   line. Bind `On Reply Received` to show the NPC's text, and `On Quest Offered` to accept a chat-
   spawned quest into the player's quest log.

---

## Core concept: the reliability guarantee

Every generation runs through a fixed pipeline:

**parse → validate → auto-fix → re-validate → deterministic fallback → commit**

The outcome is reported as an **`EEverloreProvenance`** value:

| Provenance | Meaning | Async node pin |
|---|---|---|
| `Authored` | Hand-made / from a DataTable | — |
| `LlmValidated` ("AI-Validated") | Generated and passed validation as-is | `On Completed` |
| `LlmRepaired` ("AI-Repaired") | Generated, then auto-corrected to valid | `On Repaired` |
| `TemplateFallback` ("Template (Game-Safe)") | LLM output unusable → deterministic template | `On Fallback` |

The **Generate Quest (Async)** node's `On Error` pin fires **only** for configuration mistakes
(no backend configured, no guardrails, no NPC ids) — **never** for "the model misbehaved". Your
Blueprint never needs a "no quest" branch.

## Guardrail Config reference

`UEverloreGuardrailConfig` (Primary Data Asset) — the single source of truth for what generated
content may contain.

| Property | Type | Purpose |
|---|---|---|
| `Valid Npc Ids` | Name array | Allowed NPC ids. **Must be non-empty** and include your quest giver's id. |
| `Valid Item Ids` | Name array | Allowed item ids. |
| `Valid Region Ids` | Name array | Allowed region/location ids. |
| `Valid Flags` | Name array | Allowed story-flag ids. |
| `Valid Factions` | Name array | Allowed faction ids. |
| `Max Gold` / `Max Experience` | int | Reward caps. |
| `Max Item Quantity` / `Max Reward Budget` | int | Reward caps. |
| `Max Objectives Per Quest` / `Max Objective Count` | int | Objective caps. |

---

## Backends

Configure under *Project Settings → Everlore Engine*. The active backend is `Active Backend Id`.

| Id | Location | Key required | Notes |
|---|---|---|---|
| `Gemini` | Cloud (Google) | Yes | **Default.** Model alias `gemini-flash-latest`; structured output via `responseSchema`. |
| `Ollama` | Local | No | `/api/chat` with schema-constrained `format`. |
| `LlamaServer` | Local | No | llama.cpp `llama-server`, OpenAI `/v1/chat/completions`. |
| `GenericHttp` | Any | Optional | Any OpenAI-compatible URL; set a key env-var name if needed. |

**Switch at runtime** — from Blueprint (`Get Everlore Backend Subsystem → Set Active Backend`) or the
console (`Everlore.SetBackend Ollama hermes3:8b`). List what's registered with `Everlore.Backends`.

### Gemini setup

1. Get an API key from Google AI Studio.
2. Set the environment variable named in `Gemini Api Key Env Var` (default `EVERLORE_GEMINI_KEY`) to
   your key — or, for quick editor testing, run `Everlore.SetKey Gemini <your-key>` in the console.
3. Leave `Active Backend Id = Gemini`. `Gemini Thinking Budget` defaults to `0` (recommended for
   reliable structured output).

### Ollama setup (local, no key)

1. Install Ollama and pull a model: `ollama pull llama3.1:8b` (or any model you like).
2. Set `Ollama Endpoint` (default `http://localhost:11434`) and `Ollama Model` to your pulled model.
3. Set `Active Backend Id = Ollama` (or `Everlore.SetBackend Ollama llama3.1:8b`).

### llama.cpp server setup (local, no key)

1. Run `llama-server -m your-model.gguf` (serves an OpenAI-compatible API on port 8080 by default).
2. Set `Llama Server Endpoint` (default `http://localhost:8080`).
3. Set `Active Backend Id = LlamaServer`.

### Generic OpenAI-compatible endpoint

1. Set `Generic Http Endpoint` to the full chat-completions URL
   (e.g. `https://openrouter.ai/api/v1/chat/completions`) and `Generic Http Model`.
2. If the endpoint needs a key, set `Generic Http Api Key Env Var` to the **name** of the
   environment variable holding it (never the key itself).
3. Set `Active Backend Id = GenericHttp`.

---

## API keys / BYOK

**Keys are never stored in a config file or cooked into your build.** Only the *name* of an
environment variable is ever stored. Keys resolve just-in-time, in strict precedence:

1. **Developer runtime key** — you push it from your own secure store:
   `Backend Subsystem → Set Developer API Key`. **Server-side only — never ship a developer key in a
   client build.**
2. **Player-provided (BYOK)** — the player pastes their own key at runtime:
   `Set Player API Key` / `Clear Player API Key` / `Has Player API Key`. Held in memory on the
   player's machine only.
3. **Environment variable** — the dedicated-server / developer path (default `EVERLORE_GEMINI_KEY`).
4. **Editor test field** — for trying a key in the editor.

**Wiring a BYOK UI (Blueprint):** get the *Everlore Backend Subsystem* (via *Get Engine Subsystem*),
call **Set Player API Key** with the backend id (`Gemini`) and the text the player entered, then gate
your "Generate" button on **Has Key For Active Backend**.

**Quick editor test:** run `Everlore.SetKey Gemini <your-key>` to try a key in PIE without setting
an OS variable and restarting.

> `Is Active Backend Ready` returns true only when the active backend needs no key **or** a key
> actually resolves — so a missing key is obvious instead of silently producing template fallbacks.

---

## Components

### Everlore Quest Giver Component
Drop on an NPC. `Request Quest(Theme, Seed)` is async; the `On Quest Offered(Quest, Outcome)` event
fires with an always-valid quest. `b Authority Only` (default true) keeps generation server-side in
multiplayer.

### Everlore Quest Log Component
Per-player quest state machine and the single commit point.
- **Accept Quest**, **Set/Notify Objective Progress**, **Abandon Quest**.
- Events: `On Quest Accepted / Completed / Failed`, `On Quest Log Updated`, `On Objective Progress`.
- Replicated and server-authoritative. **Rewards are signalled** (via `On Quest Completed`), never
  applied by the plugin — you decide what a reward does.
- Save with **Export/Import Save Data** (portable base64 blob) or **Save/Load Quest Log** (slot).

### Everlore Character Component
Persistent NPC identity: `Character Id`, `Display Name`, `Persona`, `Disposition`, `Known Facts`,
`Memory Blob`. Shared by quests and chat; memory survives sessions (SaveGame). `Remember Fact`,
`Append Memory`.

### Everlore Conversation Component
Free-form roleplay chat.
- **Send Message(PlayerText)** — async; results via events.
- Events: `On Reply Received`, `On Reply Delta` (streaming, when the backend supports it),
  `On Intent`, `On Quest Offered`, `On Chat Error`.
- Reads persona/memory from an Everlore Character Component on the same actor; keeps a rolling
  transcript and summarizes old turns into long-term memory.

---

## Blueprint async nodes

- **Generate Quest (Async)** — inputs: `Guardrails`, `Giver Npc Id`, `Theme`, `Seed`, `b Server Only`
  (default true). Pins: `On Completed`, `On Repaired`, `On Fallback` (each gives `Quest` + `Outcome`),
  `On Error`.
- **Talk To NPC (Async)** — inputs: a `Conversation` component + `Message`. Pins: `On Delta`
  (streamed text), `On Reply`, `On Intent`, `On Error`.

## Roleplay chat & whitelisted intents

An NPC reply may co-emit **one whitelisted intent** from a closed set — `Offer Quest`,
`Remember Fact`, `Set Disposition`, `End Conversation`. Because the set is closed and enforced by the
response schema, a chat can only ever trigger a bounded, safe effect. **Offer Quest** routes the
model's free-text theme through the *same* quest reliability pipeline, so a chat can spawn a
guaranteed-valid quest but never an invalid one. `Remember Fact` and `Set Disposition` only store
text. Old turns are summarized into the character's memory to keep context bounded.

## Editor: batch-generate a quest DataTable

Generate a pool offline, curate it, and ship it as deterministic content (no runtime LLM calls, no
keys, no latency). Run in the console:

```
Everlore.BatchQuests 20 rumors of the old mine
```

This writes a `DataTable` of `FEverloreQuestTableRow` to `/Game/EverloreDemo/DT_EverloreQuests`.
Every row is a pipeline-validated, guaranteed-valid quest you can hand-edit.

## Console commands (editor / non-shipping builds)

| Command | Purpose |
|---|---|
| `Everlore.TestQuest [theme]` | Generate + log one quest via the active backend |
| `Everlore.TestChat [line]` | Roleplay one chat turn with a demo NPC; route an offered quest |
| `Everlore.Backends` | List discovered backends, the active one, and readiness |
| `Everlore.SetBackend <id> [model]` | Switch the active backend (+ model) this session |
| `Everlore.SetKey <id> <key>` | Set a runtime key this session (editor key testing) |
| `Everlore.BatchQuests <n> [theme]` | Bake N quests into a DataTable (editor) |

## Multiplayer

Generation is gated to server authority by default (`b Authority Only` on components,
`b Server Only` on async nodes). Quest state replicates from the server. Generate on the server, or
let players use their own BYOK key. **Never ship a shared developer key inside a client build.**

## Data handling / privacy

Cloud backends (**Gemini**, or a remote **GenericHttp** endpoint) transmit your prompts **and any
player-typed chat** to that third-party service. Local backends (**Ollama**, **llama.cpp server**)
keep all data on-device. You are responsible for disclosing this and obtaining player consent as
required in your game. The plugin logs a one-time reminder at startup. **API keys are never logged,
saved, or included in a request URL** — they are sent only in a request header.

## Troubleshooting

- **Every quest is "Template (Game-Safe)" fallback** → no API key is resolving. Check
  `Everlore.Backends` (readiness) or `Has Key For Active Backend`; set the env var (and restart) or
  use `Everlore.SetKey`.
- **`On Error` fires** → a configuration problem: no guardrails assigned, guardrails have no NPC ids,
  or no backend. Check the `Error` string.
- **Local backend "connection error"** → the local server isn't running or the endpoint/port is
  wrong. Start Ollama / `llama-server` and verify the endpoint in Project Settings.
- **A typo in `Active Backend Id`** → silently means "no active backend". Use exactly one of
  `Gemini`, `Ollama`, `LlamaServer`, `GenericHttp`.

## Support

- **Documentation:** https://wiki.teufel-engineering.com/en/EverLoreEngine
- **Support:** info@teufel-engineering.com
- **Version:** 1.0.0 (UE 5.8)

*Everlore Engine — © 2026 Silvan Teufel / Teufel-Engineering.com. All rights reserved.*
