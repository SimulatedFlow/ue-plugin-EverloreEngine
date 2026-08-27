# Everlore Engine — AI Quest & Dialogue Director

**Schema-enforced, validated, game-safe AI quests, dialogue and NPC roleplay for Unreal Engine 5.8.**

Bring your own LLM backend (Google Gemini, Ollama, llama.cpp, or any OpenAI-compatible HTTP
endpoint). Everything an LLM returns is **schema-constrained, validated, auto-repaired, and — if
it still isn't usable — replaced by a deterministic template**, so generated content can *never*
put your game into an invalid state. A generated quest always references only ids you allow, stays
within the reward budget you set, and is completable.

> The reliability layer is the point. An 8-billion-parameter local model and Google Gemini both
> produce a *guaranteed-valid* quest through the same pipeline — one may pass clean, the other may
> be auto-repaired, but your game only ever sees a valid result.

---

## Requirements

- Unreal Engine **5.8**
- A backend:
  - **Google Gemini** (cloud) — a Google AI Studio API key, *or*
  - **Ollama** (local) — [ollama.com](https://ollama.com), no key, *or*
  - **llama.cpp server** (local, `llama-server`) — no key, *or*
  - **Any OpenAI-compatible endpoint** (OpenRouter, LocalAI, vLLM, Together, …)

The plugin ships **no** API keys and **no** third-party binaries. Keys are always supplied by you
at runtime and are never cooked into your build (see **API keys / BYOK**).

## Installation

1. Copy `EverloreEngine/` into your project's `Plugins/` folder.
2. Enable **Everlore Engine** in *Edit → Plugins* (or in your `.uproject`) and restart the editor.
3. Open *Project Settings → Plugins → Everlore Engine* and choose your backend + model.

## 5-minute quick start (quests)

1. **Allowed vocabulary.** Create a Data Asset of type **Everlore Guardrail Config**
   (`Add → Miscellaneous → Data Asset → EverloreGuardrailConfig`). Fill `Valid Npc Ids`,
   `Valid Item Ids`, `Valid Region Ids` (and optionally flags/factions) with your game's ids, and
   set the reward/objective bounds. The LLM may only use these ids.
2. **A quest giver.** On an NPC actor, add an **Everlore Quest Giver Component**. Set `Giver Npc Id`
   (must be in the config's `Valid Npc Ids`), assign your `Guardrails` asset, and an optional
   `Default Theme`.
3. **A player log.** On your player pawn/state, add an **Everlore Quest Log Component**.
4. **Generate.** In Blueprint call **Generate Quest (Async)** (Guardrails + Giver Npc Id + Theme),
   or call `RequestQuest` on the giver component and bind `On Quest Offered`. On a result pin, pass
   the quest to the log component's **Accept Quest**.
5. **Set a key** (Gemini only) — see below — and hit Play.

Prefer to see it working headlessly first? With a key set, run the console command
`Everlore.TestQuest a haunted mill` and watch the log for `=== Quest`.

## The reliability guarantee

Every generation runs through: **parse → validate → auto-fix → re-validate → deterministic
fallback → commit**. The outcome is reported as an **`EEverloreProvenance`** value so you can show
players (or your QA) exactly how a quest came to be:

| Provenance | Meaning |
|---|---|
| `Authored` | Hand-made / from a DataTable |
| `LlmValidated` ("AI-Validated") | Generated and passed validation as-is |
| `LlmRepaired` ("AI-Repaired") | Generated, then auto-corrected to be valid |
| `TemplateFallback` ("Template (Game-Safe)") | LLM output unusable → deterministic template |

The **Generate Quest (Async)** node exposes these as separate exec pins (`On Completed`,
`On Repaired`, `On Fallback`), plus `On Error` — which fires **only** for configuration mistakes
(no backend, no guardrails), never for "the model misbehaved". Your Blueprint never needs a
"no quest" branch.

## Backends

Configure under *Project Settings → Everlore Engine*. The active backend is `ActiveBackendId`.

| Id | Where | Key? | Notes |
|---|---|---|---|
| `Gemini` | Cloud (Google) | Yes | Default. Model alias `gemini-flash-latest`; structured output via responseSchema. |
| `Ollama` | Local | No | `/api/chat` with schema-constrained `format`. Set endpoint + a pulled model. |
| `LlamaServer` | Local | No | llama.cpp `llama-server`, OpenAI `/v1/chat/completions`. |
| `GenericHttp` | Any | Optional | Any OpenAI-compatible URL; set a key env-var name if it needs one. |

Switch at runtime from Blueprint (`Get Everlore Backend Subsystem → Set Active Backend`) or the
console (`Everlore.SetBackend Ollama hermes3:8b`). List what's registered with `Everlore.Backends`.

## API keys / BYOK

**Keys are never stored in a config file or cooked into your build.** Only the *name* of an
environment variable is stored. Keys are resolved just-in-time, in this precedence:

1. **Developer runtime key** — you push it from your own secure store: subsystem
   `Set Developer API Key`. Server-side only; **never ship a developer key in a client build.**
2. **Player-provided (BYOK)** — the player pastes their own key at runtime: subsystem
   `Set Player API Key` / `Clear Player API Key` / `Has Player API Key`. Held in memory on the
   player's machine only.
3. **Environment variable** — the dedicated-server / developer path. Default `EVERLORE_GEMINI_KEY`
   (name configurable per backend). The key lives only in the process environment.
4. **Editor test field** — for trying a key in the editor.

**Quick editor test:** run `Everlore.SetKey Gemini <your-key>` in the console to try your key in PIE
without setting an OS variable and restarting. Check readiness with the subsystem's
`Has Key For Active Backend` (or `Everlore.Backends`).

`Is Active Backend Ready` returns true only when the active backend needs no key **or** a key
actually resolves — gate your "Generate" button on it so a missing key is obvious.

## Components

- **Everlore Quest Giver Component** — drop on an NPC. `RequestQuest(Theme, Seed)` → async → the
  `On Quest Offered(Quest, Outcome)` event fires with an always-valid quest. `bAuthorityOnly`
  (default true) keeps generation server-side in multiplayer.
- **Everlore Quest Log Component** — per-player quest state machine and the single commit point:
  `Accept Quest`, `Set/Notify Objective Progress`, `Abandon Quest`, plus events
  (`On Quest Accepted/Completed/Failed`, `On Quest Log Updated`). Replicated; server-authoritative.
  Rewards are *signalled* via `On Quest Completed`, never applied by the plugin. Save with
  `Export/Import Save Data` (portable blob) or `Save/Load Quest Log` (slot).
- **Everlore Character Component** — persistent NPC identity (persona, disposition, known facts,
  rolling memory). Shared by quests and roleplay chat; memory survives sessions (SaveGame).
- **Everlore Conversation Component** — free-form roleplay chat (below).

## Roleplay chat & whitelisted intents

Add an **Everlore Conversation Component** to an NPC (it reads persona/memory from an Everlore
Character Component on the same actor). Call `Send Message(PlayerText)`, or use the
**Talk To NPC (Async)** node. Events: `On Reply Received`, `On Reply Delta` (streaming, when the
backend supports it), `On Intent`, `On Quest Offered`, `On Chat Error`.

The NPC replies in character, and — optionally — co-emits **one whitelisted intent** (a closed set:
`Offer Quest`, `Remember Fact`, `Set Disposition`, `End Conversation`). Intents are constrained by
the response schema, so a chat can only ever trigger a bounded, safe effect. The one that touches
game state, **Offer Quest**, is routed through the *same* quest reliability pipeline — so a chat can
spawn a guaranteed-valid quest but never an invalid one. Old turns are summarized into the
character's long-term memory to keep context bounded.

## Editor: batch-generate a quest DataTable

Generate a pool of quests offline, curate them, and ship them as deterministic content (no runtime
LLM calls). Console command:

```
Everlore.BatchQuests 20 rumors of the old mine
```

writes a `DataTable` of `FEverloreQuestTableRow` to `/Game/EverloreDemo/DT_EverloreQuests`. Every
row is a pipeline-validated, guaranteed-valid quest.

## Console commands (editor / non-shipping)

| Command | Purpose |
|---|---|
| `Everlore.TestQuest [theme]` | Generate + log one quest via the active backend |
| `Everlore.TestChat [line]` | Roleplay one chat turn with a demo NPC; route an offered quest |
| `Everlore.Backends` | List discovered backends + active one + readiness |
| `Everlore.SetBackend <id> [model]` | Switch active backend (+ model) this session |
| `Everlore.SetKey <id> <key>` | Set a runtime key this session (editor testing) |
| `Everlore.BatchQuests <n> [theme]` | Bake N quests into a DataTable (editor) |

## Multiplayer

Generation is gated to server authority by default (`bAuthorityOnly` on components,
`bServerOnly` on the async nodes). Quest state replicates from the server; generate on the server
(or let players use their own BYOK key) and never ship a shared developer key in a client build.

## Data handling / privacy

Cloud backends (**Gemini**, or a remote **GenericHttp** endpoint) transmit your prompts **and any
player-typed chat** to that third-party service. Local backends (**Ollama**, **llama.cpp server**)
keep all data on-device. You are responsible for disclosing this and obtaining player consent as
required in your game. The plugin logs a one-time reminder at startup. API keys are never logged,
saved, or included in a request URL (they go in a request header only).

## Support & version

- **Version:** 1.0.0 (UE 5.8)
- **Documentation:** <https://github.com/SimulatedFlow/documentation>
- **Support:** teufelsilvan@gmail.com

---

*Everlore Engine — © 2026 Silvan Teufel. All rights reserved.*

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- **Buy on Fab** (this plugin): https://www.fab.com/listings/9071cfba-70d8-4a7f-8c1e-957e4a11f3ab
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
