# Phase 1: Enum Labels in Describe - Research

**Researched:** 2026-09-19
**Domain:** C++ TOML sidecar parsing + append-only text-report rendering, reaching 5 bindings + Lua with zero ABI change
**Confidence:** HIGH

## Summary

Phase 1 has two halves, and CONTEXT.md has already made every product decision (D-01..D-31) —
this research grounds those decisions in verified source and closes the gaps CONTEXT.md left for
the planner (D-31's binding-suite call, the `:memory:` edge case, the exact fixture-file mechanics
DESC-07 requires).

**Half 1 — the parser.** A new private type (`UIConfigSet`, `src/ui_config.h`/`.cpp`) reads
`<db_dir>/ui/{main.toml, enum.toml, <collection>.toml...}` using `tomlplusplus` (already vendored
and linked `PRIVATE` on the `quiver` target — zero CMake change). The authoritative shape is
Hub's Dart parser (`C:/Development/Hub/hub1/lib/models/configuration/*.dart`), confirmed against
five real model repos on this machine (BESSOperation, Foresight, HydroThermalDispatch, SCE,
GNoMo, CarbSteeler) — every corpus case CONTEXT.md names was found and read verbatim below.

**Half 2 — the renderer.** `src/database_describe.cpp` (185 lines total) gets append-only,
non-empty-guarded clauses added to `write_collection_section` and `summarize_collection`'s
histogram loop. Because `describe()`/`describe_collection()`/`summarize_collection()` already
return `std::string` through existing C API string wrappers with **zero marshaling structs**, the
C API needs no new symbol (`quiver_database_describe` at `src/c/database.cpp:129-139` is a
6-line pass-through), and all five bindings + Lua get the new output automatically — the entire
premise of "ships as a patch" is architecturally sound, verified by reading the wrapper.

**Primary recommendation:** Build the parser against the real TOML files read below (not
`toml-schema.md`, confirmed stale on group membership), wire it into `Impl` exactly like
`load_schema_metadata` (lazy, publish-nothing-until-valid, separate `ui_load_attempted` flag), and
special-case `:memory:` databases to skip UI loading entirely — the existing `db_dir` fallback
(`fs::current_path()`) that `create_database_logger` uses for a bare filename must **not** be
reused for UI resolution, or every existing `:memory:`-based test becomes sensitive to the
process's working directory.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| TOML sidecar parsing | C++ core (`src/ui_config.*`) | — | Root CLAUDE.md: logic lives in C++, bindings stay thin; a new metadata reader is a C++ feature bound outward, never five parsers |
| Lazy config loading / caching | C++ core (`Database::Impl`) | — | Mirrors `require_schema()`; must not become part of `load_schema_metadata`'s all-or-nothing contract (D-21) |
| Text-report rendering | C++ core (`database_describe.cpp`) | — | `write_collection_section` / `summarize_collection` are the single render site; C API and bindings never touch report content |
| FFI exposure | C API (zero new symbols) | Bindings (zero new files) | `describe()` already returns `std::string`, already wrapped, already bound in all 5 languages + Lua |
| Test fixtures | `tests/schemas/ui/` (C++ suite reads them; every binding suite references them) | — | CORPUS-03: referenced, never copied into a binding |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| tomlplusplus | 3.4.0 (already vendored, `cmake/Dependencies.cmake:10-15`) | TOML parsing | `[VERIFIED: cmake/Dependencies.cmake:10-15, src/CMakeLists.txt:78]` — already linked `PRIVATE` on `quiver`; zero new dependency |

No new dependency is introduced by this phase. **Skip the Package Legitimacy Audit and
version-verification steps** — no `npm install`/`pip install`/`cargo add` occurs; `tomlplusplus`
is an existing, already-audited in-tree dependency.

### Alternatives Considered
None — CONTEXT.md D-18/D-19 already settled the parser's shape (private type, `from_directory`
factory mirroring `BinaryMetadata`) and no other TOML library is a candidate given the existing
vendored one.

## Package Legitimacy Audit

**Not applicable.** This phase installs no external packages in any ecosystem. `tomlplusplus`
v3.4.0 is pre-existing, vendored via CMake FetchContent, and already linked into the `quiver`
target — confirmed by reading `cmake/Dependencies.cmake:10-15` and `src/CMakeLists.txt:76-78`
this session `[VERIFIED: cmake/Dependencies.cmake:10-15]`.

## Architecture Patterns

### System Architecture Diagram

```
Database(path, options)                    (unchanged: opens sqlite file, no UI/schema read)
        |
        v
  describe() / describe_collection() / summarize_collection()   <- entry points (unchanged names)
        |
        v
  Impl::require_ui_config()  ------ new, mirrors require_schema()
        |
        |  ui_load_attempted == true? --yes--> return cached ui_config (optional, may be empty)
        |  no
        v
  path == ":memory:"? --yes--> mark attempted, leave ui_config empty, log debug, return
        |  no
        v
  db_dir = parent_path(path) (fallback: current_path() ONLY for the logger; do NOT reuse
            that fallback here for a bare relative db filename with no ui/ sibling possible)
        v
  <db_dir>/ui/ exists? --no--> log debug "no UI config", mark attempted, return
        |  yes
        v
  UIConfigSet::from_directory(dir, "en")
        |
        +--> parse main.toml (collections[] list, PARSE-01: never scan directory)
        +--> parse enum.toml (optional/zero-byte tolerated, PARSE-09) -> vocabularies map
        +--> for each filename in main.collections: parse <filename>.toml
        |       -> [[attribute]] entries keyed by id  (namespace A)
        |       -> [[attribute_group]] entries keyed by id (namespace B, PARSE-07/08)
        |       -> resolve localizable fields (bare | {locale:str} table, PARSE-04)
        |       -> ignore unknown keys, log once per file at debug (PARSE-05, D-26)
        +--> record files on disk not present in main.collections (unlisted_files, D-23)
        |
        v
  any exception during the whole walk? --yes--> catch (std::exception&), log WARN, publish
        |                                        nothing (ui_config stays nullopt) -- D-25/PARSE-12
        |  no
        v
  ui_config = loaded value (published only now, mirrors load_schema_metadata's all-or-nothing
              publish rule at database_impl.h:340-348)
        v
  write_collection_section() / summarize_collection() histogram loop
        |  look up ui_config->collections[collection] / ->attributes[name] -- absent = render
        |  today's line unchanged (D-07); present = append label/unit/[hidden]/vocab clauses
        v
  std::string report  --(unchanged wrapper)-->  quiver_database_describe (C API, no new symbol)
        |
        v
  Julia / Dart / Python / JS / Lua describe()/describeCollection()/summarizeCollection()
  (unchanged bindings -- string passthrough, zero new files under bindings/)
```

### Recommended Project Structure
```
src/
├── ui_config.h              # new, private (never under include/), UIConfigSet + UIConfig types
├── ui_config.cpp            # new, parser + validator (single file is fine per Claude's discretion)
├── database_impl.h          # +2 members (ui_config, ui_load_attempted) + require_ui_config()
├── database_describe.cpp    # append-only render clauses in write_collection_section + histogram
└── CMakeLists.txt           # +ui_config.cpp to the `quiver` target sources
tests/
└── schemas/ui/
    ├── <fixture-name>/
    │   ├── schema.sql        # matching SQL schema (collections/attributes the TOML describes)
    │   └── ui/                # the hand-written PSR-shaped sidecar (main.toml, enum.toml, *.toml)
    │       (generated per-suite *.sqlite + *.log land HERE at test time, already gitignored
    │        by root .gitignore's existing `*.db`/`*.sqlite`/`*.log` globs -- verified, no new
    │        gitignore entries needed)
    └── ...one directory per D-27 tolerance + the three CORPUS-01 named cases
```

### Pattern 1: Lazy, publish-nothing-until-valid config load (mirror `require_schema`)
**What:** `Impl::require_ui_config()` loads on first call, caches success *and* failure via a
separate `bool ui_load_attempted`, and only assigns `ui_config` after the entire directory parses
and validates.
**When to use:** Any `describe()`/`describe_collection()`/`summarize_collection()` call.
**Example (verified shape to mirror):**
```cpp
// src/database_impl.h:76-80 (require_schema) -- copy this shape exactly, do not fold into it
void require_schema() const {
    if (!schema) {
        load_schema_metadata();
    }
}
// src/database_impl.h:340-348 (load_schema_metadata) -- the publish-nothing-until-valid rule
void load_schema_metadata() const {
    auto loaded = std::make_unique<Schema>(Schema::from_database(db));
    SchemaValidator(*loaded).validate();
    type_validator = std::make_unique<TypeValidator>(*loaded);
    schema = std::move(loaded);
}
```
`require_ui_config()` differs in one crucial way `require_schema()` must NOT: it must catch and
swallow (log + leave unset), where `require_schema()` deliberately lets a bad database throw.

### Pattern 2: `from_toml_file` / `from_toml_content` split (copy exactly, per D-19)
**What:** A file-path factory that reads bytes and delegates to a content-based factory, so the
content-only path is unit-testable without touching disk.
**Source:**
```cpp
// src/binary/binary_metadata.cpp:220-232 -- the exact split D-19 says to copy
BinaryMetadata BinaryMetadata::from_toml_file(const std::string& file_path) {
    const auto toml_path = file_path + std::string(quiver::TOML_EXTENSION);
    if (!std::filesystem::exists(toml_path)) {
        throw std::runtime_error("Metadata file not found: " + toml_path);
    }
    std::ifstream toml_file(toml_path);
    std::string toml_content((std::istreambuf_iterator<char>(toml_file)), std::istreambuf_iterator<char>());
    return from_toml_content(toml_content);
}

BinaryMetadata BinaryMetadata::from_toml_content(const std::string& content) {
    toml::table tbl = toml::parse(content);
    // ... typed field extraction via tbl["key"].as_array() / .value<T>()
}
```
**Critical caveat found this session (not in CONTEXT.md):** `BinaryMetadata` is a **public**
`QUIVER_API` type (`include/quiver/binary/binary_metadata.h`), so `quiver_tests` calls
`BinaryMetadata::from_toml_content(...)` directly and needs no access to `src/`. `UIConfigSet` is
**private** per D-18 (`src/ui_config.h`, never under `include/`). `quiver_tests` links only
`quiver` + gtest (`tests/CMakeLists.txt:52-58`) with **no include path into `src/`**
`[VERIFIED: tests/CMakeLists.txt:52-58 — no target_include_directories reaching src/]`. This means
**the C++ suite cannot call `UIConfigSet::from_directory`/`from_content` directly at all** — every
PARSE-01..12 tolerance can only be exercised indirectly, through `Database::describe()` /
`describe_collection()` / `summarize_collection()` / `has_ui_config()` over a real on-disk
fixture. The `from_directory`/`from_content` split still earns its keep (content-only parsing is
easier to reason about and leaves a seam for a future public wrapper), but the planner must not
assume a `test_ui_config.cpp` white-box unit test file is possible without also opening a new
include path into `src/` — which nothing in CONTEXT.md authorizes and D-18 explicitly avoids.

### Pattern 3: New file-backed C++ test fixture is required (does not exist today)
**What was found:** the closest existing precedent, `LuaSandboxTest`
(`tests/test_lua_runner.h:23-35`), builds a file-backed database in
`std::filesystem::temp_directory_path()` — a **system temp dir wiped in `TearDown`**. D-28
explicitly forbids this for Phase 1: *"The database is built inside the fixture directory, not
copied to a temp dir... the sidecar must physically sit at `<db_dir>/ui/`."* This means Phase 1
needs a **new** fixture helper (not `LuaSandboxTest`) that:
1. Resolves a path under the checked-in `tests/schemas/ui/<fixture-name>/` directory (which
   already contains the hand-written `ui/` subtree and, per this phase, a `schema.sql`).
2. Calls `Database::from_schema(fixture_dir + "/cpp.sqlite", fixture_dir + "/schema.sql", options)`
   — confirmed **self-cleaning**: `from_schema` already does
   `if (db_path != ":memory:") fs::remove(db_path);` before creating
   `[VERIFIED: src/database.cpp:293-294]`, so no manual stale-file cleanup code is needed.
3. Leaves the resulting `Database` open at that real path so `impl_->path`'s directory is exactly
   the fixture directory containing `ui/`.

Per D-29, **each suite uses its own filename** in the same shared fixture directory
(`cpp.sqlite`, `capi.sqlite`, `julia.sqlite`, `dart.sqlite`, `python.sqlite`, `js.sqlite`,
`lua.sqlite`) so seven suites never collide. The Lua describe tests (`test_lua_runner_describe.cpp`)
are C++ gtest files too (they construct a `quiver::LuaRunner` over a `quiver::Database`), so they
can reuse the same new C++ fixture helper — no separate mechanism needed for Lua.

**Side effect also verified:** any non-`:memory:` `Database` writes a `quiver_database.log` file
next to the db (`src/database.cpp:63-71`, `create_database_logger`). This lands in the same
fixture directory. **No new `.gitignore` work is needed for any of this** — root `.gitignore`
already has generic `*.db` (line 62), `*.sqlite` (line 67), and `*.log` (line 66) patterns
`[VERIFIED: .gitignore:62,66,67]`. This directly contradicts D-29's stated "consequence accepted:
... needing gitignore discipline" — that consequence turns out to already be free. Flag this to
the planner as a simplification, not a task.

### Pattern 4: Directory-based test fixtures are already established
`tests/schemas/migrations/{1,2,3}/` and `tests/schemas/issues/{issue52,issue70}/` are existing
directory-shaped fixtures (each holding `up.sql`/`down.sql`)
`[VERIFIED: tests/.gitignore-tracked layout via ls, tests/CLAUDE.md:123-124]`. The new
`tests/schemas/ui/<fixture-name>/{schema.sql, ui/...}` shape is new *content* but not a new
*pattern* — no CMake precedent needs inventing, only a new leaf shape.

### Anti-Patterns to Avoid
- **Reusing `create_database_logger`'s `db_dir` fallback for UI resolution on `:memory:`
  databases.** `create_database_logger` (`src/database.cpp:62-65`) falls back to
  `fs::current_path()` when a bare filename has no path separator — that fallback exists only to
  give the log *file* somewhere to land, and is meaningless for `:memory:` (which never reaches
  that branch at all; `:memory:` short-circuits to console-only logging at
  `src/database.cpp:56-59`). If `require_ui_config()` naively computed
  `fs::path(impl_->path).parent_path()` for `:memory:`, it would get an **empty path**, and any
  fallback-to-`current_path()` logic copied from the logger would make `describe()` on an
  in-memory database probe `./ui/` relative to the test runner's **current working directory** —
  which is exactly the directory `ctest`/`gtest_discover_tests` runs from
  (`tests/CMakeLists.txt:60-62`: `WORKING_DIRECTORY $<TARGET_FILE_DIR:quiver_tests>`, i.e.
  `build/bin/`). This is a real, easy-to-introduce bug: **every existing `:memory:`-based
  describe test in `tests/test_database_describe.cpp` and `tests/test_lua_runner_describe.cpp`
  would become sensitive to whatever happens to be in `build/bin/ui/`** — normally nothing, but a
  future stray directory would silently start changing DESC-05's byte-identical guarantee.
  **Fix:** special-case `path == ":memory:"` to skip UI loading entirely (treat identically to "no
  `ui/` directory found", debug-level log), before ever computing a directory.
- **Assuming `UIConfigSet` can be unit-tested directly from `quiver_tests`.** It cannot (see
  Pattern 2). Every test in this phase's C++ suite is an integration test through `Database`'s
  already-public surface.
- **Folding `require_ui_config()`'s try/catch into `require_schema()`'s pattern.**
  `require_schema()` deliberately lets a bad database throw (PARSE database corruption is a hard
  error); `require_ui_config()` deliberately does the opposite (PARSE-11/D-25: a malformed sidecar
  must never surface as an exception to the caller of `describe()`). Do not unify these two
  "require" methods — CONTEXT.md's D-21 already forbids merging them into `load_schema_metadata`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Localizable-string resolution | A bespoke locale-fallback function | Copy Hub's `LocalizationString.ofLocale` chain verbatim (exact locale → `"en"` → first key), just don't `throw` at the end (PARSE-05 forbids it — CONTEXT.md's canonical_refs already call this out) | It is the one algorithm two independent parsers must agree on bit-for-bit; inventing a slightly different fallback order silently disagrees with Hub for some model file today |
| TOML array-of-tables iteration | Custom recursive walker | `toml::table::begin()/end()` range-based iteration (`[VERIFIED: build/_deps/tomlplusplus-src/include/toml++/impl/table.hpp:797-811]`) plus `node.as_array()`/`.value<T>()`, exactly as `binary_metadata.cpp` already does at lines 230+ | toml++ already exposes ergonomic typed accessors; `enum.toml`'s top-level keys are **not known ahead of time** (unlike `binary_metadata`'s fixed keys), so this phase is the first place in the codebase that needs the dynamic-iteration idiom — confirmed it exists, no fallback needed |
| Per-key "unknown key" logging | Logging every unrecognized TOML key individually | Accumulate unrecognized keys while parsing one file, emit **one** debug line per file (D-26) | The real corpus has 736 attributes; per-key logging is a wall of text for what is, in practice, a single real occurrence in the entire fleet of model repos checked this session |

**Key insight:** Nothing in this phase needs new infrastructure — `tomlplusplus`, the
lazy-load-with-separate-attempted-flag shape, the `from_file`/`from_content` split, and the
append-only render idiom all have exact precedent already in this codebase. The only genuinely new
piece of machinery is the file-backed-in-fixture-directory C++ test helper (Pattern 3 above),
because every existing describe test uses `:memory:` and the one existing file-backed fixture
(`LuaSandboxTest`) uses a system temp dir that D-28 explicitly rules out for this phase.

## Runtime State Inventory

Not applicable — this is a greenfield feature addition (new parser, new render clauses), not a
rename/refactor/migration phase.

## Common Pitfalls

### Pitfall 1: `:memory:` database resolving a bogus UI directory
**What goes wrong:** Every describe test in the existing C++ suite (`tests/test_database_describe.cpp`,
`tests/test_lua_runner_describe.cpp`) opens `:memory:` databases. If UI-directory resolution reuses
the logger's `db_dir` fallback (`fs::current_path()` for a bare filename), DESC-05's
byte-identical guarantee becomes contingent on the test runner's working directory.
**Why it happens:** `create_database_logger`'s fallback exists for a *different* reason (giving a
log file somewhere to land for a relative on-disk path) and looks superficially reusable.
**How to avoid:** Special-case `path == ":memory:"` in `require_ui_config()` before computing any
directory — treat identically to "absent `ui/`" (debug log, `has_ui_config() == false`).
**Warning signs:** A describe test's output changes depending on the CI runner's cwd, or on
whether `build/bin/ui/` happens to exist.

### Pitfall 2: Reusing pytest's `tmp_path` fixture for the new Python DESC-07 tests
**What goes wrong:** Python's existing `collections_db` fixture (`bindings/python/tests/conftest.py:56-58`)
already builds a file-backed database — but at `tmp_path / "collections.db"`, pytest's
per-test-run ephemeral temp directory. Reusing that fixture (or copying its pattern) for the new
enum-rendering tests would build the sqlite file in a directory with **no `ui/` sibling**, since
D-28 requires the sidecar to sit at a fixed, checked-in path
(`tests/schemas/ui/<fixture>/ui/`), not wherever pytest's `tmp_path` happens to be that run.
**Why it happens:** `tmp_path` is the idiomatic pytest pattern for "give me a scratch file", and
every *other* Python describe/database test already uses it — it is the path of least resistance.
**How to avoid:** Add a new fixture (e.g. `ui_fixture_db(name)`) that builds the sqlite file
directly under `tests/schemas/ui/<fixture-name>/python.sqlite`, sibling to the committed `ui/`
directory, not under `tmp_path`.
**Warning signs:** A new Python test passes locally but the sidecar it's supposedly reading was
never actually opened (silently falls back to "no UI config" behavior, and the test only "passes"
because it's asserting the old unchanged output).

### Pitfall 3: All four binding describe suites currently prove nothing (D-30/D-31)
**What goes wrong:** `bindings/{dart,js,python}/test*/…describe*` and
`bindings/julia/test/test_database_describe.jl` all assert only "returns a String"
`[VERIFIED: bindings/dart/test/describe_test.dart, bindings/js/test/database-describe.test.ts,
bindings/python/tests/test_database_metadata.py:214-233, bindings/julia/test/test_database_describe.jl
— read this session]`. DESC-07 requires **exact-string** assertions from all five bindings + Lua.
If the plan just "adds a test" without first making D-31's explicit call (strengthen vs. leave as
honest smoke tests vs. delete), it risks counting the existing weak tests as coverage they are not.
**Why it happens:** The comment left in each of these files ("report content is covered by the C++
core tests") was true until this phase — DESC-07 changes that by requiring the *rendering* to be
independently proven from every FFI boundary, since a marshaling bug (e.g. a truncated string,
wrong encoding) could corrupt the new Unicode-bearing content (Foresight's es/pt labels) in a
binding-specific way the C++ suite can never see.
**How to avoid:** The plan must record D-31's decision explicitly (CONTEXT.md leaves it to the
planner) and then add genuinely new exact-string assertions per binding against a shared
`tests/schemas/ui/` fixture — not just extend the existing `:memory:`-based smoke test.
**Warning signs:** A binding suite "passes" DESC-07 by asserting `.contains("enum")` against
whatever `:memory:` `collections.sql` happens to already produce (it produces nothing, since that
schema has no `ui/` sidecar).

### Pitfall 4: `enum.toml`'s vocabulary uniqueness is enforced by Hub but not required by PARSE-03
**What goes wrong:** Hub's `EnumerationConfiguration.fromTOML`
(`enumeration_configuration.dart:9-18`) throws on a duplicate id within one vocabulary. PARSE-03
only says Quiver must accept "arbitrary and gapped integer ids" — it does not require duplicate
rejection. If the C++ parser silently lets a later duplicate id overwrite an earlier one (e.g. via
`std::map`'s natural last-write-wins on insertion), that's a silent behavior divergence from Hub,
but not a requirement violation.
**Why it happens:** Both behaviors are individually reasonable; only one file source
(`enumeration_item_configuration.dart`) states Hub's actual rule.
**How to avoid:** Not a blocker — no real corpus file was found with a duplicate id this session.
Record as a documented non-requirement (no fixture needed) rather than silently deciding either
way without a note.

## Code Examples

### Real `main.toml` (PARSE-01, PARSE-10) — BESSOperation
```toml
# C:/Development/BESSOperation/BESSOperation.jl/database/ui/main.toml -- read this session
model = "BESSOperation"
extension = "bess"
executable = "BESS-Operation.bat"
dashboard = "outputs/dashboard.html"
documentation = "index.html"
collections = ["configuration", "price_source", "storage", "renewable"]
```
Note: `collections` lists **snake_case filenames**; each file's own `id` field carries the
PascalCase SQL table name (PARSE-10). Confirmed inside `storage.toml`: `id = "Storage"`.

### `enum.toml` shape (PARSE-03) — BESSOperation, all-bare-string case (CORPUS-01)
```toml
# C:/Development/BESSOperation/BESSOperation.jl/database/ui/enum.toml -- read this session, in full
[[look_ahead]]
id = 0
label = "day"

[[look_ahead]]
id = 1
label = "week"

[[yes_or_no]]
id = 0
label = "No"

[[yes_or_no]]
id = 1
label = "Yes"
```
Top-level keys (`look_ahead`, `model_representation`, `yes_or_no`) are vocabulary names, each an
array-of-tables (`[[name]]`) of `{id, label}`. Keys are **not known ahead of time** — the parser
must iterate `toml::table`'s own keys, not look up fixed names.

### Mixed-locale vocabulary within one vocabulary (PARSE-04, CORPUS-01 Foresight case)
```toml
# C:/Development/Foresight/Foresight.jl/database/ui/enum.toml -- read this session (lines ~17-40)
[[model]]
id = 2
label.en = "Local Linear Trend"
label.es = "Tendencia Lineal Local"
label.pt = "Tendência Lineal Local"

[[model]]
id = 3
label = "ARIMA"          # bare string, same vocabulary as the dotted entries above it

[[model]]
id = 4
label.en = "Residential"
label.es = "Residencial"
label.pt = "Residencial"
```
Confirms PARSE-04's "mixed within the same file" requirement is real, not hypothetical — `id = 3`
is a bare string sitting between two dotted-locale entries in the *same* `[[model]]` vocabulary.

### `degradation` as both an attribute id and a group id (PARSE-07, CORPUS-02)
```toml
# C:/Development/BESSOperation/BESSOperation.jl/database/ui/storage.toml -- read this session
[[attribute_group]]
id = "degradation"
icon = "lucide/trending-down.svg"
label = "Degradation"
date_time.format = "yyyy"
date_time.label = "Year"
date_time.tooltip = "Year (YYYY)"

[[attribute]]
id = "degradation"
label = "Degradation (%)"
tooltip = "Capacity degradation factor per year (percentage)"

[[attribute_group]]
id = "rte"
...
```
Also confirms PARSE-08 (interleaved `[[attribute]]`/`[[attribute_group]]` blocks in arbitrary
order) — this exact file interleaves them three times.

### Plain `date_time` attribute spelling (CORPUS-01, HydroThermalDispatch)
```toml
# C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/demand.toml
[[attribute]]
id = "date_time"
label.en = "Date and Time"
tooltip.en = "Date and time (YYYY-MM-DD HH:MM:SS)"
```
Confirmed present verbatim in `demand.toml`, `dc_line.toml`, `gauging_station.toml`,
`hydro_plant.toml`, `renewable_plant.toml`, `thermal_plant.toml`, `configuration.toml` — the
"plain `[[attribute]] id = date_time`" spelling GROUP-04 (Phase 4) will need to distinguish from
the nested `attribute_group.date_time.*` spelling BESSOperation uses. Phase 1 does not need to
distinguish these — both are just ordinary `[[attribute]]` entries as far as PARSE-02 is
concerned; the distinction only matters once group metadata is exposed (Phase 4).

### Worked example carried in CONTEXT.md, verified on disk
```toml
# C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/database/ui/hydro_plant.toml
[[attribute]]
id = "has_commitment"
type = "enum"
enum = "bool"
label.en = "Has\nCommitment"

# .../enum.toml
[[bool]]
id = 0
label.en = "Disable"

[[bool]]
id = 1
label.en = "Enable"
```
This is the exact `HydroPlant.has_commitment → vocabulary "bool" → {0: Disable, 1: Enable}` chain
CONTEXT.md's Specific Ideas section describes — confirmed on disk this session, not assumed.

### Orphan collection file, confirmed on disk (PARSE-01)
```
C:/Development/SCE/SCE.jl/database/ui/agent.toml   <- exists on disk
C:/Development/SCE/SCE.jl/database/ui/main.toml    <- collections = [configuration, system,
                                                        thermal_plant, hydro_plant,
                                                        renewable_plant, battery]  (no "agent")
```
`agent.toml` is a fully-formed, well-shaped collection file that is simply never listed in
`main.toml`'s `collections` array. This is the real-world case behind PARSE-01's "load only from
main.collections" requirement and D-23's `unlisted_files` bookkeeping (consumed later, by
VALID-05 in Phase 5) — confirmed present, not synthesized, on this machine.

### `hide = true` attribute, confirmed on disk (DESC-06)
```toml
# C:/Development/GNoMo/GNoMo.jl/database/ui/configuration.toml
[[attribute]]
id = "consider_meteoceanographic_conditions"
label.en = "Meteoceanographic Conditions"
tooltip.en = "Consider meteoceanographic conditions"
type = "enum"
enum = "bool"
hide = true
tab = "model_configurations"
```

### `format` table form (PARSE-06) — Hub's own two constructors
```dart
// C:/Development/Hub/hub1/lib/models/configuration/format_configuration.dart -- read in full
class FormatConfiguration {
  final String? elementView;
  final String? collectionView;
  final String? edit;
  final String? data;

  FormatConfiguration.fromTOML(Map<String, dynamic> map)
    : elementView = map["element_view"],
      collectionView = map["collection_view"],
      edit = map["edit"],
      data = map["data"];

  const FormatConfiguration(String? format)
    : elementView = format, collectionView = format, edit = format, data = format;
}
```
Confirms PARSE-06 exactly: the string form assigns one value to all four sub-fields; the table
form sets each independently. No file in the corpus checked this session uses the table form
(matches PARSE-06's own wording) — the fixture for this case is necessarily synthetic, per D-27.

### The zero-C-API-change claim, verified
```cpp
// src/c/database.cpp:129-139 -- the entire C API wrapper for describe()
QUIVER_C_API quiver_error_t quiver_database_describe(quiver_database_t* db, char** out_report) {
    QUIVER_REQUIRE(db, out_report);
    try {
        *out_report = quiver::string::new_c_str(db->db.describe());
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```
Nothing here needs to change. This is the entire proof that Phase 1 "reaches six layers for
free" — verified by reading the wrapper, not assumed from the architecture description.

### Locale fallback chain to copy (PARSE-04), verbatim from Hub
```dart
// C:/Development/Hub/hub1/lib/models/utils/localization_string.dart:38-52
String ofLocale(String locale) {
    if (translation != null) {
      return translation!;                                  // bare string always wins
    }
    if (translations.containsKey(locale)) {
      return translations[locale]!;                          // exact locale
    } else if (translations.containsKey("en")) {
      return translations["en"]!;                             // "en" fallback
    } else if (translations.isNotEmpty) {
      return translations[translations.keys.first]!;          // first key in map order
    } else {
      throw Exception("No translations found");                // Quiver must NOT do this (PARSE-05/11)
    }
}
```

## State of the Art

Not applicable in the conventional sense (no external library version drift to track). The one
relevant "state of the art" fact: Hub itself has moved past `toml-schema.md`
(`Hub/hub1/.claude/skills/psrhub-ui/references/toml-schema.md` — flagged wrong by the user and
independently confirmed here: none of the real fixture files this session read use the group
membership shape that doc would imply) — always read the Dart source, never that doc.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `EnumerationConfiguration`'s duplicate-id throw (Hub) is *not* a requirement for Quiver's parser (PARSE-03 is silent on it) | Pitfall 4 | Low — no real corpus file exercises this; worst case is a silent last-write-wins that a future fixture would need to pin down explicitly |
| A2 | Empty/zero-byte `enum.toml` parses to an empty `toml::table` without throwing (toml++ standard TOML-empty-document behavior) — not independently exercised against the vendored toml++ source this session, only inferred from standard TOML grammar | PARSE-09 | Low-Medium — if wrong, the zero-byte-`enum.toml` fixture (D-27) will surface it immediately as a failing test, which is exactly what the fixture is for; no downstream damage either way |

**Overall assumption risk is low.** The two open items are exactly the kind of thing D-27's
per-tolerance fixtures are designed to catch mechanically — neither is load-bearing on a design
decision.

## Open Questions

1. **Should `require_ui_config()` short-circuit `:memory:` before or after checking
   `ui_load_attempted`?**
   - What we know: it must short-circuit somewhere before computing a directory (Anti-Pattern 1).
   - What's unclear: whether CONTEXT.md's D-20 (`ui_load_attempted` flag) is meant to cover this
     case too, or whether `:memory:` gets its own dedicated early-return with no flag interaction.
   - Recommendation: treat it as the very first check inside `require_ui_config()`, still setting
     `ui_load_attempted = true` on the way out, so the shape stays uniform with every other
     "absence is normal" path (D-20's stated purpose: avoid re-walking on every call).

2. **Does the shared `schema.sql` per UI fixture directory live under `tests/schemas/ui/<name>/`
   or reuse an existing schema from `tests/schemas/valid/`?**
   - What we know: D-28 requires the sidecar to sit at `<db_dir>/ui/`, and the fixture directory
     is the db's directory — so the `.sql` schema must be reachable from that same directory (or
     be an existing shared one the fixture-building helper points `from_schema` at while writing
     the resulting `.sqlite` into the UI fixture directory).
   - What's unclear: whether CONTEXT.md intends a *new* `schema.sql` per fixture (so the SQL
     column set matches the miniature TOML's ids precisely) or reuse of `valid/collections.sql`
     etc. with the TOML written to match.
   - Recommendation: new small `schema.sql` per UI fixture directory — the miniature TOML fixtures
     (D-27) each target one specific tolerance, and matching column names 1:1 to a purpose-built
     tiny schema is far less fragile than back-fitting TOML ids onto an existing shared schema not
     designed for this purpose.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| tomlplusplus | Parser (already vendored) | Yes | 3.4.0, confirmed via `build/_deps/tomlplusplus-src` present on disk | — |
| CMake build (`build/` configured) | Confirming toml++ API shape | Yes | Vendored source read directly this session | — |
| C:/Development/Hub/hub1 | Canonical parser spec | Yes, present and read | n/a (source checkout) | — |
| C:/Development/{BESSOperation,Foresight,HydroThermalDispatch,SCE,GNoMo,CarbSteeler} | Real-world corpus fixtures | Yes, all present and read | n/a | — |
| C:/Development/Claw/claw | Committed consumer reference | Yes, present | n/a | — |

No missing dependencies. Everything CONTEXT.md's canonical_refs pointed at exists on this machine
and was read this session.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 (C++ core + C API), plus per-binding native frameworks (Test.jl, `test` pkg, pytest 8.4.1+, bun:test) |
| Config file | `tests/CMakeLists.txt` (C++/C API); each binding's own test runner config |
| Quick run command | `./build/bin/quiver_tests.exe --gtest_filter='*Describe*'` |
| Full suite command | `scripts/test-all.bat` |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PARSE-01..12 | Parser tolerances, exercised only through `describe()`/`has_ui_config()` (Pattern 2) | integration | `./build/bin/quiver_tests.exe --gtest_filter='DatabaseUiDescribe*'` | ❌ Wave 0 — new fixture helper + new test file |
| DESC-01..06 | Render clauses (label/unit/hidden/vocab/header) | integration | same as above | ❌ Wave 0 |
| DESC-07 | Exact-string assertions from all 5 bindings + Lua | integration | `scripts/test-all.bat` (each binding's own runner) | ❌ Wave 0 for every binding — current tests only assert `isA<String>` |
| CORPUS-01..03 | Fixture tree itself | fixture | n/a (data, not test code) | ❌ Wave 0 — `tests/schemas/ui/` does not exist yet |

### Sampling Rate
- **Per task commit:** `./build/bin/quiver_tests.exe --gtest_filter='*Describe*'`
- **Per wave merge:** `scripts/test-all.bat` (full 6-suite + CLI smoke run)
- **Phase gate:** Full suite green before `/gsd-verify-work`, plus a manual `git diff --stat` check
  that no path under `bindings/` appears (Success Criterion 5) and a byte-diff of `describe()`
  output on a sidecar-less database against `master` (Success Criterion 3 explicitly demands a
  diff, not just "tests pass").

### Wave 0 Gaps
- [ ] `tests/schemas/ui/` fixture tree — does not exist; needs one directory per D-27 tolerance
  plus the three CORPUS-01 named cases (can reuse the same directories where a tolerance and a
  named case coincide, e.g. the BESSOperation-shaped fixture also covers "all-bare-string" and
  "degradation namespace collision" and "interleaved blocks" simultaneously, since the real file
  already exhibits all three)
- [ ] New C++ test fixture helper (Pattern 3) — builds a file-backed db inside a
  `tests/schemas/ui/<name>/` directory, distinct from `LuaSandboxTest`
- [ ] `tests/test_database_ui_describe.cpp` (or similar new file name; `tests/CLAUDE.md`'s existing
  naming convention is `test_database_<area>.cpp`) — new, added to `tests/CMakeLists.txt`
- [ ] Per-binding new fixture helper for "file-backed db beside a `ui/` fixture directory,
  discovered via each binding's existing `tests_path()`/`schemas_path()`-style helper" — none of
  the 4 weak binding suites currently do this (Pitfall 2/3)
- [ ] `src/CMakeLists.txt` gains `ui_config.cpp` in the `quiver` target's source list

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | No | No auth surface touched |
| V3 Session Management | No | No session surface touched |
| V4 Access Control | No | No access-control surface touched |
| V5 Input Validation | Yes | Treat every TOML file under `<db_dir>/ui/` as **untrusted input** even though it ships with a "trusted" model repo — a malformed, truncated, or adversarially-crafted file must never crash the process or corrupt in-memory state. Mitigation: the catch-log-publish-nothing shape (D-25/PARSE-12), exactly like `load_schema_metadata`'s validate-before-publish rule but inverted to *swallow* rather than *propagate*. |
| V6 Cryptography | No | No crypto surface touched |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Malformed/truncated TOML causing an uncaught `toml::parse_error` to propagate out of `describe()` | Denial of Service | `catch (const std::exception&)` around the whole `UIConfigSet::from_directory` call inside `require_ui_config()` — confirmed `toml::parse_error` derives from `std::runtime_error` when exceptions are enabled `[VERIFIED: build/_deps/tomlplusplus-src/include/toml++/impl/parse_error.hpp:29-44]`, so a single `catch (const std::exception&)` is sufficient; no need to special-case `toml::parse_error` separately |
| A crafted `ui/` sidecar with a symlink or `..`-laden collection filename escaping the db directory | Path Traversal | Not exercised by Phase 1 (no OPT-01 path override yet; the directory is always `<db_dir>/ui/`, a fixed, non-caller-supplied suffix) — collection *filenames* come from `main.toml`'s `collections` array, which the parser should join with the `ui/` directory using ordinary path-join (`fs::path(dir) / (filename + ".toml")`) without extra sanitization for Phase 1, since a malicious `main.toml` implies the attacker already has write access to the same directory as the database file itself — no new trust boundary is crossed. Revisit if/when OPT-01 (Phase 2) lets a caller supply an arbitrary path. |
| Resource exhaustion via a deeply nested or huge TOML file | Denial of Service | toml++ has its own internal recursion/size handling (same library already used for `.qvr` metadata sidecars in production); no additional Quiver-side guard is warranted for Phase 1 given the file is locally-authored, not network-received |

## Sources

### Primary (HIGH confidence — read directly this session)
- `src/database_describe.cpp` (full file, 185 lines) — current render logic, exact line numbers
  for every quoted fragment
- `src/database_impl.h` (full file, 427 lines) — `Impl`, `require_schema`, `load_schema_metadata`
- `src/database.cpp:1-135, 270-300` — constructor, `create_database_logger`, `from_schema`'s
  self-cleaning `fs::remove` behavior
- `src/binary/binary_metadata.cpp:220-232` — the `from_toml_file`/`from_toml_content` split
- `include/quiver/options.h`, `include/quiver/attribute_metadata.h`, `include/quiver/schema.h`,
  `include/quiver/data_type.h` (full files) — struct layouts referenced by CONTEXT.md's D-08/D-16
- `src/c/database.cpp:129-139` — the C API `describe()` wrapper, proving zero-symbol-change
- `tests/test_database_describe.cpp`, `tests/test_lua_runner_describe.cpp`,
  `tests/test_utils.h`, `tests/test_lua_runner.h`, `tests/CMakeLists.txt`, `tests/CLAUDE.md` (full
  files) — existing assertion style, `LuaSandboxTest` precedent, include-path confirmation
- `bindings/dart/test/describe_test.dart`, `bindings/js/test/database-describe.test.ts`,
  `bindings/python/tests/test_database_metadata.py`, `bindings/python/tests/conftest.py`,
  `bindings/julia/test/test_database_describe.jl`, `bindings/julia/test/fixture.jl` (full/excerpted) —
  confirms the "returns a String" weakness and each binding's schema-path-resolution idiom
- `C:/Development/Hub/hub1/lib/models/configuration/attribute_configuration.dart`,
  `attribute_group_configuration.dart`, `collection_configuration.dart`,
  `database_configuration.dart`, `enumeration_configuration.dart`,
  `enumeration_item_configuration.dart`, `format_configuration.dart` (full files) — the
  authoritative parser spec
- `C:/Development/Hub/hub1/lib/models/utils/localization_string.dart`, `toml_utils.dart` (full
  files) — the locale fallback chain and TOML load/parse wrapper
- Real model-repo TOML files read in full or excerpted this session: BESSOperation's
  `main.toml`/`storage.toml`/`enum.toml`/`price_source.toml`; Foresight's
  `main.toml`/`economic_driver.toml`/`enum.toml`; HydroThermalDispatch's
  `main.toml`/`demand.toml`/`hydro_plant.toml`/`enum.toml`; SCE's `main.toml` + confirmed
  `agent.toml` orphan; GNoMo's `configuration.toml` (hide=true example, `conditions` unknown-key
  location); CarbSteeler's `main.toml`/`enum.toml` (dual boolean vocab confirmation)
- `build/_deps/tomlplusplus-src/include/toml++/impl/table.hpp:797-811` (iterator methods),
  `build/_deps/tomlplusplus-src/include/toml++/impl/parse_error.hpp:29-44` (exception base class)
  — vendored dependency source, confirms toml++ API surface used in the code examples above
- `.gitignore` (root, full file) — confirms `*.db`/`*.sqlite`/`*.log` already ignored
- `CHANGELOG.md:1-8`, `CMakeLists.txt:1-7` — confirms the `0.10.4` vs `0.10.6` version mismatch
  Success Criterion 5 requires reconciling
- `.planning/config.json` — confirms `nyquist_validation: true` and `security_enforcement: true`

### Secondary (MEDIUM confidence)
None — every claim in this document traces to a file read this session.

### Tertiary (LOW confidence)
- Empty-string `toml::parse("")` behavior (Assumption A2) — inferred from standard TOML grammar,
  not independently exercised against the vendored source

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependency, existing vendored library, verified linkage
- Architecture: HIGH — every render site, every `Impl` member, every C API wrapper read directly
- Parser spec: HIGH — read all 9 relevant Hub Dart source files in full, cross-checked against 6
  real model repos' actual TOML files on disk (not samples, not documentation)
- Test mechanics: HIGH — read every existing test file this phase touches or must mirror;
  identified two mechanics gaps (no white-box `UIConfigSet` testing possible; new fixture pattern
  needed) that CONTEXT.md did not explicitly resolve
- Pitfalls: HIGH — the `:memory:`/cwd hazard and the `tmp_path`-reuse hazard are both derived from
  reading actual code paths, not speculation

**Research date:** 2026-09-19
**Valid until:** No external drift risk (no new dependency, no framework version to track) —
valid until CONTEXT.md's decisions change or the real model repos' TOML shape changes upstream.
