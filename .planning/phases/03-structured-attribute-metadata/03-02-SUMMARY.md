---
phase: 03-structured-attribute-metadata
plan: 02
subsystem: database
tags: [c-api, ffi, cffi, python, ui-metadata, vocabulary]

requires:
  - phase: 03-structured-attribute-metadata
    provides: "03-01: public UIMetadata/UIEnumEntry types, the three C++ getter signatures, the 64-byte quiver_ui_metadata_t struct + its get/free pair, Python's get_attribute_ui_metadata"
provides:
  - "quiver_database_list_ui_vocabularies / quiver_database_get_ui_vocabulary / quiver_database_free_ui_vocabulary (the vocabulary parallel-array pair + its dedicated combined free, D-11/D-37)"
  - "include/quiver/c/database.h frozen for the rest of the phase -- no later plan (03-03..03-06) edits it"
  - "Python UiEnumEntry + Database.list_ui_vocabularies/get_ui_vocabulary, closing the reference FFI decoder for this phase"
  - "tests/test_c_api_database_ui_metadata.cpp: the 64-byte layout, struct independence, and every free path proven at the C boundary"
affects: [03-03, 03-04, 03-05, 03-06, 03-07]

actuals:
  tokens: 6759
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Vocabulary parallel-array crossing: new int64_t[]/new char*[] + quiver::string::new_c_str, freed by one dedicated combined free rather than two composed generic frees (D-37) -- copied verbatim from quiver_database_read_time_series_files/free_time_series_files"
    - "The combined free is deliberately NOT second-call safe (it takes raw pointers, cannot null the caller's copies) -- only the struct-pointer free (free_ui_metadata) gets that guarantee, and only that one is tested for it"

key-files:
  created:
    - tests/test_c_api_database_ui_metadata.cpp
  modified:
    - include/quiver/c/database.h
    - src/c/database_metadata.cpp
    - tests/CMakeLists.txt
    - bindings/python/src/quiverdb/_c_api.py
    - bindings/python/src/quiverdb/metadata.py
    - bindings/python/src/quiverdb/database.py
    - bindings/python/src/quiverdb/__init__.py
    - bindings/python/tests/test_database_ui_metadata.py

key-decisions:
  - "quiver_database_free_ui_vocabulary follows quiver_database_free_string_array's NULL-tolerant shape (plain delete[], no QUIVER_REQUIRE), not quiver_database_free_time_series_files's shape (which QUIVER_REQUIREs both arrays non-NULL) -- the plan's critical correction explicitly named free_string_array as the precedent for NULL tolerance, and free_time_series_files would have rejected the zero-count case get_ui_vocabulary can legitimately return."
  - "All three new C API functions were placed as one contiguous block immediately after quiver_database_get_attribute_ui_metadata in both database.h and database_metadata.cpp, ahead of the pre-existing 'free metadata'/'list functions' sections -- keeps the whole UI-metadata surface (record + vocabulary pair) grouped together rather than splitting the vocabulary pair across the file's existing get/free/list sections."
  - "Renamed the C API test suite from the file's own DatabaseCApi* convention to the plan-mandated CApiDatabaseUiMetadata (fixup commit) -- the plan's acceptance criteria and <verification> section both hardcode that literal gtest_filter, and a suite named the 'expected' way would have made that filter silently match zero tests."

patterns-established:
  - "A single-array-of-strings vocabulary lister (list_ui_vocabularies) reuses the existing generic quiver_database_free_string_array rather than gaining its own free -- only the two-parallel-array getter (get_ui_vocabulary) needs a dedicated combined free, per D-37's scope (paired arrays only, not every new array-returning function)."

requirements-completed: [META-03, META-04, META-05]

coverage:
  - id: D1
    description: "quiver_database_list_ui_vocabularies / quiver_database_get_ui_vocabulary let a C API caller list every loaded vocabulary and fetch one vocabulary's ordered {code, label} entries by name"
    requirement: "META-05"
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_ui_metadata.cpp#CApiDatabaseUiMetadata.ListUiVocabulariesReturnsFixtureNames"
        status: pass
      - kind: unit
        ref: "tests/test_c_api_database_ui_metadata.cpp#CApiDatabaseUiMetadata.GetUiVocabularyReturnsOrderedCodesAndLabels"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py#test_list_ui_vocabularies_returns_fixture_names"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py#test_get_ui_vocabulary_returns_ordered_entries"
        status: pass
    human_judgment: false
  - id: D2
    description: "An unknown vocabulary name surfaces the exact Pattern 2 message through quiver_get_last_error at the C boundary and as a QuiverError in Python"
    requirement: "META-05"
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_ui_metadata.cpp#CApiDatabaseUiMetadata.GetUiVocabularyUnknownNameReturnsErrorWithExactMessage"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py#test_get_ui_vocabulary_unknown_name_raises_exact_message"
        status: pass
    human_judgment: false
  - id: D3
    description: "The vocabulary crosses as two parallel arrays freed by one dedicated combined free (quiver_database_free_ui_vocabulary), which is safe on a zero-count result and tolerates NULL for either array"
    requirement: "META-05"
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_ui_metadata.cpp#CApiDatabaseUiMetadata.FreeUiVocabularyToleratesZeroCountAndNullArrays"
        status: pass
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py#test_get_ui_vocabulary_declared_but_empty_returns_empty_list"
        status: pass
    human_judgment: false
  - id: D4
    description: "sizeof(quiver_ui_metadata_t) is 64 at runtime, quiver_scalar_metadata_t/quiver_group_metadata_t still report 56 and 32 -- the two pre-existing structs are provably untouched by this phase"
    requirement: "META-03"
    verification:
      - kind: unit
        ref: "tests/test_c_api_database_ui_metadata.cpp#CApiDatabaseUiMetadata.SizeofAccessorMatchesNativeSixtyFourByteLayout"
        status: pass
      - kind: unit
        ref: "tests/test_c_api_database_ui_metadata.cpp#CApiDatabaseUiMetadata.ScalarAndGroupMetadataSizesAreUnaffected"
        status: pass
      - kind: other
        ref: "git diff --stat src/CMakeLists.txt include/quiver/attribute_metadata.h (empty, since phase start)"
        status: pass
    human_judgment: false
  - id: D5
    description: "A Python caller reads both vocabulary surfaces through UiEnumEntry, closing the FFI reference decoder for this phase, freeing through the single dedicated call"
    requirement: "META-04"
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_metadata.py (4 new cases, 11 total in file)"
        status: pass
      - kind: other
        ref: "database.py's get_ui_vocabulary body contains exactly one free call (quiver_database_free_ui_vocabulary)"
        status: pass
    human_judgment: false

duration: 14min
completed: 2026-09-20
status: complete
---

# Phase 3 Plan 2: Vocabulary Pair Across the C API Summary

**`quiver_database_list_ui_vocabularies` / `get_ui_vocabulary` / `free_ui_vocabulary` close the C API's UI-metadata surface and freeze `include/quiver/c/database.h`; Python decodes both, closing the phase's reference FFI decoder.**

## Performance

- **Duration:** ~14 min (commit-to-commit)
- **Started:** 2026-09-20T00:00:29-03:00
- **Completed:** 2026-09-20T00:14:26-03:00
- **Tasks:** 3 (plus one fixup commit for a test-suite naming correction)
- **Files modified:** 9 (1 created, 8 modified)

## Accomplishments

- `quiver_database_list_ui_vocabularies` (a plain string array, freed by the existing
  `quiver_database_free_string_array`) and `quiver_database_get_ui_vocabulary` (the two-parallel-array
  shape D-11 fixed) join `include/quiver/c/database.h` immediately after the record getter/free pair
  03-01 added, and are implemented in `src/c/database_metadata.cpp` using the exact `new[]` +
  `quiver::string::new_c_str` allocator pair `quiver_database_read_time_series_files` uses — no new
  allocator idiom, no new struct.
- `quiver_database_free_ui_vocabulary(int64_t* codes, char** labels, size_t count)` is the dedicated
  combined free D-37 requires: one call releases both arrays, tolerates NULL for either array and a
  zero count, and is documented (not tested) as unsafe against a second call on the same pointers,
  since it takes raw pointers and cannot null the caller's copies the way the struct-pointer
  `free_ui_metadata` does.
- `include/quiver/c/database.h` is now frozen for the rest of the phase — `git diff --stat` since
  the phase's first commit shows `src/CMakeLists.txt` and `include/quiver/attribute_metadata.h`
  untouched, and the two pre-existing metadata structs (`quiver_scalar_metadata_t`,
  `quiver_group_metadata_t`) are provably unchanged.
- `tests/test_c_api_database_ui_metadata.cpp` (13 cases, suite `CApiDatabaseUiMetadata`) proves the
  64-byte hole-free layout and all nine offsets at the test level, the two untouched struct sizes,
  a full single-record round trip with the non-NULL-string-fields guarantee (D-13), the two
  offset-distinguishing records (`max_generation`'s `unit`, `internal_code`'s `hidden`), both D-36
  error polarities, the vocabulary round trip plus its unknown-name error, and every documented free
  path — built directly through `quiver_database_from_schema` per the 01-05 precedent, not through
  the C++ `test_ui_fixture.h` helper.
- Python's `UiEnumEntry` frozen dataclass plus `Database.list_ui_vocabularies`/`get_ui_vocabulary`
  close the reference FFI decoder for this phase's UI-metadata surface, following
  `read_time_series_files`'s exact marshalling shape and freeing through the single
  `quiver_database_free_ui_vocabulary` call (never two generic frees — the whole point of D-37).
  Four new test cases (11 total in the file) cover the ordered round trip, the unknown-name Pattern 2
  message, and a declared-but-empty vocabulary via a scratch `ui/` sidecar written under `tmp_path`.

## Task Commits

Each task was committed atomically:

1. **Task 1: The vocabulary pair crosses the C API as two parallel arrays with one dedicated free** — `275621a` (feat)
2. **Task 2: C API test file — 64-byte layout, struct independence, and every free path** — `8b26fa6` (test)
3. **Task 3: Python decodes both vocabulary surfaces, closing the reference decoder** — `bf519ff` (feat)
4. **Fixup: rename the C API test suite to match the plan-mandated `CApiDatabaseUiMetadata` filter** — `1eae162` (fix)

## Files Created/Modified

- `include/quiver/c/database.h` - `quiver_database_list_ui_vocabularies`/`get_ui_vocabulary`/`free_ui_vocabulary` declarations, immediately after the single-record get/free pair
- `src/c/database_metadata.cpp` - the three implementations, using the file's existing `QUIVER_REQUIRE` + try/catch shape (the combined free skips both, matching `free_string_array`)
- `tests/CMakeLists.txt` - adds `test_c_api_database_ui_metadata.cpp` in alphabetical position (between `_transaction.cpp` and `_update.cpp`)
- `tests/test_c_api_database_ui_metadata.cpp` - NEW: `CApiDatabaseUiMetadata` suite, 13 tests
- `bindings/python/src/quiverdb/_c_api.py` - hand-edited cdef: the three new function declarations
- `bindings/python/src/quiverdb/metadata.py` - `UiEnumEntry` frozen dataclass
- `bindings/python/src/quiverdb/database.py` - `Database.list_ui_vocabularies`/`get_ui_vocabulary`
- `bindings/python/src/quiverdb/__init__.py` - exports `UiEnumEntry`
- `bindings/python/tests/test_database_ui_metadata.py` - 4 new test cases (11 total)

## Decisions Made

- `quiver_database_free_ui_vocabulary` copies `quiver_database_free_string_array`'s NULL-tolerant
  shape (no `QUIVER_REQUIRE`, plain `delete[]`) rather than `quiver_database_free_time_series_files`'s
  shape (which `QUIVER_REQUIRE`s both arrays non-NULL) — the plan's critical correction named
  `free_string_array` as the precedent specifically because `get_ui_vocabulary` can legitimately
  return a zero-count result with both out-pointers NULL, which `free_time_series_files`'s `QUIVER_REQUIRE`
  would have rejected.
- All three new functions were grouped as one contiguous block in both `database.h` and
  `database_metadata.cpp`, immediately after `get_attribute_ui_metadata`, rather than distributed
  into the file's pre-existing "get functions" / "free functions" / "list functions" sections — keeps
  the whole UI-metadata surface (record + vocabulary pair) readable as one unit, matching the header's
  own "so the whole UI-metadata surface sits in one block" framing.
- Renamed the new C API test suite from an initial `DatabaseCApiUiMetadata` (matching this file's own
  `DatabaseCApiMetadata`/`DatabaseCApiOptions` convention) to the plan-mandated
  `CApiDatabaseUiMetadata` in a follow-up fixup commit — the plan's acceptance criteria and
  `<verification>` section both hardcode `--gtest_filter='CApiDatabaseUiMetadata.*'` literally, and
  the plan explicitly calls out "a report of 0 tests means the file is missing from the CMake list" as
  the failure mode that filter is meant to catch; matching the mandated name avoids that filter
  silently matching nothing.

## Deviations from Plan

None beyond the test-suite naming correction above (documented as a decision, not a deviation from
scope — no plan requirement was skipped or altered).

## Issues Encountered

None.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- `include/quiver/c/database.h` is frozen: `git diff --stat` from the phase's first commit
  (`6a869c0`) through this plan's last commit shows `src/CMakeLists.txt` and
  `include/quiver/attribute_metadata.h` empty, and the diff touches no field of
  `quiver_scalar_metadata_t` or `quiver_group_metadata_t`. Plans 03-03 (Julia), 03-04 (Dart), and
  03-05 (JS) can now hand-mirror the complete C struct + all five function signatures with no risk
  of the header moving under them.
- Plan 03-06 (Lua) binds against the 03-01 C++ getter signatures, not `quiver_ui_metadata_t`, so it
  remains unblocked by anything in this plan (Correction 2, carried from the plan).
- Python's decoder pattern (`ffi.new` out-pointers → `check(...)` → decode → single dedicated free in
  `finally`) is the template the three remaining FFI decoders (Julia, Dart, JS) should match for
  parity, mirroring how 03-01's single-record decoder set that template for `get_attribute_ui_metadata`.
- No blockers identified.

---
*Phase: 03-structured-attribute-metadata*
*Completed: 2026-09-20*

## Self-Check: PASSED

All created/modified files verified present on disk; all four commits (`275621a`, `8b26fa6`,
`bf519ff`, `1eae162`) verified present in git history.
