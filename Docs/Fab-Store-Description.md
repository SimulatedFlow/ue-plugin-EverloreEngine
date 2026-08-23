<!--
  Fab Marketplace store listing for Everlore Engine.
  Copy the sections into the matching Fab form fields:
    • "PRODUCT DESCRIPTION"  -> the DESCRIPTION block
    • "TECHNICAL DETAILS"    -> the TECHNICAL DETAILS block
  (Fab supports light formatting / bullet lists in the description.)
-->

# ==================== TITLE / TAGLINE ====================

**Everlore Engine — AI Quest & Dialogue Director**

Game-safe AI quests, dialogue and NPC roleplay. Every result is validated — it can never break your game.

# ==================== SHORT PITCH (one-liner for the card) ====================

Turn any LLM into a reliable game system: schema-enforced, auto-repaired, always-valid AI quests and
NPC chat. Bring your own backend (Gemini, Ollama, llama.cpp, or any OpenAI-compatible API).

# ==================== DESCRIPTION ====================

**AI content generation, without the risk.**

Large language models are powerful but unpredictable — they hallucinate items that don't exist,
invent broken objectives, and hand you malformed JSON. Shipping that in a game is a nightmare.

**Everlore Engine fixes this.** Every quest, dialogue and NPC reply the AI produces is
**schema-constrained, validated against your own rules, automatically repaired, and — if it still
isn't usable — replaced with a deterministic template.** A generated quest can *never* reference an
id you didn't allow, exceed the reward budget you set, or become uncompletable. Your game logic only
ever sees a **guaranteed-valid** result. No "the AI returned garbage" branch. Ever.

The result is reported with a clear **provenance badge** — *AI-Validated*, *AI-Repaired*, or
*Template (Game-Safe)* — so you always know exactly how a quest came to be.

**Bring your own backend.** Everlore ships no keys and no bundled model. Use Google Gemini in the
cloud, run Ollama or a llama.cpp server locally, or point it at any OpenAI-compatible endpoint
(OpenRouter, LocalAI, vLLM, …). Switch backends at runtime. The exact same reliability guarantee
applies to every one of them — we've verified it live against Google Gemini **and** a local
8-billion-parameter model.

**Bring your own key (BYOK).** Let your players enter their own API key at runtime — keys are held
in memory on their machine only, and are **never cooked into your build, never logged, never saved**.

**Component-first, no C++ required.** Drop an *Everlore Quest Giver* on an NPC, an *Everlore Quest
Log* on the player, wire the **Generate Quest (Async)** node, and you're generating validated quests
from Blueprint in minutes. NPC roleplay chat is the same story: a persona-driven
*Conversation Component* with rolling memory and a **Talk To NPC (Async)** node.

**NPCs that remember, roleplay, and safely act.** Free-form conversation with persona + long-term
memory. NPCs can co-emit a small set of **whitelisted intents** — offer a quest, remember a fact,
change disposition — and the one intent that touches your game (offer quest) is routed through the
same validation pipeline, so a chat can spawn a guaranteed-valid quest but never an invalid one.

**Ship deterministic content too.** Use the built-in editor tool to batch-generate a pool of quests
offline and bake them into a DataTable — hand-curate them and ship with zero runtime LLM calls, zero
keys, zero latency.

**Multiplayer-ready.** Quest state replicates from the server, and generation is gated to server
authority by default so your keys stay server-side.

---

**Key features**

- Guaranteed-valid AI quests — validate → auto-repair → deterministic fallback
- Whitelisted allowed-id + reward-budget guardrails you define per game
- NPC roleplay chat with persona, long-term memory, and safe whitelisted intents
- Bring your own backend: Gemini, Ollama, llama.cpp server, any OpenAI-compatible API
- Bring your own key (BYOK) — player- or developer-supplied, never cooked or logged
- Drop-in components + async Blueprint nodes — no C++ required
- Replicated, server-authoritative quest log with save/load (portable blob or slot)
- Editor batch tool: bake generated quests into a DataTable for deterministic shipping
- Provenance reporting so you can badge AI-Validated / AI-Repaired / Template content

Everlore Engine is the reliability layer that makes generative AI safe to ship.

📖 Documentation: https://github.com/SimulatedFlow
✉️ Support: teufelsilvan@gmail.com

# ==================== TECHNICAL DETAILS ====================

**Features:**

- Schema-enforced quest generation with validate / auto-repair / deterministic-fallback pipeline
- Designer-authored Guardrail Config (allowed NPC/item/region/flag/faction ids + reward & objective bounds)
- Four gameplay components: Quest Giver, Quest Log, Character (persona + memory), Conversation (chat)
- Async Blueprint nodes: "Generate Quest (Async)" and "Talk To NPC (Async)"
- Free-form NPC roleplay chat with rolling/summarized memory and whitelisted structured intents
- Multi-backend registry: Gemini, Ollama, llama.cpp server, Generic OpenAI-compatible HTTP
- Runtime backend switching + Bring-Your-Own-Key (developer, player-BYOK, env-var, editor-test tiers)
- Replicated, server-authoritative quest state machine with objective progress and save/load
- Editor batch tool to bake generated quests into a DataTable (console: `Everlore.BatchQuests`)
- Dev console commands: `Everlore.TestQuest`, `Everlore.TestChat`, `Everlore.Backends`, `Everlore.SetBackend`, `Everlore.SetKey`

**Code Modules:**

- EverloreCore (Runtime)
- EverloreTransport (Runtime)
- EverloreLlamaLocal (Runtime; Win64 — reserved placeholder for future in-process inference)
- EverloreEditor (Editor)

**Number of Blueprints:** 0 (C++ plugin; example Blueprints are provided in documentation)
**Number of C++ Classes:** ~18 UCLASS (4 gameplay components, 2 async nodes, 1 engine subsystem, 5 backends + abstract base, guardrail data asset, settings, save game, secret provider) plus 25+ Blueprint-exposed structs and 15+ enums
**Network Replicated:** Yes (quest log state)
**Supported Development Platforms:** Windows (Win64)
**Supported Target Build Platforms:** Windows (Win64)
**Supported Engine Versions:** 5.8
**AI/Cloud service note:** Requires a user-supplied LLM backend. Cloud backends (Google Gemini, or any
remote OpenAI-compatible endpoint) send prompts and player chat to that third-party service, subject
to that provider's terms; local backends (Ollama, llama.cpp) keep data on-device. No API keys or
model binaries are included with the plugin.
**Documentation:** https://github.com/SimulatedFlow
**Support:** teufelsilvan@gmail.com

*Everlore Engine — © 2026 Silvan Teufel. All rights reserved.*
