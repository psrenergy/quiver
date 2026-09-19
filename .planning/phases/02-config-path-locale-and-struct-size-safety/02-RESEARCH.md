# Phase 2: Config Path, Locale and Struct-Size Safety - Research

**Researched:** 2026-09-19
**Domain:** C ABI struct growth across five FFI bindings; load-time layout self-checks; lazy
config-path/locale threading in the C++ core
**Confidence:** HIGH (every claim below was checked by reading the actual source this session, not
recalled from the recon or from training data — file paths and line ranges are cited throughout)

## Summary

Phase 2 has exactly one dangerous fact — `quiver_database_options_t` grows from 8 to 24 bytes by
appending two `const char*` fields — and four independent places that must agree on the number 24.
Everything else in the phase (the `DatabaseOptions`/`Impl` threading, the `require_ui_config()`
reorder, the three size accessors, the five load-time assertions) is mechanical once that number is
locked, **except** one hazard the CONTEXT.md recon did not name: JS's `makeDefaultOptions` today
returns a single `Allocation` built entirely from primitive `DataView` writes; growing it to carry
two pointer fields requires it to also **own two child string allocations that must stay reachable
for the duration of the FFI call** — the same `{ table, keepalive }` shape `allocNativeStringArray`
already uses elsewhere in the file. Fixing only the byte count (8→24) without adding a keepalive
array produces code that passes a byte-count review and then segfaults or reads garbage on the
first non-empty `uiConfigDir`, because Bun's GC is free to collect the string buffer between
`toCString()` returning and the FFI call reading the pointer stored in the options buffer.

Two release-mechanics facts also need to be planned explicitly, because they are not stated in
CONTEXT.md and both are easy to get wrong: (1) `CHANGELOG.md` already carries an **unreleased**
`## [0.10.7]` section (Phase 1's patch entry) that was never released — `CMakeLists.txt` is still
`0.10.6` — so D-18's minor bump does **not** chain a patch release first; it renames that heading to
`0.11.0`, folds the BREAKING entry into the same unreleased section, and dispatches Bump Version
once with `part=minor` (0.10.6 → 0.11.0 directly, patch reset to 0, confirmed against
`scripts/assert_version.py`'s bump table). (2) Dart's generated bindings resolve every native
symbol **lazily** (`late final _fooPtr = _lookup<...>('foo')`), not eagerly at construction —
verified by reading `bindings.dart:26-27` — so D-08's load-time assertion for Dart must **actively
call** the new size accessor right after `bindings` is built; it cannot rely on construction-time
symbol resolution to fail on an old native lib the way one might assume from other FFI generators.

**Primary recommendation:** land the header edit first (D-19), thread `DatabaseOptions` through the
one C++ constructor that already fans out to `from_schema`/`from_migrations`/`open` (all three
delegate to `Database::Database(path, options)` — verified, `src/database.cpp:94,255,286`), reorder
`require_ui_config()` by moving the `:memory:` guard below a new explicit-override branch, add
three trivial `size_t`-returning C accessors next to their structs' existing alloc/free functions,
and give JS's `makeDefaultOptions` a `{ options, keepalive }` return shape from day one so the
string-lifetime hazard never has a chance to exist.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| `DatabaseOptions` struct growth (C++ + C ABI) | C++ Core / C API | — | Single source of truth for the layout; every binding mirrors it, never redefines it independently |
| Config-path override + locale resolution | C++ Core (`Database::Impl`) | — | `require_ui_config()` already owns this; bindings only pass two new strings through existing options plumbing |
| Struct-size accessors (`SAFE-01`) | C API | — | Pure `sizeof()` wrappers; no C++ business logic involved |
| Load-time layout assertion (`SAFE-02/03`) | FFI Binding (each of 5) | — | Each binding owns its own hardcoded constants and its own native-library load sequence; the C API cannot know what a binding hardcoded |
| `has_ui_config()` binding surface (`OPT-04`) | C API → FFI Binding | — | Trivial bool getter, mirrors `is_healthy`/`in_transaction`'s existing `int* out` pattern |
| CLI host flags (`--ui-config-dir`, `--ui-locale`) | CLI (`quiver_cli`) | — | `LuaRunner` takes an already-configured `Database&`; it has no options channel of its own |

## Package Legitimacy Audit

Not applicable — this phase adds no new external dependency to any of the five ecosystems (npm,
PyPI, crates.io not used, Julia General registry, pub.dev). It only grows an existing C struct and
adds C accessor functions consumed by already-vendored FFI machinery (`bun:ffi`, CFFI, `dart:ffi`,
Clang.jl-generated bindings). No `package-legitimacy check` run was needed.

## Standard Stack

No new libraries. The phase is entirely internal C ABI plumbing over the existing toolchains:

| Component | Version (verified this session) | Role |
|-----------|-----------------------------------|------|
| tomlplusplus | 3.4.0 (unchanged, already `PRIVATE`-linked) | Unused directly by this phase — `UIConfigSet::from_directory` already exists |
| Clang.jl generator | via `bindings/julia/generator/generator.bat` (`julia +1.12.5`) | Regenerates `c_api.jl` for the grown struct — **verified locally installed**: `juliaup status` shows `1.12.5` present as an installed channel on this machine, so the generator can run without a fresh Julia download |
| ffigen | 20.1.1 (pinned in `pubspec.yaml`'s `ffigen:` block) | **Not run this phase** — `bindings.dart` is hand-edited per the root "Do Not Fix" rule (a full regen flips the three C enums from `abstract class` int constants to real Dart `enum`s and breaks Hub) |
| CFFI | ≥2.0.0 (ABI mode) | `_c_api.py`'s cdef is hand-edited; `ffi.dlopen`, not `ffi.verify`/API mode, so no compiler runs at install |

**Installation:** none — no `npm install`/`pip install`/`cargo add` needed for this phase.

## Architecture Patterns

### System Architecture Diagram

```
Caller (any binding)
   │  open(path, { readOnly, consoleLevel, uiConfigDir?, uiLocale? })
   ▼
Binding-side options builder                       ← D-12: 4 hand-written builders + 1 generated
  (makeDefaultOptions / _make_options /                (Julia regenerated struct + hand-written
   _makeOptions / build_quiver_database_options)        build_quiver_database_options wrapper)
   │  writes a 24-byte quiver_database_options_t
   │  (2 pointers now: ui_config_dir, ui_locale)
   ▼
C API: quiver_database_open / _from_schema / _from_migrations
   │  convert_database_options() — NULL-guards both new pointers (D-03)
   ▼
C++: Database::Database(path, options)              ← src/database.cpp:94 — SINGLE constructor;
   │  impl_->ui_config_dir = options.ui_config_dir;     from_schema/from_migrations delegate here,
   │  impl_->ui_locale     = options.ui_locale;         so this is the ONLY wiring point needed
   ▼
   (returns; ui_config is NOT loaded yet — lazy)
   ...later, first describe()/has_ui_config() call...
   ▼
Database::Impl::require_ui_config()                 ← src/ui_config.cpp:370 — D-01 reorder here:
   │  1. if (!ui_config_dir.empty())  →  use override, skip :memory: guard entirely
   │  2. else if (path == ":memory:") →  debug-log, return (unchanged behavior)
   │  3. else                          →  convention path = parent_path(path) / "ui"
   │  4. exists/is_directory check     →  debug (convention) vs warn (explicit override, D-05)
   │  5. UIConfigSet::from_directory(dir, impl_->ui_locale, logger)   ← locale now caller's, not "en"
   ▼
UIConfigSet cached on Impl, consumed by describe()/describe_collection()/summarize_collection()
(Phase 1 renderer, unchanged) and by has_ui_config() (new C symbol, OPT-04)


Separately — struct-size safety (SAFE-01..03), independent of the flow above:

Native library on disk (libquiver_c.{so,dylib,dll})
   │  exports quiver_database_options_sizeof() / quiver_scalar_metadata_sizeof() /
   │          quiver_group_metadata_sizeof()   ← new, trivial, no try/catch (SAFE-01)
   ▼
Binding library-load sequence (once, at first load — NOT per-call)
   │  JS:     after dlopen() in loader.ts's initLibrary()
   │  Python: after ffi.dlopen() in _loader.py's load_library()
   │  Dart:   after QuiverDatabaseBindings(library) in library_loader.dart's `bindings` getter
   │  Julia:  end of __init__() in c_api.jl
   ▼
Compare native size_t against each binding's own hardcoded constant (JS: SCALAR_METADATA_SIZE=56,
GROUP_METADATA_SIZE=32, new OPTIONS_SIZE=24; Dart/Python/Julia: sizeof of their own struct decl)
   │
   ├─ match  → proceed silently
   └─ mismatch → throw/error naming the struct + both numbers (D-09), before any FFI call runs
```

### Recommended file-touch map (not a new project structure — this phase edits existing files)

```
include/quiver/options.h              # DatabaseOptions: + ui_config_dir, ui_locale
include/quiver/c/options.h            # quiver_database_options_t: + 2 const char* fields;
                                       # + quiver_database_options_sizeof() declaration
include/quiver/database.h             # + has_ui_config() already exists (Phase 1); no change here
include/quiver/c/database.h           # + quiver_database_has_ui_config(); + 2 more *_sizeof()
src/c/options.cpp                     # quiver_database_options_default(): {0, QUIVER_LOG_INFO,
                                       #   nullptr, nullptr}; + quiver_database_options_sizeof()
src/c/database_options.h              # convert_database_options(): NULL-guard both new pointers
src/c/database_metadata.cpp           # + quiver_scalar_metadata_sizeof/quiver_group_metadata_sizeof
                                       #   (co-located with existing alloc/free per src/c/CLAUDE.md)
src/c/database.cpp OR database_read.cpp  # + quiver_database_has_ui_config (mirrors is_healthy)
src/database.cpp                      # Database::Database ctor: impl_->ui_config_dir/ui_locale = ...
src/database_impl.h                   # Impl: + std::string ui_config_dir, ui_locale = "en"
src/ui_config.cpp                     # require_ui_config(): D-01 reorder + D-05 log-level branch
src/cli/main.cpp                      # + --ui-config-dir / --ui-locale flags (D-14)
bindings/js/src/ffi-helpers.ts        # makeDefaultOptions(): 24-byte buffer, named offset consts,
                                       #   returns { options, keepalive } (own plan per D-11/D-12)
bindings/js/src/loader.ts             # + 3 new symbols (options/scalar/group sizeof) + has_ui_config
bindings/js/src/database.ts           # 3 call sites: hold keepalive alive across each check(...)
bindings/js/src/types.ts              # DatabaseOptions: + uiConfigDir?, uiLocale?
bindings/python/src/quiverdb/_c_api.py    # cdef: struct + 3 sizeof decls + has_ui_config decl
bindings/python/src/quiverdb/database.py  # _make_options(): + ui_config_dir, ui_locale kwargs
bindings/python/src/quiverdb/_loader.py   # load-time ffi.sizeof(...) assertion, 3x
bindings/dart/lib/src/ffi/bindings.dart   # HAND-EDIT: struct + 3 sizeof fns + has_ui_config fn
bindings/dart/lib/src/database.dart       # _makeOptions() x4 call sites: + uiConfigDir, uiLocale
bindings/dart/lib/src/ffi/library_loader.dart  # load-time assertion after bindings construction
bindings/dart/test/test.bat               # D-15: clear .dart_tool/hooks_runner + .dart_tool/lib
bindings/julia/generator/generator.bat    # RUN to regenerate c_api.jl (struct + 3 sizeof + has_ui_config)
bindings/julia/src/database.jl            # build_quiver_database_options(): + 2 kwargs
bindings/julia/src/c_api.jl                # load-time assertion appended inside __init__()
CHANGELOG.md                              # rename "## [0.10.7] — unreleased" → "## [0.11.0]",
                                           # add BREAKING entry
CMakeLists.txt + 4 manifests               # via Bump Version workflow dispatch (part=minor), NOT hand-edited
```

### Pattern 1: Single C++ wiring point for new options fields

**What:** `Database::from_schema` and `Database::from_migrations` do not construct `Impl` or set
its members themselves — both build a `Database` via the constructor and then call a mutating
method (`apply_schema`/`migrate_up`).
**Verified:** `src/database.cpp:255` (`auto db = Database(db_path, options); db.apply_schema(...)`)
and `:286` (`auto db = Database(db_path, options); db.migrate_up(...)`); the constructor itself is
`src/database.cpp:94-118`.
**When to use:** Any new `DatabaseOptions` field needs exactly one write site —
`Database::Database(...)` — not three. This is why D-19 can parallelize the four FFI options
builders: none of them needs to know about `from_schema` vs `from_migrations` vs `open`, because
the C++ layer already collapsed that fan-out.
**Example:**
```cpp
// src/database.cpp:94 (existing, annotated with the two new lines this phase adds)
Database::Database(const std::string& path, const DatabaseOptions& options) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;
    impl_->logger = create_database_logger(path, options.console_level);
    impl_->ui_config_dir = options.ui_config_dir;   // NEW — consumed lazily by require_ui_config()
    impl_->ui_locale = options.ui_locale;           // NEW — default "en" from DatabaseOptions
    ...
}
```

### Pattern 2: JS pointer-out-parameter keepalive (`{ table, keepalive }`)

**What:** `allocNativeStringArray` already solves "a struct field is a pointer into a
separately-allocated buffer that must not be GC'd before the FFI call reads it."
**Verified:** `bindings/js/src/ffi-helpers.ts` (full file read this session) — the function returns
`{ table: Allocation; keepalive: Allocation[] }`, and its only current caller pattern is documented
in the JS house rule: "For out-params, pass the TypedArray to the FFI call... touching `.buffer`
between `ptr(buf)` and the call materializes/relocates a small JSC typed array, leaving the stored
pointer stale" (`bindings/js/CLAUDE.md`).
**When to use:** `makeDefaultOptions` must adopt exactly this shape once it embeds two `const char*`
fields, because those fields are now the array-of-pointers case in miniature (array length 2 instead
of N).
**Example (new code this phase writes, not existing):**
```typescript
// bindings/js/src/ffi-helpers.ts — proposed shape
const OPT_OFFSET_READ_ONLY = 0;
const OPT_OFFSET_CONSOLE_LEVEL = 4;
const OPT_OFFSET_UI_CONFIG_DIR = 8;
const OPT_OFFSET_UI_LOCALE = 16;
export const OPTIONS_SIZE = 24; // must equal native quiver_database_options_sizeof()

export function makeDefaultOptions(
  options?: DatabaseOptions,
): { options: Allocation; keepalive: Allocation[] } {
  const buf = new Uint8Array(OPTIONS_SIZE);
  const dv = new DataView(buf.buffer);
  dv.setInt32(OPT_OFFSET_READ_ONLY, options?.readOnly ? 1 : 0, true);
  dv.setInt32(OPT_OFFSET_CONSOLE_LEVEL, options?.consoleLevel ?? LOG_LEVEL_INFO, true);
  const keepalive: Allocation[] = [];
  if (options?.uiConfigDir) {
    const alloc = allocNativeString(options.uiConfigDir);
    keepalive.push(alloc);
    dv.setBigUint64(OPT_OFFSET_UI_CONFIG_DIR, nativeAddress(alloc.ptr), true);
  }
  if (options?.uiLocale) {
    const alloc = allocNativeString(options.uiLocale);
    keepalive.push(alloc);
    dv.setBigUint64(OPT_OFFSET_UI_LOCALE, nativeAddress(alloc.ptr), true);
  }
  return { options: { ptr: ptr(buf), buf }, keepalive };
}
```
Every call site in `database.ts` (`fromSchema`, `fromMigrations`, `open` — 3 sites, verified by
grep) must keep `keepalive` in scope until after the `check(lib.quiver_database_...(...))` call
returns, exactly like `allocNativeStringArray`'s existing callers must.

### Anti-Patterns to Avoid

- **Bumping JS's buffer size to 24 without the keepalive array.** This is the phase's single most
  dangerous JS-specific mistake and is *not* the same mistake D-11 names (D-11 is about the
  numeric-8 search-and-replace risk to `allocPtrOut`/`allocUint64Out`). Both risks are real and
  both need their own test.
- **Treating `ui_config_dir.empty()` as "explicit empty string was passed."** C++ `DatabaseOptions`
  uses empty-string-as-unset (D-04) by design — there is no way to distinguish "caller didn't set
  it" from "caller passed `""`" at this layer, and that is intentional (mirrors the C NULL mapping,
  D-03). Do not add a `std::optional<std::string>` to work around this; it was already decided
  against in Phase 1's `UIMetadata` design for exactly this reason (see `01-CONTEXT.md` D-13's
  precedent) and would be inconsistent with `read_only`/`console_level`'s existing non-optional
  shape.
- **Regenerating Dart's `bindings.dart`.** Explicitly forbidden — see Item 6 below and the root
  "Do Not Fix" list.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Locale fallback chain | A second resolver for the `ui_locale` option | `UIConfigSet::from_directory`'s existing `locale` parameter (`src/ui_config.h:64` — signature already takes it) | Phase 2 changes its **one call site** (`src/ui_config.cpp:395`), not its logic — the fallback chain (bare string → exact locale → `en` → first key → empty) is unchanged (D-06) |
| Per-cell struct-size checks scattered through read paths | Ad hoc `if (sizeof(...) != expected)` guards at every metadata read call site | One check at library-load time per struct (D-08) | A single load-time gate makes every later read/write in that process safe by construction; scattering checks means every new read site is a place to forget one |
| A JSON/struct-diffing tool to keep `_c_api.py`/`bindings.dart` in sync with C headers | A build-time schema-diff generator | `generator/generator.py`'s existing diff-aid (Python) and manual re-diff against `include/quiver/c/` (Dart) | Explicitly deferred to v2 (`GOV-03`-adjacent, listed under Phase 2's Deferred Ideas: "Generating the JS symbol table / size constants from the C headers... a build-system change well beyond this phase") |

**Key insight:** every "don't hand-roll" in this phase is really the same insight once: the C++
core and the C API already own the single source of truth (the struct definition, the locale
resolver, the size); the five bindings' job is to *read* that truth via one mechanical accessor
each, never to re-derive or duplicate it.

## Runtime State Inventory

Not applicable — this is not a rename/refactor/migration phase. No stored data, live service
config, OS-registered state, secrets, or build artifacts carry the old struct layout in a way that
survives a rebuild: `quiver_database_options_t` is a stack-allocated, per-call marshaling struct
with no persisted on-disk representation (unlike, say, a schema version stored in a database file).
The struct-size mismatch this phase guards against is a **process-lifetime** hazard (native lib
version A loaded against binding-generated code compiled against header version B), not a
data-migration hazard.

## Common Pitfalls

### Pitfall 1: Assuming Dart's generated bindings fail loudly on a missing symbol at construction time

**What goes wrong:** A plan that says "Dart already fails at load if a symbol is missing, so no
extra assertion code is needed" ships D-08 incompletely for Dart.
**Why it happens:** Other FFI generators (and a plausible mental model of "ffigen probably binds
everything eagerly like a C `dlsym` table") suggest symbol resolution happens once, at
`DynamicLibrary.open` or at binding-object construction. It does not.
**How to avoid:** `bindings.dart:16` shows `QuiverDatabaseBindings(DynamicLibrary dynamicLibrary) :
_lookup = dynamicLibrary.lookup;` — the constructor stores a *lookup function*, and every actual
symbol (`_quiver_versionPtr` etc., line 26) is a `late final` field resolved on **first access**,
not at construction. So `bindings` (the cached getter in `library_loader.dart`) succeeding proves
nothing about any specific symbol's presence. The load-time assertion must *actively call* the new
`quiver_database_options_sizeof()` (etc.) right after building `bindings`, and that call — not the
construction — is what will throw `Invalid argument(s): Failed to lookup symbol` if the native lib
predates this phase.
**Warning signs:** A Dart test that stubs an "old" native lib (or simply doesn't rebuild) still
opens a database successfully with no error until some unrelated metadata call fails confusingly
later.

### Pitfall 2: Renaming the CHANGELOG heading instead of recognizing it needs renaming

**What goes wrong:** D-18 is read as "bump 0.10.7 → 0.11.0," which sounds like two bumps (patch,
then minor). A plan that dispatches Bump Version twice (`part=patch` then `part=minor`) produces a
real, tagged `v0.10.7` release that ships **only** Phase 1's enum-label feature with none of this
phase's content, and then immediately follows it with `v0.11.0`.
**Why it happens:** `CHANGELOG.md` already has an **unreleased** `## [0.10.7]` heading (Phase 1
added it, never dispatched Bump Version for it) — reading "0.10.7 → 0.11.0" as a fact about
committed history rather than about an unreleased changelog heading is an easy misparse.
**How to avoid:** Confirmed this session: `CMakeLists.txt:4` still reads `VERSION 0.10.6` — no
manifest was ever bumped for Phase 1. `scripts/assert_version.py`'s bump table (`minor` →
`f"{major}.{minor+1}.0"`) computes straight from the *current* manifest version, so a single
`part=minor` dispatch takes 0.10.6 → 0.11.0 directly, skipping 0.10.7 entirely. The correct action
is: rename the existing `## [0.10.7] — unreleased` heading (and its compare-link line) to
`## [0.11.0] — unreleased`, append the BREAKING entry to that same section, and dispatch Bump
Version **once** with `part=minor`.
**Warning signs:** Two Bump Version PRs open at once, or a `v0.10.7` tag that ships no
`DatabaseOptions` change existing on the release page.

### Pitfall 3: Treating the `:memory:` guard move as "delete it"

**What goes wrong:** A literal reading of D-01 ("the short-circuit must therefore move to *after*
the explicit-override check") could be implemented as removing the `:memory:` check and relying on
`fs::exists` to fail naturally on a garbage `parent_path()`-derived directory. This silently changes
behavior for the *unset* case (no explicit override, `:memory:` path) from "debug log, clean
return" to "warn log" or worse, depending on what `fs::path(":memory:").parent_path() / "ui"`
resolves to on the caller's cwd.
**Why it happens:** The existing comment at `src/ui_config.cpp:377-379` explains the ambiguity
(`fs::path(":memory:").parent_path()` is empty, so the joined path resolves against the process
cwd) — that hazard is exactly why the guard exists, and it is *not* eliminated by the reorder, only
scoped to the convention-path branch.
**How to avoid:** Keep the `:memory:` check verbatim, just move it to the `else` branch of the new
`if (!ui_config_dir.empty())` guard (pseudocode in the Architecture Patterns section above).
**Warning signs:** `MemoryDatabaseNeverLoadsUiConfig`'s "Leg 1" (schema-path probe,
`tests/test_database_ui_describe.cpp:241-249`) starts failing, or a `:memory:` database run from a
directory that happens to contain a `ui/` subfolder starts loading it.

### Pitfall 4: Forgetting CFFI resolves symbols lazily too, so an "old native lib" test needs the call, not the dlopen

**What goes wrong:** A Python load-time assertion written as `try: ffi.dlopen(path) except
Exception: raise ...` never actually exercises the new size-mismatch path, because `ffi.dlopen`
in ABI mode does not eagerly bind every symbol named in the cdef.
**Why it happens:** CFFI's ABI-mode `dlopen` (unlike API mode, which compiles a real extension)
resolves each `lib.<name>` attribute access individually via `dlsym`-on-demand; the failure mode for
a missing symbol is an `AttributeError` raised at the **call site**, not at `dlopen()`.
**How to avoid:** The load-time assertion must call `lib.quiver_database_options_sizeof()` (etc.)
explicitly inside `load_library()`, wrapped to distinguish "symbol missing → native lib older than
this binding" from "symbol present but value differs → native/binding version skew" (both are
real, distinct failure modes D-08's fresh-install question is asking about).
**Warning signs:** A test that "proves" the assertion works by pointing `_loader.py` at a stale
`.so` build directory but the test passes anyway because the exception path was never reached.

## Code Examples

### The three size accessors (SAFE-01), matching the C API's existing "trivial getter" exception

```cpp
// src/c/options.cpp — colocated with quiver_database_options_default per existing file layout
// Source: existing style at src/c/options.cpp (read this session, full file)
QUIVER_C_API size_t quiver_database_options_sizeof(void) {
    return sizeof(quiver_database_options_t);
}
```
```cpp
// src/c/database_metadata.cpp — colocated with convert_scalar_to_c / convert_group_to_c per
// src/c/CLAUDE.md's "Alloc/Free Co-location" convention (read this session)
QUIVER_C_API size_t quiver_scalar_metadata_sizeof(void) {
    return sizeof(quiver_scalar_metadata_t);
}
QUIVER_C_API size_t quiver_group_metadata_sizeof(void) {
    return sizeof(quiver_group_metadata_t);
}
```
These three join the existing five-function exception list in `src/c/CLAUDE.md`'s "Error Handling"
section ("Exceptions: `quiver_get_last_error`, `quiver_version`, `quiver_clear_last_error`,
`quiver_database_options_default`, `quiver_csv_options_default`") — no `QUIVER_REQUIRE`, no
try/catch, because a `sizeof()` cannot throw. **Exact spelling is Claude's Discretion per
CONTEXT.md** — these names are a recommendation, not a locked decision.

### `quiver_database_has_ui_config` (OPT-04), matching the existing boolean-getter shape

```c
// include/quiver/c/database.h — mirrors quiver_database_is_healthy exactly
// Source: include/quiver/c/database.h:39 (read this session — exact existing signature quoted)
// QUIVER_C_API quiver_error_t quiver_database_is_healthy(quiver_database_t* db, int* out_healthy);
QUIVER_C_API quiver_error_t quiver_database_has_ui_config(quiver_database_t* db, int* out_has_config);
```

### Julia's load-time assertion, appended inside the existing `__init__()`

```julia
# bindings/julia/src/c_api.jl — appended after the existing `global libquiver_c = ...` line
# (read this session, __init__ ends there today)
function __init__()
    dir = quiver_lib_dir()
    dep = Sys.iswindows() ? "libquiver.dll" : Sys.isapple() ? "libquiver.0.dylib" : "libquiver.so"
    Libdl.dlopen(joinpath(dir, dep); throw_error = false)
    global libquiver_c = joinpath(dir, library_name())

    # SAFE-02/D-08: fails loudly if the native lib predates this binding's struct layout.
    # A missing symbol (native lib older than this phase) surfaces as Julia's own ccall
    # "could not find function" error; catch it separately from a value mismatch so the two
    # failure modes are distinguishable (Item 2 of this research's bootstrapping question).
    try
        native_size = @ccall libquiver_c.quiver_database_options_sizeof()::Csize_t
        expected_size = sizeof(quiver_database_options_t)
        if native_size != expected_size
            error("quiver_database_options_t layout mismatch: expected $expected_size bytes, native library reports $native_size. Reinstall a matching Quiver native library.")
        end
    catch e
        error("Failed to verify quiver_database_options_t layout against the native library (it may predate this binding): $e")
    end
    # ... repeat for quiver_scalar_metadata_t / quiver_group_metadata_t
end
```
This runs safely: `@ccall` with a runtime library-path `String` global resolves the symbol via
`dlsym` at call time exactly like every other `@ccall` site in the file (e.g. `quiver_version()`,
called from ordinary runtime code elsewhere) — there is nothing special about doing it inside
`__init__`, since `__init__` itself only runs once per process at `using Quiver` time, well after
precompilation, and `libquiver_c` has already been assigned two lines above.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|---------------|--------|
| `DatabaseOptions` is a closed, 2-field, stable-since-inception struct | A 4-field, 24-byte struct with two borrowed `const char*` fields | This phase (0.11.0) | First C ABI break the project has shipped; sets the precedent (three size accessors, five load-time assertions) that any *future* struct growth should follow the same SAFE-01..03 pattern rather than re-deriving one |
| No binding can detect a native/binding version skew | Every binding hard-fails at load if `sizeof` disagrees | This phase | Converts "silent memory corruption, discovered days later in a metadata read" into "install fails immediately with a named error" |

**Deprecated/outdated:** nothing removed this phase — additive only (per the milestone's own
constraint: `describe*` output byte-identity for Phase 1 is untouched by Phase 2, and no existing
public symbol changes signature).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Bun's `dlopen(path, symbols)` resolves every declared symbol *eagerly* at the `dlopen()` call (so simply adding the three new symbols to `allSymbols` would itself throw against an old native lib, without any explicit call) | Item 2 / Pitfall 1 area (JS) | If Bun actually resolves lazily (like Dart and CFFI both turned out to), the JS load-time assertion also needs an *explicit* call to the new accessor, not just a symbol-table entry — same shape as the Dart/Python fix. Low implementation cost either way (the explicit call is trivial and safe to add regardless), but the plan should not assume JS is "free" on this axis. Recommend: verify empirically in Wave 0 by building against an unpatched native lib and observing whether `dlopen()` or the first `check(lib.quiver_database_options_sizeof())` call throws. |
| A2 | The three size-accessor function names (`quiver_database_options_sizeof`, `quiver_scalar_metadata_sizeof`, `quiver_group_metadata_sizeof`) and the JS/Dart/Python/Julia offset-constant names in the Code Examples section | Architecture Patterns, Code Examples | None structural — CONTEXT.md explicitly reserves "exact accessor spelling, offset-constant names, and test names" as Claude's Discretion. Listed here only so the planner does not mistake these for locked decisions. |
| A3 | The explicit-directory test fixture layout proposed in Item 4 below (`tests/schemas/ui_explicit/database/` + `tests/schemas/ui_explicit/external_config/`) is new, not yet built, and not specified by CONTEXT.md beyond "a `ui/` tree that is deliberately NOT beside its database" | Item 4 | Low — it is a test-only fixture with no production-code consequence; worst case the planner picks a different directory name. Verified NOT to collide with `DatabaseUiCorpus`'s directory walk (`tests/test_database_ui_corpus.cpp:23-25`, which only walks `tests/schemas/ui/`) since it lives at a sibling path, not nested inside `tests/schemas/ui/`. |

## Open Questions

1. **Does Bun's `dlopen` fail at symbol-table-declaration time or at first-call time for a missing
   native symbol?** (A1 above.) Not resolvable without an empirical test against two native builds
   (with and without the new accessors) — recommend a Wave 0 spike task rather than guessing further,
   since both possible answers have a safe, cheap fix (an explicit call after `loadLibrary()`).
2. **Where exactly should the three size accessors' C declarations live?** `quiver_database_options_sizeof`
   naturally belongs in `include/quiver/c/options.h` beside `quiver_database_options_default`. The
   other two could live in `include/quiver/c/database.h` beside `quiver_scalar_metadata_t`/
   `quiver_group_metadata_t`, or in a new small header. Recommendation: keep them beside their
   structs' existing declarations (no new header) — lowest-diff option, Claude's Discretion either way.
3. **Should the CLI's new `--ui-config-dir`/`--ui-locale` flags be validated (path exists, directory)
   before constructing `Database`, or left to degrade via D-05's existing warn-log path?** D-05
   already covers this at the C++ layer (an absent/malformed explicit directory warns, does not
   throw) — recommend the CLI does **no** extra validation and lets the existing degradation path
   handle it, for consistency with every other `DatabaseOptions` field the CLI already passes through
   unvalidated (`--read-only`, `--log-level`).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Julia (juliaup channel `1.12.5`) | Regenerating `bindings/julia/src/c_api.jl` (D-16) | ✓ | `1.12.5+0.x64.w64.mingw32` (confirmed installed via `juliaup status` on this machine) | — |
| Dart SDK | Hand-editing `bindings.dart`; running `test.bat` | ✓ | 3.11.0 stable | — |
| Bun | JS binding build/test | ✓ | 1.3.14 | — |
| uv (Python runner) | `scripts/assert_version.py`, any local Python script | ✓ | 0.12.3 | — |
| CMake | Core/C API build | ✓ | 4.3.1 | — |
| ffigen 20.1.1 | **Deliberately not run** this phase (D-16: Dart is hand-edited) | n/a | n/a | n/a (not needed) |

No missing dependencies block this phase on the machine this research ran on.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| C++ / C API | GoogleTest 1.17.0, via CTest (`cmake --build build --config Debug` then `ctest`, or run `build/bin/quiver_tests.exe` / `quiver_c_tests.exe` directly) |
| Julia | `Test.jl`, `bindings/julia/test/test.bat` |
| Dart | `package:test` 1.31.2+, `bindings/dart/test/test.bat` (D-15: must clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` first this phase) |
| Python | pytest 8.4.1+, `bindings/python/tests/test.bat` (run via `uv run python -m pytest ...` locally, plain `python` not on PATH) |
| JS | `bun:test`, `bindings/js/test/test.bat` (`bun test test`) |
| Config file | None dedicated — each suite's `test.bat` is the config |
| Quick run command | Per-suite executable/`test.bat` (all six take well under 30s except the full Dart suite after a forced native rebuild, which pays a one-time recompile cost per D-15) |
| Full suite command | `scripts/test-all.bat` (assumes already built) or `scripts/build-all.bat` (build + run all six) |

### Phase Requirements → Test Map

All nine requirements currently have **zero** test coverage — confirmed this session by reading
every relevant source file and finding no existing test for any of `ui_config_dir`, `ui_locale`,
`has_ui_config`'s C-symbol/binding surface, or any of the three struct-size accessors. This matches
the CONTEXT.md specifics ("No binding currently has any test that would catch an FFI layout
mismatch — confirmed across all four suites").

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|--------------------|--------------|
| OPT-01 | Explicit `ui_config_dir` overrides convention path, including for `:memory:` | unit (C++) | `quiver_tests.exe --gtest_filter='DatabaseUiDescribe.ExplicitConfigDirOverridesConvention*'` | ❌ Wave 0 |
| OPT-01 | Explicit dir reaches a `ui/` tree deliberately NOT beside the db (new fixture, Item 4) | unit (C++) + one test per binding | same pattern per binding's idiom | ❌ Wave 0 |
| OPT-02 | `ui_locale` defaults to `"en"`; a non-`"en"` locale changes a rendered label vs. the `en` resolution | unit (C++) + one exact-string test per binding (D-17) | `quiver_tests.exe --gtest_filter='DatabaseUiDescribe.LocaleAffectsRenderedLabel*'`; binding-idiom equivalents | ❌ Wave 0 |
| OPT-03 | `ui_config_dir`/`ui_locale` are optional kwargs on `open`/`from_schema`/`from_migrations` in all 5 bindings | unit, 1 per binding | e.g. `pytest tests/test_database_ui_options.py -x` | ❌ Wave 0 |
| OPT-04 | `has_ui_config()` bound and correct in C API + all 5 bindings + Lua | unit, C API + 5 bindings + Lua | `quiver_c_tests.exe --gtest_filter='DatabaseCApiUi.HasUiConfig*'`; per-binding equivalents; `LuaRunnerDescribe` case | ❌ Wave 0 |
| OPT-05 | JS `makeDefaultOptions` allocates 24 bytes from named constants; `allocPtrOut`/`allocUint64Out` provably unchanged (D-11) | unit (JS) | `bun test test/ffi-helpers.test.ts` | ❌ Wave 0 — file does not exist yet |
| OPT-06 | Python cdef, Dart hand-edit, Julia regen all reflect the 24-byte layout; Dart caches cleared | unit, 3 bindings + a `test.bat` behavioral check | per-binding sizeof assertion (see SAFE-02 row) | ❌ Wave 0 |
| SAFE-01 | C API exposes 3 `*_sizeof()` accessors returning native `sizeof` | unit (C API) | `quiver_c_tests.exe --gtest_filter='DatabaseCApiOptions.SizeofAccessors*'` | ❌ Wave 0 |
| SAFE-02 | Each binding asserts its hardcoded size against the native value at load, throws on mismatch | unit, 1 per binding — must construct a *deliberately wrong* expected value to prove the throw path, not just assert the happy path | e.g. a Dart test that temporarily monkeys the expected constant, or (safer) a unit test directly calling the private assertion function with an injected wrong value | ❌ Wave 0 — this is the criterion-5 test CONTEXT.md calls out ("at least one per binding that a wrong layout would actually fail") |
| SAFE-03 | Assertion covers options struct + `quiver_scalar_metadata_t` + `quiver_group_metadata_t` | same as SAFE-02, x3 structs | — | ❌ Wave 0 |

### Sampling Rate

- **Per task commit:** the relevant single suite's quick run (e.g. `quiver_tests.exe
  --gtest_filter='DatabaseUiDescribe.*'` while working on the C++ reorder)
- **Per wave merge:** the full suite for every layer touched in that wave
- **Phase gate:** `scripts/test-all.bat` full green, plus a manual CLI probe (`quiver_cli.exe`) using
  the new `--ui-config-dir`/`--ui-locale` flags against the new explicit-directory fixture, before
  `/gsd-verify-work`

### Wave 0 Gaps

- [ ] `tests/schemas/ui_explicit/` fixture tree (Item 4 below) — covers OPT-01's "not beside its
  database" proof
- [ ] `tests/test_database_ui_describe.cpp` — new cases: `ExplicitConfigDirOverridesConvention`,
  `ExplicitConfigDirLoadsEvenForMemoryDatabase`, `ExplicitConfigDirMissingLogsWarnNotDebug`,
  `LocaleAffectsRenderedLabel`
- [ ] `tests/test_c_api_database_options.cpp` (or extend an existing options test file) — the 3
  `*_sizeof()` accessors, plus `has_ui_config` C symbol round-trip
- [ ] One new test file per binding for the load-time size assertion's *failure* path (SAFE-02's
  criterion-5 test) — none of the four FFI suites currently has any file that would catch a layout
  mismatch (confirmed by grep, zero hits for `sizeof`/`SCALAR_METADATA_SIZE` assertions against a
  wrong value in any binding test directory)
- [ ] `bindings/js/test/ffi-helpers.test.ts` does not exist yet — needed to pin `allocPtrOut`/
  `allocUint64Out`'s 8-byte allocations as unchanged (D-11) and to test `makeDefaultOptions`'s new
  keepalive shape
- [ ] Framework install: none — all five test frameworks are already wired; no new dependency

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-------------------|
| V1 Architecture, Design and Threat Modeling | yes | Fail-loud-at-load-time design (SAFE-01..03) is itself the mitigating architecture: it converts an unbounded native memory-corruption class into a bounded, always-detected startup error |
| V5 Input Validation | yes (narrow sense) | The "input" here is the native library's own reported struct size, validated against each binding's compiled-in expectation before any buffer of that size is used — not user-supplied data validation in the traditional web sense |
| V12 Files and Resources | partially | `ui_config_dir` is a caller-supplied filesystem path with **no new sandboxing** — this matches the existing, deliberate precedent of `schema_path`/`migrations_path` (also caller-supplied, also unsandboxed at the C++/binding layer). Sandboxing (`resolve_sandboxed_path`) is a Lua-only policy (root design decisions) for a different reason (script-supplied paths in an embedded scripting context) and does not apply to a C++/binding caller who already has filesystem access equal to the host process |
| V6 Cryptography | no | Not applicable — no cryptographic material in this phase |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|-----------------------|
| Native struct-layout skew (binding compiled against header version A, loaded against native lib built from header version B) causing an out-of-bounds write into a caller-owned buffer (JS's fixed-size `Uint8Array` allocations are the sharpest instance) | Tampering / Denial of Service (via memory corruption, potentially exploitable UB) | Load-time `sizeof` assertion per struct, throwing before any buffer of that size is allocated or read (SAFE-01..03) — this phase's entire `SAFE` requirement group |
| Dangling native pointer read (a JS string `Allocation` GC'd before the FFI call reads the pointer stored in the options struct — the keepalive hazard identified in this research's Summary) | Tampering (reading freed/reused memory) | `{ options, keepalive }` return shape holding all child allocations reachable through the call (Architecture Patterns, Pattern 2) |
| Caller-supplied `ui_config_dir` pointing at a directory outside any expected root | Information Disclosure (in principle — a malformed TOML file elsewhere on disk could be read) | Accepted by design: matches the existing unsandboxed `schema_path`/`migrations_path` precedent; the caller already has host-process filesystem access, so this introduces no new privilege boundary. No mitigation needed beyond the existing degrade-not-throw behavior (D-05) |

## Sources

### Primary (HIGH confidence — read directly this session)
- `C:/Development/Quiver/quiver3/include/quiver/options.h`, `include/quiver/c/options.h`,
  `src/c/options.cpp`, `src/c/database_options.h` — current 8-byte struct + defaults + converter
- `C:/Development/Quiver/quiver3/src/ui_config.h`, `src/ui_config.cpp` (lines 256-276, 340-409) —
  `UIConfigSet::from_directory`, `require_ui_config()`, `has_ui_config()`
- `C:/Development/Quiver/quiver3/src/database_impl.h` (lines 1-90) — `Database::Impl` member layout
- `C:/Development/Quiver/quiver3/src/database.cpp` (lines 90-135, 235-300) — constructor,
  `from_schema`, `from_migrations`, `validate_migrations`
- `C:/Development/Quiver/quiver3/include/quiver/c/database.h` (lines 300-340) — `quiver_scalar_metadata_t`
  (56 B) / `quiver_group_metadata_t` (32 B) verbatim field lists; `quiver_database_is_healthy` etc.
  boolean-getter signature pattern
- `C:/Development/Quiver/quiver3/bindings/js/src/ffi-helpers.ts` (full file), `metadata.ts` (lines
  1-90), `loader.ts` (full file), `database.ts` (grep), `types.ts` (full file)
- `C:/Development/Quiver/quiver3/bindings/python/src/quiverdb/_c_api.py` (lines 1-50), `_loader.py`
  (full file), `database.py` (lines 1-60)
- `C:/Development/Quiver/quiver3/bindings/dart/lib/src/ffi/bindings.dart` (lines 3540-3575, 11-31),
  `lib/src/database.dart` (lines 1-150), `lib/src/ffi/library_loader.dart` (lines 1-100)
- `C:/Development/Quiver/quiver3/bindings/julia/src/c_api.jl` (lines 1-100), `src/database.jl`
  (lines 1-40), `generator/generator.bat`
- `C:/Development/Quiver/quiver3/tests/test_ui_fixture.h`, `tests/test_database_ui_describe.cpp`
  (lines 230-270), `tests/test_database_ui_corpus.cpp` (full file)
- `C:/Development/Quiver/quiver3/tests/schemas/ui/foresight_like/ui/enum.toml`,
  `ui/economic_driver.toml`, `ui/main.toml` (full contents) — exact es/pt/en literals for D-17
- `C:/Development/Quiver/quiver3/tests/schemas/ui/README.md` (full file) — the L1..L17 literal table
- `C:/Development/Quiver/quiver3/CHANGELOG.md` (head), `CMakeLists.txt` (line 3-4) — version state
- `C:/Development/Quiver/quiver3/scripts/assert_version.py` (bump table, lines 40-45)
- `C:/Development/Quiver/quiver3/.github/CLAUDE.md`, `src/CLAUDE.md`, `src/c/CLAUDE.md`,
  `bindings/{dart,python,julia,js}/CLAUDE.md`, `tests/CLAUDE.md` — full reads
- Local tool probes this session: `juliaup status`, `dart --version`, `bun --version`, `uv
  --version`, `cmake --version`

### Secondary (MEDIUM confidence)
- None used — every claim above was either read directly or is explicitly logged in the Assumptions
  Log as unverified.

### Tertiary (LOW confidence)
- Bun's symbol-resolution eagerness at `dlopen()` time (Assumption A1) — a `WebSearch` this session
  did not surface a definitive primary-source answer; recorded as an open question rather than
  asserted.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; all tooling versions confirmed installed locally
- Architecture: HIGH — every wiring point (constructor delegation, `require_ui_config` reorder,
  Dart's lazy symbol lookup, CFFI's ABI-mode lazy resolution) verified by reading the actual source
  this session, not inferred
- Pitfalls: HIGH for Pitfalls 1-4 (each backed by a specific file:line citation); MEDIUM for the
  underlying "why" of Bun's behavior (A1, genuinely unresolved without an empirical test)

**Research date:** 2026-09-19
**Valid until:** until the next C ABI-affecting change to `DatabaseOptions`, `quiver_scalar_metadata_t`,
or `quiver_group_metadata_t` — this research is about a specific, one-time struct growth and the
safety net around it, not an evergreen API reference
