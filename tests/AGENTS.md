# Tests (`tests/`)

C++ core and C API suites live here; binding suites live in each binding's `test/` (or Python's
`tests/`) directory. Run commands are in the root `AGENTS.md`.

## C++ core tests (`tests/test_*.cpp`, one file per functional area)

- Database: `test_database_lifecycle.cpp` (open/close/move/options), `test_database_create.cpp`,
  `test_database_read_{scalar,vector,set}.cpp` (read split by attribute type; element-level reads
  `read_element_ids`/`read_element_by_id`/`number_of_elements` live in the `_scalar` file),
  `test_database_update.cpp`, `test_database_delete.cpp`, `test_database_describe.cpp`,
  `test_database_metadata.cpp` (group-metadata FK flags, `list_{vector,set}_groups`),
  `test_database_query.cpp`, `test_database_time_series_{metadata,group,row,files}.cpp` (time
  series split by sub-concern: `group` = group read/update + validation, `row` =
  `upsert_time_series_row`/`read_time_series_row`; the C++ core has no `_nulls` file),
  `test_database_transaction.cpp`, `test_database_csv_export.cpp`, `test_database_csv_import.cpp`,
  `test_database_errors.cpp`
- `test_database_ui_metadata.cpp` covers the `ui/` TOML sidecar reader (`src/ui_metadata.{h,cpp}`)
  and its render into `describe`/`describe_collection`/`summarize_collection` — the one suite in
  this list that drives `from_migrations` describe output (every other describe assertion in the
  repo goes through `from_schema`, which never populates the sidecar). Two gtest fixture names
  exist so the loader-facing and render-facing halves can be filtered separately:
  `UiMetadataTest` (path resolution, shape selection, localized-value reading including the C0/C1
  control-byte collapse, and the `enum.toml` join) and `DatabaseUiMetadataTest`
  (label/tooltip/enum clause rendering, the redundancy-suppression rules, the undescribed cases,
  the malformed/degrade cases, the `summarize_collection` histogram annotation, and the SAFE-01
  no-`ui/` baseline). Its `UiTempTreeFixture` base builds a
  per-test temp-dir `migrations/` tree plus sibling `ui/` tree from caller-supplied file contents
  (extending the `MigrationsTestFixture` idiom in `test_migrations.cpp`) — **nothing may be
  committed under `tests/schemas/ui/`**, because such a directory would become a live sibling of
  `tests/schemas/migrations` for every `from_migrations` call across six suites plus the
  recursive-copy Lua migrations test.
- Supporting types: `test_element.cpp`, `test_row_result.cpp`, `test_migrations.cpp`,
  `test_schema_validator.cpp`
- Lua: `test_lua_runner_*.cpp` — per-area split mirroring the database files (`_create`, `_read`,
  `_update`, `_delete`, `_query`, `_describe`, `_return`, `_time_series`, `_transaction`,
  `_errors`, `_csv_export`, `_csv_import`, `_all_types`, `_fk`, `_lifecycle`, `_migrations`). `_return` covers the JSON
  encoding of a script's return value; `_errors` also pins text-only `load`, that a caught or
  propagated script error writes nothing to stderr, and that a dot-call (`db.commit()`) throws
  instead of crashing; `_transaction` covers `db:dry_run` (the core-level dry run
  lives in `test_database_transaction.cpp`); `_migrations` covers `db:validate_migrations` (sandboxed
  like the other file-touching Lua operations); `_lifecycle` covers moving a runner (move-construct and
  move-assign): handles a script opens after the move still close at that `run()`'s exit, both while the
  moved-from runner is alive and after it has been destroyed, and a file-scope `static_assert` that
  `LuaRunner` is pointer-sized keeps run state inside its `Impl` in Release too, where the freed-source
  pins alone do not reliably fail. The shared `LuaRunnerTest` and `LuaSandboxTest` fixtures,
  the `expect_lua_error` helper (throw + message-substring assert — plain `EXPECT_THROW` passes
  vacuously when a removed function raises "attempt to call a nil value"), and the common include
  prelude live in `test_lua_runner.h`; the single-use `LuaRunnerAllTypesTest` / `LuaRunnerFkTest`
  fixtures stay local to their files. Lua file operations are sandboxed to the database directory
  (root design decision), so every file-touching Lua test uses `LuaSandboxTest`: a file-backed db
  in a dedicated per-test temp dir, with scripts passing relative paths. The Lua binary/expression
  subsystem bindings (and the sandbox itself) are covered by `test_lua_binary.cpp` and
  `test_lua_expression.cpp`.
- `test_sandboxed_path.cpp` (`SandboxedPathTest`) unit-tests `resolve_sandboxed_path`, the gate
  every file-touching Lua operation shares, without Lua: containment, `..` and absolute escapes, a
  symlink pointing outside, the root itself, `:memory:`, and (`_WIN32` only) the device-name prefix.
  It is the only test that includes a `src/` header and compiles a `src/` TU: the function is hidden
  in the shared library, so `tests/CMakeLists.txt` adds `src/lua_runner/path_policy.cpp` to
  `quiver_tests` along with the `src/` include dir. Keep `path_policy.cpp` a one-function file, or a
  static (`QUIVER_BUILD_SHARED=OFF`) link defines a symbol twice. The suite name stays outside the
  `Lua*` filter so the Lua-layer count is unaffected. Expectations build the root from
  `weakly_canonical(sandbox)`, which is what the gate prints (macOS `/private/var`, Windows 8.3
  names), and the symlink case skips where a directory symlink cannot be created.
- Binary subsystem: `test_binary_file.cpp`, `test_binary_metadata.cpp`,
  `test_binary_time_properties.cpp`, `test_csv_converter.cpp`, `test_iteration.cpp`
- Expression subsystem: `test_expression.cpp`
- `test_issues.cpp` - issue-numbered regression tests
- `test_migrations.cpp` also covers the in-memory `validate_migrations` up-then-down round trip;
  `test_c_api_database_lifecycle.cpp` covers its C API success and error propagation;
  `test_lua_runner_migrations.cpp` covers the sandboxed `db:validate_migrations` Lua binding.
- `test_lua_runner_read_csv.cpp` covers the Lua-only `db:read_csv`/`db:read_csv_stream` bindings
  (parsing, the `separator`/`header_row` options, and the sandbox/error-catalogue negatives) —
  there is no C++ core, C API, or other-binding counterpart to mirror (root design decision), so
  this suite has no sibling elsewhere. Most of its CSV fixtures are still written at runtime into
  the `LuaSandboxTest` sandbox, since they exist only to be read back once. **`tests/fixtures/`**
  is the one exception: `ma_energia_residencial.csv` and `ma_gd_data.csv` are two real Maranhão
  utility files committed byte-exact (Phase 2, TEST-02), copied into the sandbox by the tests that
  read them rather than generated inline. They are committed rather than hand-written because
  their exact bytes are themselves what two of the parser requirements assert — a leading UTF-8
  BOM and CRLF line endings on the Energia file, neither on the GD file — and a fixture built by a
  test-writer's editor cannot be trusted to reproduce that. `.gitattributes` marks both `-text` so
  git's line-ending normalization never touches them (D-24); like `tests/schemas/`, the directory
  needs no CMake registration since both tests locate it from the compiled-in source path.
  Two of its negatives need an OS-level lever rather than a fixture, and the two platforms
  disagree about which one works. `UnreadableFileReportsParserFailure` needs a file that passes
  exists/not-a-directory/non-empty but still cannot be opened, so the csv-parser wrapper is the
  message under test: Windows takes an exclusive lock (`CreateFileW` with `dwShareMode` 0, since a
  DENY ACE there blocks the open but *not* the metadata queries, and `chmod` is a no-op for read
  access), POSIX uses `chmod 000` (which blocks the open while `stat` still succeeds) and skips
  under root. It asserts the three preconditions still pass before reading, so it cannot silently
  degrade into re-testing an earlier catalogue message. `DeviceNamePathIsReportedWithPrefix` (here
  and in `test_lua_binary.cpp`) is `_WIN32`-only because no POSIX path is reserved the way `NUL`
  is; the `test_lua_binary.cpp` copy spans `open_file`/`bin_to_csv`/`csv_to_bin` on purpose, so the
  fix stays in the shared `resolve_sandboxed_path` gate instead of regressing to a per-caller patch.
- `test_lua_runner_write_csv.cpp` covers the Lua-only `db:write_csv`/`w:write_row`/`w:close`
  binding (cell-type dispatch, the `separator`/`header` options, the max-integer-key row walk, and
  the WRITE-08 truncate-at-open behaviour) — same no-other-layer-counterpart situation as
  `test_lua_runner_read_csv.cpp` above. Every correctness assertion in it round-trips the written
  file back through `db:read_csv` rather than reading the raw bytes, for the same reason
  `export_csv`'s export-only string-search tests were a trap this project hit twice already.

## C API tests

Mirror the same areas with the `test_c_api_*` prefix (`test_c_api_database_*.cpp` per database
area — the file sets diverge slightly: the C API has no `describe`, `errors` or `ui_metadata` file
(its describe/describe_collection/summarize_collection coverage lives in
`test_c_api_database_metadata.cpp`)) plus `test_c_api_element.cpp`, `test_c_api_lua_runner.cpp`,
`test_c_api_expression.cpp`, and the binary trio `test_c_api_binary_file.cpp` /
`test_c_api_binary_metadata.cpp` / `test_c_api_csv_converter.cpp`. The same `read` →
`{scalar,vector,set}` and `time_series` → `{metadata,group,row,files,nulls}` split applies; the
C API time-series set additionally has `test_c_api_database_time_series_nulls.cpp` (per-cell
NULL-mask round-trips), which the C++ core lacks.

Free C API strings with `quiver_database_free_string`, never `delete[]`. Inside an open-for-write
binary span use `EXPECT`, not `ASSERT`, so the writer is always closed.

## Binding suites

`bindings/julia/test/`, `bindings/dart/test/`, `bindings/python/tests/`, and
`bindings/js/test/` mirror the same areas in each language's idiom. Julia additionally covers
the binary/expression subsystems (`test_binary_file.jl`, `test_binary_metadata.jl`,
`test_csv_converter.jl`, `test_expression.jl`) and the relation-map helpers
(`test_helper_maps.jl`).

The boolean convenience readers have one file per binding — `test_database_boolean.jl`,
`database_boolean_test.dart`, `test_database_boolean.py`, `database-boolean.test.ts` — all over
`valid/all_types.sql` (`some_integer` scalar, `count_value` vector, `code` set). Their NULL-cell
cases are the exception: every group column of `all_types.sql` is `NOT NULL`, so the vector ones sit
next to the integer NULL-cell tests in the vector `read` files over `valid/collections.sql`'s
`value_int` (all four bindings), and the set ones over `valid/relations.sql`'s
`Child_set_scores.score` (Python and Dart only). There is
no C++/C counterpart for the **readers**: those wrappers are binding-only, and Lua is deliberately
excluded (root design decisions).

Boolean **input** is a different matter: a native boolean is INTEGER 1/0 on every write path, and
that is tested in the Lua layer and in all four bindings. There is no C++-core or C API test,
because there is no such write path to test — `Element::set` has `int64_t`/`double` overloads and
no `bool` one, and the C API has no boolean setter (root design decision); each binding converts
before the FFI call. The Lua side lives in `test_lua_runner_create.cpp` (scalar, array, mixed
integer/boolean array, mixed float/boolean array, cell-type mismatch, update, group writer, row
upsert) with the query parameter in `test_lua_runner_errors.cpp`, over `valid/collections.sql`;
the four bindings extend their own boolean files (Python's `test_boolean_input` also opens
`valid/mixed_time_series.sql` for the row upsert, since `AllTypes` has no time-series group). Two
things to keep in mind when touching these:

- **The unsupported-type tests use a function (`print`), not a boolean** — in
  `test_lua_runner_create.cpp`, `test_lua_runner_errors.cpp` and `test_c_api_lua_runner.cpp`. They
  asserted a boolean rejection before booleans were accepted; a function is the value that still
  has no SQL counterpart. `test_lua_runner_update.cpp` keeps its boolean rejection for
  `db:update_relation`, where only `nil` may clear a relation.
- **The three mixed-array tests guard the unchecked getter.**
  `CreateElementMixedIntegerAndBooleanArray`, `CreateElementMixedFloatAndBooleanArray` and
  `CreateElementArrayCellTypeMismatchThrows` cover bugs that only manifest with
  `SOL_SAFE_GETTER` off (silent 0 / 0.0 / `""` instead of a throw). `src/CMakeLists.txt` now sets it
  off in every build, so Debug sees them too; every other sol2 safety is on in both builds, and a
  Release run is still the check that no build-specific behaviour crept in. Build Release with tests via the
  preset when touching `lua_table_to_vector`:
  `cmake --preset release && cmake --build --preset release`, then run
  `build/release/bin/quiver_tests.exe --gtest_filter='Lua*'`. Phase 2's `header_row` decoder
  (a new `sol::object` type check) was verified this way (TEST-05): 291/291 `LuaRunner*` tests
  passed in both Debug and Release, with no divergence.

The native-DateTime bindings (Julia, Dart, and Python) cover bulk scalar, vector, and set
convenience readers in the corresponding `read` test files, NULL cells included: the set wrappers
over `collections.sql`'s nullable `tag` (the wrappers parse any TEXT column), the vector ones in
Dart and Python over `multi_column_groups.sql`'s nullable `Items_vector_events.date_event`, which
is also the only `date_`-typed (DATE_TIME) column in any vector or set group. Per-cell NULLs are
covered through every vector/set reader, bulk and by id, in Python and Dart (integer, float, string
and boolean) and in the C API (integer, float and string; it has no boolean readers). JS covers
vector integers and booleans (bulk and by id), vector floats (bulk and by id), set strings (bulk and
by id) and bulk set floats, which between them reach every decode helper in `read.ts`; Julia covers
integer and boolean vectors, string sets, the DateTime set wrapper, the whole-group readers and
`set_relation_map`'s skip of a NULL cell. The pair only the LEFT JOIN's presence
column can tell apart — an element with no rows next to an element whose only row is NULL — is
pinned in the C++ core (`Read{Vector,Set}DistinguishesNoRowsFromNullOnlyRow`) and through the C ABI
(`ReadVectorIntegersPreservesNullCells`: a size-1 entry with a 0 mask, not an empty one), which is
the shape every FFI binding decodes.

The `read` → `{scalar,vector,set}` and `time_series` → `{metadata,group,row,files,nulls}` split is
mirrored in every binding using each idiom's file naming (Julia `test_database_read_scalar.jl`,
Dart `database_read_scalar_test.dart`, JS `database-read-scalar.test.ts`, Python
`test_database_read_scalar.py`). JS and Python have no `metadata` time-series file; the C++ core
has no `nulls` file. JS Database-operation test files carry a `database-` prefix
(`database-create.test.ts`, `database-lifecycle.test.ts`, …) to match the other bindings; the
non-Database files (`composites.test.ts`, `introspection.test.ts`, `lua-runner.test.ts`,
`lua-api-sync.test.ts`, `package-entry.test.ts`) keep their bare names.

`bindings/js/test/lua-api-sync.test.ts` is the only JS test file that needs neither a database nor
the native library: it parses every `.cpp`/`.h` under `src/lua_runner/` (sorted, with the open
usertype reset at each file boundary) and asserts `bindings/js/src/lua-api.ts` documents
every bound `db:`/`quiver.*` name and the exact `open_libraries` list. It also fails if any of the
`BinaryFile`, `BinaryMetadata`, `Expression` or `CsvWriter` usertypes parses to zero methods, or if
`open_libraries(` does not appear exactly once, so a missed file or usertype cannot pass vacuously.
It imports the constant from
`../src/lua-api.ts` directly rather than `../src/index.ts` specifically to avoid the FFI loader, so
it still passes on a checkout with no `build/`.

## Schemas (`tests/schemas/`)

The single shared schema set — **every** suite (C++, C, all bindings) references these files;
never copy them into a binding.

- `valid/` — `all_types.sql`, `basic.sql`, `collections.sql`, `composite_helpers.sql`,
  `csv_export.sql`, `csv_group_vector_index.sql`, `csv_import_cascade_cycle.sql`,
  `csv_import_self_cascade.sql`, `describe_multi_group.sql`, `mixed_time_series.sql`,
  `multi_column_groups.sql`, `multi_dim_time_series.sql`, `multi_time_series.sql`,
  `non_strict_vector.sql`, `nullable_time_series.sql`, `relations.sql`,
  `shared_group_columns.sql`, `time_series_date_columns.sql`
  - `csv_group_vector_index.sql` gives a set group (`Codes_set_tags`) a TEXT `vector_index` column
    and a time-series group (`Items_time_series_slots`) an INTEGER one — two collections, since one
    may not declare an attribute in two groups. Only a vector group's `vector_index` is structural,
    and `import_csv` once forced every column of that name to INTEGER and dereferenced it
    unvalidated.
  - `csv_import_self_cascade.sql` is the one schema with an `ON DELETE CASCADE` **self**-reference
    (`Node.node_parent`), plus a vector group. `import_csv` clears self-references before deleting
    the elements a CSV omits; without that, deleting an omitted parent cascades into a kept child
    and its vector rows (`ImportCSV_Scalar_OmittedElement_DoesNotCascadeThroughSelfReference`).
  - `csv_import_cascade_cycle.sql` is the one with a CASCADE cycle between two collections
    (`Item.tag_pinned` → `Tag`, `Tag.item_owner` → `Item`), plus a vector group. There the cascade
    reaches a kept element through another collection, so `import_csv` refuses and rolls back
    (`ImportCSV_Scalar_OmittedElement_CascadeIntoKeptElement_Throws`).
  - `multi_column_groups.sql` is the vector/set counterpart of the multi-column time-series
    schemas: `Items_vector_readings` (`amount`, `score`) and `Items_set_codes` (`code`, `weight`),
    both nullable, with the value columns deliberately named so the alphabetically-first one is
    not the only one — that ordering is what exposed the group-insert row-count bug — plus
    `Items_vector_events` (`date_event` DATE_TIME, `note` TEXT, both nullable) for the whole-group
    readers' DATE_TIME parsing and NULL string cells. Note every set value column must be part of
    the UNIQUE constraint.
  - `non_strict_vector.sql` is the only `valid/` schema with tables that are deliberately **not**
    STRICT: the group table `Items_vector_counts` and the collection `Items` (`code INTEGER`).
    Without STRICT, INTEGER affinity keeps a non-integral value written through raw SQL (`1.5`) as
    REAL and a non-numeric one as TEXT, so a cell's storage class can differ from its declared
    type. `DatabaseCApi.ReadVectorGroupByIdMasksRealCellInIntegerColumn` uses it to pin that the C
    API group marshaller reports such a cell absent instead of truncating it, and
    `DatabaseDescribe.SummarizeDistributionSkipsNonIntegerCells` that `summarize_collection`'s
    value distribution counts only integer cells. Keep the other `valid/` schemas STRICT.
  - `shared_group_columns.sql` has groups whose column names collide, which the validator allows
    for FK columns: `Child_vector_links` / `Child_vector_routes` and `Child_set_mentors` /
    `Child_set_sponsors` share `parent_ref`, and `Child_vector_cost` is named after `routes`'
    `cost` column without holding it, as `Child_set_tier` is after `sponsors`' `tier`. A per-column
    reader resolves a column name, so only the whole-group readers are sure to read a named group's
    own table (`ReadGroupByIdReadsItsOwnTableWhenGroupsShareAColumn`,
    `ReadGroupColumnSkipsGroupNamedAfterAColumnItLacks`, and the binding tests on this schema).
  - `time_series_date_columns.sql` pins which column is a time series' dimension.
    `Plant_time_series_events` has a nullable `date_approved` value column that sorts before its
    key column `date_time`, so a lookup that scans columns by name instead of the primary key
    picks the wrong one. `Meter_time_series_blocks` keeps its date column outside the key
    (`PRIMARY KEY (id, block)`), so it has no dimension and its metadata and reads throw — use
    `Meter` only to test that refusal.
- `invalid/` — schemas the validator must reject: `duplicate_attribute_time_series.sql`,
  `duplicate_attribute_vector.sql`, `fk_actions.sql`, `fk_not_null_set_null.sql`,
  `label_not_null.sql`, `label_not_unique.sql`, `label_wrong_type.sql`, `no_configuration.sql`,
  `set_no_parent_fk.sql`, `set_no_unique.sql`, `set_unknown_parent.sql`,
  `time_series_fk_actions.sql`, `time_series_relation_fk_actions.sql`, `unsupported_type.sql`,
  `vector_no_index.sql`.
  Each file must break exactly one rule and be otherwise valid SQL, and a new test should assert
  the message rather than a bare throw: `duplicate_attribute_time_series.sql` once "passed" on a
  trailing-comma syntax error while pointing its FKs at a table that did not exist.
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

`scripts/build-all.bat` configures and builds, then calls `test-all.bat`.
