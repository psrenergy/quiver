# Tests (`tests/`)

C++ core and C API suites live here; binding suites live in each binding's `test/` (or Python's
`tests/`) directory. Run commands are in the root `CLAUDE.md`.

## C++ core tests (`tests/test_*.cpp`, one file per functional area)

- Database: `test_database_lifecycle.cpp` (open/close/move/options), `test_database_create.cpp`,
  `test_database_read.cpp`, `test_database_update.cpp`, `test_database_delete.cpp`,
  `test_database_query.cpp`, `test_database_time_series.cpp`, `test_database_transaction.cpp`,
  `test_database_csv_export.cpp`, `test_database_csv_import.cpp`, `test_database_errors.cpp`
- Supporting types: `test_element.cpp`, `test_row_result.cpp`, `test_migrations.cpp`,
  `test_schema_validator.cpp`
- Lua: `test_lua_runner_*.cpp` — per-area split mirroring the database files (`_create`, `_read`,
  `_update`, `_delete`, `_query`, `_time_series`, `_transaction`, `_errors`, `_csv_export`,
  `_csv_import`, `_all_types`, `_fk`). The shared `LuaRunnerTest` and `LuaSandboxTest` fixtures,
  the `expect_lua_error` helper (throw + message-substring assert — plain `EXPECT_THROW` passes
  vacuously when a removed function raises "attempt to call a nil value"), and the common include
  prelude live in `test_lua_runner.h`; the single-use `LuaRunnerAllTypesTest` / `LuaRunnerFkTest`
  fixtures stay local to their files. Lua file operations are sandboxed to the database directory
  (root design decision), so every file-touching Lua test uses `LuaSandboxTest`: a file-backed db
  in a dedicated per-test temp dir, with scripts passing relative paths. The Lua binary/expression
  subsystem bindings (and the sandbox itself) are covered by `test_lua_binary.cpp` and
  `test_lua_expression.cpp`.
- Binary subsystem: `test_binary_file.cpp`, `test_binary_metadata.cpp`,
  `test_binary_time_properties.cpp`, `test_csv_converter.cpp`, `test_iteration.cpp`
- Expression subsystem: `test_expression.cpp`
- `test_issues.cpp` - issue-numbered regression tests

## C API tests

Mirror the same areas with the `test_c_api_*` prefix (`test_c_api_database_*.cpp` per database
area — the file sets diverge slightly: the C API adds `test_c_api_database_metadata.cpp` and has
no errors file) plus `test_c_api_element.cpp`, `test_c_api_lua_runner.cpp`,
`test_c_api_expression.cpp`, and the binary trio `test_c_api_binary_file.cpp` /
`test_c_api_binary_metadata.cpp` / `test_c_api_csv_converter.cpp`.

## Binding suites

`bindings/julia/test/`, `bindings/dart/test/`, `bindings/python/tests/`, and
`bindings/js/test/` mirror the same areas in each language's idiom. Julia additionally covers
the binary/expression subsystems (`test_binary_file.jl`, `test_binary_metadata.jl`,
`test_csv_converter.jl`, `test_expression.jl`) and the relation-map helpers
(`test_helper_maps.jl`).

## Schemas (`tests/schemas/`)

The single shared schema set — **every** suite (C++, C, all bindings) references these files;
never copy them into a binding.

- `valid/` — `all_types.sql`, `basic.sql`, `collections.sql`, `composite_helpers.sql`,
  `csv_export.sql`, `describe_multi_group.sql`, `mixed_time_series.sql`,
  `multi_dim_time_series.sql`, `multi_time_series.sql`, `nullable_time_series.sql`,
  `relations.sql`
- `invalid/` — schemas the validator must reject: `duplicate_attribute_time_series.sql`,
  `duplicate_attribute_vector.sql`, `fk_actions.sql`, `fk_not_null_set_null.sql`,
  `label_not_null.sql`, `label_not_unique.sql`, `label_wrong_type.sql`, `no_configuration.sql`,
  `set_no_unique.sql`, `vector_no_index.sql`
- `migrations/` — versioned `1/`, `2/`, `3/`, each with `up.sql`/`down.sql`
- `issues/` — regression migrations for specific issues (`issue52/`, `issue70/`)

## Other targets in `tests/CMakeLists.txt`

- `quiver_benchmark` — standalone transaction-performance comparison (individual vs batched).
  Built by `build-all.bat` but never executed automatically; run manually.
- `quiver_sandbox` — intentional scratch target for ad-hoc experiments. Do not delete or "clean
  up"; links only against the C++ core.

## `scripts/test-all.bat` steps

1. C++ tests (`quiver_tests.exe`)
2. C API tests (`quiver_c_tests.exe`)
3. Julia tests
4. Dart tests
5. JavaScript tests
6. Python tests
7. CLI smoke test — positive run (`--schema` + example Lua script → exit 0) and negative run
   (`--schema` and `--migrations` together → exit 2)

`scripts/build-all.bat` is also seven steps, but its step 1 is the build itself followed by the
six suites — it does not run the CLI smoke test.
