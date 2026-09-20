---
phase: 03-structured-attribute-metadata
plan: 01
subsystem: database
tags: [c++, c-api, ffi, cffi, python, sqlite, toml, ui-metadata]

requires:
  - phase: 01-enum-labels-in-describe
    provides: "UIMetadata/UIEnumEntry private types + UIConfigSet parser + require_ui_config lazy-load/swallow-on-failure contract"
  - phase: 02-config-path-locale-and-struct-size-safety
    provides: "DatabaseOptions.ui_config_dir/ui_locale, the 4-struct load-time size gate mechanism (options/scalar/group/csv) every FFI binding extends"
provides:
  - "Public quiver::UIMetadata / quiver::UIEnumEntry types (include/quiver/ui_metadata.h)"
  - "Database::get_attribute_ui_metadata / list_ui_vocabularies / get_ui_vocabulary (C++ signatures frozen for all later plans in this phase)"
  - "quiver_ui_metadata_t: hole-free 64-byte C struct, quiver_ui_metadata_sizeof(), quiver_database_get_attribute_ui_metadata/free_ui_metadata"
  - "Python UiMetadata dataclass + Database.get_attribute_ui_metadata, with the struct as the 5th entry in the load-time size gate"
affects: [03-02, 03-03, 03-04, 03-05, 03-06, 03-07]

actuals:
  tokens: 11527
  tasks: 3
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Two-source validation split for a config-backed getter: schema/column check throws (Pattern 2), sidecar lookup never does (guard the disengaged std::optional before every find_* call)"
    - "C struct fields grouped by type (all pointers, then int64_t, then ints) to stay hole-free, deliberately deviating from the declaration-order-with-padding precedent"

key-files:
  created:
    - include/quiver/ui_metadata.h
    - tests/test_database_ui_metadata.cpp
    - bindings/python/tests/test_database_ui_metadata.py
  modified:
    - src/ui_config.h
    - include/quiver/database.h
    - src/database_metadata.cpp
    - include/quiver/c/database.h
    - src/c/database_metadata.cpp
    - bindings/python/src/quiverdb/_c_api.py
    - bindings/python/src/quiverdb/metadata.py
    - bindings/python/src/quiverdb/database.py
    - bindings/python/src/quiverdb/__init__.py
    - bindings/python/src/quiverdb/_loader.py
    - bindings/python/tests/test_struct_sizes.py
    - tests/CMakeLists.txt

key-decisions:
  - "Task 1 checkpoint auto-approved 'approve' (the freeze as specified) — mode: yolo is active in .planning/config.json; this is the plan's own recommended default and matches every locked 03-CONTEXT decision."
  - "Both getters/converter joined existing database_metadata.cpp / c/database_metadata.cpp files (no new .cpp), so src/CMakeLists.txt's two hand-maintained source lists needed zero edits."
  - "quiver_ui_metadata_t groups all six string pointers first, then display_order, then configured/hidden — never mirrors quiver_scalar_metadata_t's padded declaration order."
  - "list_ui_vocabularies() relies on std::map's already-lexicographic iteration order for 'sorted names' at zero extra cost — documented with a comment against future storage-type changes."
  - "get_ui_vocabulary collapses 'no sidecar' and 'sidecar present but lacks the name' into one Pattern 2 throw — no second error string, no second code path."

patterns-established:
  - "Single-record C API getter: caller-allocated out-struct + quiver::string::new_c_str/delete[] pair + dedicated free function, matching get_scalar_metadata/free_scalar_metadata"
  - "Python UiMetadata decode: decode_string (never decode_string_or_none) on every string field, since the C side allocates every field unconditionally (D-13: empty string spells absence, never NULL)"

requirements-completed: [META-01, META-02, META-03, META-04]

coverage:
  - id: D1
    description: "get_attribute_ui_metadata returns a full UIMetadata record (label, tooltip, unit, format, icon, hidden, vocabulary, display_order) for a configured attribute"
    requirement: "META-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.ConfiguredAttributeCarriesLabelAndVocabulary"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.UnitDistinguishesOffset16FromNeighbours"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.HiddenDistinguishesOffset60FromConfigured"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py#test_configured_attribute_carries_label_and_vocabulary"
        status: pass
    human_judgment: false
  - id: D2
    description: "A real, unconfigured SQL column returns a default-constructed record (configured=False) rather than throwing; a nonexistent column throws the exact Pattern 2 message"
    requirement: "META-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.RealUnconfiguredColumnReturnsDefaultWithoutThrowing"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.NonexistentColumnThrowsExactPattern2Message"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py#test_nonexistent_column_raises_exact_pattern_2_message"
        status: pass
    human_judgment: false
  - id: D3
    description: "All three getters are safe on a database with no ui/ sidecar at all (disengaged std::optional): default record, empty vocabulary list, Vocabulary-not-found throw"
    requirement: "META-02"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.NoSidecarGetAttributeReturnsDefaultRecord"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.NoSidecarListUiVocabulariesReturnsEmpty"
        status: pass
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.NoSidecarGetUiVocabularyThrowsSameAsUndeclaredName"
        status: pass
    human_judgment: false
  - id: D4
    description: "format's 4-key table form collapses to the winning key's string verbatim (ROADMAP criterion 3, D-32) -- capacity's Storage.capacity reads back '0.0000'"
    requirement: "META-01"
    verification:
      - kind: unit
        ref: "tests/test_database_ui_metadata.cpp#DatabaseUiMetadata.FormatTableCollapsesToWinningKeyVerbatim"
        status: pass
    human_judgment: false
  - id: D5
    description: "quiver_ui_metadata_t is a hole-free 64-byte C struct (compile-time static_assert on sizeof + 9 offsetof pins), with quiver_scalar_metadata_t/quiver_group_metadata_t/attribute_metadata.h left byte-identical"
    requirement: "META-03"
    verification:
      - kind: unit
        ref: "cmake --build build --config Debug (10 static_asserts in src/c/database_metadata.cpp)"
        status: pass
      - kind: other
        ref: "git diff --stat include/quiver/attribute_metadata.h src/CMakeLists.txt src/ui_config.cpp (empty)"
        status: pass
    human_judgment: false
  - id: D6
    description: "A Python caller reads the same UI metadata record through the FFI, proving the 64-byte layout survives a hand-written decoder, and the struct joins the 5-entry load-time size gate with its wiring proven by an executed mutation"
    requirement: "META-04"
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py (7 tests)"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_struct_sizes.py#test_gate_ran_and_checked_all_five_structs_in_order"
        status: pass
    human_judgment: false

duration: 35min
completed: 2026-09-19
status: complete
---

# Phase 3 Plan 1: Structured Attribute UI Metadata (Tracer) Summary

**One attribute's PSR `ui/` sidecar record now crosses the whole stack — TOML sidecar → public C++ getter → hole-free 64-byte C struct → hand-written Python decoder — proven end to end against the `enum_basic` fixture.**

## Performance

- **Duration:** ~35 min
- **Started:** 2026-09-19T23:33:57-03:00 (last pre-existing commit)
- **Completed:** 2026-09-19T23:56:39-03:00
- **Tasks:** 3 (Task 1 checkpoint auto-approved, Task 2 tracer, Task 3 test coverage)
- **Files modified:** 15 (2 created new, 13 modified)

## Accomplishments

- `include/quiver/ui_metadata.h` publishes `UIMetadata`/`UIEnumEntry` as `QUIVER_API` types, moved verbatim out of the private `src/ui_config.h` (D-30) — `src/ui_config.cpp` needed zero edits since the type is relocated, not redefined.
- Three new `Database` methods (`get_attribute_ui_metadata`, `list_ui_vocabularies`, `get_ui_vocabulary`) implement the two-source validation split: the live-schema/column check throws Pattern 2 on a miss, the sidecar lookup never does — every path guards `Impl::ui_config`'s disengaged `std::optional` before dereferencing it.
- `quiver_ui_metadata_t` is a hole-free 64-byte C struct (six `const char*`, then `int64_t display_order`, then two `int`s), pinned by a `static_assert(sizeof(...) == 64)` plus 9 `offsetof` asserts — the load-bearing deviation from `quiver_scalar_metadata_t`'s padded, declaration-order shape.
- Python gets a full round trip: `UiMetadata` frozen dataclass, `Database.get_attribute_ui_metadata`, and the struct as the 5th entry in the load-time struct-size gate (`_STRUCT_SIZEOF_ACCESSORS`), with `_CHECKED_STRUCTS`'s wiring-evidence test extended to 5 entries.
- `tests/test_database_ui_metadata.cpp` (14 cases) closes the one confirmed corpus gap — a vocabulary declared with a name but zero entries — via a scratch sidecar, and separately proves `list_ui_vocabularies`'s alphabetical-order claim from non-alphabetical TOML source order.
- The Python struct-size gate's wiring-evidence mutation was **executed**, not just described: commenting out the dev-mode `_assert_struct_sizes()` call site turned `test_gate_ran_and_checked_all_five_structs_in_order` red (`assert [] == [...5 items...]`); restoring the call site turned the full Python suite green again (334/334).

## Task Commits

Each task was committed atomically:

1. **Task 1: Confirm the one-way freeze** — auto-approved "approve" (checkpoint:decision, no file changes; `mode: yolo` is active in `.planning/config.json`)
2. **Task 2: End-to-end tracer (C++ getters, C struct, Python decoder)** - `1c1bcca` (feat)
3. **Task 3: C++ test coverage + Python's 5th struct-size gate entry** - `6c621c2` (test)

_Note: no TDD gate applies to this plan (tasks are `type="tracer"`/`"auto"`, not `tdd="true"`)._

## Files Created/Modified

- `include/quiver/ui_metadata.h` - NEW public header: `UIMetadata`/`UIEnumEntry` (`QUIVER_API`)
- `src/ui_config.h` - deletes the two moved struct definitions, includes the new public header
- `include/quiver/database.h` - `#include "quiver/ui_metadata.h"` + three new method declarations
- `src/database_metadata.cpp` - the three C++ getters, appended after the existing scalar/group getters
- `include/quiver/c/database.h` - `quiver_ui_metadata_t`, `quiver_ui_metadata_sizeof`, get/free declarations
- `src/c/database_metadata.cpp` - 10 layout-pin `static_assert`s, the sizeof accessor, `convert_ui_metadata_to_c`/`free_ui_metadata_fields`, the getter and free function
- `bindings/python/src/quiverdb/_c_api.py` - hand-edited cdef: `quiver_ui_metadata_t` typedef + 3 function declarations
- `bindings/python/src/quiverdb/metadata.py` - `UiMetadata` frozen dataclass
- `bindings/python/src/quiverdb/database.py` - `_parse_ui_metadata` + `Database.get_attribute_ui_metadata`
- `bindings/python/src/quiverdb/__init__.py` - exports `UiMetadata`
- `bindings/python/src/quiverdb/_loader.py` - `quiver_ui_metadata_t` as the 5th `_STRUCT_SIZEOF_ACCESSORS` entry
- `tests/test_database_ui_metadata.cpp` - NEW: `DatabaseUiMetadata` suite, 14 tests
- `tests/CMakeLists.txt` - adds the new test file in alphabetical position
- `bindings/python/tests/test_database_ui_metadata.py` - NEW: 7 tests over `enum_basic`
- `bindings/python/tests/test_struct_sizes.py` - 5th size assertion, 5-element wiring-evidence list, renamed test

## Decisions Made

- **Task 1 checkpoint auto-approved "approve"** rather than pausing for a human — `.planning/config.json` has `mode: "yolo"` (autonomous execution, per `references/planning-config.md` and `universal-anti-patterns.md`'s explicit guidance to check the `yolo` flag rather than `auto_advance`/`_auto_chain_active`, which were both `false` in this project). The plan itself names "approve" as the option matching every locked 03-CONTEXT decision with no technical tradeoff against the alternatives, so this was a low-risk, plan-recommended default, not a judgment call.
- No new `.cpp` files: both C++ getters and the C API converter/getter/free joined the existing `database_metadata.cpp` / `c/database_metadata.cpp` files, so `src/CMakeLists.txt`'s two hand-maintained source lists needed zero edits (verified: `git diff --stat src/CMakeLists.txt` is empty throughout).
- The declared-but-empty-vocabulary corpus gap and the `list_ui_vocabularies` ordering proof were split into **two** separate `TEST` cases (sharing one `write_three_vocab_sidecar` helper against two independent `ScratchSidecarDir` instances) rather than one combined test, to meet the plan's "at least 14 tests" acceptance criterion cleanly rather than packing two logically distinct claims into one test.
- TOML syntax required `alpha = []` (a bare root-level key) to be written *before* any `[[table]]` header in the scratch `enum.toml` — otherwise it becomes a property of the preceding array-of-tables element rather than a new top-level vocabulary key. The three vocabularies still declare in non-alphabetical source order (`alpha, zebra, middle` — alphabetical would be `alpha, middle, zebra`), so the ordering assertion is not vacuous.

## Deviations from Plan

None — plan executed as written, including both critical corrections from `<critical_corrections>` (allocator: `quiver::string::new_c_str`/`delete[]` only; disengaged-optional guard on all three getters; no new `.cpp`; struct declared inside the existing `include/quiver/c/database.h`).

## Known Stubs

None. The single-record path (`get_attribute_ui_metadata`) is fully wired end to end for one binding (Python) per this plan's tracer scope; `list_ui_vocabularies`/`get_ui_vocabulary` are implemented and tested at the C++ layer but their C API surface (`quiver_database_list_ui_vocabularies`, `quiver_database_get_ui_vocabulary`, `quiver_database_free_ui_vocabulary`) is explicitly deferred to plan 03-02 per the plan's own `<artifacts_this_phase_produces>` table — not a gap in this plan's scope, a documented handoff.

## Issues Encountered

None beyond the TOML source-order syntax constraint noted above (resolved within Task 3, not a blocker).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- The three C++ getter signatures (`get_attribute_ui_metadata`, `list_ui_vocabularies`, `get_ui_vocabulary`) are frozen — plan 03-06 (Lua) can proceed in the same wave as the remaining FFI decoders per Correction 2, with no dependency on the C struct.
- `quiver_ui_metadata_t`'s 64-byte hole-free layout is frozen and compile-time pinned — plans 03-03 (Julia), 03-04 (Dart), 03-05 (JS) can decode it directly using the verified offsets, and plan 03-02 can add the vocabulary parallel-array C API surface without touching this struct.
- Python's `_STRUCT_SIZEOF_ACCESSORS`/`_CHECKED_STRUCTS` gate pattern (5 entries, wiring proven by an executed mutation) is the template plans 03-03/03-04/03-05 must match for parity in Julia/Dart/JS.
- No blockers identified.

---
*Phase: 03-structured-attribute-metadata*
*Completed: 2026-09-19*

## Self-Check: PASSED

All created files verified present on disk; both task commits (`1c1bcca`, `6c621c2`) verified present in git history.
