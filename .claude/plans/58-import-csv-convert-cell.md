# 58 — `import_csv`: one `convert_cell` pass before the write, one transaction tail; import and export share one group lookup

**Batch** 6 · **Severity** medium · **Breaking** no (same accepted inputs; one FK-miss message unifies on the longer existing text) · **Size** L · **Layers** C++ core (`src/database_csv_import.cpp`, `src/database_csv_export.cpp`, `src/schema.cpp`/`schema.h`, `src/database_describe.cpp`), tests
**Depends on** **01** (keeps foreign keys ON and reshapes the scalar write; this plan must preserve everything 01 added: the duplicate-label check, the delete of omitted ids, the `ON CONFLICT(id) DO UPDATE` insert, the self-FK clear/second pass), 57 (`Schema::group_table_name`), 53 (`impl_->execute`) · **Overlaps with** 60 (replaces the hand-rolled begin/commit/rollback in both import blocks with `TransactionGuard` — land 58 first, then 60 edits one tail instead of two), 61 (includes in the same files)

## Why

`src/database_csv_import.cpp` (789 lines at HEAD) converts every CSV cell through **four
near-identical copies** of the same decision tree:
- scalar validation pass (~L407-452),
- scalar insert loop (~L471-526),
- group validation pass (~L652-704),
- group insert loop (~L727-770).

Each copy does, in the same order: empty → NULL (or the NOT NULL throw), FK → label lookup (or the
FK-miss throw), DATE_TIME → `parse_datetime_import`, INTEGER → `parse_integer` or
`resolve_enum_value`, REAL → `parse_float`, otherwise TEXT. The validation copy and the insert copy
of each path must agree exactly, or a cell that validated fails mid-write, after the DELETE.

Two more copies sit on top of those:
- **Column typing** is looked up two ways. The scalar path uses `table_def->get_column(col)->type`.
  The group path builds a `type_map` from `get_*_metadata` (`group_meta` / `type_map` / `get_type`,
  ~L591-624), with dead fallbacks (`col_def ? ... : DataType::Text`, a guard at ~L685) even though
  `db_cols` comes from `SELECT *` on the same table.
- **Group-table resolution** is a hand-written three-way `has_table` chain in `import_csv`
  (~L315-337) and another in `export_csv` (`src/database_csv_export.cpp`, ~L203-223, plus a
  `type_map` block at ~L232-254).

The two write blocks each carry their own transaction + catch tail with the `UNIQUE constraint` →
"duplicate entries" rewrite.

Principles: one decision tree instead of four copies, and "converting a row *is* validating it"
(no validation/insert drift). Both are readability and simplicity.

## Constraints and decisions

- **Maintainer notes (binding):**
  - Depends on plan 01.
  - Choose each branch on `col.type` alone. `query_columns` already types every TEXT `date_` column
    as DateTime (`src/schema.cpp`), so drop the `|| is_date_time_column(col_name)` re-checks.
  - Keep enum handling: a non-integer cell in an INTEGER column with `enum_labels` for that column
    goes through `resolve_enum_value`; otherwise throw "Invalid integer value".
  - Delete `group_meta`, `type_map` and `get_type`.
  - Import and export share `Schema::find_group_table`, returning `std::optional<TableMatch>`.
  - Hoist describe's `sections[]` into one namespace-scope constant.
  - No transaction-helper lambda: one write tail after the if/else.
- **Use one FK-miss message everywhere**: the longer existing text with the suffix
  `".\nCreate the element before referencing it."`. Tests match on substrings, so check with
  `grep -rn "Could not find an existing element" tests/ bindings/`.
- Convert every row **before** the first write (before the DELETE / omitted-id delete). A bad cell
  then throws before anything changes, which is today's guarantee from the separate validation pass.
- Leave `require_well_formed_quotes`, `sniff_csv_file`/`read_csv_file` and the CSV parsing alone.
  Root CLAUDE.md says "Do not remove that check".
- Self-FK cells convert to `nullptr` in the converted rows and keep their label for the
  second pass, as today.
- `export_csv`'s type map maps `id` to Text and a non-date dimension to Text. Keep that override
  (the policy note says `value_to_csv_string` consults the type only for DateTime formatting); only
  its *source* changes to `table_def->columns`.

## Changes

### 1. `Schema::find_group_table` (`schema.h` / `schema.cpp`)

`Schema::TableMatch` already exists (`grep -n "struct TableMatch" -A5 include/quiver/schema.h`;
it holds `table_name` and `type`). Add:
```cpp
    // The group table named `group` for `collection`, trying vector, then set, then time series.
    std::optional<TableMatch> find_group_table(const std::string& collection, const std::string& group) const;
```
```cpp
std::optional<Schema::TableMatch> Schema::find_group_table(const std::string& collection, const std::string& group) const {
    for (const auto type : {GroupTableType::Vector, GroupTableType::Set, GroupTableType::TimeSeries}) {
        const auto name = group_table_name(collection, group, type);   // plan 57
        if (has_table(name)) {
            return TableMatch{name, type};
        }
    }
    return std::nullopt;
}
```
If `TableMatch`'s fields are named differently, adapt. If plan 57 has not landed, compute the name
with the three `*_table_name` helpers inline.

### 2. `import_csv` — target resolution

Replace the three-way chain (~L315-337) with:
```cpp
    auto table_name = collection;
    GroupTableType group_type{};
    if (!group.empty()) {
        const auto match = impl_->schema->find_group_table(collection, group);
        if (!match) {
            throw std::runtime_error("Cannot import_csv: group not found: '" + group + "' in collection '" +
                                     collection + "'");
        }
        table_name = match->table_name;
        group_type = match->type;
    }
    const auto& table_def = *impl_->schema->get_table(table_name);
```

### 3. One `convert_cell`

Add in the file's anonymous namespace (next to `parse_integer` / `parse_float` /
`parse_datetime_import` / `resolve_enum_value`):
```cpp
// The one CSV-cell -> Value conversion import uses for both the scalar and the group path. It is
// run over every row before anything is written, so converting a row is what validates it.
// `fk_labels` is the label -> id map for this column's foreign key, or nullptr when the column is
// not a (non-self) foreign key. A self-FK cell is converted by the caller (NULL now, resolved in
// the second pass).
Value convert_cell(const std::string& cell,
                   const std::string& col_name,
                   const ColumnDefinition& col,
                   const ForeignKey* fk,
                   const std::unordered_map<std::string, int64_t>* fk_labels,
                   const CSVOptions& options) {
    if (cell.empty()) {
        if (col.not_null) {
            throw std::runtime_error("Cannot import_csv: Column " + col_name + " cannot be NULL.");
        }
        return nullptr;
    }
    if (fk) {
        const auto it = fk_labels->find(cell);
        if (it == fk_labels->end()) {
            throw std::runtime_error("Cannot import_csv: Could not find an existing element from collection " +
                                     fk->to_table + " with label " + cell +
                                     ".\nCreate the element before referencing it.");
        }
        return it->second;
    }
    switch (col.type) {
    case DataType::DateTime:
        return parse_datetime_import(cell, options.date_time_format);
    case DataType::Integer:
        if (const auto value = parse_integer(cell)) {
            return *value;
        }
        if (options.enum_labels.count(col_name) > 0) {
            return resolve_enum_value(cell, col_name, options);
        }
        throw std::runtime_error("Cannot import_csv: Invalid integer value '" + cell + "' for column '" + col_name + "'.");
    case DataType::Real:
        if (const auto value = parse_float(cell)) {
            return *value;
        }
        throw std::runtime_error("Cannot import_csv: Invalid float value '" + cell + "' for column '" + col_name + "'.");
    default:
        return cell;
    }
}
```
Copy the throw texts **exactly** from the current validation copies; the ones above are HEAD's.
Check `parse_datetime_import`'s return type (a `std::string` canonical form, per root CLAUDE.md)
and `resolve_enum_value`'s (an `int64_t`). Check the `ColumnDefinition`/`ForeignKey` field names in
`schema.h`.

### 4. Each branch produces converted rows, before any write

- **Scalar branch.** Build the FK maps as today (plan 01's structure). Then, in one loop over
  `csv.rows`, build `std::vector<std::vector<Value>> rows` in the order the INSERT binds. That
  order is `id` first (the preserved id from `existing_label_to_id`, or `nullptr`), then `db_cols`.
  For each column:
  - a self-FK cell becomes `nullptr`, with its label kept for the second pass;
  - any other cell is `convert_cell(cell, col_name, *table_def.get_column(col_name), fk_or_null, map_or_null, options)`.

  Keep plan 01's duplicate-label check before this loop. Delete the separate validation pass
  (~L407-452) and the per-cell logic inside the insert loop (~L471-526). The insert loop now binds
  `rows[r]`.
- **Group branch.** The same, with `id` produced from the `label` column via `label_to_id` (the
  existing throw when a label is missing), the FK map excluding the parent `id` FK, and the existing
  vector_index consecutive check. Delete `group_meta`, `type_map`, `get_type` (~L591-624), the dead
  guard (~L685) and the `col_def ? ... : DataType::Text` fallbacks.

### 5. One write tail

After the if/else, both branches write inside **one** `try` with one `catch`:
```cpp
    try {
        impl_->begin_transaction();
        if (group.empty()) {
            ...plan 01's scalar write steps, now binding the pre-converted rows...
        } else {
            execute("DELETE FROM " + table_name);          // impl_->execute after plan 53
            for (const auto& row : rows) {
                execute(insert_sql, row);
            }
        }
        impl_->commit();
    } catch (const std::exception& e) {
        impl_->rollback();
        if (std::string(e.what()).find("UNIQUE constraint") != std::string::npos) {
            throw std::runtime_error("Cannot import_csv: There are duplicate entries in the CSV file.");
        }
        throw;
    }
```
The branches only differ in their write steps, so the tail exists once. There is no lambda or
helper. Plan 60 later swaps `begin_transaction`/`commit`/`rollback` for a `TransactionGuard`
declared inside this `try`.

### 6. `export_csv` (`src/database_csv_export.cpp`)

- Replace the three-way lookup (~L203-223) with `impl_->schema->find_group_table(collection, group)`
  plus the same "group not found" throw text export uses today (keep it byte-identical).
- Build `type_map` from `table_def->column_order` and `table_def->columns.at(name).type` instead of
  the `get_*_metadata` dispatch (~L232-254). Keep the `id` → Text override. Use
  `internal::find_dimension_column(*table_def)` for the time-series ORDER BY. The dead
  `else Text` dimension branch goes with the dispatch.
- Reuse the looked-up `table_def` for the FK loop (~L280) instead of a second `get_table(table_name)`.

### 7. `src/database_describe.cpp`

Hoist the local `sections[]` array (used twice; `grep -n "sections" src/database_describe.cpp`)
into one `constexpr` namespace-scope constant.

## Tests

No behaviour change except the FK-miss text becoming uniform. The regression net is the import and
export suites in every layer: `tests/test_database_csv_import.cpp`,
`tests/test_database_csv_export.cpp`, `tests/test_c_api_database_csv_import.cpp`, the Lua CSV
suites and each binding's CSV tests. These cover bad integers and floats, enums, date formats, NOT
NULL, FK misses, self-FK, vector_index gaps, header-only files, and plan 01's new cascade tests.

Add one test that pins "nothing is written when a later row is bad", in
`tests/test_database_csv_import.cpp`:
```cpp
TEST(Database, ImportCsvBadCellInLastRowWritesNothing) {
    // Import a CSV whose last row has an invalid integer into a collection that already has rows;
    // assert the call throws "Invalid integer value" and the existing rows are untouched.
    // Build the CSV with the file's temp-file helpers, e.g. a 3-row CSV whose 3rd row has some_integer=abc.
}
```
Use the file's existing fixture and helpers, which write the CSV to a temp path. Copy a
neighbouring "Invalid integer value" test and extend it to pre-existing rows plus a count check
before and after.

## Docs and changelog

- `src/CLAUDE.md`: in "DATE_TIME content is checked by both halves", the sentence about
  `parse_datetime_import` stays true. Add one line: "Import converts every cell through one
  `convert_cell` (`database_csv_import.cpp`) before writing, so validation and the write cannot
  disagree."
- `CHANGELOG.md`: no entry, unless you want one line under `### Changed` saying the group-path
  FK-miss message now carries the same "Create the element before referencing it." suffix as the
  scalar path. It is user-visible text, but not breaking.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=*Csv*:*CSV*` and then the full suite.
3. `./build/bin/quiver_c_tests.exe`
4. `scripts/test-all.bat` (every binding's CSV tests)
5. `scripts/format.bat`

## Acceptance criteria

- [ ] One `convert_cell`. No separate validation pass. Rows are converted before the first write.
- [ ] `group_meta`, `type_map` and `get_type` are deleted in import, and import/export use
      `Schema::find_group_table`.
- [ ] One transaction tail. Plan 01's behaviour (cascade on omitted elements, duplicate labels,
      self-FK) is intact.
- [ ] All CSV suites in every layer are green.

## Pitfalls

- Plan 01 inserts with `ON CONFLICT(id) DO UPDATE` and runs a self-FK second pass. Re-read the
  current code and keep the bind order `rows[r]` matches the INSERT's column list.
- The group path's `id` column is excluded from `db_cols` (`get_db_columns(..., "")` vs `"id"`).
  Keep that distinction.
- Pre-converting holds every row in memory, and the validation pass already required the whole
  parsed CSV to be in memory (`read_csv_file`). No new memory ceiling.

## Out of scope

- `TransactionGuard` (plan 60).
- Lifting the "refuses inside a transaction" precondition.
- `bin_to_csv`/`csv_to_bin` (plans 13, 14).
