# Phase 3: Structured Attribute Metadata - Research

**Researched:** 2026-09-19
**Domain:** C++/C API struct design + five-binding FFI decoder mirroring + Lua sol2 binding
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

Carried forward from Phase 1 (binding, D-08..D-17) plus this phase's own D-30..D-43 — copied
verbatim from `03-CONTEXT.md`'s `<decisions>` section. The planner must treat every `D-` item
below as settled; this research does not re-open any of them.

- **D-08** metadata is its own type, never a field on `ScalarMetadata` (= META-03).
- **D-09** two types, eleven fields; one record answers for collection, attribute **and** group.
- **D-10** `vocabulary` stays an **unresolved string**; entries come from a separate getter.
- **D-11** vocabularies cross as parallel arrays (`int64_t**`, `char***`, `size_t*`).
- **D-12** `label` non-optional; empty ≠ unconfigured; `configured` is the discriminator.
- **D-13** absence is spelled empty string, not a nullable pointer.
- **D-14** `format` is a single verbatim string (see D-32 for what "verbatim" can honestly mean).
- **D-15** `quiver_ui_metadata_t` is hole-free at 64 bytes.
- **D-16** `quiver_scalar_metadata_t` / `quiver_group_metadata_t` untouched in either direction.
- **D-30** The C++ half is a header move, not a redesign. `UIMetadata`/`UIEnumEntry` move under
  `include/quiver/` with `QUIVER_API`; `UIConfigSet` stays private. One-way reversible.
- **D-31** `icon`/`display_order` ship in the Phase 3 struct even though only Phase 4 populates
  `display_order`. Do not trim the record to Phase 3's needs.
- **D-32** The record carries one collapsed format string (D-14). Three of four author-declared
  format strings are unreachable through any Quiver surface — documented, not fixed.
- **D-33** ROADMAP criterion 3 must be amended to "the winning key's string round-trips verbatim."
  Do not write a test asserting four-key round-trip; it cannot pass.
- **D-34** Singular getter only, exactly as META-02 is written. No collection-wide listing in
  Phase 3.
- **D-35** The getter is named `get_attribute_ui_metadata(collection, attribute)`. Companions:
  `list_ui_vocabularies()` and `get_ui_vocabulary(name)`. One-way reversible (hand-mirrored in 7
  layers + the `lua-api.ts` sync test).
- **D-36** The getter validates against the live SQL schema (`require_collection` + column check,
  Pattern 2 on a miss) — matching `get_scalar_metadata`. META-02's "rather than throwing" governs
  an unconfigured attribute, not a nonexistent one.
- **D-37** The vocabulary getter ships a dedicated combined free,
  `quiver_database_free_ui_vocabulary(int64_t* codes, char** labels, size_t count)`.
- **D-38** In Lua, an empty text field arrives as `nil`, not `""`. `configured` is always present
  as the discriminator.
- **D-39** A vocabulary in Lua is a 1-based array of `{code = ..., label = ...}` record tables.
- **D-40** `quiver_ui_metadata_t` joins the load-time struct-size gate as the fifth struct,
  appended after `quiver_csv_options_t`, in all four bindings with no reordering.
- **D-41** The getter cannot throw because the sidecar is absent or malformed — only D-36's
  column check can throw.
- **D-42** The single-record getter is caller-allocated (the `get_scalar_metadata`/
  `free_scalar_metadata` pair idiom).
- **D-43** No new dependency and no CMake **dependency/target** work. Julia's generator
  auto-discovers a new header under `include/quiver/c/`; Python's generator needs one line in its
  5-entry `HEADERS` list; Dart's `bindings.dart` is hand-edited; JS's symbol table is hand-written.

### Claude's Discretion

- Whether the three getters live in a new `src/database_ui_metadata.cpp` or join an existing
  `database_metadata.cpp`.
- Exact header filename for the moved public types (`include/quiver/ui_metadata.h` suggested —
  **not** `attribute_metadata.h`, which criterion 4 requires untouched).
- Whether `list_ui_vocabularies` returns sorted or map-order names (state the choice in a test) —
  **this research resolves it for free**: see Pattern 3 below, `std::map` iteration is already
  alphabetical.
- Wave decomposition, subject to the freeze-the-header-first note below — **this research
  refines it**: see Wave Decomposition Risk below (Lua is not gated by the C struct freeze).

### Deferred Ideas (OUT OF SCOPE)

- A collection-wide plural getter (`list_attribute_ui_metadata`) — deferred by D-34, purely
  additive later.
- Four-key format access — foreclosed for this milestone by D-32's ABI-freeze argument.
- D-17 (Phase 1, collapsing FK fields into `optional<ForeignKeyRef>`) — still its own PR, after
  v1; shrinks `quiver_scalar_metadata_t`, which D-16 forbids this milestone.
- claw-side work to consume any of this (pass `uiConfigDir`, call the new getters) — belongs in
  the claw repo, not this roadmap (P-02).
- Collection- and group-level metadata (Phase 4), `validate_ui_config()` (Phase 5), any change to
  `quiver_scalar_metadata_t`/`quiver_group_metadata_t` in either direction (D-16), any work inside
  the claw repo.

</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| META-01 | A new C++ type carries a scalar attribute's UI metadata: label, tooltip, unit, format, hidden flag, enum vocabulary name, and enum values | Pattern 2 (header move mechanics) gives the exact file-by-file edit; Pattern 3 shows the type needs no new fields — it already exists as `UIMetadata`/`UIEnumEntry` in `src/ui_config.h` |
| META-02 | A public C++ getter returns that metadata for a `(collection, attribute)` pair, returning a default-constructed value for an unconfigured attribute rather than throwing | Pattern 3 provides the near-complete getter implementation, with Pitfall 4 flagging the two-source validation split (schema check throws, sidecar lookup never does) |
| META-03 | The metadata crosses the C API as its own struct with its own free function — never as new fields on `quiver_scalar_metadata_t` or `quiver_group_metadata_t` | Pattern 1 gives the exact hole-free 64-byte layout with verified offsets; Don't-Hand-Roll table gives the converter/free precedent to copy |
| META-04 | The getter is bound in all five bindings and in Lua, named per the cross-layer convention | Per-Binding Decoder Cost table names the exact file and closest function to copy in each of the five bindings |
| META-05 | A public method lists the loaded vocabularies, and another returns one vocabulary's entries by name, throwing Pattern 2 on an unknown name | Pattern 3's `list_ui_vocabularies`/`get_ui_vocabulary` implementations; Don't-Hand-Roll table gives the `read_time_series_files` parallel-array precedent for the vocabulary shape |
| META-06 | Adding any new `db:` method keeps `bindings/js/src/lua-api.ts` in sync so `lua-api-sync.test.ts` passes | Lua Sync Gate section gives the exact registration lines, the word-boundary regex behavior verified against the test file, and a character-budget estimate |

</phase_requirements>

## Summary

03-CONTEXT.md already did the scouting: the C++ half is a header move plus two trivial getters
(not a design), and every marshalling shape has a named precedent to copy. This document answers
only the six delta questions the orchestrator asked, all verified against the checked-out tree
this session (not from training memory): the exact hole-free 64-byte struct layout with byte
offsets, the precise file list for the header move, the exact per-binding decoder edits with the
closest function to copy in each, the wave decomposition (including a non-obvious finding: Lua
does **not** wait on the C-header freeze), the test-corpus gap, and the Lua-sync-gate registration
shape with its character-budget cost.

**Primary recommendation:** Order the new C struct's fields by *type*, not by the C++ struct's
declaration order — six `const char*` first, then the `int64_t`, then the two `int`s. This is a
deliberate **deviation** from the `quiver_scalar_metadata_t` precedent (which mirrors the C++
field order verbatim and tolerates 8 bytes of padding to reach 56). D-15's "hole-free" requirement
cannot be met while interleaving pointer and non-pointer fields.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| UI metadata storage/parsing | C++ core (`ui_config.{h,cpp}`) | — | Already built in Phase 1; TOML parsing must never re-happen at any FFI layer |
| Public type definition (`UIMetadata`/`UIEnumEntry`) | C++ core (`include/quiver/`) | — | Value types are Rule-of-Zero, cross the ABI by value/pointer only through the C API, per root Pimpl-vs-value-type convention |
| Schema-existence validation (Pattern 2) | C++ core (`Database::get_attribute_ui_metadata`) | — | One validation source; every layer inherits the same throw text (root error-pattern rule) |
| Struct marshalling (flatten C++ → flat C struct) | C API (`src/c/`) | — | The only layer that knows both the C++ type and the C ABI struct; converter/free pair pattern already established |
| Byte-offset decoding | Each FFI binding (Julia/Dart/Python/JS) | — | No shared decoder exists across bindings by design (per-language marshalling idiom, root "thin bindings" + per-binding boilerplate-is-house-style rules) |
| Direct C++ binding (no struct) | Lua (`src/lua_runner.cpp`, sol2) | — | LuaRunner calls C++ directly, never through the C API (root architecture) — the struct-layout work is **irrelevant** to Lua |
| Agent-facing doc sync | JS (`bindings/js/src/lua-api.ts`) | — | Shipped as prompt payload; `lua-api-sync.test.ts` enforces token-level parity with `lua_runner.cpp` regardless of which binding did the Lua work |

## Standard Stack

No new external dependency. `tomlplusplus` v3.4.0 stays `PRIVATE` on the `quiver` target
(confirmed: it is not linked to `quiver_c`, so the new C struct/converter code — which only reads
already-parsed `std::string`/`int64_t` fields off `UIMetadata`/`UIEnumEntry` — needs no toml++
include anywhere in `src/c/`).

### Installation
No `npm install` / `pip install` / `cargo add` — this phase adds zero packages in any binding.

## Package Legitimacy Audit

**N/A — this phase installs no external packages in any ecosystem.** No table, no audit needed.

## Architecture Patterns

### System Architecture Diagram

```
                         PSR <db_dir>/ui/*.toml  (already parsed by Phase 1)
                                     |
                                     v
                    Database::Impl::require_ui_config()  (existing, unchanged)
                                     |
                                     v
                         UIConfigSet (private, src/ui_config.h)
                     .collections[coll].attributes[attr] -> UIMetadata (PUBLIC after move)
                     .vocabularies[name] -> vector<UIEnumEntry> (PUBLIC after move)
                                     |
              +----------------------+-----------------------+
              v                                               v
   Database::get_attribute_ui_metadata()          Database::list_ui_vocabularies()
   (Pattern-2 column check, then                   Database::get_ui_vocabulary()
    find_attribute() lookup)                       (Pattern-2 on unknown name)
              |                                               |
              v                                               v
   quiver_database_get_attribute_ui_metadata()     quiver_database_list_ui_vocabularies()
   -> flat quiver_ui_metadata_t (caller-alloc)      quiver_database_get_ui_vocabulary()
   -> quiver_database_free_ui_metadata()            -> parallel int64_t[]/char*[] (dedicated free,
                                                        D-37: quiver_database_free_ui_vocabulary)
              |                                               |
   +----------+----------+----------+----------+              |
   v          v          v          v          v              v (same shape, all 5 bindings)
 Julia      Dart      Python       JS         Lua      (Lua bypasses the C API entirely --
(regen)  (hand-edit) (hand-edit) (hand-write) (sol2,     see LuaRunner note below)
                                              direct C++)
```

### Recommended Project Structure

No new directories. New files (Claude's Discretion on exact split, but these are the natural
homes given the existing per-area-file convention documented in `src/CLAUDE.md`/`src/c/CLAUDE.md`):

```
include/quiver/ui_metadata.h        # NEW public header: UIMetadata, UIEnumEntry (moved, QUIVER_API)
include/quiver/c/ui_metadata.h      # NEW C API header: quiver_ui_metadata_t + the 3 functions + sizeof + 2 frees
src/database_ui_metadata.cpp        # NEW (or fold into database_metadata.cpp): 3 getters
src/c/ui_metadata.cpp               # NEW (or fold into c/database_metadata.cpp): converter/free/get/list
```

### Pattern 1: The struct-field-order deviation (the load-bearing finding)

**What:** `quiver_scalar_metadata_t` (the only prior "6+ field C struct" precedent) mirrors its
C++ source struct's field declaration order verbatim and accepts padding: `name`(ptr)@0,
`data_type`(int)@8, `not_null`(int)@12, `primary_key`(int)@16, `default_value`(ptr)@24 (4 bytes of
padding at 20-23 to re-align to 8), `is_foreign_key`(int)@32, `references_collection`(ptr)@40 (4
bytes of padding at 36-39), `references_column`(ptr)@48. Total: 56 bytes, verified by reading
`include/quiver/c/database.h:319-327` field order against the `56`
`static_assert` — 4 pointers × 8B (32) + 4 ints × 4B (16) + 8B padding = 56.

**Why this phase cannot copy that pattern:** D-15 requires the new struct to be **hole-free** at
exactly 64 bytes. 6×`const char*` (48B) + 1×`int64_t` (8B) + 2×`int` (8B) = 64B with **zero**
padding **only if grouped by type** — pointers first (all 8-byte aligned, packed contiguously),
then the `int64_t` (already 8-aligned at offset 48), then the two `int`s (4-aligned, both fit in
the last 8 bytes). Interleaving a 4-byte `int`/`bool` field between pointers (as
`quiver_scalar_metadata_t` does) reintroduces exactly the padding D-15 forbids.

**Verified layout** (arithmetic checked field-by-field):

```c
// include/quiver/c/ui_metadata.h
typedef struct {
    const char* label;        // offset  0  (8 bytes)
    const char* tooltip;      // offset  8  (8 bytes)
    const char* unit;         // offset 16  (8 bytes)
    const char* format;       // offset 24  (8 bytes)
    const char* icon;         // offset 32  (8 bytes)
    const char* vocabulary;   // offset 40  (8 bytes)
    int64_t display_order;    // offset 48  (8 bytes) -- already 8-byte aligned, no padding
    int configured;           // offset 56  (4 bytes)
    int hidden;                // offset 60  (4 bytes)
} quiver_ui_metadata_t;        // sizeof == 64, no padding anywhere
```

Offset math: 6 × 8 = 48 (0,8,16,24,32,40); `display_order` at 48 needs 8-byte alignment — 48 is
already a multiple of 8, so no gap; the two `int`s at 56 and 60 need only 4-byte alignment, both
satisfied, ending exactly at 64. `configured`/`hidden` as C `int` (not `uint8_t`) matches the
existing convention: `quiver_scalar_metadata_t.not_null`/`primary_key`/`is_foreign_key` are all
`int`, never `uint8_t` or `bool`, for booleans crossing the C ABI in this codebase.

**Field-name-to-offset order need not match the C++ struct's own declaration order**
(`configured, label, tooltip, unit, format, icon, hidden, vocabulary, display_order` per
`src/ui_config.h:31-45`) — the converter function (`convert_ui_metadata_to_c`, mirroring
`convert_scalar_to_c` in `src/c/database_helpers.h:167-177`) freely reassigns each C++ field to
whatever C offset the layout needs; there is no requirement that struct field order match across
the two languages, only that the flattening code assigns every field correctly. **Any** ordering
that groups all pointers together, then the `int64_t`, then the two `int`s, is equally hole-free —
the specific string-field order chosen above (label/tooltip/unit/format/icon/vocabulary) simply
mirrors the C++ struct's declaration order for readability, which is convenience, not a
correctness requirement.

**Verification the planner must run:** add
`static_assert(sizeof(quiver_ui_metadata_t) == 64, "...");` plus one `offsetof` assert per field
in `src/c/database_metadata.cpp` (or wherever the getter lands), mirroring the six `static_assert`s
already in `src/c/options.cpp` for `quiver_database_options_t`. This is the only way anyone
downstream (especially JS, which decodes by hand-written literal offset) can trust the numbers
above without re-deriving them.

### Pattern 2: The header move (mechanics, file-by-file)

D-30 already decided *that* the move happens; here is exactly *what changes*, verified against
the current tree:

1. **NEW `include/quiver/ui_metadata.h`** — copy `UIEnumEntry` and `UIMetadata` verbatim from
   `src/ui_config.h:21-45`, add `QUIVER_API` to both (matching the existing
   `struct QUIVER_API ScalarMetadata` precedent in `include/quiver/attribute_metadata.h:12`),
   `#include "export.h"` + `<cstdint>` + `<string>` (no `<map>`/`<vector>` needed here —
   `UIEnumEntry`/`UIMetadata` themselves hold no map, only `UIConfigSet`/`UICollectionConfig` do,
   and those stay private).
2. **`src/ui_config.h`** — delete lines 21-45 (the two struct definitions), add
   `#include "quiver/ui_metadata.h"` at the top. `UICollectionConfig`/`UIConfigSet` (lines 49-86)
   are unchanged text — they already reference `UIMetadata`/`UIEnumEntry` by name, and the type is
   now defined elsewhere, not duplicated.
3. **`src/ui_config.cpp`** — **no functional change**. Every use (`meta.configured = true`, etc.)
   is unchanged since the type is identical, just relocated. The file's `#include "ui_config.h"`
   already transitively pulls in the new public header.
4. **`include/quiver/database.h`** — add `#include "quiver/ui_metadata.h"` (alongside the existing
   `#include "quiver/attribute_metadata.h"` at line 5), and declare the three new public methods
   inside `class QUIVER_API Database` (near `get_scalar_metadata` at line 154, matching how
   `has_ui_config()` was added near the other Phase-1/2 UI methods at line 230).
5. **`src/CMakeLists.txt`** — **this is a real edit D-43 does not call out and is easy to miss.**
   D-43's "no CMake work" means no new dependency/target — it does **not** mean no edit at all. The
   `QUIVER_SOURCES` list (lines 2-38) and the `quiver_c` target's explicit source list (lines
   118-138) are both **hand-maintained flat lists**, verified by reading the file directly — there
   is no `file(GLOB ...)`. A new `src/database_ui_metadata.cpp` and `src/c/ui_metadata.cpp` (or
   whatever filenames Claude's Discretion picks) **must** be added to these two lists by hand, or
   the new `.cpp` silently never compiles while the header still declares the symbol — a link
   error, not a compile error, and one that only appears once a binding tries to call the new
   function.
6. **No header install-list edit needed** — `install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/quiver
   ...)` (`src/CMakeLists.txt:112`) recursively installs the whole `include/quiver/` tree, so a new
   header under it needs no separate `install()` line.
7. **`QUIVER_API` on a plain value type under hidden visibility** — already answered by precedent,
   not a new question: `ScalarMetadata`/`GroupMetadata` are exactly this shape (Rule-of-Zero
   aggregate, no virtuals, crosses the DLL boundary by value through an exported method) and both
   already carry `QUIVER_API` (`include/quiver/attribute_metadata.h:12,21`). Follow the same
   convention for `UIMetadata`/`UIEnumEntry` — no new investigation needed.

### Pattern 3: The C++ getters are nearly one-liners

Because `find_attribute`/`find_vocabulary` (`src/ui_config.h:89-91`) already return
`const UIMetadata*` / `const std::vector<UIEnumEntry>*` and — after the move — `UIMetadata` **is**
the public return type with no conversion step, the three getters are almost trivial:

```cpp
// Source: derived from src/database_metadata.cpp:6-16 (get_scalar_metadata's validation shape,
// D-36) + src/ui_config.h:88-91 (the lookup helpers, unchanged by the move)
UIMetadata Database::get_attribute_ui_metadata(const std::string& collection,
                                                const std::string& attribute) const {
    impl_->require_collection(collection, "get_attribute_ui_metadata");
    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def->get_column(attribute)) {
        throw std::runtime_error("Scalar attribute not found: '" + attribute + "' in collection '" +
                                 collection + "'");
    }
    impl_->require_ui_config();
    if (!impl_->ui_config) {
        return UIMetadata{};
    }
    const auto* meta = find_attribute(*impl_->ui_config, collection, attribute);
    return meta ? *meta : UIMetadata{};
}

std::vector<std::string> Database::list_ui_vocabularies() const {
    impl_->require_ui_config();
    std::vector<std::string> names;
    if (impl_->ui_config) {
        for (const auto& [name, entries] : impl_->ui_config->vocabularies) {
            names.push_back(name);  // std::map key order == alphabetical, for free
        }
    }
    return names;
}

std::vector<UIEnumEntry> Database::get_ui_vocabulary(const std::string& name) const {
    impl_->require_ui_config();
    const std::vector<UIEnumEntry>* vocab =
        impl_->ui_config ? find_vocabulary(*impl_->ui_config, name) : nullptr;
    if (!vocab) {
        throw std::runtime_error("Vocabulary not found: '" + name + "'");
    }
    return *vocab;
}
```

**Resolves the "Claude's Discretion — sorted or map-order" question for free**: `UIConfigSet::
vocabularies` is `std::map<std::string, std::vector<UIEnumEntry>>` (`src/ui_config.h:60`), and
`std::map`'s iteration order is already the key's `operator<` (lexicographic for `std::string`) —
so "iterate the map" *is* "return sorted names" with zero extra code. No decision to make, only a
comment to leave stating the coincidence is not accidental should the storage type ever change.

### Anti-Patterns to Avoid

- **Do not duplicate `UIMetadata`/`UIEnumEntry`** into a new public struct with a converter from
  the private one — D-30 is explicit that this is a move, and `find_attribute`/`find_vocabulary`
  already return the type by pointer, so a duplicate type would need a needless copy step at every
  call site.
- **Do not mirror `quiver_scalar_metadata_t`'s field order** for the new C struct — see Pattern 1.
  This is the one place in the phase where copying the closest precedent literally produces the
  wrong answer.
- **Do not skip the `offsetof` static_asserts** — `src/c/options.cpp` already sets the precedent
  (6 `static_assert`s for a 4-field, 24-byte struct); a 9-field, 64-byte struct with a hand-decoded
  JS consumer needs it more, not less.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Vocabulary parallel-array crossing | A bespoke two-array marshaller per binding | Copy `read_time_series_files`'s exact shape (`include/quiver/c/database.h:527-531`, allocation in `src/c/database_time_series.cpp:414-427`, free at `:462-473`) | Already crosses all 6 layers; only the element types differ (`int64_t`+`char*` here vs `char*`+`char*` there) |
| Single-record caller-allocated getter | A bespoke alloc idiom per binding | Copy `get_scalar_metadata`/`free_scalar_metadata` (`include/quiver/c/database.h:349-352,370`, `src/c/database_metadata.cpp:20-30`) | Already fixes the idiom for Julia `Ref`, Dart `arena<T>()`, Python `ffi.new("T*")`, JS `new Uint8Array(SIZE)` |
| Struct-size load-time gate | A sixth from-scratch gate mechanism | Extend the existing 4-struct gate in all four bindings to 5 (append, same fixed order) | `src/c/CLAUDE.md:98-110` already states this is "not a decision to revisit per struct" |
| Lua record-array shape | A code-keyed Lua table for the vocabulary | Copy `dimension_to_lua` (`src/lua_runner.cpp:1077-1092`) — a 1-based array of record tables | The JSON return encoder makes a code-keyed table shape-unstable (D-39, verified against `src/lua_runner.cpp:137-166`) |

**Key insight:** every marshalling shape this phase needs already exists once in the codebase.
The work is applying five known patterns to one new struct, not inventing a sixth pattern.

## Common Pitfalls

### Pitfall 1: Forgetting the `src/CMakeLists.txt` source-list edit
**What goes wrong:** the new `.cpp` file is created and the header declares the new symbols, but
nothing adds the `.cpp` to `QUIVER_SOURCES` / the `quiver_c` explicit list — the build succeeds
(nothing referenced the new symbol yet in-tree) until a binding's FFI call fails to resolve, or
`quiver_tests`/`quiver_c_tests` link-fails on an undefined reference.
**Why it happens:** D-43 says "no CMake work," which reads as "skip CMakeLists.txt entirely" if
not read carefully — it means "no new dependency/target," not "no source-list edit."
**How to avoid:** the planner's task list must explicitly include the two `src/CMakeLists.txt`
line-list edits as their own line items, not assume they're bundled into "add the new file."
**Warning signs:** `quiver_tests.exe`/`quiver_c_tests.exe` builds green with zero new tests running
— a sign the new `.cpp` compiled nothing because it isn't in the source list at all (silent, since
CMake doesn't warn about an uncompiled file sitting in a source directory).

### Pitfall 2: Copying `quiver_scalar_metadata_t`'s field order into the new struct
**What goes wrong:** the struct ends up hole-y (e.g. 72 bytes with 8 bytes of padding) if fields
are declared in C++ declaration order (`configured, label, tooltip, ...`) with `bool`s/`int`s
interleaved among pointers.
**Why it happens:** `quiver_scalar_metadata_t` is the only precedent for a "many-field C struct,"
and copying its shape (mirror the C++ order, accept padding) is the path of least resistance.
**How to avoid:** group by type — see Pattern 1's exact field order — and add the `static_assert`s
before writing a single binding decoder.
**Warning signs:** the `static_assert(sizeof(...) == 64, ...)` fails at compile time — this is
the gate that catches the mistake immediately, provided it is written first.

### Pitfall 3: Treating "freeze the header before any binding starts" as applying to Lua
**What goes wrong:** the plan sequences Lua's `get_attribute_ui_metadata_lua` binding after the C
struct is frozen, adding an unnecessary dependency edge and shrinking the parallel window.
**Why it happens:** the ROADMAP's freeze-first note is phrased generally ("the five decoders
parallelize once it is frozen"), and Lua is easy to lump in with "the binding work."
**How to avoid:** `LuaRunner` calls `Database::get_attribute_ui_metadata` **directly in C++**
(verified: `src/lua_runner.cpp`'s `get_scalar_metadata_lua` at line 1645 calls
`db.get_scalar_metadata(...)` with no C API involvement — root architecture doc: "LuaRunner ...
NOT through the C API"). Lua's binding depends only on the **C++ getter signatures**, which can be
frozen far earlier than the C struct layout. See Wave Decomposition below.
**Warning signs:** none at build time — this is a scheduling inefficiency, not a correctness bug,
but it costs a full wave of calendar time if missed.

### Pitfall 4: Reusing `find_attribute`'s nullptr-on-miss contract as "throws" for META-02
**What goes wrong:** `find_attribute`/`find_vocabulary` never throw (D-41 already documents this:
absence-because-no-sidecar and absence-because-not-configured both return `nullptr`/default). The
getter for META-02 must distinguish "attribute doesn't exist in the SQL schema" (Pattern 2, throws)
from "attribute exists but the sidecar doesn't configure it" (returns default-constructed,
`configured == false`) — these are two different checks against two different sources (the live
`Schema`, and the loaded `UIConfigSet`), and it is easy to conflate them into one lookup.
**Why it happens:** the private helper's contract ("never throws") reads like it should also
govern the public getter's contract, but D-36 is explicit that the public getter's validation
happens **before** the private lookup is even consulted.
**How to avoid:** structure the getter exactly as in Pattern 3 above — schema/column check first
(can throw), sidecar lookup second (never throws, returns default on miss).

## Runtime State Inventory

Not applicable — this is a greenfield-within-milestone phase (new getters), not a rename, refactor,
or migration. No stored data, live service config, OS-registered state, secrets, or build
artifacts carry an old name that this phase changes.

## Code Examples

Verified against `tests/schemas/ui/enum_basic/ui/storage.toml` and `enum.toml`
(read this session):

```toml
# ui/enum.toml
[[bool]]
id = 0
label = "Disabled"
[[bool]]
id = 1
label = "Enabled"

# ui/storage.toml
[[attribute]]
id = "has_commitment"
enum = "bool"
label = "Has Commitment"

[[attribute]]
id = "notes"
label = ""
```

Worked results any test in this phase can assert against, once the getters land:

```cpp
// get_attribute_ui_metadata("Storage", "has_commitment")
// -> UIMetadata{ configured=true, label="Has Commitment", vocabulary="bool", ... }

// get_ui_vocabulary("bool")
// -> { UIEnumEntry{0, "Disabled"}, UIEnumEntry{1, "Enabled"} }

// get_attribute_ui_metadata("Storage", "notes")
// -> UIMetadata{ configured=true, label="", ... }  -- D-12: declared-blank, not "unconfigured"

// get_attribute_ui_metadata("Storage", "<a real column the TOML never mentions>")
// -> UIMetadata{ configured=false, label="", ... }  -- META-02's "default-constructed" case

// get_attribute_ui_metadata("Storage", "no_such_column")
// -> throws "Scalar attribute not found: 'no_such_column' in collection 'Storage'"  -- D-36
```

### JS decoder (the concrete pattern to copy, adapted from `metadata.ts:38-50`)

```typescript
// Source: pattern copied from bindings/js/src/metadata.ts's readScalarMetadataAt
function readUiMetadataAt(base: Pointer, byteOffset: number): UiMetadata {
  const labelPtr = read.ptr(base, byteOffset + 0);
  const tooltipPtr = read.ptr(base, byteOffset + 8);
  const unitPtr = read.ptr(base, byteOffset + 16);
  const formatPtr = read.ptr(base, byteOffset + 24);
  const iconPtr = read.ptr(base, byteOffset + 32);
  const vocabPtr = read.ptr(base, byteOffset + 40);
  return {
    label: labelPtr === 0 ? "" : new CString(labelPtr as Pointer).toString(),
    tooltip: tooltipPtr === 0 ? "" : new CString(tooltipPtr as Pointer).toString(),
    unit: unitPtr === 0 ? "" : new CString(unitPtr as Pointer).toString(),
    format: formatPtr === 0 ? "" : new CString(formatPtr as Pointer).toString(),
    icon: iconPtr === 0 ? "" : new CString(iconPtr as Pointer).toString(),
    vocabulary: vocabPtr === 0 ? "" : new CString(vocabPtr as Pointer).toString(),
    displayOrder: Number(read.i64(base, byteOffset + 48)),
    configured: read.i32(base, byteOffset + 56) !== 0,
    hidden: read.i32(base, byteOffset + 60) !== 0,
  };
}
```

`UI_METADATA_SIZE = 64` belongs in `ffi-helpers.ts` alongside `SCALAR_METADATA_SIZE` /
`GROUP_METADATA_SIZE` / `CSV_OPTIONS_SIZE` (same cycle-avoidance reason documented in
`bindings/js/CLAUDE.md`: `loader.ts` needs it for the gate, and importing from a
`database.ts`-importing module would create a cycle).

## Per-Binding Decoder Cost (delta question 3)

For **each** of the three new C functions
(`quiver_database_get_attribute_ui_metadata`, `quiver_database_list_ui_vocabularies`,
`quiver_database_get_ui_vocabulary`) plus their frees
(`quiver_database_free_ui_metadata`, `quiver_database_free_ui_vocabulary`) and the size accessor
(`quiver_ui_metadata_sizeof`):

| Binding | (a) Struct + size accessor joining the gate | (b) Single-record getter | (c) Vocabulary parallel-array pair + free | Closest existing function to copy |
|---|---|---|---|---|
| **Julia** | Add `quiver_ui_metadata_t` line to `_assert_struct_sizes`/`_CHECKED_STRUCTS` in `generator/prologue.jl` (5th entry, after `quiver_csv_options_t`) — regenerate, don't hand-edit `c_api.jl` | `get_scalar_metadata` (`bindings/julia/src/database_metadata.jl:45-53`) — same `Ref(...)` + `check(...)` + `free_*` shape | `read_time_series_files` (`bindings/julia/src/database_read.jl:744-772`) — same `unsafe_wrap(Array, ptr, count)` + zip-into-result + combined-free shape | `read_time_series_files` for (c); `get_scalar_metadata` for (b) |
| **Dart** | Add a `callNativeSizeof('quiver_ui_metadata_sizeof', ...)` line to `library_loader.dart`'s gate (hand-edited, 5th) | `getScalarMetadata` (`bindings/dart/lib/src/database_metadata.dart:6-28`) — `Arena` + `arena<T>()` + `.fromNative(ref)` + `finally { arena.releaseAll() }` | `readTimeSeriesFiles` (`bindings/dart/lib/src/database_read.dart:1348-1381`) — same paired-`Pointer<Pointer<Char>>` + manual zip shape | `readTimeSeriesFiles` for (c); `getScalarMetadata` for (b) |
| **Python** | Add `("quiver_ui_metadata_t", "quiver_ui_metadata_sizeof")` tuple to `_loader.py`'s `_STRUCT_CHECKS`-equivalent list (5th) | `get_scalar_metadata` (`bindings/python/src/quiverdb/database.py:1277`, `_parse_scalar_metadata` at `:2337`) — `ffi.new("quiver_scalar_metadata_t*")` + `check(...)` + parse + free | `read_time_series_files` (`bindings/python/src/quiverdb/database.py:1889-1911`) — same `ffi.new("char***")` × N + zip + `finally: free(...)` shape | `read_time_series_files` for (c); `get_scalar_metadata` for (b) |
| **JS** | Add `UI_METADATA_SIZE` to `ffi-helpers.ts` + one line to `assertNativeStructSizes` in `loader.ts` (5th, after `quiver_csv_options_t`) | `getScalarMetadata` (`bindings/js/src/metadata.ts:72-85`) — `new Uint8Array(SIZE)` + `check(...)` + `readXAt(ptr(buf), 0)` + `free` | `readTimeSeriesFiles` (`bindings/js/src/time-series.ts:385-418`) — same `allocPtrOut()` × 2 + `decodeStringArray`/manual-pointer-loop + combined free shape | `readTimeSeriesFiles` for (c); `getScalarMetadata` for (b) |
| **Lua** | N/A — no struct crosses; `LuaRunner` calls C++ directly | `get_scalar_metadata_lua` (`src/lua_runner.cpp:1645-1651`) — thin lambda calling the C++ getter, converting via a `scalar_metadata_lua`-style table builder | `dimension_to_lua` (`src/lua_runner.cpp:1077-1092`) — build a 1-based `sol::table` of `{code=..., label=...}` record tables | Both precedents live in the **same file**; no FFI struct involved at all |

The C API's own new file (`src/c/ui_metadata.cpp`) needs one converter/free pair per D-42/D-37,
directly modeled on `convert_scalar_to_c`/`free_scalar_fields`
(`src/c/database_helpers.h:167-183`) for the single-record shape, and the allocation/free bodies
of `quiver_database_read_time_series_files`/`quiver_database_free_time_series_files`
(`src/c/database_time_series.cpp:397-473`) for the vocabulary shape — verbatim except the second
array's element type (`int64_t` instead of `char*`).

## State of the Art

Not applicable in the traditional sense (no library upgrade / deprecated API here) — but worth
recording as the phase's own "state of the art" progression: this is the **fifth** struct to join
the load-time gate (options → scalar_metadata → group_metadata → csv_options → **ui_metadata**),
and the pattern has now repeated enough times (Phase 2's csv_options was the fourth, added
retroactively in gap-closure) that `src/c/CLAUDE.md:98-110` already promotes it to a standing rule
rather than a per-struct decision. Nothing here is new territory; it is the fourth repetition of an
established mechanism.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Recommended file split (`database_ui_metadata.cpp` / `c/ui_metadata.cpp` / `ui_metadata.h` names) | Recommended Project Structure | None — 03-CONTEXT.md explicitly leaves this to Claude's Discretion; any consistent naming works, this is a suggestion not a requirement |
| A2 | The vocabulary getter's Pattern-2 message text (`"Vocabulary not found: '<name>'"`) | Pattern 3 code example | Low — no locked wording exists yet in any CONTEXT.md; must match root Pattern 2 shape (`"{Entity} not found: {identifier}"`) but the exact noun is undecided; a planner should confirm no existing test somewhere already expects different wording before locking it |

**All other claims in this document are `[VERIFIED]`** against files read this session (cited
inline with path:line) — struct offsets, source-list contents, decoder function bodies, the
LuaRunner-bypasses-C-API architecture claim, and the `std::map` ordering claim were all confirmed
by reading the actual code, not recalled from training data.

## Open Questions

1. **Exact vocabulary-not-found message wording**
   - What we know: root Pattern 2 shape (`"{Entity} not found: {identifier}"`); `get_scalar_metadata`'s
     sibling message is `"Scalar attribute not found: '<attr>' in collection '<coll>'"`.
   - What's unclear: whether the entity noun should be `"Vocabulary"`, `"UI vocabulary"`, or
     something else — no locked decision exists in 03-CONTEXT.md.
   - Recommendation: planner picks one (`"Vocabulary not found: '<name>'"` is the simplest Pattern-2
     fit) and states it explicitly in the plan; it becomes load-bearing across 6 layers immediately
     (mirrored error-channel tests), so get it right once.

2. **Whether `database_ui_metadata.cpp`/`c/ui_metadata.cpp` are new files or additions to existing
   `database_metadata.cpp`/`c/database_metadata.cpp`**
   - What we know: both existing files already hold the sibling scalar/group metadata getters and
     converters; either choice compiles and works identically.
   - What's unclear: nothing technical — this is purely an organizational call left to Claude's
     Discretion by 03-CONTEXT.md.
   - Recommendation: new files, matching the "UI sidecar gets its own file" precedent already set
     by `ui_config.cpp` being separate from `database_describe.cpp` even though only the latter
     consumes it.

## Environment Availability

Skipped — this phase has no external tool/service/runtime dependency beyond what Phases 1-2
already require (a C++20 compiler, CMake, and the five binding toolchains already in daily use).
No new probe is needed.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 (C++/C API), Test.jl (Julia), `package:test` (Dart), pytest (Python), `bun:test` (JS) |
| Config file | `tests/CMakeLists.txt` (C++/C API); each binding's existing test runner — none is new |
| Quick run command | `./build/bin/quiver_tests.exe --gtest_filter='*UiMetadata*'` (once the new test file exists) |
| Full suite command | `scripts/test-all.bat` |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| META-01 | New C++ type carries 8 fields + vocabulary name | unit | `quiver_tests.exe --gtest_filter='DatabaseUiMetadata.*'` | ❌ Wave 0 — new `tests/test_database_ui_metadata.cpp` |
| META-02 | Getter returns default-constructed for unconfigured; throws Pattern 2 on unknown column | unit | same file | ❌ Wave 0 |
| META-03 | C struct is separate from `quiver_scalar_metadata_t`/`quiver_group_metadata_t` (byte-diff check) | unit (C API) | `quiver_c_tests.exe --gtest_filter='CApiDatabaseUiMetadata.*'` | ❌ Wave 0 — new `tests/test_c_api_database_ui_metadata.cpp` |
| META-04 | Bound in all 5 bindings + Lua, named per convention | integration | each binding's own new test file/case | ❌ Wave 0 for each binding |
| META-05 | `list_ui_vocabularies`/`get_ui_vocabulary`, Pattern 2 on unknown name, in all layers | unit + integration | same files as above | ❌ Wave 0 |
| META-06 | `lua-api-sync.test.ts` stays green after the 3 new `db:` names land | automated | `bun test test/lua-api-sync.test.ts` | ✅ exists — enforces itself, no new file needed |

### Sampling Rate
- **Per task commit:** the relevant single suite (`quiver_tests.exe --gtest_filter=...`, or the
  one binding's own `test.bat`).
- **Per wave merge:** `scripts/test-all.bat` (all six suites + CLI smoke test).
- **Phase gate:** full suite green before `/gsd-verify-work`, plus the four struct-size gate
  tests (`struct-sizes.test.ts`, `test_struct_sizes.jl`, `test_struct_sizes.py`,
  `struct_sizes_test.dart`) updated to assert **five** structs, not four.

### Wave 0 Gaps
- [ ] `tests/test_database_ui_metadata.cpp` — covers META-01/02/05 at the C++ layer
- [ ] `tests/test_c_api_database_ui_metadata.cpp` — covers META-03 at the C API layer (byte-layout
      + struct-independence proof)
- [ ] One new test file/section per binding (Julia/Dart/Python/JS) — covers META-04
- [ ] **A new fixture gap, confirmed this session**: no fixture in `tests/schemas/ui/` exercises a
      vocabulary **declared with a non-empty name but zero entries** (`my_vocab = []` in
      `enum.toml`). `empty_enum` (per `tests/schemas/ui/README.md:29`) is the **zero-byte file**
      case — an absent map entirely, which throws Pattern 2 the same way an undeclared name would.
      A declared-but-empty vocabulary is a **present key with an empty `vector`** (`find_vocabulary`
      returns a non-null pointer to an empty vector, not `nullptr`) — structurally different, and
      currently untested anywhere in the corpus. **Confirmed, not merely repeated from
      03-CONTEXT.md**: verified by reading `UIConfigSet::parse_enum_content`
      (`src/ui_config.cpp:149-180`) — a vocabulary name mapped to a zero-length TOML array parses
      to `vocabularies[name] = {}` (an empty vector, present key), which is exactly the
      untested state. Add either a new tiny fixture or a scratch-directory TOML string (matching
      Phase 1's `ScratchSidecarDir` pattern used for edge cases not worth a tracked fixture) to
      prove `get_ui_vocabulary("empty_but_declared")` returns `[]` rather than throwing.
- [ ] No other corpus gap found beyond the one above — every other META-0x behavior (configured vs
      not, empty-string label vs absent, bare-string vocabulary) already has a covering fixture
      from Phase 1 (`enum_basic`, `htd_like`, `bess_like`).

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | No | Not applicable — no auth surface in this phase |
| V3 Session Management | No | Not applicable |
| V4 Access Control | No | Not applicable — a UI metadata getter has no access-control dimension beyond the existing schema/collection checks already in place |
| V5 Input Validation | Yes | The two Pattern-2 checks (unknown column, unknown vocabulary name) — both are validation-and-reject, no new parsing surface (the TOML content is already validated by Phase 1's parser) |
| V6 Cryptography | No | Not applicable |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Buffer over-read from a stale/mismatched struct size in a binding | Tampering / DoS | The struct-size load-time gate (SAFE-01..03, already built in Phase 2) — the new struct joins it (D-40); this is the primary risk class this phase touches, already fully mitigated by an existing mechanism |
| Attacker-controlled TOML content reaching a getter | Information Disclosure | Not new — Phase 1's parser already treats the TOML sidecar as trusted, PSR-authored content, not untrusted user input; this phase reads already-parsed data and adds no new parsing |
| Unfree'd C allocation (leak, not corruption) from a decoder that forgets the dedicated combined free | DoS (resource exhaustion) | D-37's dedicated `quiver_database_free_ui_vocabulary` exists precisely to prevent a caller from forgetting to free one of two parallel arrays — each binding's test should assert the free path runs (no valgrind/ASan requirement beyond what `quiver_tests`/`quiver_c_tests` already run under CI, per `.github/CLAUDE.md`) |

## Lua Sync Gate — Exact Registration Shape and Character-Budget Estimate (delta question 6)

### Registration (C++ side, `src/lua_runner.cpp`)

Three new lines beside the existing metadata registrations (`bind.set_function("get_scalar_metadata",
...)` block at lines 665-672), in the **same commit** as the `lua-api.ts` edit per the canonical
refs' explicit instruction:

```cpp
bind.set_function("get_attribute_ui_metadata", &get_attribute_ui_metadata_lua);
bind.set_function("list_ui_vocabularies", &list_ui_vocabularies_lua);
bind.set_function("get_ui_vocabulary", &get_ui_vocabulary_lua);
```

Each `*_lua` function follows `get_scalar_metadata_lua`'s exact shape (`src/lua_runner.cpp:1645-
1651`): a static function taking `(Database&, ..., sol::this_state)`, calling the new C++ getter,
then building a `sol::table` via a `*_lua`-suffixed converter analogous to `scalar_metadata_lua`.
Per D-38, the converter must map every **empty** string field to `sol::lua_nil` (not `""`), the
same treatment `scalar_metadata_lua` already gives `default_value`/`references_collection`/
`references_column` — extended here to `label`/`tooltip`/`unit`/`format`/`icon`/`vocabulary`
whenever they are empty, with `configured` always present as the discriminator (D-38, already
decided — not re-litigated here).

### `lua-api-sync.test.ts` word-boundary matching (verified against the test file, this session)

Read directly from `bindings/js/test/lua-api-sync.test.ts:19-21,44-45`: the check is
`` new RegExp(`${token}(?![a-z0-9_])`).test(LUA_DB_API_REFERENCE) `` — a negative lookahead for a
following identifier character. This means:
- `db:get_attribute_ui_metadata` in the doc satisfies `get_attribute_ui_metadata` but **not**
  `get_ui_vocabulary` or `list_ui_vocabularies` — each of the three needs its **own** literal
  occurrence in `lua-api.ts`, exactly as the canonical refs already state (no shortcut via a
  shared prefix).
- Because `set_function("<name>", ...)` is parsed by regex from `lua_runner.cpp` directly (Pass 1
  in the sync test, `/\b(bind|ns)\.set_function\(\s*"([a-z_][a-z0-9_]*)"/g`), the three literal
  strings above are picked up automatically — no separate registration list to keep in sync beyond
  the `bind.set_function(...)` calls themselves.

### Character-budget estimate

The existing `## Metadata` section in `lua-api.ts` (lines 510-561, verified by reading the file:
`get_scalar_metadata`/`get_vector_metadata`/`get_set_metadata`/`get_time_series_metadata` plus
their four list forms plus two table-shape examples) is **≈1,650 characters** for **4 getters + 4
listers + 2 shape blocks**. The new surface is smaller — 1 getter + 1 lister + 1 by-name fetcher +
1 shape block (9 fields, roughly the same size as the existing scalar-metadata shape block) — a
reasonable estimate is **≈700-1,000 characters**, comfortably inside the **≈5.6k character
headroom** 03-CONTEXT.md already recorded (from `C:/Development/Claw/claw1/test/prompt.test.ts:38-
46`'s 60k budget against ~54.4k actual usage). No budget risk; the estimate is provided so the
planner does not need to re-derive it, not because it is close to the limit.

## Wave Decomposition Risk (delta question 4)

**The non-obvious finding**: the ROADMAP's "freeze the C header before any binding decoder starts"
note applies to the **four FFI bindings** (Julia/Dart/Python/JS) only. **Lua depends on none of
it** — `LuaRunner` calls `Database::get_attribute_ui_metadata`/`list_ui_vocabularies`/
`get_ui_vocabulary` directly in C++ (verified: `src/lua_runner.cpp`'s existing metadata lambdas at
lines 1645-1720 call the C++ `Database` methods with zero C API involvement — confirmed
architecturally in `src/CLAUDE.md`'s "LuaRunner ... NOT through the C API" statement, and in
`CLAUDE.md` root's "LuaRunner ... executes Lua scripts against a database (direct sol2 binding,
NOT through C API)"). Lua's binding is blocked only on the **C++ getter signatures** being frozen —
a much earlier, cheaper freeze point than the C struct's byte layout.

### Smallest tracer slice (MVP mode)

A single vertical slice proving the whole path end-to-end, mirroring Phase 1's `01-01-PLAN.md`
tracer-slice style:

1. Header move (`include/quiver/ui_metadata.h` + edits to `ui_config.h`/`database.h`).
2. The three C++ getters (Pattern 3 above), with C++-level tests over the `enum_basic` fixture.
3. The C struct (Pattern 1's exact layout) + its `static_assert`s + `quiver_ui_metadata_sizeof` +
   the single-record get/free pair — **skip the vocabulary pair in the tracer** (it is a second,
   independent shape; proving the single-record path end-to-end is sufficient to validate the
   struct layout and the caller-allocated idiom).
4. **One** FFI binding's decoder for the single-record getter (pick whichever binding the team can
   iterate fastest in — no technical reason favors one over another; Python's cdef/hand-written
   decoder has the shortest edit-test loop since it needs no code generation step at all, unlike
   Julia's regenerate-and-diff cycle).
5. Lua's binding for all three methods (cheap — no struct decode, and per the finding above, not
   blocked on step 3's struct work at all; can literally happen in parallel with step 3).
6. The `lua-api.ts` doc edit, in the same commit as step 5's `bind.set_function` calls.

### What genuinely parallelizes after the tracer

Once step 3 lands (the struct is frozen — sizes, offsets, and the get/free pair proven against
one binding in step 4), the **remaining three** FFI bindings' single-record decoders parallelize
freely (disjoint files per binding, confirmed by the ROADMAP's own parallelization note). The
vocabulary pair (parallel-array shape) can be added by each binding in the **same or a later
wave** — it reuses each binding's own `read_time_series_files` precedent, so it carries no
additional cross-binding coordination risk once the single-record shape is proven.

Lua (step 5) and the tracer's struct work (step 3) can run **fully in parallel** — this is a wave
the existing ROADMAP notes do not call out, and skipping it costs one full wave of unnecessary
sequencing.

## Sources

### Primary (HIGH confidence — verified by reading the file this session)
- `src/ui_config.h`, `src/ui_config.cpp` — full read, current `UIMetadata`/`UIEnumEntry`/
  `UIConfigSet` definitions and parse/lookup logic
- `include/quiver/c/database.h:319-336` (struct layouts), `:514-545` (time-series-files precedent)
- `src/c/options.cpp` (static_assert precedent), `src/c/database_metadata.cpp` (sizeof accessors +
  get/free bodies), `src/c/database_helpers.h:150-210` (converter/free pair), `src/c/database_time_
  series.cpp:397-473` (parallel-array alloc/free precedent)
- `src/CMakeLists.txt` (explicit source lists, both `quiver` and `quiver_c` targets — confirmed no
  glob)
- `bindings/js/src/metadata.ts`, `ffi-helpers.ts`, `loader.ts`, `time-series.ts:370-418` — full
  decoder/gate/precedent read
- `bindings/julia/src/database_metadata.jl`, `database_read.jl:700-772`, `generator/prologue.jl`,
  `generator/generator.jl` — full decoder/gate/generator-discovery read
- `bindings/dart/lib/src/database_metadata.dart:1-40`, `database_read.dart:1330-1382` — decoder read
- `bindings/python/src/quiverdb/_loader.py`, `database.py:1870-1930` — gate + decoder read
- `bindings/python/generator/generator.py:1-40` — confirmed 5-entry `HEADERS` list
- `src/lua_runner.cpp` (multiple ranges: 530-540, 660-672, 1077-1092, 1640-1700) — registration
  shape + converter precedents
- `bindings/js/test/lua-api-sync.test.ts:1-60` — word-boundary regex, full read
- `bindings/js/src/lua-api.ts` (grep for section headers + full read of lines 505-561)
- `tests/schemas/ui/README.md`, `tests/schemas/ui/enum_basic/ui/*.toml` — fixture corpus read
- `.planning/config.json` — `nyquist_validation: true`, `security_enforcement: true` confirmed

### Secondary (MEDIUM confidence)
- None — every claim above was either verified this session or is explicitly flagged in the
  Assumptions Log.

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- Struct layout (Pattern 1): HIGH — arithmetic independently verified against the existing 24-byte
  and 56-byte precedents, both read from source
- Header move mechanics: HIGH — every file touched was read this session; the CMakeLists.txt gap
  is a new finding not present in 03-CONTEXT.md
- Per-binding decoder cost: HIGH — every "closest function to copy" was read in full, not inferred
- Wave decomposition (Lua bypass finding): HIGH — verified against both the root and `src/`
  CLAUDE.md architecture statements and the actual `lua_runner.cpp` call sites
- Test corpus gap: HIGH — verified by reading the parser code path that produces the untested state
- Lua sync gate cost: HIGH for the mechanism (regex read directly), MEDIUM for the exact character
  estimate (a reasonable extrapolation from the existing Metadata section's size, not a written doc)

**Research date:** 2026-09-19
**Valid until:** No expiry driver — this is an in-repo architecture document, not a
version-pinned external dependency; valid until the C API header or any of the five bindings'
struct-decoder files change underneath it.
