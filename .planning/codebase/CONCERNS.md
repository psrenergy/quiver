# Codebase Concerns

**Analysis Date:** 2026-10-02

Scope note: items listed under "Design Decisions" and "Do Not 'Fix'" in `AGENTS.md` are settled and are not concerns. Where one of them has a *documented* known limit, it appears below labelled **[accepted/open]**. No `TODO`/`FIXME`/`HACK`/`XXX` markers exist in `src/`, `include/`, or the binding sources, so the open debt lives in AGENTS.md notes and follow-up files.

## Tech Debt

**Array fan-out in `create_element` / `update_element` [accepted/open]:**
- Issue: arrays are routed by column name, so when two groups share a column name (legal for FK columns) the write goes to every matching table. A warning is logged, but the call is not rejected.
- Files: `src/database_impl.h` (`Impl::prepare_group_data`), `tests/schemas/valid/relations.sql`; behaviour pinned by `UpdateElementSharedColumnNameWritesEveryMatchingGroup`
- Impact: an `update_element` (delete_existing) silently rewrites groups the caller never named
- Fix approach: reject `matches.size() > 1` when `delete_existing` (AGENTS.md "Not yet fixed"). This is a breaking change, so it needs a `0.x` minor bump and a **BREAKING** CHANGELOG entry. Only `bindings/julia/test/test_helper_maps.jl` relies on the fan-out, and only through `create_element!`.

**Julia type-stability follow-up [accepted/open]:**
- Issue: `read_time_series_group` value columns are always `Vector{Optional{T}}`, even for `NOT NULL` columns.
- Files: `bindings/julia/type_stability_followup.md`, `bindings/julia/src/database_read.jl` (~line 531)
- Impact: downstream Julia code gets type-unstable columns
- Fix approach: use the per-column `get_time_series_metadata(...).value_columns[i].not_null` to pick concrete vs optional, as the scalar readers do. Then update the root AGENTS.md design note. `_by_id` and `query_*` stay optional by contract.

**`import_csv` nesting [accepted/open]:**
- Issue: the call refuses to run inside an open transaction. Whether it should nest is recorded as "an open decision for the maintainer".
- Files: `src/database_csv_import.cpp`, `src/AGENTS.md` (~line 355)
- Impact: callers cannot combine an import with other writes atomically
- Fix approach: needs a maintainer decision. A SAVEPOINT design was already rejected for the general writers.

**`lua_runner.cpp` size:**
- Issue: 2539 lines in one translation unit. It holds the sol2 bindings, the JSON encoder, the sandbox, the CSV reader/writer glue and the binary/expression bindings.
- Files: `src/lua_runner.cpp`
- Impact: it is the largest file in the core by about 4x (next is `src/database_csv_import.cpp` at 670), and it has the most cross-cutting policy (sandbox, writer registry).
- Fix approach: optional. A split along existing seams (`json_encode`, the sandbox, the binary bindings) into sibling `.cpp` files would not change behaviour. Only do it alongside real work in this file.

**CHANGELOG release ritual unsettled [accepted/open]:**
- Issue: `CHANGELOG.md` is edited by hand. The root AGENTS.md "Versioning" section says its release ritual "is not settled".
- Files: `CHANGELOG.md`, `scripts/assert_version.py`
- Impact: see Known Bugs: the version has already drifted.

## Known Bugs

**CHANGELOG lacks the current version section:**
- Symptoms: all five manifests are at `0.13.0` (`CMakeLists.txt`, `bindings/python/pyproject.toml`, `bindings/js/package.json`, `bindings/dart/pubspec.yaml`, `bindings/julia/Project.toml`), but the top section of `CHANGELOG.md` is `## [0.12.8] — 2026-10-01` and `0.13` appears nowhere in the file.
- Files: `CHANGELOG.md`
- Trigger: the version bump (commit `fccb5c7`, "bump version to 0.13.0") runs `assert_version.py bump`, which does not touch `CHANGELOG.md`.
- Workaround: add `## [0.13.0] — unreleased` plus its compare link by hand. To stop it recurring, have `assert_version.py` check or bump `CHANGELOG.md` too.

## Security Considerations

**Lua filesystem sandbox:**
- Risk: path escape out of the database directory through `db:open_file`, `db:read_csv*`, `db:write_csv`, `db:import_csv`/`export_csv`, `expr:save`, and the other db-scoped file operations
- Files: `src/lua_runner.cpp` (`resolve_sandboxed_path`)
- Current mitigation: `weakly_canonical` plus strict containment; `:memory:` databases reject all file operations; `dofile`/`loadfile` are removed; `os`/`io`/`package`/`debug` are not loaded
- Recommendations: `weakly_canonical` resolves symlinks only for path components that already exist, so check that tests cover a symlink or junction inside the database directory that points outside it, and a write to a not-yet-existing path under such a link. String `load` stays enabled by design, so the sandbox depends entirely on the library set staying limited. Any new sol2 binding that takes a path must route through `resolve_sandboxed_path`.

**Raw SQL via `query_*`:**
- Risk: a caller (or a Lua script) can run a bare `COMMIT` and end a dry run early, so its changes persist
- Files: `src/database.cpp` (`end_dry_run` guards on `sqlite3_get_autocommit`)
- Current mitigation: documented and accepted. Do not rely on dry runs as an isolation boundary for untrusted scripts.

## Performance Bottlenecks

**Per-column vector/set reads:**
- Problem: the composites `read_vectors_by_id` / `read_sets_by_id` (and Lua's per-column readers) issue one query per column.
- Files: `src/database_read.cpp` (or the equivalent read TU), binding composites in `bindings/*/src`
- Cause: column-at-a-time design
- Improvement path: use `read_vector_group_by_id` / `read_set_group_by_id` (one statement per group) where a binding needs a whole group. Lua deliberately lacks these.

**Lua JSON result encoding:**
- Problem: the whole result is built in memory, capped at 64 MiB
- Files: `src/lua_runner.cpp`
- Improvement path: none needed until large results show up. Scripts that produce big outputs should write a file with `db:write_csv`.

## Fragile Areas

**Writers' validate-before-first-write invariant [accepted/open]:**
- Files: `src/database_impl.h` (`TransactionGuard`, `prepare_group_data`), `src/database_create.cpp`, `src/database_update.cpp`, `src/database_time_series.cpp`
- Why fragile: there are no SAVEPOINTs. Inside a caller-owned transaction or a dry run, any check placed after the first INSERT/UPDATE/DELETE leaves a partial write behind. SQLite-only failures (UNIQUE, NOT NULL, CHECK, FK, triggers) already do this, as documented.
- Safe modification: when adding a writer, do all resolution and validation first. Add a test that runs the failing call inside `begin_transaction` and asserts that nothing changed.

**`import_csv` quote check and delete-first semantics:**
- Files: `src/database_csv_import.cpp` (`require_well_formed_quotes`, `sniff_csv_file`, `read_csv_file`), `src/csv/csv_read.cpp`
- Why fragile: the import deletes the rows the CSV omits, and a mis-parsed file would wipe data. Correctness depends on the pre-parse quote check matching csv-parser's own behaviour.
- Safe modification: never remove `require_well_formed_quotes`. Re-run `tests/test_database_csv_import.cpp` (2208 lines) after any csv-parser bump in `cmake/Dependencies.cmake`.

**DATE_TIME grammar replicated in three binding parsers:**
- Files: `src/utils/datetime.h` (`is_valid_iso8601`), Julia `string_to_date_time`, Dart `stringToDateTime`, Python `_parse_datetime`
- Why fragile: four implementations must accept exactly the same set of strings, and each host parser is wider than the core in a different direction.
- Safe modification: change all four together, and add the same accept/reject vectors to every binding's tests.

**Hand-maintained FFI surfaces:**
- Files: `bindings/js/src/loader.ts` (hand-written symbol table), `bindings/python/src/**/_c_api.py` (cdecls edited by hand; the generator only prints a diff aid), `bindings/dart/lib/src/ffi/bindings.dart` (generated), `bindings/julia/src/c_api.jl` (generated)
- Why fragile: when `include/quiver/c/database.h` (747 lines) changes, JS and Python can drift silently until a runtime call fails.
- Safe modification: after a C API change, run `scripts/generator.bat` and update `loader.ts` and `_c_api.py` by hand in the same change.

**Dependency pins with load-bearing flags:**
- Files: `cmake/Dependencies.cmake`, `cmake/Platform.cmake`
- Why fragile: removing the `CSV_NO_SIMD` pin brings back SIGILL on pre-AVX2 x86. Without the macOS 13.3 floor, dylibs get stamped with the runner's OS version. The `sqlite3_ENABLE_THREADSAFE` FORCE must stay.
- Safe modification: keep these when bumping versions, and keep the explanatory comments.

## Scaling Limits

**Lua result encoder:**
- Current capacity: 64 MiB output, 32 nesting levels
- Limit: scripts that return larger or deeper tables throw
- Scaling path: write the data out with `db:write_csv` instead of returning it.

## Dependencies at Risk

**Shared-library versioning split:**
- Risk: `QUIVER_UNVERSIONED_SHARED` is ON only in the Dart hook (`bindings/dart/hook/build.dart`). Julia hardcodes `libquiver.0.dylib`, and `scripts/ci/native_s3.sh` / `publish-s3.yml` ship versioned names.
- Impact: a SOVERSION change or a naming change breaks Julia or Dart loading. Only the Linux Dart Coverage CI job exercises the ON configuration, so no macOS or Windows job covers it.
- Migration plan: add a macOS Dart hook build job if Dart on macOS matters.

## Missing Critical Features

Not detected. The gaps that do exist (binary/expression subsystems outside Julia and Lua, Lua whole-group readers, Lua boolean readers) are documented exceptions.

## Test Coverage Gaps

**clang-tidy excludes the binary subsystem:**
- What's not tested: static analysis of `src/binary/*`
- Files: `scripts/tidy.bat` (filter `src[\\/](?!binary)`)
- Risk: lint debt goes unseen in the binary hot path
- Priority: Low

**ON configuration of unversioned shared libraries:**
- What's not tested: `QUIVER_UNVERSIONED_SHARED=ON` on macOS and Windows
- Files: `CMakeLists.txt` (line 28), `.github/workflows/*`
- Risk: the Dart native-assets hook registers nothing but reports success
- Priority: Medium

**Cross-binding DATE_TIME parser parity:**
- What's not tested: one shared accept/reject vector set across the three binding parsers. Each binding keeps its own cases.
- Files: `bindings/julia/test/`, `bindings/dart/test/`, `bindings/python/tests/`
- Risk: the parsers drift and a value the core stores becomes unreadable in one binding
- Priority: Medium. A shared fixture under `tests/` (as `tests/schemas/` is for schemas) would pin it.

---

*Concerns audit: 2026-10-02*
