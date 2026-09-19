# Phase 2: Config Path, Locale and Struct-Size Safety - Context

**Gathered:** 2026-09-19
**Status:** Ready for planning
**Mode:** Smart discuss (autonomous) — three forks put to the user, the rest defaulted with rationale

<domain>
## Phase Boundary

A consumer in any binding opens a database pointing at a UI config directory **anywhere on disk**,
in a **locale of its choosing** — unblocking claw, whose config lives at
`<installRoot>/database/ui` and whose read sandbox cannot reach `<db_dir>/ui/`. Separately, any
binding whose hardcoded FFI layout constants have drifted from the native library **fails loudly at
load** instead of writing past a caller-owned buffer.

In scope: the `DatabaseOptions` growth (8 → 24 bytes, a deliberate C ABI break), its four FFI
builders, `has_ui_config()` reaching every layer, the C API size accessors, the four load-time
assertions, and the CLI host flags.

Out of scope: structured attribute metadata (Phase 3), collection/group metadata (Phase 4),
`validate_ui_config()` (Phase 5). No new UI *rendering* — Phase 1 owns the reports.

</domain>

<decisions>
## Implementation Decisions

### Config path and locale

- **D-01 (user decision): an explicit config directory loads even for a `:memory:` database.**
  Phase 1's `:memory:` short-circuit narrows to guarding the **convention path only**. The guard
  exists because `fs::path(":memory:").parent_path()` is empty, so `parent_path() / "ui"` resolves
  against the process working directory (`build/bin` under the test runner) — that ambiguity does
  not exist for a directory the caller named outright. The short-circuit must therefore move to
  *after* the explicit-override check and *before* the convention-path computation
  (`src/ui_config.cpp:380` and `:386` respectively). The existing
  `MemoryDatabaseNeverLoadsUiConfig` test must keep passing unchanged — it plants a `ui/` in the
  cwd and asserts `has_ui_config()` stays false, which is still true when no explicit directory is
  given.
- **D-02:** two fields are appended to `quiver_database_options_t`:
  `const char* ui_config_dir` then `const char* ui_locale`. Resulting layout on a 64-bit target:
  `read_only`@0, `console_level`@4, `ui_config_dir`@8, `ui_locale`@16 — **`sizeof` == 24**.
  This is the number every binding must agree on.
- **D-03:** `NULL` on either pointer means "not specified" — convention path for `ui_config_dir`,
  `"en"` for `ui_locale`. `convert_database_options` (`src/c/database_options.h:11-16`) needs a NULL
  guard on both before constructing a `std::string`; today it uses designated initializers and
  would construct from a null pointer. `quiver_database_options_default` (`src/c/options.cpp:5-7`)
  is a **positional** aggregate initializer `{0, QUIVER_LOG_INFO}` and must gain the two NULLs.
- **D-04:** C++ `DatabaseOptions` gains `std::string ui_config_dir` (empty = unset) and
  `std::string ui_locale = "en"`. Empty-string-as-unset mirrors the C NULL mapping with no
  `std::optional` marshalling at the boundary.
- **D-05: an explicit directory that is absent or malformed degrades — it does not throw — but logs
  at `warn`, not `debug`.** `open()` never throwing is a Phase 1 invariant worth keeping (D-24/D-25,
  PARSE-11). But an absent *convention* path is the normal state for every non-PSR database
  (hence `debug`), whereas an explicit path the caller typed and got wrong is a caller error that
  must be visible. Same degradation, louder log. `has_ui_config()` reports false either way.
- **D-06:** the locale-resolution chain is unchanged from Phase 1 — bare string wins outright,
  then exact locale, then `en`, then first key in map order, then empty string. Only the `locale`
  argument becomes caller-supplied. `UIConfigSet::from_directory` already takes a `locale`
  parameter; `src/ui_config.cpp:395` is its **only** call site and currently hardcodes `"en"`.

### Struct-size safety

- **D-07:** one size accessor per struct, no parameters, returning a plain `size_t` — Bun can call
  that shape (it cannot call `quiver_database_options_default`, which returns a struct by value,
  bun#6139). Three accessors: options, `quiver_scalar_metadata_t`, `quiver_group_metadata_t`.
- **D-08 (user decision): each binding asserts at library load and throws on mismatch.** All three
  structs are checked the moment the library opens, so a version-skewed install fails immediately
  and totally rather than corrupting a metadata read far from the cause. Accepted consequence: a
  consumer who never touches metadata still hard-fails on skew. That is the intent — a silent
  stride bug is worse than a loud refusal.
- **D-09:** the mismatch message is **locally crafted in each binding** and must name the struct and
  both numbers (expected vs native). This is the documented exception to "all error messages live
  in C++": the C API cannot diagnose a disagreement about its own layout, exactly as the boolean
  wrappers craft their own conversion errors.
- **D-10:** JS's three hardcoded constants **stay** (`SCALAR_METADATA_SIZE = 56`,
  `GROUP_METADATA_SIZE = 32`, and a new options-size constant) and are *asserted* against the
  accessors — SAFE-02's wording is "asserts its hardcoded struct size against the native value".
  Both numbers are correct today; verified field-by-field against
  `include/quiver/c/database.h:320-336`.
- **D-11:** `makeDefaultOptions` allocates from **named offset constants** with a field-order
  comment. `allocPtrOut` and `allocUint64Out` in the same file also allocate `new Uint8Array(8)` for
  wholly unrelated reasons (pointer-out and u64-out params) and must be **provably unchanged** — a
  search-and-replace over "8" in that file is itself the bug. Pin them with a test.

### Binding surface

- **D-12:** `ui_config_dir` and `ui_locale` are optional parameters on `open`, `from_schema` and
  `from_migrations` in all five bindings, matching the existing `read_only` / `console_level`
  pattern exactly (Julia `build_quiver_database_options`, Dart `_makeOptions` ×4 call sites,
  Python `_make_options`, JS `makeDefaultOptions`).
- **D-13:** `has_ui_config()` gains a C symbol (`quiver_database_has_ui_config`) and is bound in
  Julia, Dart, Python, JS and Lua. This is OPT-04, deliberately deferred from Phase 1 by D-22.
- **D-14:** Lua's host is `quiver_cli`. `LuaRunner` takes an already-configured `Database&` and has
  no options channel of its own, so the new surface is two flags on `src/cli/main.cpp`
  (`--ui-config-dir`, `--ui-locale`) consumed before `Database` construction, alongside
  `--read-only` / `--log-level`. `LuaRunner` itself is unchanged.
- **D-15 (user decision):** `bindings/dart/test/test.bat` clears `.dart_tool/hooks_runner/` and
  `.dart_tool/lib/` before running. Today it is literally `dart test %*` with no cache
  invalidation, and native-assets keys its cache on a checksum that does not cover the hook's own
  defines — so on an ABI-changing phase the suite can pass against the **old** struct layout.
  Accepted cost: a rebuild per run.
- **D-16:** generator discipline differs per binding and must be followed exactly — Julia's
  `c_api.jl` is **regenerated** (`bindings/julia/generator/generator.bat`); Dart's `bindings.dart`
  is **hand-edited, never regenerated** (a full ffigen regen flips enums and breaks Hub); Python's
  `_c_api.py` cdef is **hand-edited** (CFFI ABI mode — a stale cdef corrupts silently with no
  compile error); JS's `loader.ts` symbol table is hand-written and gains the three accessors.

### Proving the value crossed the FFI

- **D-17:** criterion 1 demands a **locale-specific label read back**, not merely that `open()`
  returned. The `foresight_like` fixture from Phase 1 already carries `es`/`pt` labels
  (`Regresión Lineal`, `Ingenuo Estacional`, `Ingênuo Sazonal`) whose `en` resolutions differ — so
  opening it with `ui_locale = "es"` and reading a label that differs from the `en` one is the
  proof, per binding. No new fixture is needed for the locale half; an explicit-directory fixture
  (a `ui/` tree that is *not* beside its database) is new.

### Release

- **D-18:** ships as a **minor** bump — 0.10.7 → **0.11.0**. A `0.x` minor signals breaking changes
  and this is a deliberate C ABI break. Dispatch the Bump Version workflow; do not hand-edit the
  five manifests. `CHANGELOG.md` gets a **BREAKING**-prefixed entry saying what a caller must do.
- **D-19:** the C header edit lands **before** any of the four FFI options builders; those four are
  then independent of each other and may be planned as parallel work.

### Claude's Discretion

- Exact accessor spelling, offset-constant names, and test names.
- Whether the three JS size constants live in `ffi-helpers.ts` or stay in `metadata.ts`.
- How each binding's load-time assertion is structured internally, provided it throws a named error
  naming the struct and both numbers (D-09).

</decisions>

<code_context>
## Existing Code Insights

### Reusable assets
- `UIConfigSet::from_directory(ui_dir, locale, logger)` (`src/ui_config.h:64-70`) **already takes a
  locale** — Phase 2 changes its single call site, not its signature.
- `Database::has_ui_config()` (`include/quiver/database.h:230`, `src/ui_config.cpp:404-407`) exists
  and is documented in-source as "C++-only in this phase; the C API and every binding gain it in
  Phase 2 (OPT-04)".
- The `foresight_like` fixture already carries mixed `en`/`es`/`pt` labels — the locale proof needs
  no new vocabulary, only a new explicit-directory fixture layout.
- `read_only` / `console_level` give a complete, four-binding worked example of the exact plumbing
  pattern the new fields follow.

### Established patterns
- Per-method FFI boilerplate is the house style in Dart and Python — do not collapse it.
- Dart's `_makeOptions` is hand-written and duplicated at **four** call sites (`database.dart:50`,
  `:68`, `:97`, `:140`); all four need the new fields.
- `quiver_database_options_default()` returns the struct **by value**, which is precisely why Bun
  cannot call it and JS must hand-roll the buffer.

### Integration points
- `Database::Impl::require_ui_config()` (`src/ui_config.cpp:370-399`) is where the override
  intercepts: `:memory:` short-circuit at `:380`, convention path computed at `:386`, locale
  hardcoded at `:395`. D-01 reorders the first two.
- Load-time assertion seams: JS `initLibrary`/`getSymbols` after `dlopen` (`loader.ts`), Python
  `load_library()` after `ffi.dlopen` (`_loader.py`), Dart after `DynamicLibrary` open
  (`library_loader.dart`), Julia end of `__init__()` (`c_api.jl:52-60`).

</code_context>

<specifics>
## Specific Ideas

- claw is the concrete consumer driving OPT-01: its config lives at `<installRoot>/database/ui`
  and its read sandbox cannot reach it, so the convention path is unusable there.
- `bindings/js/src/ffi-helpers.ts` `makeDefaultOptions` gets **its own plan**. It is the only place
  in the repo where a wrong number is a native out-of-bounds write with no compile error, no
  exception, no generator and no fallback symbol.
- No binding currently has any test that would catch an FFI layout mismatch — confirmed across all
  four suites. Criterion 5 requires at least one per binding that a wrong layout would actually
  fail.

</specifics>

<deferred>
## Deferred Ideas

- Generating the JS symbol table / size constants from the C headers instead of hand-maintaining
  them — a real fix for the whole hazard class, but a build-system change well beyond this phase.
- A CI job that runs the Dart native-assets hook (today none does, on any OS; Dart is built and
  published by hand).
- Making `assert_version.py` or CI verify that the four generated/hand-edited FFI layers actually
  match the current headers.

</deferred>
