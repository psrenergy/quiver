# Project Research Summary

**Project:** Quiver — UI Metadata Layer
**Domain:** Brownfield C++20 library milestone — a TOML sidecar reader threaded through an existing C API and five FFI bindings
**Researched:** 2026-09-17
**Confidence:** HIGH

## Executive Summary

This is not a greenfield product. It is a milestone on a shipped library whose stack, dependencies,
error patterns, naming contract and release ritual are already settled. **Recommendation #1 from
STACK.md is: add nothing.** `tomlplusplus` v3.4.0 is already vendored and linked `PRIVATE` to the
`quiver` core target, and `src/binary/binary_metadata.cpp` is a working, shipped precedent for
exactly the shape needed (a `.toml` sidecar read beside a data file, split into
`from_toml_content` / `from_toml_file`). A UI-config parser in `src/` compiles with **zero CMake
change**. The research effort therefore went not into choosing technology but into mapping the
*constraints* the existing choices impose: what ships for free, what breaks the ABI, and what
silently corrupts memory when a hand-written number drifts.

The single most consequential finding is a **build-order lever**: `describe`,
`describe_collection` and `summarize_collection` already return `std::string` through trivial
`new_c_str` wrappers in the C API. Rendering enum labels into those three reports closes the
triggering defect — the histogram that prints `values {0: 8, 1: 4}` where labels belong, in a code
path whose own constant comment calls it *"the enum/category case"* — and reaches **all five
bindings and Lua with no ABI change, no new C symbol, and no binding code at all**. It ships as a
patch. The `DatabaseOptions` ABI break (`ui_config_path` + `ui_locale`, 8 -> 24 bytes) delivers a
different capability to a different consumer and must not be entangled with it.

The main risks are of two kinds. **Mechanical:** four of the five bindings mirror the C ABI by hand
(JS symbol table, Python CFFI ABI-mode cdef, Dart hand-edited ffigen output), and in JS the size
constants are *out-buffer allocations* handed to C — a stale one is a native write past a JS-owned
`Uint8Array`, with no compile error and no generator to catch it. Only Julia fails loudly.
**Semantic:** the TOML corpus is hand-maintained, unversioned, untested by any CI anywhere, and
already contains at least one inverted label (`HasCommitment`: Julia says `YES = 0`, the TOML says
`0 = "Disable"`). A reader alone hands that inversion to an LLM agent that will reason on it.
`validate_ui_config()` is the only mitigation and ships last by explicit decision — so the window is
a recorded, accepted state, not a discovery.

## Key Findings

### Recommended Stack

Nothing is acquired. Every technology is present, linked and exercised by shipped code. The useful
output is the linkage fact that decides *where* code goes: `tomlplusplus` is `PRIVATE` on `quiver`,
which does not propagate — `quiver_c`, `quiver_cli` **and `quiver_tests`** cannot include toml++
without a new link line. Parsing therefore belongs in the core, and the C++ test suite must exercise
the parser through Quiver's public API, never by constructing `toml::table`s. Do not add a toml++
link line to the test target to make testing easier.

**Core technologies:**
- **C++20 / CMake >= 3.26** — one parser in the core serves Lua and all five bindings; five parsers is the rejected alternative (root `CLAUDE.md`: logic in C++, bindings thin)
- **tomlplusplus v3.4.0** — already `PRIVATE` on `quiver`; four API calls cover the whole job (`toml::parse`, `as_array()`, `value<T>()`, `as_table()` + table iteration for `enum.toml`, whose top-level keys *are* the vocabulary ids)
- **SQLite3 v3.50.2 + the existing `Schema`** — group membership is recovered from table names (`{Collection}_vector_{id}`, `_time_series_{id}`), PRAGMA-derived as always; Quiver has never parsed a byte of SQL text and must not start
- **spdlog v1.17.0** — `impl_->logger` already exists on `Impl`; unknown keys log a warning, never throw. That log line is the ecosystem's only drift detector
- **sol2 v3.5.0** — one converter beside `scalar_metadata_lua` (`src/lua_runner.cpp` L1166) covers every Lua metadata entry point

**Explicitly not used:** any new TOML library, a TOML dependency in any binding, SQL-text parsing,
JSON anywhere in this path, a directory scan of `ui/`, new fields on `ScalarMetadata` /
`GroupMetadata`, an ffigen regeneration, or a `version` key in the TOML (zero of 311 files carry one).

### Expected Features

**Must have (table stakes — the ABI-free first phase):**
- **F1 UI sidecar parser in C++** — `main.toml` -> the files named by `main.collections` -> `enum.toml`, locale resolved once at parse time. Ten non-negotiable tolerances, all *intra*-model variation the single Dart parser already absorbs
- **F2 enum vocabulary + code->label bound to scalars** — the knowledge the agent is missing
- **F3 enum labels in `summarize_collection`'s histogram** — **the triggering defect; PROJECT.md's Core Value; if only this ships, the milestone succeeded**
- **F3b labels / units / enum vocabulary in `describe` / `describe_collection`** — the same three format strings, the same phase
- **F4 graceful degradation + `has_ui_config()`** — without it, v1 is a breaking change for every existing caller
- **F5 convention path `<db_dir>/ui/`** — correct for 100% of the surveyed corpus; **this is what makes v1 ABI-free**

**Should have (the real work for the committed consumer):**
- **F6 `ui_config_path` + `ui_locale` on `DatabaseOptions`** — the ABI break; unblocks claw, whose config dir is `<installRoot>/database/ui` and whose read sandbox cannot reach it at all
- **F7 structured `get_ui_metadata` getter in a *new* C struct** — the committed consumer: `Claw/claw/src/core/study-config.ts` already carries the comment *"enriching it from the TOML belongs in quiverdb"*

**Defer (elaboration + the advisory validator):**
- **F8 collection-level metadata + `main.collections` display order** — knowledge SQL cannot express at all
- **F9 `[[attribute_group]]` metadata absorbing both `date_time` spellings** — a third of the corpus loses its dimension metadata if only one spelling is handled
- **F10 `validate_ui_config()`** — ships last by explicit decision; the first thing in the ecosystem that checks the sidecar against anything

**Anti-features, settled in PROJECT.md and not re-opened here:** `[[card]]`, `themes/*.toml`,
`[[attribute_query]]`, `[[scalar_tab]]`, `[[card]].conditions`, `[[enum item]].hide`, string-keyed
vocabularies (HTD), write-side scaffolding, enum enforcement on write, two live locales.

### Architecture Approach

The layer is threaded through Quiver's existing shape without altering it: a new value-type header
(`include/quiver/ui_metadata.h`) and one new translation unit (`src/database_ui_config.cpp`, the only
toml++ TU outside `src/binary/`) feed a third lazy member on `Database::Impl`, parsed inside
`load_schema_metadata` beside `schema` and `type_validator` and published by move only on success.
Three format strings in `src/database_describe.cpp` render the enrichment **append-only**. Structured
access leaves through a brand-new `quiver_ui_metadata_t` with its own size constant and its own free
function — never as extra fields on the 56-byte / 32-byte structs four bindings mirror by hand.

**Major components:**
1. **`src/database_ui_config.cpp` (new)** — parse, resolve locale once, warn on unknown keys, never throw; lives in the `quiver` target because that is the only one that can see toml++
2. **`Database::Impl`** — owns the parsed config as a third lazy member; the UI dir path is set by the **constructor** (the load runs long after `open()` returned) and the publish-nothing-until-valid invariant extends to it in both directions
3. **`src/database_describe.cpp`** — three append-only format strings; a no-op when no config is present, which is what keeps today's output byte-identical
4. **New C struct + getters + five binding decoders + one Lua converter** — modelled on `convert_scalar_to_c` / `free_scalar_fields` for the converter pair, `quiver_csv_options_t` for grouped parallel arrays, `quiver_database_free_time_series_data` for the free signature
5. **`tests/schemas/ui/` fixture corpus** — distilled from the real 117 files. ARCHITECTURE.md's judgement: **worth more than the parser**, because it is the only artifact that can arbitrate against Hub

Four structural rules carry the design: a new struct never new fields (memory safety); locale
collapses at parse time so the `string | table` union never crosses a boundary (the largest free
simplification); structure from SQL, decoration from TOML — group membership is joined against table
names, never inferred from declaration order; and append-only rendering under a byte-identical
baseline.

### Critical Pitfalls

1. **JS `makeDefaultOptions` hardcodes an 8-byte options struct** (`bindings/js/src/ffi-helpers.ts`) — growing `quiver_database_options_t` to 24 bytes makes C write 16 bytes past a Bun-owned buffer. Bun cannot call `quiver_database_options_default` (struct-by-value, bun#6139), so JS has **no generated fallback**, and there is no JS generator at all. *Careless-grep trap, verified in-repo:* the file contains **three** `new Uint8Array(8)` — only L11 is the options struct. Avoid: keep the first phase off this file entirely (convention path); give this one edit its own plan plus a C API size accessor returning `sizeof(...)` as a plain `size_t` that Bun *can* call, asserted at load.
2. **`SCALAR_METADATA_SIZE = 56` / `GROUP_METADATA_SIZE = 32` are out-buffer allocations, not read strides** (`bindings/js/src/metadata.ts` L30-31, allocated at L81/96/111/126). Same corruption class, invisible in review. Avoid: a new `quiver_ui_metadata_t`; treat any edit to `attribute_metadata.h` or those two C typedefs as a stop sign for this milestone.
3. **Silent ABI drift in Python and Dart** — Python's cdef *is* the layout in CFFI ABI mode (no compile step, no error); Dart's `bindings.dart` must be hand-edited because an ffigen regen flips enums and breaks Hub, and stale `.dart_tool/hooks_runner/` + `.dart_tool/lib/` make the suite silently link the old native. Avoid: generator-diff and cache-clear as written plan steps, plus one test per binding that a wrong layout would actually fail.
4. **Quiver becomes an authoritative repeater of unchecked labels** — `HasCommitment` is inverted between Julia and the TOML today, nothing in the ecosystem catches it, and after the first phase an LLM agent reasons on it. Avoid (not resequence — the ordering is settled): always render the **code beside the label**, never the label alone; write the accepted window into the phase artifacts; run `validate_ui_config()` against HTD as its acceptance case.
5. **Breaking `describe` output — while four binding suites give a false green** — `tests/test_database_describe.cpp` L84 and L104 are `EXPECT_FALSE` assertions that trip on *any* brace-list after a float scalar or any literal `values {`, wherever it lands. The four binding describe suites assert only "returns a String". Avoid: the byte-identical-without-a-config rule protects the old output; the new output needs new fixtures with exact-string assertions in the C++ and Lua suites, and a written call on whether the binding suites get strengthened or left honest.

Also load-bearing and cheap to get wrong: write the parser against Hub's **Dart source**, not the
`toml-schema.md` doc (which is already wrong about group membership, affecting the 15 interleaved
files); load collections only from `main.collections` (SCE's `agent.toml` is a fully-formed orphan);
never index a vocabulary by code (GNoMo's `weekday` is 1-based, two others are gapped `[0, 2]`);
accept `format` as string **or** 4-key table from day one (four lines, and no corpus file exercises
the branch); render the **full vocabulary** structurally so unobserved codes are not lost; and
reconcile `CHANGELOG.md` (heads `[0.10.4]` against `CMakeLists.txt` 0.10.6) before any bump dispatch.

## Implications for Roadmap

### The build-order constraint that matters most

**The enum rendering in `describe` / `describe_collection` / `summarize_collection` closes the
triggering defect with NO ABI change and NO binding work, so it sequences first. The
`DatabaseOptions` ABI break is isolated and sequences after.**

The mechanism: `describe*` already returns a `std::string` through existing `new_c_str` wrappers
(`src/c/database.cpp` L129/141/155). Enum labels ride that already-bound string into Julia, Dart,
Python, JS **and** Lua for free. `quiver_scalar_metadata_t` stays 56 bytes,
`quiver_group_metadata_t` stays 32, `quiver_database_options_t` stays 8. No new C symbol, no
`loader.ts` entry, no cdef edit, no `bindings.dart` hand-edit, no generator run, no layout audit.
It ships as a **patch**.

What makes it possible is the convention path (`<db_dir>/ui/`), which is correct for 100% of the
surveyed corpus. The explicit `ui_config_path` option is a *sequencing* addition, not a
prerequisite — **do not couple them**. Coupling drags the highest-risk edit in the milestone, four
hand-maintained ABI mirrors, a minor bump and the full release ritual into the phase that delivers
all of the triggering value.

The fallback property is the point: **if the milestone stalls after this phase, the defect is
closed in every layer and nothing is broken.** That is the reason it goes first and the reason the
ABI phase must not be folded into it.

### Sequence

**Phase 1 — Parser + `Impl` wiring + enum rendering (ABI-free, ships as a patch)**
Rationale: everything depends on the parser; the renderer needs a config on `Impl`; and this triple
is the whole Core Value. Delivers F1+F2+F3+F3b+F4+F5 plus the `tests/schemas/ui/` corpus.
Addresses: the triggering defect in all six layers. Avoids: Pitfalls 1, 2, 3 entirely (the diff
touches no `bindings/` file), and Pitfall 5 is *recorded* here, not solved.
Strictly sequential inside: parser -> `Impl` wiring -> rendering.
Prepend a one-file reconcile task: `CHANGELOG.md` -> 0.10.6.

**Phase 2 — `DatabaseOptions` ABI break (minor bump, full release ritual)**
Rationale: pointless before there is something to point at; isolated so the defect fix does not wait
on it. Delivers F6. The C header edit lands first, then the four FFI options builders.
**The JS `makeDefaultOptions` edit deserves its own plan** — see below.

**Phase 3 — Structured getter (`quiver_ui_metadata_t` + five binding decoders + Lua)**
Rationale: the committed consumer (claw dropping the per-attribute half of `study-config.ts`).
Delivers F7. Note the research finding that F7 *technically* does not need F6 — the convention path
already supplies a config — but a getter claw cannot point at its own UI dir is useless to the one
consumer it exists for, so they ship in the same release. Freeze the C header before any binding
starts, or five decoders get re-edited simultaneously.

**Phase 4 — Collection-level and `[[attribute_group]]` metadata**
Rationale: pure elaboration once Phase 3's struct/converter/free idiom exists; the parse work was
already done in Phase 1. Delivers F8 + F9, including both `date_time` spellings absorbed behind one
answer.

**Phase 5 — `validate_ui_config()`**
Rationale: ships last by explicit PROJECT.md decision; needs F2 + F8 + F9 to have anything to check.
It is **not a stretch goal** — it is the only mitigation that will ever exist for Pitfall 5, and its
first run has a known expected output on real repos (HTD's `inflow_type`, the 9 untyped `Configuration`
flags, `Interconnection`, SCE's orphan `agent.toml`, the 4 dead vocabularies).

### What can parallelize across bindings, and what cannot

**Can parallelize:**
- Inside Phase 1: the parser and the fixture corpus are separate work items (a BESSOperation slice for the all-bare-string case, a Foresight slice for en/es/pt, an HTD slice for the plain-`date_time`-attribute spelling).
- **Phases 2 and 3 are independent of each other** once Phase 1's `Impl` wiring lands. They touch disjoint files even in JS (`ffi-helpers.ts` vs a new `ui-metadata.ts`). They should *ship* in the same release because both are native changes, but they can be built concurrently.
- Inside Phase 2: the four FFI options builders (Julia `build_quiver_database_options`, Dart `_makeOptions`, Python `_make_options`, JS `makeDefaultOptions`) are four independent edits **once the C header is fixed**.
- Inside Phase 3: all five binding decoders, once the new C header is final.
- Phases 4 and 5 are independent of each other.

**Cannot parallelize:**
- Parser -> `Impl` wiring -> rendering. No renderer without a config on `Impl`; no config without a parser.
- The C header edit precedes every binding edit that mirrors it, in both Phase 2 and Phase 3.
- The release is sequential and whole: five manifests -> `publish-s3` -> tag -> Julia/Python/JS in parallel -> **Dart published by hand** (no CI job runs `hook/build.dart` on any OS). Dart is the leg with no automation and the easiest to declare "shipped" while it is not.
- Any phase that adds a `db:` Lua name must edit `src/lua_runner.cpp` and `bindings/js/src/lua-api.ts` as **one** edit — `bindings/js/test/lua-api-sync.test.ts` is a hard build gate that fails as an unrelated JS error in a C++ phase.

### The one edit that deserves its own plan

**`bindings/js/src/ffi-helpers.ts` `makeDefaultOptions`.** It is the only place in the repo where a
wrong number is a native out-of-bounds write with no compile error, no exception, no generator and
no fallback symbol (Bun cannot call the struct-by-value default-options function). It allocates
`new Uint8Array(8)` and writes `setInt32(0)` / `setInt32(4)`; the options struct grows to 24 bytes.
The file contains **two other** `new Uint8Array(8)` allocations (`allocPtrOut`, `allocUint64Out`)
that must not be touched — a search-and-replace over "8" in this file is itself the bug. The plan
should carry: the size **and** offset changes, a C API size accessor returning a plain `size_t`
asserted at load, named offset constants with a field-order comment (as `metadata.ts` does), and a
JS test that round-trips a value *through* the new option rather than merely calling `open()`.

### Research flags

Phases likely needing deeper investigation during planning:
- **Phase 1:** the parser must be written against ~719 lines of Dart across 15 classes
  (`Hub/hub1/lib/models/configuration/*.dart` + `lib/models/utils/toml_utils.dart`), key by key. The
  only written spec is known-wrong. Budget the read; cite the Dart class per key in the plan.
- **Phase 2:** not research so much as a per-binding layout audit with a verification step each.
- **Phase 5:** the validator's rule set is not yet enumerated — what counts as drift, and what it
  says about each kind.

Phases with established patterns (skip deeper research):
- **Phase 3 and 4:** the converter/free/decoder idiom exists five times over in the C API and every
  binding; the shape precedents are named (`convert_scalar_to_c` / `free_scalar_fields`,
  `quiver_csv_options_t`, `quiver_database_free_time_series_data`).

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Every version, path and constant read out of the repo or quoted verbatim; nothing from memory. toml++ linkage CONFIRMED by direct read of `src/CMakeLists.txt` and `cmake/Dependencies.cmake` |
| Features | HIGH / MEDIUM | HIGH on format shape, parse feasibility and build cost (two adversarial verifications, one CONFIRMED / one REFUTED-and-corrected, plus a file-by-file cost survey). MEDIUM on consumer uptake past claw — Hub consuming Quiver's reader is explicitly not a milestone goal |
| Architecture | HIGH | Every component, file path and line anchor read from the repo or the two survey journals; the publish-nothing-until-valid invariant and the JS out-buffer hazard both verified in source |
| Pitfalls | HIGH | The JS and CHANGELOG hazards were re-verified directly in-repo while writing PITFALLS.md |

**Overall confidence:** HIGH

### Gaps to Address

- **The format is a moving target with no owner.** 311 `database/ui/*.toml` across ~30 repos, zero
  version keys, no CI anywhere, Hub's parsers at 59 commits (`view = "chart"` added 2026-06-24) and
  Foresight's TOMLs edited *today*. Handle during execution by: ignoring unknown keys with a logged
  warning (the drift detector), building the fixture corpus from real files, and recording in the
  phase artifacts that hub1 is what this was surveyed against — `format_configuration.dart` already
  differs hub1 vs hub3.
- **The 4-key table form of `format` is unexercised by any corpus file.** No amount of real-file
  fixturing can warn you. Handle by writing the branch in Phase 1 anyway (four lines) and fixturing
  the table form synthetically.
- **The four binding describe suites prove nothing today** ("returns a String"). Handle by making an
  explicit, written call in Phase 1 — strengthen them or leave them honest — and never counting them
  as five-layer coverage in a phase audit.
- **Label correctness is unverifiable within Quiver.** The `HasCommitment` inversion has no technical
  recovery inside this repo: the data is right, the label is wrong. Handle by always emitting the
  code beside the label, so a consumer can be corrected without a Quiver release.

## Sources

### Primary (HIGH confidence)
- `C:/Development/Quiver/quiver3/.planning/PROJECT.md` — the settled scope, constraints and Key Decisions. Authoritative; nothing in this research re-opens it
- `.planning/research/STACK.md`, `FEATURES.md`, `ARCHITECTURE.md`, `PITFALLS.md` — the four documents synthesized here
- Direct repo reads (all quoted values re-verified): `CMakeLists.txt`, `CHANGELOG.md`, `cmake/{Dependencies,Platform}.cmake`, `src/CMakeLists.txt`, `src/binary/binary_metadata.cpp`, `src/database_describe.cpp`, `src/database_impl.h`, `src/schema.cpp`, `src/lua_runner.cpp`, `src/c/{database.cpp,database_helpers.h,database_options.h,options.cpp}`, `include/quiver/{options.h,attribute_metadata.h}`, `include/quiver/c/{database.h,options.h}`, `bindings/js/src/{ffi-helpers.ts,metadata.ts,csv.ts,loader.ts,lua-api.ts}`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/dart/lib/src/ffi/bindings.dart`, `bindings/julia/src/c_api.jl`, `tests/test_database_describe.cpp`, `tests/test_lua_runner_describe.cpp`
- `C:/Development/Hub/hub1/lib/models/configuration/*.dart` + `lib/models/database.dart` + `lib/models/utils/toml_utils.dart` — **the de facto format spec**; write the parser against this
- Survey journal `.../subagents/workflows/wf_92804be2-64e/journal.jsonl` — key inventory across 117 files / 13 models, format owner, layer-by-layer Quiver build cost, plus two adversarial verifications (`verify:one-parser-possible` -> **CONFIRMED**; `verify:no-owner` -> **REFUTED**, two live consumers exist). Preferred wherever the two journals differ
- Survey journal `.../subagents/workflows/wf_e5d70f00-efb/journal.jsonl` — the triggering defect, `kMaxDistributionCardinality = 64` and its "enum/category case" comment, Foresight's DDL comments, claw's tool surface and sandbox roots

### Secondary (MEDIUM confidence)
- `C:/Development/Claw/claw/src/core/study-config.ts`, `src/tools/describe-data.ts`, `src/prompt.ts`, `src/tools/native.ts`, `src/core/study.ts` — the committed consumer's current shape and its sandbox limits. Its uptake past the structured getters is intent, not shipped code
- The 13-model corpus (`database/ui/` in BESSOperation, Boost, CHain, CarbSteeler, Foresight, GNoMo, HyCO, HydroThermalDispatch, OptBio, SCE, SORA, SORA2, `Templates/PSRExample.jl`) — counts are exact as surveyed; live reach is estimated at 6-8 models

### Tertiary (LOW confidence — do not build on)
- `C:/Development/Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md` — titles itself "authoritative reference" and is **already wrong** about group membership; also documents dead keys. Listed only so nobody rediscovers it and trusts it

---
*Research completed: 2026-09-17*
*Ready for roadmap: yes*