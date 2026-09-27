# Quiver code-quality review → improvement plan

## Context

The user asked for a critical, whole-repo review against five principles in priority order: human
readability, clean over defensive code, simple over abstract, delete unused code, breaking changes OK.
Architecture rule: all logic lives in C++, bindings stay thin, and every layer has tests.

How the review was done: 13 area reviewers covered every tracked source, test, build and doc file.
A completeness critic then added 3 areas that were in scope but unexamined: the Lua reference text,
error-pattern conformance and test-code quality. Each finding went to two independent skeptics told
to refute it. One checked the facts against the code; the other checked it against the CLAUDE.md
Design Decisions / Do-Not-Fix lists and against the principles. 143 findings were raised, 5 were
refuted and 138 survived. I spot-checked the high-severity claims myself (import_csv, binary time
math, the savepoint history) and merged the duplicates, leaving about 100 items. One verifier suggestion is **dropped** on purpose:
SAVEPOINT-based atomicity. The v0.3 research rejected it (Pitfall 4, commit f92af8d).

**Verdict.** The codebase is in good shape. Error handling is disciplined, the C API is genuinely
thin, and the Design Decisions are well recorded. These areas are clean, with nothing worth changing:
- `src/csv/` reader/writer, `ui_metadata`, the describe renderer, `utils/`, Migration(s)
- the Lua JSON encoder and sandbox gate
- expression-node design and the C API macro/error channel
- Julia's `GC.@preserve` discipline, Dart's hook/loader, JS's Bun workarounds

The problems that remain fall into four groups:
1. **Real correctness bugs**: `import_csv`, time-series edge cases, the binary time-coordinate math,
   and the Python/JS array marshallers.
2. **Logic leaking into bindings**: Julia and Python rebuild whole-group readers that the C API
   already provides, and get the rows wrong; four bindings skip core validation by returning early;
   Python coerces types itself.
3. **Moderate duplication** in the core, the Lua runner and a few bindings.
4. **Stale docs and tooling**, including a CLI smoke test that fails on every run.

0.11.0 is still unreleased and is already a minor bump. Every BREAKING item below therefore goes
into its CHANGELOG section with no further manifest bump.

Batches are ordered by value, and each is one commit series on `rs/quality`, with no push unless
asked. Items marked *(opt)* are verified but marginal. **They are skipped unless you ask for them.**

**Your decisions**
- Scope: all seven batches, *(opt)* items excluded.
- Q2, mixed numeric column: **widen to FLOAT** if any cell is a float, in Python *and* Dart, so
  `[1, 2.5]` means the same everywhere. It already does in JS and Julia.
- Q3, broadcast label rule: **relaxed**, the ternary rule (non-singleton label sets must match,
  singletons broadcast), applied to binary ops too. A non-breaking relaxation.
- Q4, `example/`: **move the smoke script to `tests/cli/smoke.lua`**. Point `test-all.bat:134,141`
  at it, and drop the `example/` references in root CLAUDE.md:24, :356 and tests/CLAUDE.md:198.

**How the work runs.** Batches go in order, 1 → 7. Several touch the same files (for example
`database_csv_import.cpp` in Batches 1 and 6, and `lua_runner.cpp` in 1 and 5), so each batch lands
before the next starts. Within a batch, independent layers can be done in parallel. Each batch ends
with the full suite (Verification below), a CHANGELOG entry and the CLAUDE.md updates, then one
commit per batch.

---

## Batch 1: Core correctness (C++, highest value)

1. **`import_csv` silently corrupts relations.** The scalar path runs `PRAGMA foreign_keys = OFF`
   and then `DELETE FROM C` (`src/database_csv_import.cpp:363-374, 454-573`). CASCADE and SET NULL
   never fire, so an element missing from the CSV leaves orphaned group rows and dangling FKs.
   (Confirmed by reading the code.)
   Fix:
   - Keep FKs ON throughout and delete every PRAGMA toggle and the zero-row branch.
   - In the validation pass, reject a duplicate label with the existing "duplicate entries" message.
   - `DELETE ... WHERE id = ?` each existing label that is absent from the CSV.
   - Keep the single INSERT, binding the preserved id or NULL, and add
     `ON CONFLICT(id) DO UPDATE SET col = excluded.col`. Never use `INSERT OR REPLACE`, which
     cascades.
   - The group path just drops the PRAGMAs.
   - Tests (C++ and C API): an omitted element leaves no orphan group rows; an omitted parent nulls
     `Child.parent_id`.
   - Leave the "refuses inside a transaction" precondition. Its premise goes away, but lifting it is
     the user's call.
2. **Two definitions of the time-series dimension column.** `find_dimension_column` scans the
   alphabetical column map, so a second `date_` value column becomes the dimension for reads while
   writes use the PK (`src/database_internal.h:93-125`). Fix: return the first PK column from
   `find_dimension_columns` that satisfies the current predicate
   (`type == DateTime || is_date_time_column`). Add a new valid schema with a nullable `date_end`
   value column, plus C++ tests for metadata, read order and `read_time_series_row`.
3. **`update_time_series_group` drops columns that appear only in later rows.** It builds the INSERT
   from `rows[0]` (`src/database_time_series.cpp:172-189`). Fix: take the union of row keys, as
   `update_group_rows` does. Add one C++ test, and delete the "rows[0]" caveats in src/CLAUDE.md,
   src/c/CLAUDE.md, `database_helpers.h:215` and `lua_runner.cpp:2024`.
4. **`read_time_series_row` on a multi-dimension group** can return NULL, or a value from an
   arbitrary block. Fix: throw Pattern 1 when the table has more than one dimension column, checked
   after `find_dimension_columns` succeeds. Also add `AND t.<attr> IS NOT NULL` to the outer ON.
   **BREAKING.** Add C++ and C API tests and a docs/time_series.md sentence.
5. **`create_element`/`update_element` write the scalar row before validating arrays**, so a
   rejected call leaves a partial write inside a caller transaction
   (`src/database_create.cpp:22-47`, `src/database_update.cpp:25-54`, `database_impl.h:173-347`).
   Fix: split `insert_group_data` into two steps:
   - route + FK-resolve + validate, with no writes;
   - write.
   Run the first step before the first INSERT/UPDATE. `resolve_element_fk_labels` then handles
   scalars only, and its false "FK columns have unique names" comment goes. Add two dry-run
   regression tests. No savepoints.
6. **SchemaValidator enforces the parent FK only for vector tables.** Set and time-series tables
   escape it (`src/schema_validator.cpp:28-39, 109-201, 317-323`). Fix:
   - Extract `validate_group_parent(name, kind)` and call it for all three group kinds.
   - Collapse `validate_foreign_keys` to one rule.
   - Delete the seven dead `if (!table)` guards.
   - Add invalid schemas `set_no_parent_fk.sql` and `time_series_fk_actions.sql` with tests that
     assert the message.
   **BREAKING.** Fix the set examples in docs/rules.md and docs/attributes.md.
7. **The list-groups functions disagree on an unknown collection.** `list_vector_groups`,
   `list_set_groups` and `list_time_series_groups` return `[]`, while `list_scalar_attributes`
   throws. Fix: use `require_collection` in all three. Flip
   `test_database_time_series_group.cpp:180` and add the vector/set cases. **BREAKING**: the
   composites `read_vectors_by_id` / `read_sets_by_id` now throw too.

## Batch 2: Binary subsystem correctness (Julia + Lua surface)

1. **The time-coordinate validator rejects valid cells.** It rejects them for most parent/child
   layouts (for example daily under yearly from day 32 on, or hourly under monthly), because
   `TimeProperties::datetime_to_int` ignores the parent (`src/binary/time_properties.cpp:43-61`).
   (Confirmed by reading the code.) Fix:
   - One parent-aware position function, shared by `validate_dimension_values` and the
     initial-value computation. A Weekly parent is anchored on `initial_datetime`, not Jan 1.
   - `add_offset_from_int` keeps the time of day for Monthly/Yearly.
   - `from_toml_content` validates before computing initial values.
   - Test: walk every cell of all 8 reachable pairs, including one non-midnight mid-period start and
     one weekly layout that crosses Dec 31. Mirror one layout in the C API and Julia tests.
2. **`initial_value` goes stale after `aggregate()`**, so the saved output is shifted. Fix:
   - Compute it in one `BinaryMetadata` function, called by `from_toml_content` and
     `ExpressionAggregate`. Keep the stored field: it is on the hot path.
   - When the outermost time dimension is reduced, rebase the output's `initial_datetime` to the
     start of that period.
   - Test in C++, C API and Julia: aggregate "year" over [year, month] from 2025-03-01.
3. **The dimension-start rule exists twice and checks only the immediate parent**
   (`iteration.cpp:115-129`, `expression_aggregate.cpp:100-112`). Fix: one
   `dimension_start_at_values` that walks the ancestor chain, used by both. Test: monthly × daily ×
   hourly from 2025-01-01T10:00.
4. **The metadata factory indexes parallel arrays without length checks.** This is UB reachable from
   Julia and Lua (`binary_metadata.cpp:191-331`). Fix:
   - Move the post-parse body into an anon-namespace function taking the fields plus the operation
     name. It checks both length pairs (Pattern 1).
   - `from_element` calls it directly, which drops the TOML round trip.
   - `from_toml_content` reads keys through one throwing helper.
   - Tests in C++, C API, Julia and Lua.
5. **Delete the unused C API builder family**: `quiver_binary_metadata_create`, the `set_*`
   functions, `add_dimension`/`add_time_dimension`, and the C++ `BinaryMetadata::add_*` methods
   along with their tests. Port the getter and null-argument tests to `from_toml` handles, and
   regenerate `c_api.jl`. **BREAKING.** Also fix root CLAUDE.md:717, src/CLAUDE.md:697 and
   src/c/CLAUDE.md:112.
6. **The `csv_to_bin` round trip is lossy.**
   - The reader uses `std::stod`: it accepts trailing garbage and truncates under a `,` locale. Fix:
     move `parse_float` into `src/utils/number.h`, use it in `read_line`, and throw Pattern 1 with
     the label.
   - The writer uses `{:.6g}`, which loses digits. Fix: use `utils::append_number`.
   - Tests: trailing garbage, a `,` LC_NUMERIC round trip, and a 1.2345678 round trip.
7. **`CSVConverter` reads past the end on a short row.** Fix: a field-count check with a line number
   in `read_line`. In the same change, add `split_fields`, `header_fields`, `dimension_cells` and
   `join` helpers that replace the copies, and make the constructor private.
8. **Binary and ternary broadcast metadata are two copies of one N-ary algorithm, and their label
   rules disagree** (`expression_helpers.h:112-270`). Fix: one
   `build_broadcast_metadata({sources}, primary)` that keeps the current dimension order and datetime
   fallback, with the **relaxed** label rule. Delete `compute_output_labels`,
   `compute_ternary_output_labels` and `build_ternary_broadcast_metadata`. Add a single-label
   binary-op test in C++, C API, Julia and Lua (`agg_max - agg_min` gets label "max"). CHANGELOG:
   non-breaking relaxation. Update src/CLAUDE.md.
9. **Two identical aggregation enums.** Fix: one namespace-scope `AggregationOperation`, one C enum,
   one `from_c` and one Lua parser, each taking the operation name so messages stay the same.
   **BREAKING** (C/Julia constants).
10. *(opt)* Small leftovers:
    - the stale `select_agents` header comment
    - `TimeProperties::set_initial_value`, a setter for a public field
    - the unreachable Weekly-under-Yearly size branch and its `WEEKS_IN_YEAR` constants

## Batch 3: C API contract and cross-binding leaks

1. **`read_time_series_row` returns 0 / NaN for "no data" in all four FFI bindings**, while Lua and
   the docs say null. Fix:
   - Add `uint8_t** out_mask` (freed by `quiver_database_free_mask`) and map mask 0 to
     `nothing`/`None`/`null`.
   - Julia returns `Vector{Optional{T}}`, which closes that item in `type_stability_followup.md`.
   - Update the C API NaN test and Julia's `isnan` assertions. Add a no-data INTEGER test per
     binding.
   - Delete the sentinel note in src/c/CLAUDE.md. **BREAKING.**
2. **Julia and Python compose `read_{vector,set}_group_by_id` from NULL-dropping per-column reads.**
   Once a cell is NULL they throw BoundsError/IndexError or mispair rows, although the native C
   readers exist. JS binds neither reader. Fix:
   - Julia and Python call `quiver_database_read_{vector,set}_group_by_id` through one private
     decoder shared by vector and set only. It follows Dart's `_decodeGroupRows`: parse DATE_TIME,
     masked cell → null. Leave `read_time_series_group` alone.
   - JS adds `readVectorGroupById`/`readSetGroupById` returning **rows**, matching
     Dart/Python/Julia, with DATE_TIME kept as strings. Share the mask decode with
     `readTimeSeriesGroup` rather than copying it.
   - NULL-cell tests go against `multi_column_groups.sql`, with a NULL in each column in turn.
     Switch `test_database_update.py:392` to assert through the reader.
   - Update the root CLAUDE.md table, its "still compose" caveat and bindings/python/CLAUDE.md.
3. Add a C API test for `quiver_database_read_set_group_by_id` over `Items_set_codes` (two nullable
   columns, one NULL cell).
4. **All four FFI bindings return early on an empty `update_time_series_files` map**, which skips
   the core's collection and table checks. Fix: always call the C API with `(NULL, NULL, 0)`. Dart
   must not allocate zero bytes, and JS must not build zero-length tables. Add a test per binding
   that an unknown collection throws.
5. **Delete dead C API symbols**, each chain end to end:
   - `quiver_clear_last_error`
   - `quiver_element_has_scalars`/`has_arrays`/`scalar_count`/`array_count`, plus the C++
     `Element::has_scalars`/`has_arrays` (the tests use `scalars().empty()` instead)
   - their Python cdefs; then regenerate the Julia and Dart FFI and reword the `common.h`
     error-channel comment
   **BREAKING.**
6. **One C++ query method maps to two C functions.** Fix: delete the plain
   `quiver_database_query_{string,integer,float}` and rename the `_params` forms to the plain
   names. `(NULL, NULL, 0)` means no parameters. Every binding drops its branch; Julia gets one
   method per type with `parameters::Vector = []`, and so do `query_boolean`/`query_date_time`.
   JS passes `null` rather than `ptr()` of an empty array. **BREAKING (C only).**
7. Delete the REAL→INTEGER truncation branch in `marshal_group_rows_to_c`
   (`database_helpers.h:329-331`). A REAL cell in an INTEGER column then reads back masked, the same
   as `Row::get_integer`. Add one C test.
8. *(opt)* C API consistency:
   - `convert_database_options`/`convert_csv_options` take a pointer and own the NULL-means-defaults
     rule.
   - Drop the `bad_alloc` arms. Where one is the only catch, replace it with the standard
     `std::exception` catch.
   - `quiver_database` becomes an aggregate.
   - `quiver_element_destroy` accepts NULL.
   - The element array setters take a `size_t` count with the `QUIVER_REQUIRE` guard
     (**BREAKING ABI**; update all four FFI declarations).
   - Rename the `copy_*_to_c` helpers by shape and return `void`.

## Batch 4: Binding fixes

**Python** (`bindings/python/src/quiverdb/`)
1. **The group writers and `Element._set_array` take a column's type from its first cell.** They then
   coerce every cell with `int()`/`float()`, so `[1, 2.5]` into REAL stores `[1, 2]` and `"7"`
   becomes 7. Fix:
   - Classify each column from all its non-None cells: FLOAT if any cell is a float, otherwise
     INTEGER, with bool as 1/0. Dart's `_marshalGroupColumn` gets the same whole-column rule.
   - Raise `TypeError(... cell {r} of column '{name}')` for a non-numeric cell.
   - Never call `int()`/`float()` on a str.
   - Fix the Dart and JS comments that cite Python's `int(v)`, and bindings/python/CLAUDE.md.
2. **`datetime` is rejected by `Element.set`, `_set_array` and `_marshal_row_columns`**, although the
   group writer, Julia and Dart accept it. Fix:
   - One `format_datetime` in `_helpers.py`, which avoids a circular import. An aware value converts
     to UTC when `utcoffset()` is not None, because the readers return UTC.
   - Use it at all five sites.
   - Tests: write back the value `read_scalar_date_time_by_id` returned, and an upsert with a
     datetime.
3. `LuaRunner.__init__`: set `self._closed = True` before the FFI call and `False` after `_ptr` is
   set, so a failed construction doesn't warn in `__del__`. Add a test.
4. Add `/` after the positional parameters of `create_element`, `update_element`,
   `upsert_time_series_row` and `upsert_time_series_row_by_label` (the recorded rule, currently
   applied to only one method). Add a test that `update_element("C", id, id=...)` works.
   **BREAKING.**
5. Delete the redundant `bool` branches (bool is an int subclass), keep `test_boolean_input` as the
   pin, and fix root CLAUDE.md's boolean passage, which names the branch.
6. Delete dead code:
   - `Makefile`; move its `ruff check --fix` into `format.bat` first
   - the `dotenv` dev dependency
   - the `tests_path`/`csv_db_export`/`csv_db_import` fixtures
   - the shadowing module-local `collections_db` fixture in `test_database_metadata.py`
   - `Element.clear`/`_ensure_valid`
7. Fix the stale docstrings: the `upsert` type line and its D-03 reference, the `_c_api.py` "Phase 1"
   header, and the `_marshal_group_columns` wording.
8. *(opt)* `_marshal_csv_options` seeds from `quiver_csv_options_default()`.

**JavaScript** (`bindings/js/`)
1. **The group-column numeric branch coerces non-number cells, and `setElementArray` normalizes
   booleans from cell 1 only**, so `[true, 5]` is written wrong. Fix:
   - One small `numericCells(caller, name, values)`: per-cell bool → 1/0, and throw on a non-number.
   - Use it in `updateGroupColumns` and `setElementArray`.
   - Tests: `[true, 5, false, 7]` round-trips; `[1.5, "abc"]` throws naming the column.
2. `mod.ts` becomes `export * from "./src/index.ts"`, keeping the doc comment. Export
   `DATA_TYPE_INTEGER/FLOAT/STRING/DATE_TIME`. Add one test that imports `../mod.ts`.
3. Accept `bigint` in `GroupColumns` and `QueryParam`. In the FLOAT fallback, map a bigint with
   `Number()`. Add tests above 2^53.
4. *(opt)* `read.ts`: replace the six per-type helpers with typed `readBulk`/`readById` generics that
   take symbol refs rather than strings (removes the `Record<string, Function>` casts and about
   120 lines). `listMetadata` takes typed refs as well.

**Julia** (`bindings/julia/`)
1. `Element`: add a `setindex!(::Nothing)` that calls `quiver_element_set_null`; clearing a scalar
   is impossible today. Widen the scalar string setter to `AbstractString`. Add tests.
2. `helper_maps.jl` makes N `_by_id` queries plus O(N·M) `findfirst` calls. Fix: two bulk reads and
   a `Dict`, plus the shadowed local variable name.
3. `read_time_series_group` leaks the C buffers when decoding throws. Fix: `try ... finally` free.
4. *(opt)* In `database_update.jl`, delete the three element-by-element array copies and merge the
   DateTime/String branch pairs. In the generator, delete the unused `dlopen`, `Libdl` and the
   `jll_pkg_name` comment.
5. *(opt)* Re-export the `QUIVER_LOG_*` and aggregation constants from `Quiver` so user code avoids
   `Quiver.C.`. Add a `console_level` test.
6. Add a test for `dry_run`'s exception path.

**Dart** (`bindings/dart/`)
1. Add `_marshalGroupColumns(arena, data)`, which handles the empty map and names the column in the
   length error, for the six group writers. That removes about 280 duplicated lines. Upserts keep
   `_marshalGroupColumn`.
2. Delete `Element.set`'s `Map` case, which silently discards its key (Julia deleted its twin in
   #198). Rewrite the issue-70 test flat, with an assertion. **BREAKING.**
3. `_decodeGroupRows` and `readTimeSeriesGroup` leak on a DateTime parse failure. Fix: free in
   `finally`. Add a malformed-date test.
4. *(opt)* Element setters use `Arena` like the rest of the binding, and the four typed `List<T>`
   cases are deleted.
5. Add a test for `dryRun`'s exception path.
6. *(opt)* `_fillCSVOptions` seeds from `quiver_csv_options_default()`.

**All bindings**
- Fix the stale "not positionally aligned / only elements that own rows" comments (14 sites, listed
  in the review) to: "One entry per element, aligned with read_element_ids (an element with no rows
  is an empty list); NULL cells are dropped."
- Rename Julia/Python `read_{vector,set}_date_time_by_id` to `_date_times_by_id`, matching the
  plural rule and Dart. Update the composites and the root table. **BREAKING.** Keep Julia's
  `date_time` keyword.

## Batch 5: Lua runner (`src/lua_runner.cpp`) and the agent-facing reference

0. **Correct the agent-facing Lua reference** (`bindings/js/src/lua-api.ts`, shipped on npm as
   `LUA_DB_API_REFERENCE` into an LLM prompt). Three of its claims lead an agent into silent data
   loss. These are prose fixes unless noted:
   - It promises that a failed script "rolls back" (lines 86-92, 454). Nothing wraps `run()`: writes
     that already finished stay, and only `db:transaction` / `db:dry_run` undo their block. An
     unclosed `begin_transaction` stays open.
   - The CSV section calls import/export time-series-only and never says `import_csv` **replaces the
     whole table**. It also omits `group = ""` for scalars.
   - `update_time_series_files`: "nil clears that column" should say "replaces the whole row; an
     omitted column is cleared".
   - The boolean rule contradicts itself (line 466). Several quoted errors are never emitted. Quote
     `Cannot begin_transaction: transaction already active`, and add the begin_dry_run-inside-a-
     transaction case.
   - `query_*` do not convert, so a mismatch reads as `nil`.
   - Scalar reads have `nil` holes (`ipairs`/`#` stop early; loop over `read_element_ids`, and fix
     the "iterate with ipairs" rule and the dangling "see Reading" reference). Vector/set reads drop
     NULL cells.
   - A `nil` query parameter binds NULL only in the interior of the table (`{nil, 5}`); a trailing
     one hits the count check. Add Lua tests for both.
   - Code: `collect_group_columns` checks that a key is a string before `as<std::string>()`, so a
     table of rows gets one stable Pattern 1 message in Debug and Release. Quote that message in the
     reference.
1. **A `BinaryFile` held in a Lua global is never closed at `run()` exit**, so its path stays blocked
   in the write registry. Fix:
   - `db:open_file` returns a `shared_ptr`, and a `weak_ptr` goes into `open_binary_files` next to
     `open_writers`. `close_open_writers()` closes readers and writers.
   - Add a pin test in `test_lua_binary.cpp` and a line in lua-api.ts.
   **BREAKING**: reopen the file in each run.
2. One `lua_to_value(obj, caller, what)`, a sibling of `lua_cell_as`, replaces four copies of the
   nil/bool/int/double/string dispatch. Messages stay byte-identical.
3. Errors name internal helpers. Fix: `table_to_element`, `lua_table_to_value_map` and
   `lua_table_to_values` take the public `caller`. Update the four pinned assertions.
4. **Strict decoders**:
   - `export_csv`/`import_csv` take `sol::object` options, checked through `csv_options_entries`
     (allowed keys and types).
   - `quiver.metadata` rejects unknown keys and wrong types.
   - `rename_agents` goes through `lua_cell_as<std::string>`.
   - Add negative tests and run them on a **Release** build (`SOL_SAFE_GETTER` is off there).
   - Update lua-api.ts. **BREAKING.**
5. One `group_metadata_lua` replaces five builders. Rename the `data_type_to_string` shadow to
   `lua_data_type_name` and drop the no-op `else t[k] = sol::lua_nil`.
6. `read_vectors_by_id_lua` / `read_sets_by_id_lua` become one `to_lua_table` call per case.
7. Add Lua tests for the six untested metadata getters (`get_scalar/vector/set_metadata`,
   `list_scalar_attributes/vector_groups/set_groups`), which are promised in lua-api.ts.
8. *(opt)* Readability:
   - Register every `db:` method with `bind.set_function`, grouped by topic.
   - Strip the 46 D-xx/FMT-xx/RESEARCH.md planning references and keep the prose, in lua_runner.cpp,
     `src/csv/`, `ui_metadata.cpp` and src/CLAUDE.md.
   - `quiver_cli`: make `script` a required positional and drop the duplicate exists check.

## Batch 6: Core structure cleanups (no behaviour change unless noted)

1. Move `execute` into `Database::Impl` as a **const** member:
   - Drop the `Database& db` back-reference from the eight Impl helpers.
   - Delete `query_int_rows` and the hand-rolled `current_version`, which also fixes its 32-bit
     `sqlite3_column_int`.
   - Guard the `summarize` histogram with `typeof(col) = 'integer'`.
   - Update src/CLAUDE.md.
2. Delete the dead `Row`/`Result` members, which only their own tests call, and forward-declare
   `Result` in `database.h`.
3. Replace `TypeValidator` with src-only free functions, and move `schema.h`, `schema_validator.h`
   and `type_validator.h` into `src/` without `QUIVER_API`; nothing outside the DLL uses them.
   `load_schema_metadata` then publishes `schema` alone. **BREAKING** (C++ headers). Update root
   CLAUDE.md (lazy-schema text) and src/CLAUDE.md.
4. One typing policy:
   - `validate_value` calls `value_matches_type`.
   - `resolve_fk_label` becomes pure resolution (via `get_foreign_key`), keeping the early throw
     with `caller`.
   - The unknown-column message becomes Pattern 1 with `require_column`'s wording.
   - Delete `Schema::get_data_type`.
   - Pin both messages with `EXPECT_STREQ`.
5. Add `Schema::group_table_name`, lifted from describe, and `Impl::require_group_table`, which
   throws one Pattern 2 message. Use them in the metadata functions, `update_group_rows` and the
   four time-series ops. Delete `find_time_series_table`, its files twin and the six unreachable
   null checks. The time-series "not found" message joins Pattern 2 (CHANGELOG).
6. `import_csv` (after Batch 1):
   - One `convert_cell`, run over every row before the DELETE, so converting is the validation.
   - One shared transaction tail.
   - Delete `group_meta`, `type_map` and `get_type`.
   - Import and export share one `Schema::find_group_table`.
7. Reuse existing helpers:
   - `query_*` → `read_single_value<T>(execute(...))`
   - both FK loops → `get_foreign_key`
   - one `bulk_group_sql` for the six LEFT JOIN readers, which carries the "don't revert" comment
8. Add an `Impl::exec(sql, what)` helper for the five `sqlite3_exec` blocks. `TransactionGuard`
   replaces the hand-rolled begin/commit/rollback in `migrate_up`/`migrate_down`/`apply_schema` and
   in both import blocks, declared inside the try so rollback precedes `PRAGMA foreign_keys = ON`.
9. Remove the unused includes left in `database_csv_export.cpp`/`database_csv_import.cpp` by the
   rapidcsv drop.
10. **Error-message pattern conformance** (the root 3-pattern rule), all with CHANGELOG lines:
    - Schema loading throws `Unknown data type: X`, naming no table or column, before the validator
      runs. Fix: `data_type_from_string` returns `std::optional`, and `query_columns` throws
      `Failed to validate schema: column 'c' in table 't' has unsupported type 'BLOB'`. One
      `is_safe_identifier` check at the top of the `load_from_database` loop replaces the three
      per-function guards. Add `tests/schemas/invalid/unsupported_type.sql` with C++ and C API
      tests.
    - The unknown-attribute error in create/update (`Column 'x' not found in table 'y'`, no pattern)
      is covered by 6.4, using `require_column`'s exact text.
    - The `Schema::find_{vector,set}_table` misses become Pattern 2 (`Vector attribute not found:
      ...`), matching their twins. The time-series pair is covered by 6.5.
    - `migrate_up` takes `const char* operation`, so errors name `from_migrations` /
      `validate_migrations`. `migrate_down` and `apply_schema` hardcode their single caller, keeping
      "up/down migration N". Update the three `test_migrations.cpp` pins.
    - `convert_params` takes the caller, so errors say `Cannot query_string:` rather than
      `Cannot query:`. Fold this into 3.6.
    - Delete the two unreachable `catch (...)` "Unknown error" branches in `src/c/lua_runner.cpp`.

## Batch 7: Tests, docs, tooling

**Tests**
- Fix the CLI smoke test, which fails on every run: `example/` was deleted in #295 but
  `scripts/test-all.bat:134,141` still runs `example1.lua`. Add `tests/cli/smoke.lua` with the old
  script's content (`git show 4af1397^:example/example1.lua`) and point both lines at it. Remove
  the `example/` mentions from the docs.
- Assert `current_version == 3` after `from_migrations` in the C API, Lua, Python and JS. Their
  current tests pass even for a wrapper that returns 0.
- Delete the leftover "describe runs" tests in the four binding lifecycle files, plus C++
  `capture_describe` and `DescribeDoesNotThrow`. Move the five describe-content tests to
  `test_database_describe.cpp` under names without "Printed".
- `test_database_errors.cpp`: delete the stale segfault and `find_set_table` notes, fix the "missing
  table" comments, and delete the duplicate `CreateElementEmptyArraySkipsSilently`.
- C API tests leak handles:
  - Five update tests never destroy `update` (`test_c_api_database_update.cpp:29/59/89/812/900`).
    Destroy it, and check `create_element`.
  - In `test_c_api_expression.cpp:88/167/189/1468`, an `ASSERT` inside an open-for-write span skips
    the close, so one failure poisons the rest of the fixture through the write registry. Make them
    `EXPECT`.
  - Replace the 12 raw `delete[]` frees with `quiver_database_free_string`, and drop the two
    `if (ids != nullptr)` guards.
  - No suite-wide RAII rewrite.
- Four C API update tests pass only because the element doesn't exist, and never assert the NULL
  string-cell path they are named after. Replace them with one test next to
  `UpdateGroupNullStringEntryIsNull`: `tag = {"a", nullptr, "c"}`, then
  `COUNT(*) ... WHERE tag IS NULL == 1`.
- Delete the leftover duplicates from the per-type update API removed in #100:
  - the 12 NullDb/NullCollection copies, keeping the two NullAttribute tests
  - `UpdateScalar{Integer,Float,String}` and both `UpdateElementNoFkColumnsUnchanged`
  - `UpdateVector{Integers}InvalidColumnThrows`, `Update{Vector,Set}InvalidCollection`, and the
    `test_database_errors.cpp:182-233` family
  - `test_c_api_database_lifecycle.cpp:310-501`
- Convert 17 Lua tests that use a bare `EXPECT_THROW` (the vacuous-pass pattern tests/CLAUDE.md
  warns against) to `expect_lua_error` with each call's actual message substring.
- *(opt)* Delete the unused `Configuration` preamble from ~191 C++/C tests and the "Configuration
  required first" comments. Keep it in the four tests that read it, with a comment.
- *(opt)* Add one `open_quiet(schema)` helper to `tests/test_utils.h` for the ~300 in-memory
  literal sites and the 7 local wrappers. Deduplicate `read_file`/`write_file` there.
- *(opt)* Strip the planning IDs (TEST-nn, FMT-nn, D-nn, CAPI-nn, BUG-nn and so on) from test
  comments and Lua assert messages, in the same pass as 5.8.

**Docs**
- docs/rules.md and docs/attributes.md:
  - the Configuration example needs `label`, and its trailing comma goes
  - self-FK: `ON UPDATE CASCADE ON DELETE SET NULL`, with the prose rule reworded to match the
    validator
  - add the parent FK to the vector-relation examples, and the missing commas
  - replace the nonexistent `Quiver.set_migrations_folder`/`create_migration`/`apply_migrations!`
    with `from_migrations`
  - fix the `../rules.md` link
- JS README: `describe()` returns a string; add `describeCollection`/`summarizeCollection`, the
  boolean/bigint types and `GroupColumns`.
- tests/CLAUDE.md: say "empty inner list", not "omission"; add `test_database_metadata.cpp` and
  `_describe` to the file lists; fix the C API file-set comparison.
- src/CLAUDE.md: `query_int_rows`' location (moot after Batch 6.1) and the "every QUIVER_SOURCES
  entry has a public header" claim.
- .github/CLAUDE.md:112: drop `.JuliaFormatter.toml`. Style.jl wraps JuliaFormatter, so the other
  mentions are correct.
- CHANGELOG: add a `[0.11.0]` link definition, and point `[0.10.9]` at `v0.10.8...v0.10.9`.

**Tooling**
- `scripts/tidy.bat` lints 0 files unless the checkout directory is named `quiver`. Fix:
  - a name-independent file regex
  - `HeaderFilterRegex`/`ExcludeHeaderFilterRegex: '_deps'` in `.clang-tidy`
  - find `run-clang-tidy` on PATH
  - delete the unused CMake `tidy` target
  - keep CRLF
- CI `dart-coverage` builds the library twice. Fix: delete its `build-cpp` and "Copy shared
  libraries" steps, and fix the "no CI job runs the hook" claims in bindings/dart/CLAUDE.md and
  root CLAUDE.md.
- `publish-{s3,julia,js}.yml`: delete the `version` inputs and resolve the version from
  `assert_version.py` at checkout. Drop the `version=$VERSION` dispatch args in `publish.yml` in the
  same commit. Keep the `ref` inputs.
- `build-all.bat`: after building, `call "%ROOT_DIR%\scripts\test-all.bat"`. That deletes its copy
  of the six suites, and it gains the CLI smoke test.
- `publish-js.yml`: delete the "Verify native libraries" step, which can never fail, and check the
  tarball against `find libs -type f`.
- `test-wheel-install.bat`: `set EXIT_CODE=1` on install failure. `test-wheel.bat`: use
  `uv run --no-project python`.
- `CMakePresets.json` is used by nothing. Slim it to `dev`/`release` (both Ninja, tests + C API ON)
  and fix the tests/CLAUDE.md trap note.
- Root `CMakeLists.txt`:
  - the format glob becomes the five recursive patterns CI checks
  - drop the duplicate `include(CTest)`/`enable_testing()`
  - drop pyproject's redundant `cmake.args` and `OUTPUT_NAME quiver`
- `.pre-commit-config.yaml`: drop the never-applied `cmake-format` hook, and exclude `\.bat$` from
  `mixed-line-ending`.
- Stale names: `release.yml` → `publish.yml` in three comments; make the `setup-node@v6` references
  version-neutral.

*(The missed-areas round has been folded in: the lua-api.ts text in 5.0, error-pattern
conformance in 6.10, and test-code quality in the Tests list above.)*

---

## Explicitly not doing
- SAVEPOINT atomicity (rejected in v0.3, Pitfall 4).
- Splitting `lua_runner.cpp`: lua-api-sync.test.ts parses that path, and removing duplication helps
  more.
- Re-tagging the time-series dimension column DATE_TIME, and deduplicating the three 3-line
  column-list loops. Both were refuted as low value, and the first conflicts with Julia's type
  follow-up.
- Any FFI closure-helper collapse in Dart or Python (Do-Not-Fix).

## Verification
- Configure and build Debug (`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`), then `scripts/test-all.bat`: C++, C API,
  Julia, Dart, JS and Python suites plus the CLI smoke test. After Batch 7 this must print all PASS;
  it cannot today.
- For Lua decoder changes, also run a Release test tree
  (`-B build-release -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON`) with
  `--gtest_filter=LuaRunner*:LuaBinary*`.
- After any C API signature change, run `scripts/generator.bat`. Hand-edit `_c_api.py`,
  `loader.ts` and `bindings.dart` in their existing style (Dart: no ffigen regen, per
  bindings/dart/CLAUDE.md). Then run `bindings/js/test/lua-api-sync.test.ts`.
- Every fixed bug gets a regression test that fails before the fix, at every layer the behaviour is
  visible in.
- Run `scripts/format.bat` and `scripts/tidy.bat` (the latter only works after the Batch 7 fix). Add
  CHANGELOG entries under 0.11.0, and update the nearest CLAUDE.md for each change.
