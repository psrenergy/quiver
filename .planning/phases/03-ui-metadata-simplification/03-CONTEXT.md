# Phase 3: UI Metadata Simplification - Context

**Gathered:** 2026-09-20
**Status:** Ready for planning
**Mode:** Discuss — three forks put to the user; the rest settled by the scope correction already recorded in ROADMAP.md

> **This file replaces the CONTEXT.md for the deleted "Structured Attribute Metadata" phase.**
> That phase's decisions (D-30…D-43 in the previous revision) are void. Its plans 03-01 and 03-02
> shipped real code, which this phase reverts; their PLAN and SUMMARY files are kept as history.

<domain>
## Phase Boundary

Make `describe` the **only** consumer of the `ui/` sidecar, and return `quiver_database_options_t`
to its published 8-byte layout — so the milestone ships with no FFI surface change, no ABI delta
against v0.10.7, and a patch release instead of a minor one.

Four removals, all seven layers:

1. `ui_config_dir` — replaced by deriving `ui/` as a sibling of the migrations directory
2. `ui_locale` — replaced by a file-local `constexpr kLocale = "en"`
3. `has_ui_config()` — answered instead by the `UI config:` header `describe` already prints
4. The structured attribute-metadata surface that plans 03-01/03-02 shipped — the public
   `UIMetadata`/`UIEnumEntry` types, `quiver_ui_metadata_t`, the three C++ getters, the C API
   surface, the Python decoder

In scope: the four removals, the `parent(migrations)/ui` derivation, the options struct shrink
across four bindings, re-tuning the four struct-size gates, the CHANGELOG rewrite, and the
0.11.0 → 0.10.8 version bump.

Out of scope: **SAFE-01…03 are not touched** (see D-08). Phase 4's collection/group rendering.
Anything in the claw repo. Any change to `<db_dir>/ui` convention behaviour beyond adding the
derivation alongside it.

</domain>

<decisions>
## Implementation Decisions

### Path resolution (user decisions)

- **D-01:** `ui/` is **assumed to be a sibling of the migrations directory**. When a migrations
  path is known, the sidecar directory is `parent_path(migrations_path) / "ui"`. Verified against
  the consumer: claw resolves migrations as `join(configDir, "..", "migrations")`, so `migrations/`
  and `ui/` are guaranteed siblings in its layout.
- **D-02:** (user decision) **No precedence machinery, and no change to `<db_dir>/ui` behaviour.**
  The existing convention path stays exactly as it is today. The derivation applies when a
  migrations path is known; the convention applies otherwise.
  — **Why this needs no precedence rule, verified rather than assumed:** every one of the 13
  fixture directories under `tests/schemas/ui/` is opened with `from_schema`
  (`tests/test_ui_fixture.h:23`), with the database built inside the fixture dir next to `ui/`.
  **Not one has a migrations directory.** The derivation and the convention are therefore mutually
  exclusive in every case that exists today, so "which wins" is a question with no live instance.
  If a database ever has both and they differ, that is a **future milestone's** question — the user
  ruled explicitly that it is not this phase's.
- **D-03:** The derivation lives in `Database::migrate_up`, set **before the nothing-to-apply early
  return**. This covers `from_migrations` and a manual `open` + `migrate_up` in one place. The
  early-return detail is load-bearing: claw's normal case is an *already-migrated* database, so a
  hint set after the early return would never fire for the actual consumer.
  — *Claude's call.* The user ruled out adding a path argument to `open()` ("lets not change the
  path"); between `migrate_up` and `from_migrations`, `migrate_up` strictly dominates because it
  covers both entry points and adds no argument anywhere.
- **D-04:** The **no-migrations gap is accepted.** claw falls back to `Database.open(dbPath)` when
  `findMigrationsDir` returns null (`C:/Development/Claw/claw1/src/core/db.ts:13`), and neither the
  derivation nor `<db_dir>/ui` reaches the model sidecar on that path. Accepted because that path
  already has no schema metadata either — it is an existing degraded mode, not a new one.
- **D-05:** The `:memory:` early return stays **unconditional**. With the derivation in place, a
  `validate_migrations` throwaway in-memory database would otherwise load the model's real sidecar
  for nothing.

### Rendering (user decision)

- **D-06:** The `(locale: <locale>)` suffix is **dropped** from the header. `write_ui_header`
  (`src/database_describe.cpp:29-35`) emits `UI config: <path>` and nothing more.
  — **Consequence accepted:** this changes `describe` / `describe_collection` /
  `summarize_collection` output for every database that *has* a sidecar, and updates 6 test
  assertions. It is **not** a DESC-05 violation — that rule governs the no-sidecar case, which stays
  byte-identical because the whole header is still gated on `ui` being engaged.
  — Chosen over keeping a constant `(locale: en)` because a report advertising a knob that no longer
  exists is precisely the stale surface this correction removes.
- **D-07:** That header is also the **replacement for `has_ui_config()`**. It fires from all three
  reports (`src/database_describe.cpp:209, 226, 240`), so `report.find("UI config:") != npos`
  answers the same question and names the path — strictly more informative than the bool. The 19
  Phase 1 assertions that used `has_ui_config()` are rewritten against it. Three of those
  (`tests/test_database_ui_describe.cpp:213, 288, 298`) are bare trigger calls between
  `CaptureStderr`/`GetCapturedStderr` and become `db.describe();` — a one-token swap.

### What is kept, deliberately

- **D-08:** **SAFE-01…03 survive untouched** — the four `*_sizeof` accessors and the four load-time
  struct-size gates. They sit in the same phase as the retired OPT block but are independent of UI
  metadata, cost nothing ongoing, and already caught a real defect: a merge that silently dropped
  two struct fields from `bindings/julia/src/c_api.jl` and broke the entire Julia suite at load.
  `quiver_database_options_sizeof` stays valid at 8 bytes; the other three never had anything to do
  with UI.
- **D-09:** OPT-06's **Dart cache-clearing is kept** despite OPT-06 being retired.
  `bindings/dart/test/test.bat` clears `.dart_tool/hooks_runner/` and `.dart_tool/lib/`; native
  assets key their cache on a checksum that does not cover the hook's own defines, so without it an
  ABI-changing run can pass against the **old** layout. This phase is exactly such a run.
- **D-10:** Two assertions in `tests/test_database_ui_metadata.cpp` are **migrated, not deleted**:
  the declared-but-empty vocabulary case (a present map key holding an empty vector — structurally
  different from `empty_enum`'s zero-byte-file case) and the 4-key `format` collapse. Neither is
  covered anywhere else. Both move to `tests/test_database_ui_parse.cpp` before the file is deleted.

### Execution discipline

- **D-11:** **Forward-delete, never `git revert`.** Reverting the Phase 2 option plumbing hits 6
  conflicts, every one at a seam where a later struct-size-gate commit edited adjacent lines — all
  resolving the same way (keep the `_sizeof` lines, drop the ui lines). That is 13 revert commits
  plus 6 hand-resolutions to reach a state one forward-delete commit reaches.
- **D-12:** **Do not run `/gsd-undo`.** `.planning/.phase-manifest.json` does not exist, so it falls
  back to git-log matching: `--phase 3` hits 32 commits (20 from already-merged milestones),
  `--phase 2` hits 102, truncated at 50.
- **D-13:** **Do not drop `ui_locale` alone.** Locale-only leaves the options struct at 16 bytes —
  still an ABI break, still every binding edit and every size gate, for half the deletion. The two
  option fields go together or not at all.
- **D-14:** Removing an exported C symbol while a binding still names it is a **load failure, not a
  compile error** (Bun `dlopen` throws at `bindings/js/src/loader.ts:45`; CFFI resolves lazily at
  `_c_api.py:60`; Dart `lookup` throws at `bindings.dart:234`). There is no cross-layer
  symbol-coverage test, so a half-done removal ships silently. Each C symbol's deletion and its
  binding references must land in the **same plan**.
- **D-15:** Plan order, because each step shrinks the next one's surface: (a) delete the structured
  surface; (b) `ui_config_dir` + derivation; (c) `ui_locale` + `kLocale`; (d) `has_ui_config` +
  the 19 assertion rewrites; (e) shrink the options struct across four bindings and re-tune the
  gates; (f) CHANGELOG + `assert_version.py bump patch`.

### Claude's Discretion

- Whether `ui_dir_hint` is a `std::filesystem::path` or a `std::string` on `Impl`.
- Whether the derivation is a private `Impl` member or a parameter threaded to
  `UIConfigSet::from_directory`.
- Exact wording of the CHANGELOG entry, subject to D-16 below.
- Whether the parser keeps `UIConfigSet::locale` as a dead member or removes it (D-13 only requires
  the *option* to go).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### The scope correction that created this phase
- `.planning/ROADMAP.md` §"Phase 3: UI Metadata Simplification" — the six success criteria.
- `.planning/ROADMAP.md` §Coverage Notes → "Scope Correction" — why 20 requirements were retired.
- `.planning/REQUIREMENTS.md` — the struck-through OPT / META / VALID sections carry the reasoning
  per block.

### What is being deleted
- `include/quiver/options.h:23-27`, `include/quiver/c/options.h:18-27`, `src/c/options.cpp:9-17`
  (two offset static_asserts), `src/c/database_options.h:10-19` (the NULL/empty guard),
  `src/database_impl.h:65-67`, `src/database.cpp:96-97`, `src/ui_config.cpp:376-407` (the
  explicit-override branch), `src/cli/main.cpp:102-107` (`--ui-config-dir` / `--ui-locale`).
- `include/quiver/ui_metadata.h` (whole file — types move back into `src/ui_config.h` as private),
  `quiver_ui_metadata_t` and its nine offset static_asserts, `quiver_ui_metadata_sizeof`,
  `quiver_database_get_attribute_ui_metadata`, `quiver_database_free_ui_metadata`, the vocabulary
  getters, `Database::get_attribute_ui_metadata`.
- `include/quiver/database.h:240-243`, `src/ui_config.cpp:418-425`,
  `include/quiver/c/database.h:41-44`, `src/c/database.cpp:45-52`, `src/lua_runner.cpp:536-537`
  (`has_ui_config`), plus one wrapper per binding and `bindings/js/src/lua-api.ts:153`.
- Whole test files losing their subject: `tests/test_database_ui_options.cpp`,
  `tests/test_lua_runner_ui_options.cpp`, the four binding `*_ui_options` suites.
  **Move each one's struct-size-gate cases out to a struct-size test file FIRST** — those survive.

### What is being changed, not deleted
- `src/ui_config.cpp:55-70` `resolve_localizable` — loses the exact-locale leg and the parameter.
  The **bare-string-wins** and **first-key** legs stay: `foresight_like` entry 4 has no `en` key and
  is the only coverage of the first-key leg.
- Six parse signatures lose the `locale` parameter: `src/ui_config.h:51,58,63` and
  `src/ui_config.cpp:103,150,183,256`.
- `src/database_describe.cpp:29-35` `write_ui_header` — drops the `(locale: …)` suffix (D-06).
- `bindings/js/src/ffi-helpers.ts:64-88` `makeDefaultOptions` — the tail-allocation body collapses
  back to a fixed 8-byte buffer with two `setInt32`s. This was the riskiest edit in the milestone;
  it is now a deletion.

### Ground truth for the derivation
- `C:/Development/Claw/claw1/src/core/study.ts:160` — `join(configDir, "..", "migrations")`, the
  sibling guarantee D-01 rests on.
- `C:/Development/Claw/claw1/src/core/study.ts:169-182, 280-289` — the sidecar at
  `<modelPath>/database/ui` vs the session copy at `<studyDir>/.claw/<db>/<session>/original/`.
  This is why `<db_dir>/ui` alone never served claw.
- `C:/Development/Claw/claw1/src/core/db.ts:13` — the no-migrations fallback D-04 accepts.

### Fixtures and tests
- `tests/test_ui_fixture.h:18-32` — every UI fixture opens via `from_schema` with the db beside
  `ui/`. **No fixture has migrations**; this is the evidence behind D-02.
- `tests/schemas/ui/foresight_like/` — keep its es/pt data as-is; entry 4 is the first-key leg's
  only coverage.
- `tests/schemas/ui_golden/` — golden baselines. Check whether any pins the header line before
  landing D-06.

### Project rules
- `CLAUDE.md` (root) — error patterns, cross-layer naming, the "Do Not Fix" list.
- `src/c/CLAUDE.md` "String Handling" — the C API allocates with `quiver::string::new_c_str`
  (`new char[]`) and frees with `delete[]`. **Never** strdup/malloc/free. This bit the previous
  incarnation of this phase in review.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- The `UI config:` header already exists and already fires from all three reports — D-07's
  replacement for `has_ui_config()` needs no new code, only new assertions.
- The four struct-size gates already exist and already work; they turn this phase's options-struct
  shrink from a silent-corruption risk into a loud load-time failure if a binding is missed.

### Established Patterns
- Per-binding generator discipline is unchanged and non-negotiable (02-CONTEXT D-16): Julia's
  `c_api.jl` is **regenerated**, Dart's `bindings.dart` is **hand-edited, never regenerated**,
  Python's cdef is **hand-edited** (ABI mode — a stale cdef corrupts silently), JS's symbol table
  is hand-written.
- Phase 1's degradation policy holds throughout: a sidecar problem logs and degrades, never throws.

### Integration Points
- `Impl::require_ui_config()` is the single resolution point; the derivation feeds it a hint.
- `Database::migrate_up` is the new hint's only writer (D-03).

</code_context>

<specifics>
## Specific Ideas

- The user's framing, worth preserving verbatim because it is the phase's whole test: *"the only
  important thing here is that describe can access this info."* Any plan that adds a public surface
  has misread this phase.

</specifics>

<deferred>
## Deferred Ideas

- **Precedence between the derived path and `<db_dir>/ui`** when a database has both and they
  differ — explicitly deferred by the user to a future milestone (D-02).
- **Covering claw's no-migrations `open()` path** — D-04. If it ever matters, the fix is an argument
  on `open()`, not a field in the options struct.
- **A structured metadata getter** — retired (META-01…06). If a consumer ever genuinely needs one,
  the previous phase's design work survives in git history at commits `1c1bcca`…`810e7b3` and in
  the superseded CONTEXT at `aa08c78~1`.
- **`validate_ui_config()`** — retired (VALID-01…08). The struck-through requirements remain in
  `REQUIREMENTS.md` as the record of what a future validator should check.
- **Non-English locales** — retired with OPT-02. Hub ships `[en, es, pt]`, so this may genuinely
  return; if it does, the cheaper shape is a setter, not an options field.
- **`.planning/.phase-manifest.json` does not exist** — unrelated to this phase, but it makes
  `/gsd-undo` destructive on this repo (D-12). Worth fixing via `/gsd-health` at some point.

</deferred>

---

*Phase: 3-UI Metadata Simplification*
*Context gathered: 2026-09-20*
