# Stack Research

**Domain:** Brownfield C++20 library milestone — a TOML sidecar reader inside an existing SQLite wrapper with a C API and five language bindings
**Researched:** 2026-09-17
**Confidence:** HIGH (every version, path and constant below was read out of the repo or quoted verbatim in the two survey journals; nothing is inferred from memory)

> **This is not a stack selection.** Every technology this milestone needs is already vendored,
> already linked, and already exercised by shipped code. The useful content here is the set of
> *constraints* those existing choices impose on the work — what compiles for free, what breaks the
> ABI, and what silently corrupts memory if a hardcoded number is not updated in lockstep.
> Recommendation #1 is: **add nothing.**

---

## Recommended Stack

### Core Technologies (all already present — no acquisition step)

| Technology | Version | Purpose | Why it is the choice here |
|------------|---------|---------|---------------------------|
| C++20 | `CMAKE_CXX_STANDARD 20`, `CXX_EXTENSIONS OFF` (`CMakeLists.txt`) | The parser, the metadata structs, the `describe` rendering | Root `CLAUDE.md`: "Logic resides in C++ layer. Bindings/wrappers remain thin." One parser in C++ serves Lua + all five bindings; five parsers is the rejected alternative |
| CMake | `cmake_minimum_required(VERSION 3.26.0)` | Build | Floor is 3.26 deliberately — `FetchContent_Declare(... EXCLUDE_FROM_ALL)` needs 3.28 and is documented as unavailable in `cmake/Dependencies.cmake`. Do not raise it casually |
| **tomlplusplus** | **v3.4.0**, header-only, FetchContent (`cmake/Dependencies.cmake` L10-15) | Parsing `main.toml`, `<collection>.toml`, `enum.toml` | **Already linked `PRIVATE` to the `quiver` core target** (`src/CMakeLists.txt` L73-84). A UI parser in `src/database_*.cpp` compiles with **zero CMake change** — confirmed by direct read of both files |
| SQLite3 | v3.50.2 (sjinks/sqlite3-cmake) | The database Quiver wraps | Linked `PUBLIC` on `quiver`. Relevant here only because group membership must be recovered from table names (`{Collection}_vector_{id}`, `_time_series_{id}`) via `PRAGMA`, exactly as Hub does it |
| spdlog | v1.17.0 | The warning channel for unknown TOML keys / missing UI dir | `impl_->logger` already exists on `Database::Impl` (`src/database.cpp` ctor L94-102). "Unknown keys are ignored, never rejected" (tolerance (c)) needs a log line, not an exception |
| sol2 | v3.5.0 | The Lua leg | `src/lua_runner.cpp` binds C++ classes directly. One new converter next to `scalar_metadata_lua` (L1166-1188) covers every Lua metadata entry point — all seven route through it |

### The toml++ linkage caveat that decides *where* code goes

`tomlplusplus::tomlplusplus` is `PRIVATE` on `quiver`. `PRIVATE` does **not** propagate.
`quiver_c`, `quiver_cli` and `quiver_tests` **cannot** `#include <toml++/toml.hpp>` without a new
link line. Parsing therefore belongs in the core (`src/`), which is where the design already puts
it. A test that wants to parse TOML directly would need a CMake edit — write tests against
Quiver's API instead, using fixture files.

### Supporting Libraries (binding-side, all already pinned)

| Toolchain | Version / pin | Purpose | Constraint it imposes |
|-----------|---------------|---------|-----------------------|
| Clang.jl | `Clang = "=0.19.2"` (`bindings/julia/generator/Project.toml`), run via `julia +1.12.5` | Regenerates `bindings/julia/src/c_api.jl` | **Safe to regenerate.** Julia's positional `C.quiver_scalar_metadata_t(...)` calls in `database_metadata.jl` (L46, L56) fail loudly with `MethodError` on arity drift — the one binding whose mismatch is not silent. Julia compat floor `julia = "1.11"` |
| ffigen | `^20.1.1`, Dart SDK `^3.10.0` (`bindings/dart/pubspec.yaml`) | *Would* regenerate `lib/src/ffi/bindings.dart` | **DO NOT REGENERATE.** A full regen flips enums and breaks Hub, which pins `quiverdb` at commit `e598fb46` (v0.10.6). Hand-add struct fields at `bindings.dart` L3555. Also clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` or tests silently run the old layout |
| CFFI | `cffi>=2.0.0`, `requires-python = ">=3.13"` (`bindings/python/pyproject.toml`) | **ABI mode** — `_c_api.py` carries a hand-written cdef | The cdef *is* the struct layout (`_c_api.py` L27-30 for `quiver_database_options_t`; L192-204, L378-384 for metadata). ABI mode means **no compile step and no error** on a stale cdef — it corrupts silently. `bindings/python/generator/generator.bat` only *prints* cdecls as a diff aid |
| Bun FFI | hand-written symbol table, `bindings/js/src/loader.ts` | No generator at all | Every struct size and byte offset is a literal in TypeScript. The highest-risk surface in the milestone — see Version Compatibility below |
| GoogleTest | v1.17.0 (tests only) | C++ + C API suites | New `.cpp` files must be added to `tests/CMakeLists.txt` |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `scripts/build-all.bat` (`--release`) | Build + run all six suites | First-time configure: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON` |
| `scripts/test-all.bat` | Six suites + `quiver_cli` smoke | The `quiver_cli` binary is also the fastest way to eyeball real `describe` output against a fixture schema |
| `scripts/generator.bat` | Runs all three FFI generators in sequence | **Do not run it wholesale this milestone** — it includes the Dart regen that is forbidden above |
| `scripts/assert_version.py` | Asserts / bumps all five manifests | All five currently read `0.10.6` (`CMakeLists.txt` L4, `bindings/{python/pyproject.toml,js/package.json,dart/pubspec.yaml,julia/Project.toml}`) — verified in agreement |
| `scripts/format.bat` / `tidy.bat` | clang-format + per-binding formatters; run-clang-tidy | `.gitattributes` enforces LF for `.cpp/.h/.dart/.jl/.py`; working-tree `.bat` files are CRLF |
| `uv run python ...` | Local Python | Plain `python`/`py` are not on PATH in this environment |

### Installation

```bash
# Nothing to install. Verify instead:
grep -n -A14 'target_link_libraries(quiver$' src/CMakeLists.txt   # tomlplusplus already PRIVATE
grep -n 'tomlplusplus' -A4 cmake/Dependencies.cmake               # v3.4.0, FetchContent
cmake --build build --config Debug                                # must still be green before any edit
```

---

## TOML Parsing: the idiom to copy

`src/binary/binary_metadata.cpp` is the **only** translation unit in the repo that uses toml++
(`grep -rln "toml++" src/ include/` returns that one file). It is a working, shipped, tested
precedent for exactly the shape this milestone needs: a `.toml` sidecar read from disk beside a
data file.

**The split to copy** (`src/binary/binary_metadata.cpp`):

```cpp
// L220-228 — the file wrapper: existence check, slurp, delegate. Owns the I/O and the Pattern 2 error.
BinaryMetadata BinaryMetadata::from_toml_file(const std::string& file_path) {
    const auto toml_path = file_path + std::string(quiver::TOML_EXTENSION);
    if (!std::filesystem::exists(toml_path)) {
        throw std::runtime_error("Metadata file not found: " + toml_path);
    }
    std::ifstream toml_file(toml_path);
    std::string toml_content((std::istreambuf_iterator<char>(toml_file)), std::istreambuf_iterator<char>());
    return from_toml_content(toml_content);
}

// L230-232 — the parser: takes a std::string, never a path. Unit-testable with no filesystem.
BinaryMetadata BinaryMetadata::from_toml_content(const std::string& content) {
    toml::table tbl = toml::parse(content);
```

**Why the split matters here more than it did there:** the UI reader must tolerate a missing
`enum.toml`, a zero-byte `enum.toml`, and a missing `ui/` directory (tolerances (b) and the
"degrade silently" decision in PROJECT.md). Existence/readability policy lives in the `_file` half;
the `_content` half stays a pure function of a string and is where every parse fixture test points.

**The exact toml++ API calls already in use** — these four are the entire vocabulary needed:

| Call | Used at | Maps to in the UI format |
|------|---------|--------------------------|
| `toml::parse(content)` → `toml::table` | L232 | The whole file. Throws `toml::parse_error` on malformed input (exceptions are on; the code relies on it) |
| `tbl["key"].as_array()` → `toml::array*`, null when absent/wrong type | L235, L244, L252, L261, L273 | `main.collections`, `[[attribute]]`, `[[attribute_group]]`, `[[enum-name]]` arrays-of-tables. The null return **is** the optional-key guard — it is Hub's `containsKey` in C++ |
| `elem.value<std::string>()` / `.value<int64_t>()` → `std::optional<T>` | L237, L246 | `id`, `label`, `unit`, `format` (string) and enum `id` (int64 — matches the settled `UIEnumEntry.code` = `int64_t` decision) |
| `tbl["key"].value<std::string>().value()` | L269-270 | The required-or-throw spelling, for `main.model` / `main.collections` (Hub throws on both) |

Two calls this milestone needs that `binary_metadata.cpp` does not currently make, both stock
toml++ v3.4.0: `node.as_table()` (for the `string \| table` localizable union — tolerance (a), and
for the `format` 4-key table form — tolerance (j)) and iteration over a `toml::table`'s key/value
pairs (for `enum.toml`, whose **top-level keys are the vocabulary ids**, not a fixed set). Neither
needs a new dependency; both are the same header.

**Also copy the writer precedent if it is ever needed:** `to_toml()` (L348-380) builds a
`toml::table{...}` and streams it. Not needed this milestone — write-side scaffolding is
explicitly Out of Scope in PROJECT.md.

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| **Any new TOML library** (`toml11`, `cpptoml`, a hand-rolled scanner) | toml++ v3.4.0 is vendored, header-only, `PRIVATE`-linked to the exact target the parser lands in, and already parses a sidecar in production. Adding one costs a `Dependencies.cmake` edit, a link line, a license review, and the S3/wheel/npm native rebuild ritual — for zero capability | `#include <toml++/toml.hpp>` in `src/` |
| **A TOML dependency in any binding** (`smol-toml`, Dart `toml`, `tomllib`, Julia `TOML`) | Root `CLAUDE.md`: bindings stay thin, logic lives in C++. Five parsers is five accept-sets that drift. Claw's `study-config.ts` already says the quiet part: *"enriching it from the TOML belongs in quiverdb"* | One C++ parser; bindings call a getter |
| **Parsing SQL text** (`sqlite_master.sql`, DDL comments, CHECK constraints) | Quiver has **never** read a byte of SQL text — `src/schema.cpp` L317 selects `name` only, everything else is `PRAGMA`-derived. The DDL-comment route is separately dead: 46 free-text Portuguese comments in three formats, covering 18 of 25 enum-suspect columns, **zero** CHECK constraints, and the richest instance (the block above `CREATE TABLE Consumption`) sits *outside* the parens so `sqlite_master` drops it entirely | `PRAGMA table_info` / `foreign_key_list` as today, plus the TOML sidecar |
| **JSON anywhere in this path** | The format on disk is TOML. The only JSON in Quiver is `LuaRunner::run`'s return encoder, a self-contained anonymous-namespace encoder written specifically so *no binding gains a JSON dependency* (settled decision, root `CLAUDE.md`) | TOML in, C structs out |
| **A directory scan of `database/ui/`** | Tolerance (e): collections load **only** from `main.collections`. SCE ships a fully-formed orphan `agent.toml` that Hub never loads; a scan would load it and diverge from the reference implementation on a real repo | Iterate `main.collections`, resolve `<stem>.toml` |
| **Adding fields to `ScalarMetadata` / `GroupMetadata`** | `bindings/js/src/metadata.ts` L30-31 hardcodes `SCALAR_METADATA_SIZE = 56` / `GROUP_METADATA_SIZE = 32`, and those constants are the **out-buffer allocations** passed to C at L81/96/111/126 — not just read strides. A stale constant is a native write past a JS-owned `Uint8Array`. It also puts an enum map on every `list_scalar_attributes` read | A new `quiver_ui_metadata_t` with its own size constant |
| **`ffigen` regeneration** | Flips enums; breaks Hub at its pinned `e598fb46` | Hand-edit `bindings.dart` L3555 |
| **A `version` / `schema_version` key in the TOML** | Zero of 311 `database/ui/*.toml` files carry one; no parser reads one. There is nothing to extend and nothing to negotiate against | Tolerate unknown keys, log them, pin behaviour with a fixture corpus |

---

## Stack Patterns by Phase Shape

**If the phase changes no C struct (the enum-in-`describe` fix):**
- toml++ in `src/` + three format strings in `src/database_describe.cpp` (`write_collection_section` L56, `print_group_columns` L20, `summarize_collection` L118).
- `describe*` already returns `std::string` through trivial `new_c_str` wrappers (`src/c/database.cpp` L129/141/155) — **all five bindings and Lua get it with zero binding code**.
- Ships as a **patch** bump. No generator runs. No layout audit.

**If the phase grows `quiver_database_options_t` (explicit UI path + locale):**
- This is a **C ABI break**: 8 → 24 bytes (`int` + enum + two `const char*` with padding).
- Every layer that declares the struct by hand must move in the same commit: `include/quiver/c/options.h`; `src/c/options.cpp` (`return {0, QUIVER_LOG_INFO};` is **positional aggregate init** — a new field silently default-inits unless this line is edited); `src/c/database_options.h` `convert_database_options` (needs a NULL guard — NULL = no UI dir); Julia `c_api.jl` (regen) + `database.jl` `build_quiver_database_options` L11-23; Dart `bindings.dart` L3555 (hand) + `database.dart` `_makeOptions` L50-61; Python `_c_api.py` cdef L27-30 + `database.py` `_make_options` L30-37; **JS `ffi-helpers.ts` `makeDefaultOptions`**.
- Lua needs nothing — `LuaRunner` borrows an already-open `Database`; Lua has no lifecycle methods.
- **Minor** bump (a `0.x` minor signals breaking; patch does not) + full release ritual.

**If the phase adds a new C struct + getter (structured UI metadata):**
- New struct, new `free_*`, new symbol in `bindings/js/src/loader.ts` (hand), new size constant in JS, new cdef entry in Python, hand-added Dart binding, regenerated Julia.
- Existing 56/32-byte structs stay untouched — nothing already shipped is at risk.
- Shape precedents for a code→label map, both already in the C API: `quiver_csv_options_t`'s grouped parallel arrays (names[] / entry_counts[] / flattened labels[] + values[], documented in `include/quiver/c/options.h`) for the **layout**, and `quiver_database_free_time_series_data(names, types, data, has_value, column_count, row_count)` for the **free-function signature**.

**If the phase adds any `db:` name in Lua:**
- `bindings/js/test/lua-api-sync.test.ts` fails until `bindings/js/src/lua-api.ts` is hand-updated (the metadata table shape is documented there at L523-547). This is a hard build gate, not a convention.

---

## Version Compatibility — the numbers that constrain the work

| Where | Hardcoded value | Failure mode if it drifts | Confidence |
|-------|-----------------|---------------------------|------------|
| `bindings/js/src/ffi-helpers.ts` L11-14 | `new Uint8Array(8)`, `dv.setInt32(0, ...)`, `dv.setInt32(4, ...)` (three occurrences: L11, L20, L33) | **Native out-of-bounds write.** Grow the options struct to 24 bytes and C writes 16 bytes past a JS-owned buffer. Bun **cannot** call `quiver_database_options_default` (struct-by-value, bun#6139), so JS has no generated fallback to fall back to. **Highest-risk edit in the milestone — own plan, runtime assert** | HIGH (read directly) |
| `bindings/js/src/metadata.ts` L30-31 | `SCALAR_METADATA_SIZE = 56`, `GROUP_METADATA_SIZE = 32`, offsets 0/8/12/16/24/32/40/48 and 0/8/16/24 | Same class — these are out-buffer allocations at L81/96/111/126, not just strides | HIGH (read directly) |
| `bindings/js/src/csv.ts` L24 | `new Uint8Array(56)` + offsets 0/8/16/24/32/40/48 for `quiver_csv_options_t` | Same class. Relevant if the existing `CSVOptions::enum_labels` path is ever wired to the new reader | HIGH |
| `bindings/python/src/quiverdb/_c_api.py` L27-30 (+ L192-204, L378-384) | CFFI **ABI-mode** cdef of every struct | **Silent corruption, no compile error.** ABI mode never sees the real header | HIGH (read directly) |
| `bindings/dart/lib/src/ffi/bindings.dart` L3555 | ffigen output, must be **hand-edited** | Regen flips enums → breaks Hub (pinned `e598fb46` = v0.10.6). Stale `.dart_tool/hooks_runner/` + `.dart_tool/lib/` make tests run the old layout with no warning | HIGH |
| `bindings/julia/src/c_api.jl` + `database_metadata.jl` L46, L56 | Positional `C.quiver_scalar_metadata_t(C_NULL, …, C_NULL)` | **Loud `MethodError`** — the one binding that fails safely | HIGH |
| `cmake/Platform.cmake` L13-14 | `CMAKE_OSX_DEPLOYMENT_TARGET` floored at **13.3** | libc++ marks the floating-point `std::to_chars` used by `database_csv_export.cpp` and `lua_runner.cpp` unavailable below 13.3. It is the **core's** floor, not a binding's. Removing it makes clang stamp the CI runner's own OS version into every published dylib. A higher explicit target is respected; a lower one is raised | HIGH (read directly) |
| `CMakeLists.txt` L4 vs the four binding manifests | all five read `0.10.6` | `scripts/assert_version.py` refuses to bump from a disagreeing state — verified in agreement today | HIGH (read directly) |
| `CHANGELOG.md` L8 | heads `## [0.10.4] — unreleased` | **Already drifted two releases behind `CMakeLists.txt` 0.10.6.** Reconcile by hand *before* the Bump Version workflow runs | HIGH (read directly) |
| `cmake/Dependencies.cmake` | CMake floor 3.26 blocks `FetchContent_Declare(... EXCLUDE_FROM_ALL)` (needs 3.28) | Not touched by this milestone; noted so nobody "fixes" the lua_bin/luac_bin build noise as a side quest | HIGH |

### Compatibility facts that are *not* constraints (checked, clear)

- **toml++ needs no CMake change** for core-side parsing. Verified in `src/CMakeLists.txt` L73-84. It ships in `libquiver` on every platform — `cmake/Platform.cmake` has no per-platform gating of it.
- **`QUIVER_UNVERSIONED_SHARED` stays OFF.** Only the Dart hook sets it. Nothing about TOML parsing changes that.
- **Lua's leg is one function.** `scalar_metadata_lua` (`src/lua_runner.cpp` L1166-1188) is the single scalar→table converter; all seven Lua metadata entry points route through it.
- **`Impl::load_schema_metadata`** (`src/database_impl.h` L342-348) is where parsing lands. Its documented invariant — publish **neither** `schema` nor `type_validator` until validation passes — extends to any UI member: a half-loaded config must not survive a failed lazy load. The UI path must be stored on `Impl` **by the constructor**, because `load_schema_metadata` runs long after `open()`.
- **The no-config byte-identical rule is cheap to honour.** Every assertion in `tests/test_database_describe.cpp` and `tests/test_lua_runner_describe.cpp` is a substring check on fixtures that have no `ui/` directory. The two brittle ones to respect are the *negative* assertions: L84 `EXPECT_FALSE(... "some_float: 2 non-null, 1 null; values")` and L104 `EXPECT_FALSE(db.summarize_collection("AllTypes"), "values {")` — any enrichment emitting a brace-list after a float scalar, or the literal `values {`, fails them regardless of position.

---

## Alternatives Considered

| Recommended | Alternative | When the alternative would be right |
|-------------|-------------|-------------------------------------|
| Parse TOML in the C++ core | Host supplies a pre-parsed map through the API | If every consumer already had a parser *and* filesystem reach. Claw does not: its read sandbox (`Claw/claw/src/tools/native.ts` `readRoots`) cannot reach `database/ui`, which is why this was rejected (PROJECT.md) |
| toml++ v3.4.0 | toml11 / cpptoml | Only if toml++ failed on a real corpus file. It does not — all 117 in-scope files parse with a stock TOML parser (adversarially CONFIRMED), with exactly one unknown key corpus-wide (`conditions`, GNoMo) |
| New `quiver_ui_metadata_t` struct | Extra fields on `quiver_scalar_metadata_t` | Only if JS's 56/32 constants were generated rather than hand-written. They are not, and there is no JS generator at all |
| Convention (`<db_dir>/ui/`) for the first increment | `DatabaseOptions` field from day one | The field is correct eventually (PROJECT.md commits to it) — but it is the expensive path: five call-site sets *plus* the 8→24-byte JS buffer. Convention is correct for 100% of the surveyed corpus, so it is a sequencing lever, not a scope change |
| Read-only, descriptive | Codegen from Julia `@enumx` | Not available. The sets diverge both ways (HTD: 19 `@enumx` vs 11 TOML vocabularies; GNoMo: 3 vs 11, with five vocabularies having no Julia declaration at all) and naming is unmappable across three styles. Foresight's own `enum.toml` header says the values *"mirror the @enumx definitions"* — mirror, not generate |
| Fixture corpus in `tests/schemas/ui/` | Depend on Hub's `test/schema/` fixtures | Hub's 7 negative fixtures (`duplicate_attribute_id`, `missing_attribute_group_id`, `missing_attribute_id`, `missing_collections_array`, `missing_enum_item_id`, `missing_main`, `unknown_type`) never run against a model repo, and Hub ships as an app, not a package. Quiver's own corpus — distilled from the real 117 files — is the only thing that can pin behaviour. Per `tests/CLAUDE.md`, schemas live under `tests/schemas/` and are **never** copied into a binding |

---

## Toolchain Risk Ranking (for phase sequencing)

1. **JS options buffer** (`ffi-helpers.ts`) — silent native OOB write, no generator, no fallback symbol. Isolate in its own plan with a runtime assert.
2. **Python CFFI cdef** — silent corruption, no compile error, ABI mode.
3. **Dart hand-edit + stale `.dart_tool/`** — silent wrong layout in tests; regen is forbidden.
4. **Release ritual** — a native ABI change ships nothing until `publish-s3` → tag → Julia/Python/JS in parallel, with **Dart published by hand** (no CI job runs `hook/build.dart` on any OS). Reconcile `CHANGELOG.md` first.
5. **JS ↔ Lua sync test** — a hard gate, but it fails loudly and the fix is one file.
6. **C++ / toml++ / CMake** — lowest risk in the milestone. Zero build-system change, one shipped idiom to copy.

---

## Sources

- `.../wf_92804be2-64e/journal.jsonl` — the larger survey (5 surveys + 2 adversarial verifications + synthesis). Primary for: Quiver build cost per layer, the JS/Python/Dart/Julia layout-coupling inventory, `tomlplusplus` linkage CONFIRMED-with-caveat, the describe-assertion break list, the release ritual, the format-owner findings. **Preferred wherever the two journals differ** (e.g. card count 68, not the earlier 91; interleaving in 15 files, not 9).
- `.../wf_e5d70f00-efb/journal.jsonl` — earlier survey. Used for: `src/database_describe.cpp` internals and verbatim `quiver_cli` output, `kMaxDistributionCardinality = 64` and its "enum/category case" comment, the `CSVOptions::enum_labels` prior art, `src/schema.cpp` L317 as the only `sqlite_master` touch, toml++ used by exactly one translation unit.
- `C:/Development/Quiver/quiver3/.planning/PROJECT.md` — authoritative scope, settled decisions, out-of-scope list. Nothing above re-opens it.
- Direct repo reads this session (all quoted values re-verified, not taken on trust): `CMakeLists.txt`, `cmake/Dependencies.cmake`, `cmake/Platform.cmake`, `src/CMakeLists.txt`, `src/binary/binary_metadata.cpp` (L230-275, L220-228), `include/quiver/c/options.h`, `bindings/js/src/ffi-helpers.ts`, `bindings/js/src/metadata.ts`, `bindings/python/src/quiverdb/_c_api.py`, `bindings/python/pyproject.toml`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml`, `bindings/julia/generator/Project.toml`, `bindings/js/package.json`, `CHANGELOG.md`.

---
*Stack research for: brownfield C++20 library milestone — TOML UI-metadata sidecar reader*
*Researched: 2026-09-17*
