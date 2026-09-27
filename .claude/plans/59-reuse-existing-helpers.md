# 59 — Reuse `read_single_value`, `get_foreign_key` and one `bulk_group_sql`

**Batch** 6 · **Severity** low · **Breaking** no (identical behaviour) · **Size** S · **Layers** C++ core only (+ src/AGENTS.md sentence)
**Depends on** 53 (`execute` → `impl_->execute`; adjust the calls below to whichever form exists) · **Overlaps with** 56 (rewrites `resolve_fk_label` with `get_foreign_key` already — if 56 landed, skip step 2's `resolve_fk_label` half), 22 (query C API; this is the C++ side only)

## Why

Three existing helpers are re-implemented inline.

1. **`query_*` re-implement `internal::read_single_value<T>`.** `src/database_query.cpp` has three
   bodies of this shape:
   ```cpp
   std::optional<std::string> Database::query_string(const std::string& sql, const std::vector<Value>& parameters) {
       auto result = execute(sql, parameters);
       if (result.empty()) {
           return std::nullopt;
       }
       return result[0].get_string(0);
   }
   ```
   `internal::read_single_value<T>(const Result&)` (`src/database_internal.h`, ~L86) does exactly
   this, and the `_by_id` scalar readers already use it.
2. **Two hand-rolled FK loops** duplicate `TableDefinition::get_foreign_key(column)` (added in
   `src/schema.cpp` for `update_relation`): `internal::scalar_metadata_with_fk`
   (`src/database_internal.h`, ~L182-189):
   ```cpp
    for (const auto& fk : table_def.foreign_keys) {
        if (fk.from_column == col_name) {
            meta.is_foreign_key = true;
            meta.references_collection = fk.to_table;
            meta.references_column = fk.to_column;
            break;
        }
    }
   ```
   The second loop is in `Impl::resolve_fk_label` (`src/database_impl.h`), which plan 56 rewrites.
3. **The load-bearing LEFT JOIN is pasted six times.** Each of the six bulk vector/set readers
   (`src/database_read.cpp`, ~L59, 69, 79, 116, 126, 136) builds
   `"SELECT c.id, g." + attribute + " FROM " + collection + " c LEFT JOIN " + table + " g ON g.id = c.id ORDER BY c.rowid, <order>"`.
   `src/AGENTS.md` warns "Don't 'simplify' the SQL back to `SELECT id, value FROM <group_table>`".
   Six copies means six places where someone can do that.

Principle: reuse what exists and delete duplication.

## Constraints and decisions

- Behaviour-preserving. `read_single_value<double>` goes through `Row::get_float`, which widens an
  INTEGER. That is exactly what `query_float`'s comment says, and `src/AGENTS.md` already documents
  it.
- Build only the SQL in the helper. Leave each reader's own
  `require_collection` / `find_*_table` / `require_column` lines alone, so each error still names its
  own operation.
- The new SQL helper lives in `database_read.cpp`'s anonymous namespace, not in a header.

## Changes

### 1. `src/database_query.cpp`

Add `#include "database_internal.h"` (the file includes only `database_impl.h`). Replace the three
bodies:
```cpp
std::optional<std::string> Database::query_string(const std::string& sql, const std::vector<Value>& parameters) {
    return internal::read_single_value<std::string>(execute(sql, parameters));
}

std::optional<int64_t> Database::query_integer(const std::string& sql, const std::vector<Value>& parameters) {
    return internal::read_single_value<int64_t>(execute(sql, parameters));
}

std::optional<double> Database::query_float(const std::string& sql, const std::vector<Value>& parameters) {
    return internal::read_single_value<double>(execute(sql, parameters));
}
```
Delete the `// Row::get_float widens ...` comment. The rule is documented once, at
`read_single_value<double>` / `Row::get_float`. First confirm that `read_single_value` has
`std::string`, `int64_t` and `double` specializations or branches:
`sed -n 80,105p src/database_internal.h`. If it lacks one, add it there rather than keeping the
inline body.

### 2. `internal::scalar_metadata_with_fk` (`src/database_internal.h`)

```cpp
inline ScalarMetadata scalar_metadata_with_fk(const TableDefinition& table_def, const std::string& col_name) {
    auto meta = scalar_metadata_from_column(table_def.columns.at(col_name));
    if (const auto* fk = table_def.get_foreign_key(col_name)) {
        meta.is_foreign_key = true;
        meta.references_collection = fk->to_table;
        meta.references_column = fk->to_column;
    }
    return meta;
}
```
If plan 56 has not landed, make the same `if (const auto* fk = table_def.get_foreign_key(column))`
replacement in `Impl::resolve_fk_label`, wrapping the lookup and throw. If 56 landed, it already did.

### 3. `src/database_read.cpp` — one SQL builder for the six bulk readers

At the top of the file's anonymous namespace (add one before `read_vector_integers` if none exists;
the file already has one for `group_select_sql` — move that namespace up if needed):
```cpp
// One entry per element, aligned with read_element_ids: LEFT JOIN the group table onto the
// collection so an element with no group rows still yields a (NULL) row, which
// read_grouped_values_all turns into an empty inner vector. Do not "simplify" this back to
// `SELECT id, value FROM <group_table>` -- that is the shape that skipped elements.
std::string bulk_group_sql(const std::string& collection, const std::string& table, const std::string& attribute,
                           const char* order_by) {
    return "SELECT c.id, g." + attribute + " FROM " + collection + " c LEFT JOIN " + table +
           " g ON g.id = c.id ORDER BY c.rowid, " + order_by;
}
```
The three vector readers use `bulk_group_sql(collection, vector_table, attribute, "g.vector_index")`
and the three set readers use `bulk_group_sql(collection, set_table, attribute, "g.rowid")`. Copy
the exact current SQL of one vector and one set reader first, and make sure the helper reproduces
it character for character, including the ORDER BY columns. `sed -n 55,62p src/database_read.cpp`
and `sed -n 112,120p src/database_read.cpp` show them.

## Tests

No new tests. These existing suites pin the behaviour:
- `*Query*` (query_* NULL and no-row cases, and the float widening at `tests/test_database_query.cpp` ~L278/~L301);
- `*Metadata*` (FK fields);
- the vector/set bulk reader tests, including "element with no rows is an empty vector".

## Docs and changelog

- `src/AGENTS.md`, "`read_grouped_values_all<T>` ... requires the LEFT JOIN their SQL builds" bullet:
  change the tail to "... requires the LEFT JOIN `bulk_group_sql` (`database_read.cpp`) builds for
  all six. Don't 'simplify' it back to ...".
- No CHANGELOG entry.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`
3. `scripts/format.bat`

## Acceptance criteria

- [ ] The `query_*` bodies are one line each.
- [ ] No `for (const auto& fk : table_def.foreign_keys)` loop remains in `scalar_metadata_with_fk`
      (or in `resolve_fk_label`).
- [ ] The LEFT JOIN SQL appears once (`grep -c "LEFT JOIN" src/database_read.cpp` prints 1).
- [ ] Suites green.

## Pitfalls

- `order_by` must be a trusted literal (`"g.vector_index"` / `"g.rowid"`), never user input.
- `attribute` and `table` are already validated by `require_column` / `find_*_table` before the SQL
  is built. Keep that order.

## Out of scope

- The `_by_id` readers' SQL.
- The C API query entry points (plan 22).
