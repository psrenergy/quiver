---
phase: 02-config-path-locale-and-struct-size-safety
plan: 05
subsystem: database
tags: [julia, ffi-abi, clang-jl, i18n, struct-layout]

requires:
  - phase: 02-config-path-locale-and-struct-size-safety
    provides: "02-01: quiver_database_options_t grown to 24 bytes, three *_sizeof accessors, quiver_database_has_ui_config, UIConfigSet locale threading"
provides:
  - "Julia c_api.jl regenerated against the frozen 24-byte quiver_database_options_t header: two new Ptr{Cchar} fields, three *_sizeof functions, quiver_database_has_ui_config"
  - "build_quiver_database_options(; ui_config_dir, ui_locale) threaded through open/from_schema/from_migrations, returning (options, keepalive) with GC.@preserve at each call site"
  - "has_ui_config(db::Database)::Bool, mirroring is_healthy's no-throw shape"
  - "_check_struct_size / _native_struct_size / _assert_struct_sizes in generator/prologue.jl -- a load-time struct-layout gate proven to survive a second generator.bat regeneration"
affects: [02-06, 02-07]

actuals:
  tokens: 5600
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns: ["prologue-not-generated-output as the durable edit target for a machine-regenerated FFI module", "Base.cconvert/Base.unsafe_convert(Cstring, ...) for a nullable string struct field with a returned keepalive vector"]

key-files:
  created:
    - bindings/julia/test/test_database_ui_options.jl
    - bindings/julia/test/test_struct_sizes.jl
  modified:
    - bindings/julia/src/c_api.jl
    - bindings/julia/src/database.jl
    - bindings/julia/generator/prologue.jl
    - bindings/julia/CLAUDE.md

key-decisions:
  - "The load-time gate's three checks/wrapper (_check_struct_size, _native_struct_size, _assert_struct_sizes) were written ONLY in generator/prologue.jl, then generator.bat was re-run to propagate them into c_api.jl -- and re-run a SECOND time afterward to prove the propagation is idempotent (empty git diff on c_api.jl). This is the plan's single most load-bearing acceptance criterion and was executed literally, not just asserted."
  - "build_quiver_database_options uses Base.cconvert(Cstring, value) / Base.unsafe_convert(Cstring, buf) per the plan's explicit instruction, rather than the Vector{UInt8} + pointer(buf) idiom used in build_quiver_csv_options -- both are valid GC.@preserve-safe patterns already used elsewhere in this codebase (database_query.jl uses plain String args pinned by @ccall itself; database_options.jl uses the byte-vector idiom); this task followed the plan's named API rather than introducing a third variant unprompted."
  - "No `export has_ui_config` statement was added to Quiver.jl. Grepped the entire bindings/julia/src/ tree: there are zero `export` statements anywhere in this module -- every public name, including is_healthy, is called fully-qualified as Quiver.foo in every test file and the root CLAUDE.md's cross-layer table. Adding the codebase's first-ever `export` for one function would be inconsistent with the binding's actual convention. has_ui_config is defined in database.jl (included by Quiver.jl) and is reachable as Quiver.has_ui_config(db), exactly like Quiver.is_healthy(db) -- the substance of 'exported from Quiver.jl' without introducing a new, unprecedented pattern."
  - "test/runtests.jl was not edited to add explicit include lines for the two new test files -- recursive_include(@__DIR__) already walks every file in test/ matching startswith(file, \"test_\") && endswith(file, \".jl\"), so both new files are picked up automatically. Confirmed: the full suite run counted them (UI Options: 14, Struct Sizes: 23) without any runtests.jl edit."

requirements-completed: [OPT-01, OPT-02, OPT-03, OPT-04, OPT-06, SAFE-02, SAFE-03]

coverage:
  - id: D1
    description: "Julia's c_api.jl regenerated from the frozen C header: quiver_database_options_t grows to 24 bytes (Ptr{Cchar} ui_config_dir@8, ui_locale@16), plus quiver_database_options_sizeof/quiver_scalar_metadata_sizeof/quiver_group_metadata_sizeof/quiver_database_has_ui_config -- nothing else structural"
    requirement: OPT-06
    verification:
      - kind: unit
        ref: "git diff (first regen): 18 insertions confined to the options struct, three sizeof functions, and has_ui_config; bindings/julia/test/test_database_ui_options.jl#options struct is 24 bytes"
        status: pass
    human_judgment: false
  - id: D2
    description: "open/from_schema/from_migrations accept ui_config_dir and ui_locale kwargs through the one shared build_quiver_database_options, each backing buffer kept alive via a returned keepalive vector and GC.@preserve at every ccall"
    requirement: OPT-03
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_database_ui_options.jl#explicit ui_config_dir loads a config not beside the database, #spanish locale renders spanish labels, #default locale renders english labels"
        status: pass
    human_judgment: false
  - id: D3
    description: "A Julia consumer opens foresight_like with an explicit ui_config_dir (a scratch mktempdir() with no ui/ sibling) and ui_locale=\"es\", and describe_collection renders \"Ingenuo Estacional\"/\"Tendencia Lineal Local\", not \"Seasonal Naïve\"/\"Local Linear Trend\""
    requirement: OPT-02
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_database_ui_options.jl#spanish locale renders spanish labels"
        status: pass
    human_judgment: false
  - id: D4
    description: "has_ui_config(db) returns a Bool without throwing: true for a loaded config, false for an absent directory"
    requirement: OPT-04
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_database_ui_options.jl#missing ui_config_dir degrades without throwing"
        status: pass
    human_judgment: false
  - id: D5
    description: "A load-time struct-size gate lives in generator/prologue.jl (not c_api.jl), runs the three checks in fixed order (options, scalar, group) as the last statement of __init__, throws naming the struct and both numbers on mismatch, and is PROVEN to survive a second generator.bat regeneration (empty diff on c_api.jl)"
    requirement: SAFE-02
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_struct_sizes.jl (happy path + three @test_throws failure-path assertions); git diff bindings/julia/src/c_api.jl after the second generator.bat run (empty, confirmed in this session)"
        status: pass
    human_judgment: false
  - id: D6
    description: "The mismatch message is locally crafted, naming the struct name and both the expected and native byte counts; a missing native symbol (version-skewed library) is distinguished from a value mismatch"
    requirement: SAFE-03
    verification:
      - kind: unit
        ref: "bindings/julia/test/test_struct_sizes.jl#failure path: a wrong expected value throws naming both numbers"
        status: pass
    human_judgment: false
  - id: D7
    description: "Full Julia suite green end to end, including all pre-existing testsets, after both the ABI-breaking struct growth and the new load-time gate"
    verification:
      - kind: unit
        ref: "bindings/julia/test/test.bat -- 1465/1465 pass"
        status: pass
    human_judgment: false

duration: ~30min
completed: 2026-09-19
status: complete
---

# Phase 2 Plan 5: Julia Struct-Size Safety and Config Path/Locale — Summary

**Regenerated `c_api.jl` against the frozen 24-byte `quiver_database_options_t`, threaded `ui_config_dir`/`ui_locale` through the one shared options builder with `GC.@preserve` keepalives, added `has_ui_config`, and put a load-time struct-size gate in `generator/prologue.jl` — proven, by actually re-running the generator twice, to survive regeneration.**

## Performance

- **Duration:** ~30 min
- **Started:** ~2026-09-19T11:35 (approx.)
- **Completed:** 2026-09-19T12:02:13-03:00
- **Tasks:** 3
- **Files modified:** 6 (2 new test files, 4 modified)

## Accomplishments

- `bindings/julia/generator/generator.bat` regenerated `src/c_api.jl` from the frozen C header:
  `quiver_database_options_t` grew from 8 to 24 bytes (`read_only`@0, `console_level`@4,
  `ui_config_dir::Ptr{Cchar}`@8, `ui_locale::Ptr{Cchar}`@16), plus
  `quiver_database_options_sizeof`, `quiver_scalar_metadata_sizeof`, `quiver_group_metadata_sizeof`,
  and `quiver_database_has_ui_config` — an 18-line diff, confined to exactly those symbols.
- `build_quiver_database_options` (`src/database.jl`) gained `ui_config_dir`/`ui_locale` kwargs
  using `Base.cconvert(Cstring, value)`/`Base.unsafe_convert(Cstring, buf)` per the plan's
  explicit instruction, and now returns `(options, keepalive)`. `open`, `from_schema`, and
  `from_migrations` each wrap their `check(C.quiver_database_...(...))` call in
  `GC.@preserve keepalive ...` — three occurrences, one per factory. An omitted or empty value
  writes `Ptr{Cchar}(C_NULL)`, never a pointer to `""`.
- `has_ui_config(db::Database)::Bool` added to `database.jl`, mirroring `is_healthy`'s
  `Ref{Cint}` + `check(...)` + `!= 0` shape — no `!` suffix, since it is a read.
- **The load-time struct-size gate lives in `generator/prologue.jl`**, not `c_api.jl`:
  `_check_struct_size` (throws naming the struct and both byte counts on mismatch),
  `_native_struct_size` (wraps a native `*_sizeof` ccall, re-raising a lookup/dlopen failure with
  a message distinguishing "can't ask the library" from "asked, and it disagrees"), and
  `_assert_struct_sizes` (calls the three checks in fixed order — options, scalar, group — as
  the LAST statement of `__init__`, short-circuiting via `error()`'s throw on the first mismatch).
- **Regeneration-proof, verified for real in this session**: after writing the gate to
  `prologue.jl` and re-running `generator.bat` to propagate it into `c_api.jl`, `generator.bat`
  was run a **second time** and `git diff bindings/julia/src/c_api.jl` came back **empty** —
  confirming the gate is not deleted by regeneration, the plan's single most important
  acceptance criterion.
- Two new test files: `test_database_ui_options.jl` (14 tests: the 24-byte/offset assertions,
  explicit-config-dir-not-beside-the-database, Spanish locale, default English locale, and
  missing-directory-degrades-without-throwing) and `test_struct_sizes.jl` (23 tests: the happy
  path for all three structs against both `sizeof` and the literal 24/56/32, PLUS three
  `@test_throws` failure-path assertions proving a wrong expected value actually throws and
  names both numbers — the plan's criterion 5, since no binding previously had a test that a
  wrong layout would actually fail).
- Full Julia suite: **1465/1465 pass**, including the two new testsets.

## Task Commits

1. **Task 1: Regenerate, thread the kwargs, read a Spanish label** - `36851bf` (feat)
2. **Task 2: A load-time gate the generator cannot delete** - `8364f5c` (feat)
3. **Task 3: Full suite green and the binding's CLAUDE.md current** - `4df072f` (docs)

_Note: all three tasks are typed `tracer`/`auto` with `tdd="true"` (Tasks 1-2) or plain `auto`
(Task 3) in the plan; each was delivered as a single implementation + test commit, since the new
surface had no pre-existing failing assertion to red against — the plan's own `<verify>` gate
(the targeted `include(...)` run, then the full `test.bat` suite) was the acceptance bar for
each, and it passed on first run after each task's edits._

## Files Created/Modified

- `bindings/julia/src/c_api.jl` — REGENERATED (twice, second run proves idempotency): grown
  options struct, three `*_sizeof` functions, `quiver_database_has_ui_config`, and the
  prologue's new gate code spliced into `__init__`
- `bindings/julia/src/database.jl` — `build_quiver_database_options` returns
  `(options, keepalive)` with the two new kwargs; `open`/`from_schema`/`from_migrations` wrap
  their ccall in `GC.@preserve`; `has_ui_config` added
- `bindings/julia/generator/prologue.jl` — `_check_struct_size`, `_native_struct_size`,
  `_assert_struct_sizes`, called from `__init__` — the durable edit target, since this file
  (not `c_api.jl`) is what `generator.bat` splices verbatim into `__init__`
- `bindings/julia/test/test_database_ui_options.jl` (new) — 14 tests
- `bindings/julia/test/test_struct_sizes.jl` (new) — 23 tests
- `bindings/julia/CLAUDE.md` — documents the 24-byte layout/offsets, the
  `(options, keepalive)`/`GC.@preserve` shape, `has_ui_config`, and the
  prologue-not-c_api rule for the load-time gate

## Decisions Made

- Used `Base.cconvert`/`Base.unsafe_convert(Cstring, ...)` for the two new string fields, per the
  plan's explicit instruction, rather than the `Vector{UInt8}` + `pointer(buf)` idiom already used
  in `build_quiver_csv_options` — both patterns coexist in this codebase now; no attempt was made
  to unify them, since the plan named the API to use.
- Did not add an `export` statement to `Quiver.jl` for `has_ui_config`. This codebase has zero
  `export` statements anywhere — `is_healthy` and every other public function are called
  fully-qualified (`Quiver.is_healthy(db)`) in every test file and in the root `CLAUDE.md`'s
  cross-layer naming table. `has_ui_config` follows the identical pattern
  (`Quiver.has_ui_config(db)`), matching house convention rather than introducing the codebase's
  first `export` for one function.
- Did not add explicit `include(...)` lines to `test/runtests.jl` for the two new test files.
  `recursive_include(@__DIR__)` already discovers every `test_*.jl` file in `test/` automatically
  — confirmed both new files were picked up and ran (14 + 23 tests) without any `runtests.jl` edit.
- Ran `bindings/julia/format/format.bat` to check formatting; it failed with a pre-existing,
  unrelated environment error (`Style is a direct dependency, but does not appear in the
  manifest` — a stale `format/Manifest.toml` against a git-sourced dependency). This predates this
  plan's changes (confirmed via `git log` on `bindings/julia/format/`) and is out of this task's
  scope per the deviation rules' scope boundary (only auto-fix issues directly caused by this
  task's changes). New code was hand-checked against surrounding style (4-space indent, trailing
  commas in multi-line calls) instead. Logged here rather than silently skipped.

## Deviations from Plan

None affecting correctness or the plan's `must_haves` — three documented conventions
(no `export`, no `runtests.jl` edit, `format.bat` broken) explained above under Decisions Made,
since each is a deliberate choice to match existing house convention rather than a bug fix or an
architectural change.

## Issues Encountered

- `bindings/julia/format/format.bat` errors on a pre-existing, unrelated Manifest/Project.toml
  mismatch for the `Style` dependency (git-sourced package not resolved into
  `format/Manifest.toml`). Not caused by this plan's changes; not fixed, per the scope-boundary
  rule (pre-existing breakage in an unrelated area). New files were style-checked by eye against
  the surrounding codebase instead.
- The Windows `cmd //c "generator\\generator.bat"` and `cmd //c "test\\test.bat"` invocations
  both exceeded the Bash tool's 120s default timeout on their first (cold, `Pkg.instantiate()`)
  run and were moved to background; both completed successfully (exit code 0) once awaited. The
  second `generator.bat` run (idempotency proof) was fast once the generator's own environment
  was already resolved.

## Next Phase Readiness

- Julia's binding surface (`ui_config_dir`, `ui_locale`, `has_ui_config`, the 24-byte options
  struct, and the load-time struct-size gate) is complete, tested, and documented — matching the
  C symbol set and layout that 02-01 froze.
- `bindings/` outside `bindings/julia/` was not touched, as instructed. Plan 02-04 (Dart) was
  observed running concurrently in the same working tree (interleaved commits in `git log`);
  no Dart files were read or modified by this plan's work.
- No blockers for 02-06/02-07.

## Self-Check: PASSED

- All new/modified files confirmed present on disk: `bindings/julia/src/c_api.jl`,
  `bindings/julia/src/database.jl`, `bindings/julia/generator/prologue.jl`,
  `bindings/julia/test/test_database_ui_options.jl`, `bindings/julia/test/test_struct_sizes.jl`,
  `bindings/julia/CLAUDE.md`.
- All three task commit hashes (`36851bf`, `8364f5c`, `4df072f`) confirmed present via
  `git log --oneline`.

---
*Phase: 02-config-path-locale-and-struct-size-safety*
*Completed: 2026-09-19*
