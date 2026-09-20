# Project Research Summary

**Project:** Quiver — UI Metadata in `describe`
**Domain:** Brownfield C++20 library milestone — a TOML sidecar reader feeding three existing text reports
**Researched:** 2026-09-20 (supersedes the 2026-09-17 research, which was scoped to a milestone ~2x this one)
**Confidence:** HIGH — every number below was measured against the real corpus or the real source, not inferred

## Executive Summary

Nothing is acquired. `tomlplusplus` v3.4.0 is already linked `PRIVATE` on the `quiver` target and
`src/binary/binary_metadata.cpp` is a shipped precedent for reading a `.toml` sidecar. The whole
feature is **one new parser file, one member on `Database::Impl`, one call site in
`from_migrations`, and two append-before-newline edits in `src/database_describe.cpp`**. There is no
C API symbol, no binding code, no generator run, and no ABI change — `describe*` already returns
`std::string` through a `new_c_str` wrapper, so the richer report reaches all five bindings and Lua
for free.

The research effort went into the *constraints*, which are where this milestone can silently fail.
Three findings changed the design outright, and each was proven rather than argued:

1. **`main.toml` is not needed.** Across all 117 corpus files, `main.toml` and `enum.toml` never
   carry both a top-level string `id` and an `attribute` array, while all 67 collection files do.
   Selecting on that shape deletes a parser, the snake_case→PascalCase filename mapping (`dc_line`
   → `DCLine`), the orphan-file case, and the listed-but-missing case.
2. **`parent_path()` alone is wrong.** Compiled and run: a trailing separator yields
   `<migrations>/ui` (never exists) and a bare relative path yields `./ui` against the process CWD
   (may exist, unrelated). Trailing-slash migrations paths already appear in this repo's tests.
   `fs::weakly_canonical(...).parent_path() / "ui"` is the fix.
3. **UI parsing must live in `from_migrations`, not in schema loading.** `migrate_up` early-returns
   at `src/database.cpp:398-401` and `:406-409` before reaching `load_schema_metadata()` at `:437`.
   Foresight's `load_study` — opening an existing study — hits the already-up-to-date early return
   every single time. Hooking onto schema loading would work on create and silently no-op on load.

## Key Findings

### Stack

Nothing new. `tomlplusplus` is `PRIVATE` on `quiver` (`src/CMakeLists.txt:81`), which does not
propagate — `quiver_c`, `quiver_cli` and `quiver_tests` cannot include toml++. The parser must
therefore be a `.cpp` in `QUIVER_SOURCES`, and the C++ suite must drive it through the public API.
`tests/test_binary_metadata.cpp` is the precedent: 819 lines, zero toml++ includes.

Do **not** copy `binary_metadata.cpp`'s error posture — it throws on malformed TOML
(`toml::parse_error`) and on a missing scalar key (`std::bad_optional_access` from a bare
`.value()`), and has no test for either. The UI load must degrade: one try/catch around the whole
thing → `logger->warn` → empty map, following `src/database.cpp:80-86`.

### Corpus

13 `database/ui/` dirs, all siblings of `database/migrations/`. 117 `.toml` files — 13 `main.toml`,
67 collection files, 12 `enum.toml`, 25 `themes/*.toml`. All 117 parse with stock `tomllib`:
0 failures, 0 BOMs, 3 zero-byte `enum.toml` (CHain, SORA, PSRExample) and 1 repo with none (Boost).

The sibling layout survives packaging — the compiled PSR app ships the whole `database/` directory
next to the binary (`Foresight.jl/compile/compile.jl:26,43`; `src/inputs.jl:30`).

| Measure | Value |
|---|---|
| `[[attribute]]` entries | 736 across 67 files |
| `[[attribute]]` keys that ever occur | `id` 736, `label` 713, `tooltip` 507, `hide` 360, `unit` 324, `format` 141, `type` 127, `enum` 127, `tab` 68, `enabled_if` 13 |
| enum-bound attributes | 127 (`type = "enum"` and `enum = "<vocab>"` coincide exactly) |
| …reachable by `summarize_collection` | **121** (main-table scalars; the other 6 are group columns) |
| vocabularies / entries / dangling refs | 62 / 164 / **0** |
| `label` at `[[attribute]]` level | 678 table / 35 bare / 23 absent |
| `tooltip` at `[[attribute]]` level | 476 table / 31 bare / 229 absent |
| locale tables lacking `en` | **0 of 817** |
| strings containing a literal newline | 137 of 1751 |
| non-ASCII UTF-8 strings | 344 |
| attributes naming a column that does not exist | 15 of 736 (2% drift) |
| files reusing one id for both `[[attribute]]` and `[[attribute_group]]` | 15 |

### Architecture

Two injection sites, both append-before-newline, no refactor:

- `write_collection_section` (`src/database_describe.cpp:56`) is an anon-namespace free function
  shared by `describe()` (`:104`) and `describe_collection()` (`:114`). It takes no `Impl`, so the
  UI store arrives as one nullable pointer parameter — that single parameter covers both reports.
  The scalar line is built at `:62-72`; append after the flags, before the `"\n"` at `:71`.
- `summarize_collection` (`:118-183`) does **not** share that helper. Its scalar line is at `:136`
  and its histogram closes at `:156-160`. That is the second site, and the one the enum annotation
  lands in.

`Database::Impl` takes one plain (non-`mutable`) member. The three describe readers are `const` but
reach `Impl` through `impl_->`, and constness does not propagate through `unique_ptr::operator->`.

### Pitfalls

- **A committed `tests/schemas/ui/` is a landmine.** It would become a live sibling of
  `tests/schemas/migrations` for every `from_migrations` call in the C++, C API, Julia, Dart, Python
  and JS suites, plus the Lua migrations test that copies the tree recursively. Build
  `migrations/` + `ui/` in a per-test temp dir (`tests/test_migrations.cpp:12-32`, `:194-199`).
- **Three brittle negative assertions** break if injected text contains `Vectors:` / `Sets:` /
  `Time Series:` or `values {` (`tests/test_database_lifecycle.cpp:396-436`, `:438-460`,
  `:487-500`), and `:463-485` pins the literal prefix `"    - <name> "` with its trailing space.
- **Byte-identity is free, not engineered.** Every existing describe test in every layer opens via
  `from_schema`, never `from_migrations` — none of them can see this feature.
- **Newlines and UTF-8.** 137 strings carry `\n` (`"Mean\nProduction\nFactor"`) and would shred a
  line-oriented report; 344 are non-ASCII and must pass through untranscoded.
- **Case sensitivity.** `Schema` keys tables in a `std::map` while SQLite matches names
  case-insensitively, so a ui `id` with different casing silently matches nothing.
- **Output budget.** GNoMo has 236 attributes; whole-DB `describe()` grows ~15–20 KB.

### The accepted risk

`enum.toml` is unversioned, untested by any CI, and already wrong in places: HTD's Julia declares
`HasCommitment` as `YES = 0` while its `enum.toml` says `id = 0 → "Disable"`. 2 of 8
mechanically-checkable attributes are inverted. Rendering it means Quiver states those inversions to
an LLM as fact. `validate_ui_config()` is the only mitigation and is out of scope by decision.

## Implications for Roadmap

- The reader and the renderer are separable and testable independently, but the reader is worthless
  until something renders it — so a single vertical slice (parse + render + test) is a legitimate
  first phase, not an artificially fat one.
- `describe` / `describe_collection` share one injection site; `summarize_collection` is a second,
  and it is the only one that needs the enum→histogram join. That is a natural seam if the
  milestone is split in two.
- The `CHANGELOG.md` heading reconciliation (`0.10.7` → `0.10.8`) is a prerequisite for whichever
  phase writes the first changelog entry.
- Test footprint is small and C++-only: binding churn is zero, and a `FromSchemaNeverReadsUi` test
  would prove nothing (`from_schema` takes a `.sql` file and never has a migrations path).

## Sources

- `C:/Development/Quiver/quiver4/src/database_describe.cpp`, `src/database.cpp`,
  `src/database_impl.h`, `src/schema.cpp`, `src/c/database.cpp`, `src/CMakeLists.txt`
- `C:/Development/Quiver/quiver4/tests/test_database_lifecycle.cpp`, `tests/test_migrations.cpp`,
  `tests/test_binary_metadata.cpp`, `tests/CLAUDE.md`
- `C:/Development/Quiver/quiver4/bindings/js/src/metadata.ts`, `bindings/js/src/ffi-helpers.ts`
- 13 × `C:/Development/<repo>/<repo>.jl/database/ui/` (117 `.toml` files, parsed with `tomllib`)
- `C:/Development/Foresight/Foresight.jl/src/inputs.jl`, `compile/compile.jl`
- `C:/Development/HydroThermalDispatch/HydroThermalDispatch.jl/src/collections/hydro_plant.jl`
