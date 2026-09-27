# Quiver improvement plans

One self-contained implementation plan per review item, written to be executed in a fresh context with only the repo and the plan file. Repo: `C:\Development\Quiver\quiver3` (branch `rs/quality`, reviewed at HEAD `58dfe7a`).

## How to use

- Execute plans **in numeric order, one at a time**, except where the order notes below say otherwise. Each plan lists its **Depends on** and **Overlaps with** plans in its header, and anchors every edit by function name plus a quoted excerpt, because earlier plans shift line numbers.
- Every plan ends with Verification and Acceptance criteria. Run the full `scripts/test-all.bat` at the end of each batch.
- 0.11.0 is unreleased and already a minor bump, so every BREAKING change goes under `## [0.11.0] — unreleased` in `CHANGELOG.md` with no manifest bump.
- **Quality note:** plans 32–88 were written and checked against the code interactively. Plans 01–31 were written by per-item agents whose adversarial checkers never ran (weekly usage limit). Plan 01 has since been corrected for wrongly named `AGENTS.md` files, and 03 was spot-checked. Give 01–31 a quick read against the code before executing.

## Order notes (exceptions and tight couplings)

- **56 before 55.** 56 rewrites the `TypeValidator` bodies, then 55 turns them into free functions and moves the headers.
- **46 before 47.** 47 changes the `caller` strings that 46's `lua_to_value` receives.
- **31 before 33** (33 extends `numericCells`), and **24 before 38** (same Dart file).
- **01 before 58 before 60.** All three touch `import_csv`: 01 keeps foreign keys ON, 58 adds one `convert_cell` pass and one write tail, 60 adds `TransactionGuard`.
- **43 before 44.** Both edit `bindings/js/src/lua-api.ts`.
- **53 early in Batch 6.** It removes the `Database& db` back-references that later core plans no longer pass.
- **65 before 82.** `build-all` starts calling `test-all`, whose smoke test 65 fixes.
- **61 (unused includes) after 01/13/58**, which change the same files' includes.

## Maintainer decisions baked into the plans

- Mixed int/float numeric column: **widen to FLOAT**, in Python and Dart as well (24).
- Expression broadcast label rule: **relaxed**, the ternary rule applied to binary ops too (15).
- `example/`: **move the smoke script to `tests/cli/smoke.lua`**; do not restore it (65).
- JS whole-group readers return **rows**, for homogeneity (18). `read_time_series_row` gains a **presence mask** (17).
- **No SAVEPOINTs** (rejected in the v0.3 research, Pitfall 4). Element writes validate before the first write instead (05).
- Items marked optional in the review are **not planned** (see the end of this file).

## Batch 1 — Core correctness (C++)

| # | Plan | Severity | Breaking | Depends on |
|---|---|---|---|---|
| 01 | [import_csv: keep foreign keys ON so dropped elements cascade](01-import-csv-fk-integrity.md) | h | yes | — |
| 02 | [One definition of the time-series dimension column (PK-based)](02-time-series-dimension-column.md) | m | yes | — |
| 03 | [update_time_series_group: build INSERT from the union of row keys](03-update-time-series-group-union-columns.md) | m | no | — |
| 04 | [read_time_series_row: reject multi-dimension groups and filter NULL in the join](04-read-time-series-row-multi-dim.md) | medium | yes | 02 |
| 05 | [create_element/update_element: validate every array before the first write](05-element-write-validate-before-write.md) | m | no | — |
| 06 | [SchemaValidator: enforce parent FK for set and time-series tables](06-schema-validator-group-parent.md) | medium | yes | — |
| 07 | [list_{vector,set,time_series}_groups throw on an unknown collection](07-list-groups-unknown-collection.md) | low | yes | — |

## Batch 2 — Binary subsystem correctness

| # | Plan | Severity | Breaking | Depends on |
|---|---|---|---|---|
| 08 | [Binary: parent-aware time-coordinate validation and time-of-day-preserving offsets](08-binary-time-coordinates-parent-aware.md) | high | yes | — |
| 09 | [Binary: recompute initial_value in one place and rebase aggregate output start date](09-binary-initial-value-after-aggregate.md) | high (silent data corruption in saved expression output) | yes | 08 |
| 10 | [Binary: one dimension-start helper that walks the whole parent chain](10-binary-dimension-start-ancestors.md) | medium | no | 08, 09 |
| 11 | [BinaryMetadata factory: length checks, missing-key errors, no TOML round trip in from_element](11-binary-metadata-factory-length-checks.md) | medium | no | 08, 09 |
| 12 | [Delete the unused C API binary-metadata builder family and C++ add_dimension/add_time_dimension](12-binary-delete-metadata-builders.md) | m | yes | 08, 11 |
| 13 | [csv_to_bin/bin_to_csv: strict whole-cell float parse and shortest round-trip output](13-csv-to-bin-lossless-round-trip.md) | high | yes | — |
| 14 | [CSVConverter: field-count check on read and one set of split/header/dimension helpers](14-csv-converter-short-row.md) | medium | no | 13 |
| 15 | [Expressions: one N-ary broadcast-metadata builder with the relaxed label rule](15-expression-broadcast-unify.md) | medium | no | 08, 14 |
| 16 | [Expressions: one AggregationOperation enum across C++, C API, Lua and Julia](16-expression-single-aggregation-enum.md) | low | yes | — |

## Batch 3 — C API contract and cross-binding leaks

| # | Plan | Severity | Breaking | Depends on |
|---|---|---|---|---|
| 17 | [read_time_series_row C API: presence mask so every binding returns null for no data](17-time-series-row-null-mask.md) | high | yes | 04 |
| 18 | [Whole-group readers: Julia/Python call the native C API; JS gains them](18-whole-group-readers-native.md) | high (silent wrong data, or a crash, on any nullable multi-column group) | no | — |
| 19 | [C API test for quiver_database_read_set_group_by_id](19-c-api-read-set-group-test.md) | medium | no | — |
| 20 | [All bindings: an empty update_time_series_files map still reaches the core](20-update-time-series-files-empty-map.md) | low | no | — |
| 21 | [Delete quiver_clear_last_error and the element counter functions (C API + C++)](21-delete-dead-c-api-symbols.md) | l | yes | 12 |
| 22 | [Query C API: one function per C++ query method (drop the _params split)](22-query-single-c-entry-point.md) | m | yes | — |
| 23 | [C API group marshaller: stop narrowing REAL cells into INTEGER columns](23-group-marshaller-no-real-truncation.md) | low | no | 22 |

## Batch 4 — Binding fixes

| # | Plan | Severity | Breaking | Depends on |
|---|---|---|---|---|
| 24 | [Python (+Dart): type numeric columns from all cells, never coerce with int()/float()](24-python-numeric-column-typing.md) | high (silent data loss) | yes | — |
| 25 | [Python: accept datetime on every write path, converting aware values to UTC](25-python-datetime-writes.md) | medium | no | 17, 24 |
| 26 | [Python LuaRunner: a failed construction must look closed to __del__](26-python-luarunner-failed-init.md) | low | no | — |
| 27 | [Python: positional-only "/" on every **kwargs method](27-python-positional-only-kwargs.md) | low | yes | — |
| 28 | [Python: delete redundant bool branches (bool is an int subclass)](28-python-drop-bool-branches.md) | low | no | 24 |
| 29 | [Python: delete Makefile, dotenv dev-dep, unused fixtures, Element.clear/_ensure_valid](29-python-dead-code.md) | low | no | — |
| 30 | [Python: fix stale docstrings (upsert types, _c_api.py header, marshaller wording) and a duplicate test](30-python-stale-docstrings.md) | low | no | 18, 21, 24, 25, 27, 28, 29 |
| 31 | [JS: per-cell numeric check and boolean normalization in both array marshallers](31-js-numeric-cells.md) | medium | yes | — |
| 32 | [JS: mod.ts re-exports src/index.ts; export DATA_TYPE_* constants](32-js-package-exports.md) | medium | no | — |
| 33 | [JS: accept bigint in group writers and query parameters](33-js-bigint-writers.md) | low | no | 31 |
| 34 | [Julia Element: setindex! for nothing and AbstractString scalars](34-julia-element-null-abstractstring.md) | medium | no | — |
| 35 | [Julia helper_maps: two bulk reads and a Dict instead of N queries](35-julia-helper-maps-bulk.md) | medium | no | — |
| 36 | [Julia read_time_series_group: free the C buffers in finally](36-julia-time-series-decode-finally.md) | low | no | — |
| 37 | [Dart and Julia: test the dry-run wrapper exception path](37-dry-run-exception-tests.md) | low | no | — |
| 38 | [Dart: one _marshalGroupColumns helper for the six group writers](38-dart-marshal-group-columns.md) | medium | no | 24 |
| 39 | [Dart Element.set: delete the Map case that silently drops its key](39-dart-element-map-case.md) | medium | yes | — |
| 40 | [Dart group decoders: free the C result in finally](40-dart-group-decoders-finally.md) | low | no | — |
| 41 | [All bindings: fix stale "not positionally aligned" reader comments](41-bindings-stale-alignment-comments.md) | low | no | — |
| 42 | [Julia/Python: rename read_{vector,set}_date_time_by_id to _date_times_by_id](42-date-times-by-id-rename.md) | low | yes | 18 |

## Batch 5 — Lua runner and agent-facing reference

| # | Plan | Severity | Breaking | Depends on |
|---|---|---|---|---|
| 43 | [Lua reference: fix the rollback, import_csv-replaces-table and update_time_series_files claims](43-lua-api-reference-data-loss-claims.md) | high (the text leads an agent into silent data loss) | no | 01 |
| 44 | [Lua reference: boolean rule, quoted errors, query conversion, NULL shapes, nil params; stable key-type error](44-lua-api-reference-accuracy.md) | medium | no | 43 |
| 45 | [Lua: close BinaryFile handles (readers and writers) when run() returns](45-lua-binaryfile-close-at-run-exit.md) | medium | yes | — |
| 46 | [Lua: one lua_to_value converter replaces four dispatch chains](46-lua-to-value-helper.md) | medium | no | — |
| 47 | [Lua: converter errors name the public method the script called](47-lua-error-public-method-names.md) | medium | no | 46 |
| 48 | [Lua: strict option decoders for export/import_csv, quiver.metadata and rename_agents](48-lua-strict-decoders.md) | medium | yes | — |
| 49 | [Lua: one group_metadata_lua builder; rename the data_type_to_string shadow; drop no-op nil branches](49-lua-group-metadata-helper.md) | low | no | — |
| 50 | [Lua: read_vectors_by_id/read_sets_by_id use to_lua_table](50-lua-by-id-composites.md) | low | no | — |
| 51 | [Lua: register the eight operator metamethods once for BinaryFile and Expression](51-lua-operator-metamethods-once.md) | low | no | — |
| 52 | [Lua tests for the six untested metadata getters (+ C++ list_vector/set_groups)](52-lua-metadata-getter-tests.md) | medium (bound, documented to agents, untested) | no | — |

## Batch 6 — Core structure cleanups

| # | Plan | Severity | Breaking | Depends on |
|---|---|---|---|---|
| 53 | [Move Database::execute into Impl (const) and drop the Database& back-references](53-execute-into-impl.md) | medium | no | — |
| 54 | [Delete dead Row/Result members and forward-declare Result in database.h](54-row-result-dead-members.md) | low | yes | 53 |
| 55 | [TypeValidator to free functions; move schema.h, schema_validator.h, type_validator.h into src/](55-internal-headers-to-src.md) | low | yes | 53, 54, 56 |
| 56 | [One scalar typing policy: TypeValidator uses value_matches_type; Pattern 1 messages for unknown columns and strings on INTEGER](56-one-typing-policy.md) | medium | no | 53, 55 |
| 57 | [One group-table name helper and one require_group_table lookup; Pattern 2 for every miss](57-group-table-lookup.md) | low | no | 53 |
| 58 | [import_csv: one convert_cell pass before the write, one shared transaction tail; shared import/export group lookup](58-import-csv-convert-cell.md) | medium | no | 01, 53, 57 |
| 59 | [Reuse read_single_value, get_foreign_key and one bulk_group_sql](59-reuse-existing-helpers.md) | low | no | 53 |
| 60 | [Impl::exec for sqlite3_exec blocks; TransactionGuard replaces hand-rolled transactions](60-exec-helper-transaction-guard.md) | low | no | 01, 53, 58 |
| 61 | [Remove unused includes left by the rapidcsv drop](61-csv-unused-includes.md) | low | no | — |
| 62 | [Schema loading errors name the table and column (unsupported type, invalid table name)](62-schema-load-error-messages.md) | medium | no | — |
| 63 | [Migration and schema-file errors name the public operation](63-migration-error-operation-names.md) | low | no | 60 |
| 64 | [C API LuaRunner: delete the unreachable catch(...) branches](64-c-api-lua-runner-dead-catch.md) | low | no | — |

## Batch 7 — Tests, docs, tooling

| # | Plan | Severity | Breaking | Depends on |
|---|---|---|---|---|
| 65 | [Fix the CLI smoke test: move the deleted example script under tests/](65-cli-smoke-test.md) | high (`test-all.bat` always reports failure) | no | — |
| 66 | [Assert current_version == 3 after from_migrations in C API, Lua, Python and JS](66-current-version-tests.md) | low | no | — |
| 67 | [Delete leftover describe-does-not-throw tests; move C++ describe content tests](67-describe-leftover-tests.md) | low | no | — |
| 68 | [test_database_errors.cpp: stale notes, wrong comments, duplicate test](68-database-errors-test-cleanup.md) | low | no | — |
| 69 | [C API tests: fix handle leaks and raw delete[] frees](69-c-api-test-leaks.md) | medium (a leaked binary writer breaks every later test in the fixture) | no | — |
| 70 | [Replace four wrong-reason update tests with one real NULL string-cell test](70-null-string-cell-update-test.md) | medium (the tests claim coverage that does not exist) | no | — |
| 71 | [Delete duplicate tests left from the removed per-type update API](71-leftover-update-api-tests.md) | medium | no | — |
| 72 | [Convert 17 bare EXPECT_THROW Lua tests to expect_lua_error with real substrings](72-lua-bare-expect-throw.md) | medium | no | 47 |
| 73 | [docs/rules.md and attributes.md: valid schema examples and real migration functions](73-docs-schema-examples-and-migrations.md) | medium (user-facing docs teach schemas the validator rejects) | no | 06 |
| 74 | [JS README: describe returns a string; list missing methods and types](74-js-readme.md) | low | no | 32, 33 |
| 75 | [tests/CLAUDE.md: fix claims that contradict the tests](75-tests-claude-md.md) | low | no | — |
| 76 | [src/CLAUDE.md: query_int_rows location and public-header claim](76-src-claude-md-fixes.md) | low | no | — |
| 77 | [.github/CLAUDE.md: drop the deleted .JuliaFormatter.toml from the mirror file list](77-github-claude-md-juliaformatter-toml.md) | low | no | — |
| 78 | [CHANGELOG: add [0.11.0] link and fix [0.10.9] compare range](78-changelog-links.md) | low | no | — |
| 79 | [tidy.bat: checkout-name-independent filters and runner discovery](79-tidy-bat.md) | medium (`scripts/tidy.bat` silently lints zero files) | no | — |
| 80 | [CI dart-coverage: drop the dead build-cpp and copy steps](80-ci-dart-double-build.md) | medium (CI builds the C++ library twice, and three docs repeat a false premise) | no | — |
| 81 | [Publish workflows: remove redundant version inputs](81-publish-version-inputs.md) | medium (a manual dispatch can publish artifacts under the wrong version) | no | — |
| 82 | [build-all.bat builds then calls test-all.bat](82-build-all-calls-test-all.md) | medium | no | 65 |
| 83 | [publish-js: delete the never-failing verify step; check the tarball against what was downloaded](83-publish-js-verify-step.md) | low | no | — |
| 84 | [Wheel test scripts: fail on install failure; run the validator through uv](84-wheel-test-scripts.md) | low | no | — |
| 85 | [CMakePresets.json: two presets that mirror the scripts](85-cmake-presets-slim.md) | low | no | — |
| 86 | [Root CMakeLists: recursive format glob and remove no-op lines](86-cmakelists-format-glob.md) | low | no | 79 |
| 87 | [.pre-commit-config.yaml: drop cmake-format; keep .bat files CRLF](87-pre-commit-config.md) | low | no | — |
| 88 | [Stale names in CI scripts and docs (release.yml, setup-node@v6)](88-stale-ci-names.md) | low | no | — |

## Not planned

**Optional items (verified but marginal; excluded by scope choice).** Ask for any of them to be planned:
- Binary: stale `select_agents` comment, `set_initial_value` setter, unreachable Weekly-under-Yearly branch.
- C API: `convert_*_options` taking a pointer, `bad_alloc` arms, `quiver_database` as an aggregate, NULL-tolerant `quiver_element_destroy`, `size_t` count on the element array setters, renaming the `copy_*_to_c` helpers.
- Julia: array copies and branch merge in `database_update.jl`, the dead `dlopen` in the generator, re-exporting log-level/aggregation constants.
- Dart: `Arena` in the `Element` setters, `_fillCSVOptions` seeded from `quiver_csv_options_default()` (Python too).
- JS: typed `readBulk`/`readById` helpers in `read.ts`.
- Lua: registration style and grouping, stripping planning IDs from comments, `quiver_cli` required positional.
- Tests: removing the unused `Configuration` preamble from about 191 tests, one `open_quiet` helper, stripping planning IDs from test comments.

**Refuted during adversarial review (do not implement):**
- De-duplicating the three 3-line group-read column-list loops in the C API (not worth it).
- Re-tagging the time-series dimension column DATE_TIME in the C API (conflicts with the Julia type-stability follow-up; low value).
- Renaming "JuliaFormatter" in root/Julia CLAUDE.md (Style.jl wraps JuliaFormatter; only the `.JuliaFormatter.toml` line is stale, see plan 77).
- A lua-api.ts sync test that greps quoted error strings (too much machinery for cosmetic drift).
- Table-driving the expression arithmetic tests (churn outweighs gain).
