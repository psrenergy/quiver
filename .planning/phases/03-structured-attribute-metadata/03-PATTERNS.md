# Phase 3: Structured Attribute Metadata - Pattern Map

**Mapped:** 2026-09-19
**Files analyzed:** ~28 (new + modified, across C++/C API/5 bindings/tests)
**Analogs found:** 27 / 28 (one file — `lua-api.ts` doc block — has a shape precedent but no code analog)

CONTEXT.md and RESEARCH.md already did the scouting for this phase (five parallel code scouts +
a dedicated research pass) and named exact file:line precedents for nearly every file this phase
touches. This document reorganizes those citations into the File Classification / Pattern
Assignment shape the planner consumes, verifies the ones load-bearing for a memory-layout bug, and
adds the test-file mirror mapping RESEARCH.md did not tabulate explicitly.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `include/quiver/ui_metadata.h` (NEW) | model (public value type) | transform (move only) | `include/quiver/attribute_metadata.h` (`ScalarMetadata`/`GroupMetadata`) | exact |
| `src/ui_config.h` (MODIFIED — delete 2 structs, add include) | model (private aggregate) | transform | itself, pre-move | exact (surgical edit) |
| `src/ui_config.cpp` (MODIFIED — no functional change) | service | transform | itself | exact (no-op edit) |
| `include/quiver/database.h` (MODIFIED — 3 new method decls + include) | controller (public API surface) | request-response | existing `get_scalar_metadata` decl block (line 154), `has_ui_config()` decl (line 230) | exact |
| `src/database_ui_metadata.cpp` (NEW) | controller (C++ getters) | request-response (CRUD-read) | `src/database_metadata.cpp:6-16` (`get_scalar_metadata`) | exact |
| `src/CMakeLists.txt` (MODIFIED — 2 source-list edits) | config | batch | existing `QUIVER_SOURCES` / `quiver_c` explicit lists (lines 2-38, 118-138) | exact |
| `include/quiver/c/ui_metadata.h` (NEW) | model (C ABI struct) + route (function decls) | request-response | `include/quiver/c/database.h:319-336` (`quiver_scalar_metadata_t`) for struct shape (deviated per Pattern 1); `:527-531,538` (`read_time_series_files`) for vocabulary decls | exact (struct shape), exact (vocab decls) |
| `src/c/ui_metadata.cpp` (NEW) | controller (C API converter/free/get/list) | request-response | `src/c/database_helpers.h:167-183` (`convert_scalar_to_c`/`free_scalar_fields`); `src/c/database_time_series.cpp:397-473` (alloc/free for parallel arrays) | exact |
| `src/c/database_metadata.cpp` or wherever `static_assert`s land | test/config (compile-time gate) | — | `src/c/options.cpp:9-14` (6 `static_assert`s for `quiver_database_options_t`) | exact |
| `bindings/julia/generator/prologue.jl` (MODIFIED) | config (struct-size gate) | batch | existing 4-entry list, `quiver_csv_options_t` entry | exact |
| `bindings/julia/src/database_metadata.jl` (MODIFIED — add 3 methods) | service (FFI decoder) | request-response | `get_scalar_metadata` (lines 45-53) | exact |
| `bindings/julia/src/database_read.jl` (MODIFIED — add vocabulary reader) | service (FFI decoder) | request-response | `read_time_series_files` (lines 744-772) | exact |
| `bindings/julia/test/test_database_metadata.jl` (MODIFIED/NEW cases) | test | — | itself (existing `get_scalar_metadata` test cases) | exact |
| `bindings/dart/lib/src/ffi/bindings.dart` (MODIFIED, hand-edited) | config (FFI symbol decls) | — | existing hand-edited entries for `quiver_scalar_metadata_t`/`get_scalar_metadata` | exact |
| `bindings/dart/lib/src/ffi/library_loader.dart` (MODIFIED — 5th gate entry) | config | batch | existing 4-entry `callNativeSizeof` gate | exact |
| `bindings/dart/lib/src/database_metadata.dart` (MODIFIED — add 3 methods) | service (FFI decoder) | request-response | `getScalarMetadata` (lines 6-28) | exact |
| `bindings/dart/lib/src/database_read.dart` (MODIFIED — add vocabulary reader) | service (FFI decoder) | request-response | `readTimeSeriesFiles` (lines 1348-1381) | exact |
| `bindings/dart/test/metadata_test.dart` (MODIFIED/NEW cases) | test | — | itself | exact |
| `bindings/python/generator/generator.py` (MODIFIED — 1 line in `HEADERS`) | config | batch | existing 5-entry (soon) `HEADERS` list | exact |
| `bindings/python/src/quiverdb/_c_api.py` (MODIFIED, hand-edited cdef) | config (FFI cdef) | — | existing `quiver_scalar_metadata_t` cdef block | exact |
| `bindings/python/src/quiverdb/_loader.py` (MODIFIED — 5th gate entry) | config | batch | existing struct-check list | exact |
| `bindings/python/src/quiverdb/database.py` (MODIFIED — add 3 methods) | service (FFI decoder) | request-response | `get_scalar_metadata`/`_parse_scalar_metadata` (lines 1277, 2337); `read_time_series_files` (lines 1889-1911) | exact |
| `bindings/python/tests/test_database_metadata.py` (MODIFIED/NEW cases) | test | — | itself | exact |
| `bindings/js/src/loader.ts` (MODIFIED — symbol table + 5th gate entry) | config (hand-written FFI symbols) | — | existing `quiver_scalar_metadata_t` gate entry (lines 338-343, 413-443) | exact |
| `bindings/js/src/ffi-helpers.ts` (MODIFIED — `UI_METADATA_SIZE` const) | config | — | `SCALAR_METADATA_SIZE`/`GROUP_METADATA_SIZE`/`CSV_OPTIONS_SIZE` | exact |
| `bindings/js/src/metadata.ts` (MODIFIED — add decoder) | service (FFI decoder) | request-response | `readScalarMetadataAt` (lines 38-50, 72-85) | exact |
| `bindings/js/src/time-series.ts` (MODIFIED — add vocabulary reader) | service (FFI decoder) | request-response | `readTimeSeriesFiles` (lines 385-418) | exact |
| `bindings/js/test/database-metadata.test.ts` (MODIFIED/NEW cases) | test | — | itself | exact |
| `bindings/js/test/struct-sizes.test.ts` / `bindings/julia/test/test_struct_sizes.jl` / `bindings/python/tests/test_struct_sizes.py` / `bindings/dart/test/struct_sizes_test.dart` (all MODIFIED — 4→5 count) | test | — | themselves | exact |
| `src/lua_runner.cpp` (MODIFIED — 3 `*_lua` functions + 3 `bind.set_function` calls) | route + service (sol2 binding) | request-response | `get_scalar_metadata_lua` (lines 1645-1651) + `scalar_metadata_lua` (1660-1675) for the record converter; `dimension_to_lua` (1077-1092) for the vocabulary array shape | exact |
| `bindings/js/src/lua-api.ts` (MODIFIED — doc tokens) | config (doc/prompt payload) | — | `## Metadata` section (lines 510-561) | exact |
| `tests/test_database_ui_metadata.cpp` (NEW) | test | — | `tests/test_database_metadata.cpp` | exact |
| `tests/test_c_api_database_ui_metadata.cpp` (NEW) | test | — | `tests/test_c_api_database_metadata.cpp` | exact |
| `tests/schemas/ui/*` (fixture additions, if the empty-vocabulary gap is closed with a tracked fixture) | fixture | — | `tests/schemas/ui/enum_basic/ui/*.toml` | exact |

## Pattern Assignments

### `include/quiver/ui_metadata.h` (model, header move)

**Analog:** `include/quiver/attribute_metadata.h:12,21` (`struct QUIVER_API ScalarMetadata` / `GroupMetadata`)

**Pattern to copy** — a Rule-of-Zero aggregate crossing the DLL boundary by value:
```cpp
#include "quiver/export.h"
#include <cstdint>
#include <string>

struct QUIVER_API UIEnumEntry {
    // copy verbatim from src/ui_config.h:21-? (fields: code, label)
};

struct QUIVER_API UIMetadata {
    // copy verbatim from src/ui_config.h:31-45 — do NOT reorder fields here;
    // C++ struct field order is irrelevant to the C struct's hole-free layout (Pattern 1 lives
    // only in the C API struct, not this one)
};
```
No `<map>`/`<vector>` needed — only `UIConfigSet`/`UICollectionConfig` (which stay private) hold
those containers. **Do not duplicate the type with a converter** — `find_attribute`/
`find_vocabulary` (`src/ui_config.h:89-91`) already return this type by pointer.

### `src/database_ui_metadata.cpp` (NEW) — the three C++ getters

**Analog:** `src/database_metadata.cpp:6-16` (`get_scalar_metadata`'s two-source validation shape)

RESEARCH.md Pattern 3 already wrote these nearly verbatim (reproduced here for the planner to
paste directly):

```cpp
UIMetadata Database::get_attribute_ui_metadata(const std::string& collection,
                                                const std::string& attribute) const {
    impl_->require_collection(collection, "get_attribute_ui_metadata");
    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def->get_column(attribute)) {
        throw std::runtime_error("Scalar attribute not found: '" + attribute + "' in collection '" +
                                 collection + "'");
    }
    impl_->require_ui_config();
    if (!impl_->ui_config) return UIMetadata{};
    const auto* meta = find_attribute(*impl_->ui_config, collection, attribute);
    return meta ? *meta : UIMetadata{};
}
```
Schema/column check first (can throw, Pattern 2); sidecar lookup second (never throws, per D-41).
**Do not** let `find_attribute`'s nullptr-on-miss contract stand in for the whole getter's
contract — Pitfall 4 in RESEARCH.md is exactly this conflation.

`list_ui_vocabularies()` / `get_ui_vocabulary(name)` — same file, same analog file, same pattern;
`get_ui_vocabulary` throws Pattern 2 (`"Vocabulary not found: '<name>'"` — RESEARCH.md's Open
Question 1 flags this exact wording as unlocked; pick it and record it, it becomes load-bearing
across 6 layers immediately).

### `src/c/ui_metadata.cpp` (NEW) — C API struct + converter + free + vocabulary pair

**Analog for the single record:** `src/c/database_helpers.h:167-183` (`convert_scalar_to_c` /
`free_scalar_fields`) + `include/quiver/c/database.h:349-352,370` + `src/c/database_metadata.cpp:
20-33,67-76` (`get_scalar_metadata`/`free_scalar_metadata`, the caller-allocated idiom, D-42).

**Analog for the vocabulary pair:** `src/c/database_time_series.cpp:397-473`
(`quiver_database_read_time_series_files` / `quiver_database_free_time_series_files`) — copy
verbatim except the second array's element type is `int64_t` here, not `char*` (D-37's dedicated
`quiver_database_free_ui_vocabulary(int64_t* codes, char** labels, size_t count)`).

**CRITICAL — struct field order deviates from the closest textual analog.** Do NOT mirror
`quiver_scalar_metadata_t`'s declaration-order-with-padding shape. Verified hole-free 64-byte
layout (RESEARCH.md Pattern 1, arithmetic double-checked here):
```c
typedef struct {
    const char* label;        // offset  0
    const char* tooltip;      // offset  8
    const char* unit;         // offset 16
    const char* format;       // offset 24
    const char* icon;         // offset 32
    const char* vocabulary;   // offset 40
    int64_t display_order;    // offset 48 (already 8-aligned, no gap)
    int configured;           // offset 56
    int hidden;                // offset 60
} quiver_ui_metadata_t;        // sizeof == 64, zero padding
```
Group by type — all pointers, then the `int64_t`, then the two `int`s — never interleave. Add
`static_assert(sizeof(quiver_ui_metadata_t) == 64, ...)` plus one `offsetof` assert per field,
mirroring `src/c/options.cpp:9-14`'s six `static_assert`s, **before** any binding decoder is
written — this is the gate that catches a padding mistake at compile time instead of at a
hand-decoded JS offset three layers downstream.

### Per-binding single-record decoder (4 FFI bindings)

| Binding | File to edit | Analog (file:line) | What to copy |
|---|---|---|---|
| Julia | `bindings/julia/src/database_metadata.jl` | `get_scalar_metadata`, lines 45-53 | `Ref(...)` alloc + `check(...)` + `free_*` call shape |
| Dart | `bindings/dart/lib/src/database_metadata.dart` | `getScalarMetadata`, lines 6-28 | `Arena` + `arena<T>()` + `.fromNative(ref)` + `finally { arena.releaseAll() }` |
| Python | `bindings/python/src/quiverdb/database.py` | `get_scalar_metadata` (line 1277) + `_parse_scalar_metadata` (line 2337) | `ffi.new("quiver_scalar_metadata_t*")` + `check(...)` + parse + free |
| JS | `bindings/js/src/metadata.ts` | `readScalarMetadataAt`, lines 38-50, 72-85 | `new Uint8Array(SIZE)` + `check(...)` + `readXAt(ptr(buf), 0)` + `free`; the exact byte-offset decoder is already written out in RESEARCH.md's Code Examples section — copy it directly, only the field values change |

### Per-binding vocabulary parallel-array decoder (4 FFI bindings)

| Binding | File to edit | Analog (file:line) | What differs from the analog |
|---|---|---|---|
| Julia | `bindings/julia/src/database_read.jl` | `read_time_series_files`, lines 744-772 | `unsafe_wrap(Array, ptr, count)` twice — one `Int64`, one `Cstring` — then zip; single combined free |
| Dart | `bindings/dart/lib/src/database_read.dart` | `readTimeSeriesFiles`, lines 1348-1381 | Two `Pointer<...>` (one `Pointer<Int64>`, one `Pointer<Pointer<Char>>`) + manual zip |
| Python | `bindings/python/src/quiverdb/database.py` | `read_time_series_files`, lines 1889-1911 | `ffi.new("int64_t**")` alongside `ffi.new("char***")`, same `finally: free(...)` shape |
| JS | `bindings/js/src/time-series.ts` | `readTimeSeriesFiles`, lines 385-418 | `allocPtrOut()` × 2, one decoded as an int64 array, one via `decodeStringArray`/manual pointer loop; combined free |

### Struct-size gate — 4th → 5th entry (4 files, mechanical)

**Analog:** each binding's own existing 4-entry gate (all four were written in Phase 2 for
`quiver_csv_options_t` — same file, same list, append one line, no reordering, per D-40):
- `bindings/js/src/loader.ts:338-343,413-443`
- `bindings/julia/generator/prologue.jl:126-133`
- `bindings/python/src/quiverdb/_loader.py:22-27`
- `bindings/dart/lib/src/ffi/library_loader.dart:94-99`

Their four test counterparts (4→5 assertion count):
`bindings/js/test/struct-sizes.test.ts:26-47`, `bindings/julia/test/test_struct_sizes.jl:23-38`,
`bindings/python/tests/test_struct_sizes.py:23-44`, `bindings/dart/test/struct_sizes_test.dart:23-40`.

### `src/lua_runner.cpp` (MODIFIED) — Lua binding, no C API involved

**Analog for the getter wrapper:** `get_scalar_metadata_lua`, lines 1645-1651 — thin lambda
calling the C++ getter directly (LuaRunner never goes through the C API — root architecture rule).

**Analog for the record converter:** `scalar_metadata_lua`, lines 1660-1675 — maps every absent/
empty optional to `sol::lua_nil`. Per D-38, extend the same nil-mapping to
`label`/`tooltip`/`unit`/`format`/`icon`/`vocabulary` whenever empty (this is Lua's one deliberate
divergence from D-13's "empty string means absent" — document it in root `CLAUDE.md`'s per-binding
divergence list per the context's instruction). `configured` is always present as the discriminator.

**Analog for the vocabulary array shape:** `dimension_to_lua`, lines 1077-1092 — a 1-based
`sol::table` of `{code = ..., label = ...}` record tables. **Do not** build a code-keyed map — the
JSON return encoder (`src/lua_runner.cpp:137-166,173-184`) would silently vary the output shape
with the data (array when keys are `1..n`, sorted-object otherwise) or drop codes entirely.

**Registration:** three `bind.set_function(...)` lines beside the existing metadata block
(lines 665-672):
```cpp
bind.set_function("get_attribute_ui_metadata", &get_attribute_ui_metadata_lua);
bind.set_function("list_ui_vocabularies", &list_ui_vocabularies_lua);
bind.set_function("get_ui_vocabulary", &get_ui_vocabulary_lua);
```
Must land in the **same commit** as the `lua-api.ts` doc edit (see below) — canonical refs are
explicit about this.

### `bindings/js/src/lua-api.ts` (MODIFIED) — doc/prompt sync

**Analog:** the existing `## Metadata` section, lines 510-561 (4 getters + 4 listers + 2 shape
blocks, ≈1,650 characters) — copy its structure (one getter block, one lister block, one
by-name-fetcher block, one shape-example block).

**No code analog exists for the sync mechanism itself** — `lua-api-sync.test.ts` is the analog for
correctness, not for prose shape. Key constraint verified directly against the test file
(`bindings/js/test/lua-api-sync.test.ts:19-21,44-45`): matching is
word-boundary-suffixed (`` new RegExp(`${token}(?![a-z0-9_])`) ``), so each of
`get_attribute_ui_metadata`, `list_ui_vocabularies`, `get_ui_vocabulary` needs its **own** literal
occurrence in the doc text — a shared prefix does not satisfy all three. Estimated new text size:
≈700-1,000 characters, well inside the ≈5.6k character headroom already recorded in CONTEXT.md.

## Shared Patterns

### The caller-allocated get/free idiom (single record)
**Source:** `include/quiver/c/database.h:349-352,370` + `src/c/database_metadata.cpp:20-33,67-76`
**Apply to:** `quiver_database_get_attribute_ui_metadata` / `quiver_database_free_ui_metadata`, and
every binding's decoder of it. Fixes the allocation idiom per language: Julia `Ref`, Dart
`arena<T>()`, Python `ffi.new("T*")`, JS `new Uint8Array(SIZE)`.

### The two-parallel-array crossing with a dedicated combined free
**Source:** `include/quiver/c/database.h:527-531,538` + `src/c/database_time_series.cpp:397-473`
(`read_time_series_files` / `quiver_database_free_time_series_files`)
**Apply to:** `quiver_database_get_ui_vocabulary` / `quiver_database_free_ui_vocabulary` (D-37) and
all four bindings' decoders of it. This is why D-37 sides with the cited precedent over the
"reuse two generic frees" budget line — a caller that frees only one of two arrays leaks silently.

### The load-time struct-size gate (now a 5-struct list)
**Source:** `src/c/CLAUDE.md:98-110` — "every hand-allocated struct joins this list by default."
**Apply to:** all four FFI bindings' gate files + their four test files. Purely mechanical
(append, same order: options → scalar_metadata → group_metadata → csv_options → **ui_metadata**).

### Pattern 2 (not-found) error text, two instances this phase introduces
**Source:** root `CLAUDE.md` error pattern table + `get_scalar_metadata`'s sibling message
(`"Scalar attribute not found: '<attr>' in collection '<coll>'"`)
**Apply to:** (1) the getter's own column-not-found check (reuses the existing scalar-attribute
message verbatim — same wording, do not invent a UI-specific variant); (2) `get_ui_vocabulary`'s
unknown-name check, wording **not yet locked** — RESEARCH.md recommends
`"Vocabulary not found: '<name>'"`; the planner must state this choice explicitly since it is
mirrored by hand in 7 layers plus each layer's error-channel test.

### Lua's "empty string arrives as nil" divergence (D-38)
**Source:** `src/lua_runner.cpp:772-781` (the `db:read_csv` header-nil precedent) +
`scalar_metadata_lua:1660-1675` (the existing optional→nil mapping)
**Apply to:** the new `*_lua` converter for `UIMetadata`. This is documented as a sixth entry on
the per-binding divergence list — add it to root `CLAUDE.md` per the context's own instruction,
not just to the new converter code.

## Test File Mirror Map (all seven layers)

| New test file | Mirrors (existing file) | What to copy from it |
|---|---|---|
| `tests/test_database_ui_metadata.cpp` (NEW) | `tests/test_database_metadata.cpp` | Fixture setup via `tests/schemas/ui/enum_basic`, one `TEST` per getter behavior (configured, declared-blank, unconfigured-but-real-column, nonexistent-column-throws, vocabulary-found, vocabulary-not-found) |
| `tests/test_c_api_database_ui_metadata.cpp` (NEW) | `tests/test_c_api_database_metadata.cpp` | Struct independence/byte-layout assertions (`sizeof`, `offsetof` checks mirrored at the test level, not just compile-time `static_assert`), get/free round-trip, vocabulary get/free round-trip, leak-shape assertion (free called on every path) |
| `bindings/julia/test/test_database_metadata.jl` (add cases) | itself | Existing `get_scalar_metadata` test structure — same fixture, same `@test` shape |
| `bindings/dart/test/metadata_test.dart` (add cases) | itself | Existing `getScalarMetadata` test group |
| `bindings/python/tests/test_database_metadata.py` (add cases) | itself | Existing `get_scalar_metadata` test class/functions |
| `bindings/js/test/database-metadata.test.ts` (add cases) | itself | Existing `getScalarMetadata` test block |
| Lua test cases (wherever `db:get_scalar_metadata` is exercised via `LuaRunner`, likely `tests/test_lua_runner_metadata.cpp` or similar — grep for existing Lua metadata test file before creating a new one) | the sibling C++ test that runs a Lua script string and inspects the JSON-encoded return | Existing `get_scalar_metadata` Lua-script test's structure (script string → `run()` → JSON-decode → assert) |
| `bindings/js/test/lua-api-sync.test.ts` | itself (no new file — self-enforcing) | N/A — verify green after the 3-line `lua_runner.cpp` + `lua-api.ts` edits, no test-file changes needed |
| The 4 struct-size test files | themselves | Bump asserted struct count 4 → 5, add one line naming `quiver_ui_metadata_t` and its expected size (64) |

**Untested state to close (from RESEARCH.md's Wave 0 Gaps, repeated here because it affects test
file content, not just file classification):** no fixture in `tests/schemas/ui/` currently
exercises a vocabulary **declared with a non-empty name but zero entries** (as opposed to
`empty_enum`'s zero-byte-file case, which is a different code path — an absent map entry, not a
present key with an empty vector). Either add a new tiny fixture under `tests/schemas/ui/` or use
a scratch-directory TOML string (Phase 1's `ScratchSidecarDir` pattern) inside
`tests/test_database_ui_metadata.cpp` to prove `get_ui_vocabulary("declared_but_empty")` returns
`{}` rather than throwing.

## No Analog Found

| File/Concern | Role | Reason |
|---|---|---|
| `include/quiver/c/ui_metadata.h`'s specific hole-free field-grouped layout | model (C struct) | No prior C struct in this codebase groups fields by type instead of mirroring C++ declaration order — `quiver_scalar_metadata_t` is the only "many-field" precedent and it is explicitly the wrong shape to copy here (Pattern 1). Treat RESEARCH.md's verified layout as the source of truth, not a codebase analog. |
| Lua test file location for the new `db:` methods | test | Could not confirm the exact existing file name that exercises `db:get_scalar_metadata` via Lua script in this pass — grep `tests/` for `get_scalar_metadata` inside a Lua script string literal before creating a new test file, to avoid duplicating an existing one. |
| `"Vocabulary not found: '<name>'"` exact wording | error message | No existing test or CONTEXT.md decision locks this string. It is Pattern-2-shaped but the noun is genuinely new — RESEARCH.md's Open Question 1. The planner, not an analog, must pick it. |

## Metadata

**Analog search scope:** `include/quiver/`, `src/`, `src/c/`, `tests/`, all five `bindings/*/`
directories — all already enumerated with file:line precision in 03-CONTEXT.md's canonical-refs
and 03-RESEARCH.md's Per-Binding Decoder Cost table; this pass verified those citations are
internally consistent and added the test-mirror table and no-analog callouts RESEARCH.md left
implicit.
**Files scanned:** ~40 (context/research citations cross-checked; test directory listing done live)
**Pattern extraction date:** 2026-09-19
