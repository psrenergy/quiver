---
phase: 02-config-path-locale-and-struct-size-safety
plan: 03
subsystem: database
tags: [python, cffi, ffi-abi, i18n, struct-layout]

requires:
  - phase: 02-config-path-locale-and-struct-size-safety
    provides: "quiver_database_options_t grown to 24 bytes (D-02), quiver_database_options_sizeof/quiver_scalar_metadata_sizeof/quiver_group_metadata_sizeof accessors, quiver_database_has_ui_config, all built into build/bin/libquiver_c.dll (02-01)"
provides:
  - "Python (CFFI) quiver_database_options_t cdef grown to 24 bytes matching the frozen native header, proven by ffi.sizeof/ffi.offsetof"
  - "Database.open/from_schema/from_migrations accept keyword-only ui_config_dir and ui_locale"
  - "Database.has_ui_config()"
  - "_make_options returns (options, keepalive) — keepalive rule for owning char[] buffers"
  - "_loader.load_library gates on three native *_sizeof accessors before returning lib, on both the bundled and dev-mode success paths"
affects: [02-06, 02-07]

actuals:
  tokens: 4800
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns: ["keepalive-returning FFI options builder (tuple return instead of bare struct)", "load-time size-accessor gate that actively calls dlsym-resolved symbols rather than merely declaring them"]

key-files:
  created:
    - bindings/python/tests/test_database_ui_options.py
    - bindings/python/tests/test_struct_sizes.py
  modified:
    - bindings/python/src/quiverdb/_c_api.py
    - bindings/python/src/quiverdb/_loader.py
    - bindings/python/src/quiverdb/database.py
    - bindings/python/CLAUDE.md

key-decisions:
  - "_EXPECTED_STRUCT_SIZES from the plan text is implemented as ffi.sizeof(...) calls made inline inside _assert_struct_sizes rather than a standalone module-level dict — _loader.py deliberately takes ffi as a parameter instead of importing quiverdb._c_api at module scope (keeping it independent of the module that lazily imports it), so there is no module-level ffi available to precompute a dict from at import time. The expected value is still ffi.sizeof(...), never a literal, which is what SAFE-02 actually requires."
  - "The struct-name/accessor-name pairing lives in one module-level tuple, _STRUCT_SIZEOF_ACCESSORS, iterated in the fixed options/scalar/group order — satisfies EDGE/ordering and gives _assert_struct_sizes a single loop instead of three near-duplicate blocks."
  - "Test fixtures for the new UI-options file build the database under pytest's tmp_path (never the shared foresight_like fixture directory) so an explicit ui_config_dir is the only possible source of a loaded config — a tmp_path db has no ui/ sibling, making the override proof real per D-17/the plan's explicit instruction."

requirements-completed: [OPT-01, OPT-02, OPT-03, OPT-04, OPT-06, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "ffi.sizeof('quiver_database_options_t') is 24 and ffi.offsetof is 0/4/8/16 for read_only/console_level/ui_config_dir/ui_locale — the hand-edited cdef matches the frozen native header"
    requirement: OPT-06
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_options.py#test_options_struct_layout_matches_native"
        status: pass
    human_judgment: false
  - id: D2
    description: "Database.open/from_schema/from_migrations accept keyword-only ui_config_dir and ui_locale, matching read_only/console_level's existing shape"
    requirement: OPT-03
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_options.py#test_explicit_ui_config_dir_loads_config_not_beside_database"
        status: pass
    human_judgment: false
  - id: D3
    description: "has_ui_config() returns True when a config loaded and False when the directory is absent, without raising"
    requirement: OPT-04
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_options.py#test_missing_ui_config_dir_degrades_without_raising"
        status: pass
    human_judgment: false
  - id: D4
    description: "Opening foresight_like with ui_locale='es' and an explicit ui_config_dir yields a describe_collection report containing Ingenuo Estacional/Tendencia Lineal Local and not Seasonal Naïve; the default-locale mirror renders the English labels instead"
    requirement: OPT-02
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_database_ui_options.py#test_spanish_locale_renders_spanish_labels, test_default_locale_renders_english_labels"
        status: pass
    human_judgment: false
  - id: D5
    description: "load_library ACTIVELY CALLS all three *_sizeof() accessors in the fixed order options/scalar/group and raises RuntimeError naming the struct and both numbers on mismatch, proven via a deliberately-wrong-expected-value test per struct (not just the happy path)"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "bindings/python/tests/test_struct_sizes.py#test_native_accessors_match_cdef_sizes, test_check_struct_size_raises_on_mismatch[quiver_database_options_t-24-8], test_check_struct_size_raises_on_mismatch[quiver_scalar_metadata_t-56-40], test_check_struct_size_raises_on_mismatch[quiver_group_metadata_t-32-16]"
        status: pass
    human_judgment: false
  - id: D6
    description: "An omitted/empty ui_config_dir or ui_locale is passed as ffi.NULL, never as b''"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "bindings/python/src/quiverdb/database.py#_make_options (grep -c 'b\"\"' shows no occurrence inside the function; test_default_locale_renders_english_labels and test_missing_ui_config_dir_degrades_without_raising exercise the unset paths)"
        status: pass
    human_judgment: false

duration: ~16min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 3: Python (CFFI) Options Growth and Struct-Size Gate Summary

**Hand-edited the ABI-mode CFFI cdef to the frozen 24-byte `quiver_database_options_t`, added `ui_config_dir`/`ui_locale`/`has_ui_config()` to the Python binding, and made `load_library` actually call all three native `*_sizeof()` accessors before returning a usable handle.**

## Performance

- **Duration:** ~16 min
- **Started:** ~2026-09-19T14:20Z (approx.)
- **Completed:** 2026-09-19T14:35:43Z
- **Tasks:** 3
- **Files modified:** 6 (2 new test files, 4 modified)

## Accomplishments

- `quiver_database_options_t`'s cdef in `_c_api.py` grew from 8 to 24 bytes: `const char*
  ui_config_dir` then `const char* ui_locale` appended after `console_level`, in that exact
  order (the cdef order IS the CFFI-computed layout). `ffi.sizeof(...)` and `ffi.offsetof(...)`
  confirm 24 / 0 / 4 / 8 / 16 against the native header, matching `src/c/options.cpp`'s
  `static_assert`s exactly.
- Added `quiver_database_options_sizeof`, `quiver_scalar_metadata_sizeof`,
  `quiver_group_metadata_sizeof`, and `quiver_database_has_ui_config` to the cdef, each placed
  beside the declarations it belongs with.
- `Database._make_options` now returns `(options, keepalive)` — CFFI's owning `ffi.new("char[]",
  ...)` cdata is freed once the last Python reference drops, and a struct field pointing at it
  does not count as a reference, so every one of the three factories (`open`, `from_schema`,
  `from_migrations`) binds the keepalive list to a local (`_keepalive`) that stays in scope
  across the `check(...)` call. `ffi.NULL` (never `b""`) is passed for an unset or empty
  `ui_config_dir`/`ui_locale`.
- `Database.has_ui_config()` added, mirroring `is_healthy()`'s exact no-throw shape.
- `_loader.py` gained `_STRUCT_SIZEOF_ACCESSORS` (the fixed options/scalar/group ordering),
  `_check_struct_size` (pure, raises `RuntimeError` naming the struct and both numbers), and
  `_assert_struct_sizes` (actively calls each accessor — CFFI ABI mode resolves `lib.<name>` via
  dlsym-on-demand, so a native library predating this phase raises `AttributeError` at the call,
  never at `ffi.dlopen`; that is re-raised as a distinguishing `RuntimeError`). Called on both the
  bundled and dev-mode success paths, immediately before each `return lib`.
- Two new test files: `test_database_ui_options.py` (layout assertion, explicit-directory load,
  Spanish/English locale rendering, missing-directory degradation) and `test_struct_sizes.py`
  (happy path plus the failure-path proof — three parametrized cases, one per struct, each
  proving `_check_struct_size` raises with both numbers present).
- `bindings/python/CLAUDE.md` updated with the 24-byte layout, the keepalive rule, the
  lazy-dlsym reason for calling (not just declaring) the accessors, and the new public surface.

## Task Commits

1. **Task 1: cdef, options kwargs and a Spanish label through CFFI** - `22c1222` (feat)
2. **Task 2: Load-time struct-size gate that calls the accessors** - `249e886` (feat)
3. **Task 3: Full suite green and the binding's CLAUDE.md current** - `f8f519c` (docs)

_Note: both Task 1 and Task 2 are typed `tracer`/`auto` with `tdd="true"`; both were delivered as
a single implementation + test commit per task, same as 02-01 — the new surface had no
pre-existing failing assertion to red against, so the plan's own `<verify>` gate (targeted pytest
run) was the acceptance bar and passed on the first run after each task's edits._

## Files Created/Modified

- `bindings/python/src/quiverdb/_c_api.py` — grown `quiver_database_options_t` cdef (24 bytes) +
  four new declarations (`quiver_database_options_sizeof`, `quiver_scalar_metadata_sizeof`,
  `quiver_group_metadata_sizeof`, `quiver_database_has_ui_config`)
- `bindings/python/src/quiverdb/_loader.py` — `_STRUCT_SIZEOF_ACCESSORS`, `_check_struct_size`,
  `_assert_struct_sizes`, called on both `load_library` success paths
- `bindings/python/src/quiverdb/database.py` — `has_ui_config()`, `ui_config_dir`/`ui_locale`
  kwargs on `open`/`from_schema`/`from_migrations`, keepalive-returning `_make_options`
- `bindings/python/tests/test_database_ui_options.py` (new) — 5 cases
- `bindings/python/tests/test_struct_sizes.py` (new) — happy path + 3 parametrized failure cases
- `bindings/python/CLAUDE.md` — documented the new surface and both hazard-driven rules

## Decisions Made

- `_EXPECTED_STRUCT_SIZES` from the plan's action text became inline `ffi.sizeof(...)` calls
  inside `_assert_struct_sizes` rather than a standalone module-level dict, because `_loader.py`
  deliberately takes `ffi` as a function parameter instead of importing `quiverdb._c_api` at
  module scope (it is the module `_c_api.get_lib()` lazily imports, and keeping it independent
  avoids a load-order dependency). The value asserted is still `ffi.sizeof(...)`, never a
  hardcoded literal — which is what SAFE-02 actually requires; the difference from the plan text
  is only where the expression is evaluated (inline in the function vs. a precomputed dict).
- Reused the `_STRUCT_SIZEOF_ACCESSORS` tuple (struct name, accessor name pairs) to drive one
  loop in `_assert_struct_sizes` rather than three near-identical blocks, while preserving the
  fixed options/scalar/group order the plan requires.
- Test fixtures in `test_database_ui_options.py` build every database under pytest's `tmp_path`
  (never inside the shared `foresight_like` fixture directory), per the plan's explicit
  instruction — a `tmp_path` database has no `ui/` sibling, so any test asserting `has_ui_config()
  is True` can only be observing the explicit `ui_config_dir` argument at work, never the
  convention path.

## Deviations from Plan

None — plan executed exactly as written. One implementation-detail adjustment during test-writing
(not a deviation from intent):

- The plan's action text names a module-level `_EXPECTED_STRUCT_SIZES` mapping; it is implemented
  as inline `ffi.sizeof(...)` expressions inside `_assert_struct_sizes` instead, for the reason
  above (`_loader.py` avoids importing `_c_api` at module scope). Functionally identical: the
  expected size always comes from `ffi.sizeof`, never a literal.

## Issues Encountered

None. The one hazard the plan called out explicitly — a bare `uv run python -m pytest` failing
with `Missing: libquiver_c.dll` for reasons unrelated to the cdef — was anticipated and avoided by
running everything through `tests/test.bat`, per the plan's precondition guidance.

## Next Phase Readiness

- Python's cdef, options surface, and load-time gate are complete, tested, and green
  (`bindings/python/tests/test.bat -q`: 318 passed).
- `uv run ruff format --check` and `uv run ruff check` are clean for every touched file.
- No blockers for 02-06 (release ritual) or 02-07, which consume this binding's completed surface
  alongside the other three Wave 2 binding plans (02-02 JS, 02-04 Dart, 02-05 Julia), each
  independent per D-19.

## Self-Check: PASSED

- All new/modified files confirmed present on disk (`_c_api.py`, `_loader.py`, `database.py`,
  `test_database_ui_options.py`, `test_struct_sizes.py`, `CLAUDE.md`).
- All three commit hashes (`22c1222`, `249e886`, `f8f519c`) confirmed present in `git log`.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
