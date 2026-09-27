# 18 — Whole-group readers: Julia/Python call the native C API; JS gains them

**Batch** 3 · **Severity** high (silent wrong data, or a crash, on any nullable multi-column group) · **Breaking** no: dense groups read back exactly as before; a group with NULL cells used to crash or mis-pair and now returns `nothing`/`None` in place; JS only gains methods · **Size** M · **Layers** Julia binding, Python binding (+ cdef), JS binding (+ loader), one test schema table, tests in Julia/Python/JS, root + `bindings/{julia,python,js}` + `tests/` AGENTS.md, JS README, CHANGELOG. **No** C++ core, C API, Lua or Dart code change.

**Depends on** none.

**Overlaps with**
- **17** (read_time_series_row presence mask), which runs first. It edits `readTimeSeriesRow` in `bindings/js/src/time-series.ts`, `read_time_series_row` in `bindings/julia/src/database_read.jl` and `bindings/python/src/quiverdb/database.py`, and the loader/cdef entries for that function. The only shared thing is the file. Step 7's unused-import cleanup must be done against the file as plan 17 left it (grep before deleting an import).
- **19** (C API test for `quiver_database_read_set_group_by_id`) owns the C API test that this plan does not add. This plan appends a table to `tests/schemas/valid/multi_column_groups.sql` and leaves the existing `Items_vector_readings` and `Items_set_codes` tables unchanged, so plan 19's test on `Items_set_codes` is unaffected.
- **23** (C API group marshaller stops narrowing REAL cells into INTEGER columns) changes `marshal_group_rows_to_c`, which is what the new decoders read. The decoders dispatch on the per-column type tag, so they need no change for 23.
- **31 / 33** (JS per-cell numeric check, bigint in group writers) edit `updateGroupColumns` in `bindings/js/src/group-columns.ts`. This plan adds a sibling read function and the `TimeSeriesData` type to the same file, and does not touch `updateGroupColumns`.
- **32** (JS `mod.ts` re-exports `src/index.ts`): this plan changes which module `src/index.ts` takes `TimeSeriesData` from. It does not touch `mod.ts`, which keeps working because it re-exports the name from `./src/index.ts`.
- **36** (Julia `read_time_series_group` frees in `finally`) and **40** (Dart group decoders free in `finally`): this plan deliberately leaves `read_time_series_group` (Julia, Python, JS logic) and Dart's `_decodeGroupRows` unchanged apart from moving the JS body. The new Julia/Python decoders already free in `finally`.
- **42** (rename `read_{vector,set}_date_time_by_id` → `_date_times_by_id`): after this plan, `read_vector_group_by_id` / `read_set_group_by_id` no longer call those functions, so plan 42 has two fewer call sites (`read_vectors_by_id` / `read_sets_by_id` still call them).
- **41** (stale "not positionally aligned" comments), **30** (Python stale docstrings, `_c_api.py` header), **21 / 22** (`_c_api.py` / `loader.ts` symbol edits), **74** (JS README), **75** (`tests/AGENTS.md`): these touch the same files, not the same lines. This plan adds two README lines, and plan 74 does the full listing.
- **01–17** may already have added `### Added` / `### Fixed` entries under `## [0.11.0] — unreleased`. Append to them; don't overwrite.

## Why

`Database::read_vector_group_by_id` / `read_set_group_by_id` (C++, `src/database_read.cpp` ~L196-216) run **one** SELECT over every value column and keep SQL NULL cells as `Value{nullptr}`. They are exposed in C as `quiver_database_read_{vector,set}_group_by_id` (`src/c/database_read.cpp` ~L399-473): columnar typed arrays, a per-cell presence mask, freed by `quiver_database_free_time_series_data`. Dart calls them natively (`bindings/dart/lib/src/database_read.dart` ~L1020-1176, `_decodeGroupRows`). Root AGENTS.md tells callers to "prefer `read_vector_group_by_id` / `read_set_group_by_id`, which are row- and NULL-correct".

**Julia** has the generated ccalls (`bindings/julia/src/c_api.jl` ~L298-304) but never calls them. `bindings/julia/src/database_read.jl`, `read_vector_group_by_id` (currently ~L541-582; `read_set_group_by_id` ~L584-625 is the same with `set` in place of `vector`) zips the NULL-dropping per-column readers instead:

```julia
    for col in columns
        name = col.name
        values = if col.data_type == C.QUIVER_DATA_TYPE_INTEGER
            read_vector_integers_by_id(db, collection, name, id)
        ...
        column_data[name] = values
        row_count = length(values)          # the LAST column's length
    end
    ...
    for i in 1:row_count
        ...
            row[name] = values[i]           # indexes every column
```

**Python** has no cdef for either symbol and repeats the same composition in `bindings/python/src/quiverdb/database.py`, `read_vector_group_by_id` (currently ~L1985-2019) / `read_set_group_by_id` (~L2021-2053):

```python
            column_data[name] = values
            row_count = len(values)

        return [{"vector_index": i, **{name: vals[i] for name, vals in column_data.items()}} for i in range(row_count)]
```

The per-column `_by_id` readers go through `internal::read_column_values<T>` (`src/database_internal.h`), which skips NULL cells. So the columns come back with different lengths:

Reproduction with `tests/schemas/valid/multi_column_groups.sql` (`Items_vector_readings`: `amount REAL`, `score REAL`, both nullable, read in that order):

| write | per-column reads | Julia / Python result | truth |
|---|---|---|---|
| `amount=[1.5, 2.5], score=[NULL, 20.5]` | amount `[1.5, 2.5]`, score `[20.5]` → row_count 1 | `[{amount: 1.5, score: 20.5}]`: a pairing never written, row 2 lost | `[{1.5, NULL}, {2.5, 20.5}]` |
| `amount=[NULL, 2.5], score=[10.5, 20.5]` | amount `[2.5]`, score `[10.5, 20.5]` → row_count 2 | `BoundsError` / `IndexError` | `[{NULL, 10.5}, {2.5, 20.5}]` |
| one column, `parent_ref=[1, NULL, 2]` (relations.sql) | `[1, 2]` | 2 rows; Python's `vector_index` renumbered 0,1 | 3 rows |

The test suites work around it instead of testing it: `bindings/julia/test/test_database_update.jl` (~L929) says "asserted in SQL: the per-column reader drops NULLs", and `bindings/python/tests/test_database_update.py` `test_accepts_null_cells` (~L387) says "Asserted in SQL, not through read_vector_group_by_id". The C++ core is right: `UpdateGroupKeepsColumnPresentOnlyInALaterRow` (`tests/test_database_update.cpp` ~L1302) passes today (confirmed with `build/bin/quiver_tests.exe --gtest_filter=*UpdateGroupKeepsColumnPresentOnlyInALaterRow*`).

**JS** binds neither symbol. `bindings/js/src/loader.ts` has no entry, and a grep for `GroupById` in `bindings/js/src` finds nothing. JS does bind all four group *writers*. So a JS caller has no NULL-correct multi-column group read at all: `bindings/js/test/database-update.test.ts` "writes null cells as SQL NULL" (~L200) also asserts in SQL. The Lua reference that ships in this package (`bindings/js/src/lua-api.ts` ~L391, ~L856) tells script authors to "do that read in the host binding", which JS cannot do today. No documented exception covers the omission (root AGENTS.md lists JS-datetime, binary/expression, the Lua whole-group readers, Lua booleans and Lua CSV).

Principles violated: "Intelligence: Logic resides in C++ layer. Bindings/wrappers remain thin" (Julia and Python re-implemented the reader, wrongly), "All public C++ methods should be bound to C API, then to Julia/Dart/Python/JS/Lua" (JS), and "Homogeneity".

## Constraints and decisions

- **Maintainer decisions (binding, from the item notes):**
  - Julia and Python get **one private decoder shared by the vector and set readers only**, modelled on Dart's `_decodeGroupRows`. A DATE_TIME column is parsed, a masked cell is `nothing` / `None`, and the C result is freed in `finally`.
  - **Leave `read_time_series_group` alone** in Julia and Python.
  - **Python keeps the synthetic 0-based `vector_index`.**
  - JS `readVectorGroupById` / `readSetGroupById` return **rows**, `Record<string, number | string | null>[]`, for homogeneity with Dart/Python/Julia. **DATE_TIME stays a string.** The mask decode is **shared with `readTimeSeriesGroup` via one helper in `group-columns.ts`**, not copied.
  - The C API set-reader test is plan 19.
  - Update the root AGENTS.md "Multi-column group readers" table and the "still compose" caveat, `bindings/python/AGENTS.md` and `bindings/js/AGENTS.md`.
- **Root design decision "group writers are column-oriented while the group readers are row-oriented"** (root AGENTS.md ~L67-71). Row shape is kept in every binding, and JS joins it. Its binding list is updated (docs step), not relitigated.
- **Root design decision "JS keeps a string-based datetime surface"**: the JS readers return DATE_TIME cells as the stored ISO string.
- **Root design decision "Lua has no row-aligned whole-group readers"**: Lua is not touched.
- **Root "Do Not Fix": "Collapsing per-method FFI boilerplate in Dart/Python into closure-parameterized helpers"**. Python therefore keeps one expanded FFI call block per reader and shares only the *decoder*, a data codec like `_marshal_group_columns`. Julia's own documented convention (`bindings/julia/AGENTS.md`, "One marshaller for every group writer": `_update_group_columns(db, update, ...)` takes the C entry point as an argument) is followed there: `_read_group_rows(db, read_group, ...)`. JS follows its own precedent, `updateGroupColumns(handle, caller, cFn, ...)`.
- **"Tests must exist in every layer the behaviour is visible in"**: the behaviour changes in Julia, Python and JS, and those layers get tests. The C++ core is already tested (`UpdateGroupMultiColumnRoundTrips`, `UpdateGroupKeepsColumnPresentOnlyInALaterRow`, `ReadSetByIdOrderMatchesGroupReader`). The C API vector reader already has `ReadVectorGroupByIdPreservesNullCells`, and the set one is plan 19. Dart is unchanged.
- **Set row order is "consistent across every reader of the group, otherwise unspecified"** (root design decision). Set tests compare contents, never literal positions.
- Error messages: no new message is crafted. Unknown group or collection errors still come from the C++ core (`Vector group not found: ...`, raised by the `get_vector_metadata` call inside `quiver_database_read_vector_group_by_id`), so they are unchanged in Julia and Python.

Alternatives considered and rejected:
- *Fold `read_time_series_group` into the shared Julia/Python decoder* (the original proposal). That reader parses only the dimension column, by name, and returns every other DATE_TIME column as a string, so sharing would silently change its return types. The maintainer rejected it.
- *JS returns columns (`TimeSeriesData`)*. It would round-trip with the writers, but it breaks homogeneity with the three row-shaped bindings. The maintainer rejected it.
- *Copy `readTimeSeriesGroup`'s decode loop into the JS readers*. That would be three copies of ~70 lines. The maintainer rejected it.
- *Python helper that also makes the FFI call*. It falls under the closure-parameterized FFI helper that "Do Not Fix" forbids.
- *Test only on the existing tables*. No schema anywhere has a DATE_TIME (`date_`-prefixed) column in a vector or set group, so the DATE_TIME branch of all three decoders (and the JS "stays a string" decision) would go untested. One nullable table is appended to `multi_column_groups.sql` instead of adding a schema file. It was probed through `quiver_cli` against a copy of the schema: it validates, `date_event` reports `date_time` and `note` reports `text`, and NULL cells write.
- *Keep the "Multi-column group readers" table under "Binding-Only Convenience Methods"*. That section opens "These have no direct C++ or C API counterpart", which is false for these readers. The table becomes two rows of the Representative Cross-Layer Examples table.

Corrections to the source finding:
- The proposal "all three readers can share one decode loop" is wrong for Julia and Python (see above). It holds only for JS, because JS keeps DATE_TIME as a string there, so its time-series and group decodes really are identical.
- The JS proposal said to decode "through the existing masked-column reader used by readTimeSeriesGroup". No such helper exists; the loop is inline in `readTimeSeriesGroup` (`time-series.ts` ~L32-125) and is extracted here.
- The Python "~40 lines removed" claim does not hold: the house style keeps an expanded FFI block per reader. The gain is correctness and thinness, not size.

## Changes

No change in `src/`, `include/`, `src/c/`, `src/lua_runner.cpp`, `bindings/dart/`, `bindings/julia/src/c_api.jl` or `bindings/dart/lib/src/ffi/bindings.dart`. Both C symbols already exist: `libquiver_c.dll` exports them, and Julia's `c_api.jl` already has the generated wrappers. **Do not run any FFI generator.**

### 1. `tests/schemas/valid/multi_column_groups.sql`: append one table

Append at the end of the file (after `Items_set_codes`). Do not edit the existing tables:

```sql

-- A DATE_TIME (date_-prefixed) and a TEXT value column, both nullable: the whole-group readers
-- parse the first (Julia/Python/Dart; JS keeps the ISO string) and must never decode the NULL
-- char* of a masked-out cell
CREATE TABLE Items_vector_events (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    date_event TEXT,
    note TEXT,
    PRIMARY KEY (id, vector_index)
) STRICT;
```

Why: this gives the three decoders a DATE_TIME column and a nullable TEXT column to test against. The column names are unique across `Items`' groups, so `validate_no_duplicate_attributes` is satisfied. It also changes `list_vector_groups("Items")` on this schema from 1 group to 2. At HEAD no test asserts that count. Grep the files that load `multi_column_groups.sql` for `list_vector_groups` / `listVectorGroups` / `read_vectors_by_id` / `readVectorsById` / `describe` before committing, in case an earlier plan added one.

### 2. `bindings/julia/src/database_read.jl`: native readers + one shared decoder

Replace the whole of `function read_vector_group_by_id(...)` (currently ~L541-582) **and** `function read_set_group_by_id(...)` (currently ~L584-625). Current start of each:

```julia
function read_vector_group_by_id(db::Database, collection::String, group::String, id::Int64)
    metadata = get_vector_metadata(db, collection, group)
    columns = metadata.value_columns
```
```julia
function read_set_group_by_id(db::Database, collection::String, group::String, id::Int64)
    metadata = get_set_metadata(db, collection, group)
    columns = metadata.value_columns
```

with:

```julia
function read_vector_group_by_id(db::Database, collection::String, group::String, id::Int64)
    return _read_group_rows(db, C.quiver_database_read_vector_group_by_id, collection, group, id)
end

function read_set_group_by_id(db::Database, collection::String, group::String, id::Int64)
    return _read_group_rows(db, C.quiver_database_read_set_group_by_id, collection, group, id)
end

# Shared by the two whole-group readers; `read_group` is the C entry point (the
# `_update_group_columns` convention). The native reader runs one SELECT over every value column
# and keeps NULL cells, so rows stay aligned: a masked cell is `nothing`, a DATE_TIME column is
# parsed. read_time_series_group keeps its own decode: it returns columns and parses only the
# dimension column.
function _read_group_rows(db::Database, read_group::Function, collection::String, group::String, id::Int64)
    out_col_names = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_col_types = Ref{Ptr{Cint}}(C_NULL)
    out_col_data = Ref{Ptr{Ptr{Cvoid}}}(C_NULL)
    out_col_has_value = Ref{Ptr{Ptr{UInt8}}}(C_NULL)
    out_col_count = Ref{Csize_t}(0)
    out_row_count = Ref{Csize_t}(0)

    check(
        read_group(
            db.ptr, collection, group, id,
            out_col_names, out_col_types, out_col_data, out_col_has_value, out_col_count, out_row_count,
        ),
    )

    col_count = out_col_count[]
    row_count = out_row_count[]
    if col_count == 0 || row_count == 0
        return Dict{String, Any}[]
    end

    try
        name_ptrs = unsafe_wrap(Array, out_col_names[], col_count)
        type_vals = unsafe_wrap(Array, out_col_types[], col_count)
        data_ptrs = unsafe_wrap(Array, out_col_data[], col_count)
        mask_ptrs = unsafe_wrap(Array, out_col_has_value[], col_count)

        rows = [Dict{String, Any}() for _ in 1:row_count]
        for c in 1:col_count
            name = unsafe_string(name_ptrs[c])
            col_type = type_vals[c]
            mask = unsafe_wrap(Array, mask_ptrs[c], row_count)
            for r in 1:row_count
                if mask[r] == 0
                    rows[r][name] = nothing
                elseif col_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
                    rows[r][name] = unsafe_load(reinterpret(Ptr{Int64}, data_ptrs[c]), r)
                elseif col_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
                    rows[r][name] = unsafe_load(reinterpret(Ptr{Float64}, data_ptrs[c]), r)
                else
                    # STRING or DATE_TIME. The mask test above keeps a NULL char* out of unsafe_string.
                    s = unsafe_string(unsafe_load(reinterpret(Ptr{Ptr{Cchar}}, data_ptrs[c]), r))
                    rows[r][name] =
                        col_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME) ? string_to_date_time(s, collection, name) : s
                end
            end
        end
        return rows
    finally
        C.quiver_database_free_time_series_data(
            out_col_names[], out_col_types[], out_col_data[], out_col_has_value[],
            Csize_t(col_count), Csize_t(row_count),
        )
    end
end
```

Why:
- The Julia-side metadata lookup and per-type dispatch disappear. The return type stays `Vector{Dict{String, Any}}`, with `Int64` / `Float64` / `String` / `DateTime` values as before, plus `nothing`.
- The early return frees nothing because the C API returns NULL arrays when there are no rows (`marshal_group_rows_to_c`, `src/c/database_helpers.h`).
- A `string_to_date_time` `ArgumentError` no longer leaks the C buffers.

Do **not** touch `read_time_series_group` (plan 36 owns its `finally`), `read_vectors_by_id` or `read_sets_by_id`.

### 3. `bindings/python/src/quiverdb/_c_api.py`: declare the two readers

Right after `quiver_database_read_set_strings_by_id` (currently ~L166-168), which is the header's declaration order:

```
    quiver_error_t quiver_database_read_set_strings_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        char*** out_values, size_t* out_count);
```

insert:

```
    // Read a whole vector/set group by ID: columnar typed arrays + per-cell mask,
    // freed by quiver_database_free_time_series_data
    quiver_error_t quiver_database_read_vector_group_by_id(quiver_database_t* db,
        const char* collection, const char* group, int64_t id,
        char*** out_column_names, int** out_column_types,
        void*** out_column_data, uint8_t*** out_column_has_value,
        size_t* out_column_count, size_t* out_row_count);
    quiver_error_t quiver_database_read_set_group_by_id(quiver_database_t* db,
        const char* collection, const char* group, int64_t id,
        char*** out_column_names, int** out_column_types,
        void*** out_column_data, uint8_t*** out_column_has_value,
        size_t* out_column_count, size_t* out_row_count);
```

These match `include/quiver/c/database.h` ~L231-251 exactly. `quiver_database_free_time_series_data` is already declared (~L355).

### 4. `bindings/python/src/quiverdb/database.py`: native readers + `_decode_group_rows`

**4a.** Replace the body of `Database.read_vector_group_by_id` (currently ~L1985-2019; its current body starts `metadata = self.get_vector_metadata(collection, group)` and ends `return [{"vector_index": i, **{name: vals[i] for name, vals in column_data.items()}} for i in range(row_count)]`) with:

```python
    def read_vector_group_by_id(
        self,
        collection: str,
        group: str,
        id: int,
    ) -> list[dict]:
        """Read a multi-column vector group as row dicts, in vector_index order.

        Each row maps column names to typed values and adds a synthetic 0-based 'vector_index'.
        A SQL NULL cell is None, so rows stay positionally aligned. DATE_TIME columns are parsed
        to datetime objects.
        """
        self._ensure_open()
        lib = get_lib()
        out_names = ffi.new("char***")
        out_types = ffi.new("int**")
        out_data = ffi.new("void***")
        out_has_value = ffi.new("uint8_t***")
        out_col_count = ffi.new("size_t*")
        out_row_count = ffi.new("size_t*")
        check(
            lib.quiver_database_read_vector_group_by_id(
                self._ptr,
                collection.encode("utf-8"),
                group.encode("utf-8"),
                id,
                out_names,
                out_types,
                out_data,
                out_has_value,
                out_col_count,
                out_row_count,
            )
        )
        rows = _decode_group_rows(
            collection, out_names, out_types, out_data, out_has_value, out_col_count[0], out_row_count[0]
        )
        return [{"vector_index": i, **row} for i, row in enumerate(rows)]
```

**4b.** Replace `Database.read_set_group_by_id` (currently ~L2021-2053, body starts `metadata = self.get_set_metadata(collection, group)`) with the same block, with these differences:
- Docstring:
  ```python
        """Read a multi-column set group as row dicts.

        Each row maps column names to typed values. A SQL NULL cell is None, so rows stay
        positionally aligned. DATE_TIME columns are parsed to datetime objects. Row order is
        consistent across every reader of the group, otherwise unspecified.
        """
  ```
- The call is `lib.quiver_database_read_set_group_by_id(...)`, with the same arguments.
- It ends with
  ```python
        return _decode_group_rows(
            collection, out_names, out_types, out_data, out_has_value, out_col_count[0], out_row_count[0]
        )
  ```
  (no `vector_index`).

**4c.** Add the module-level decoder immediately **above** `def _marshal_group_columns(data: dict[str, list]) -> tuple:` (currently ~L2166), as its read-side counterpart:

```python
def _decode_group_rows(
    collection: str, out_names, out_types, out_data, out_has_value, col_count: int, row_count: int
) -> list[dict]:
    """Decode a whole-group read's columnar typed arrays + per-cell mask into row dicts, then free them.

    Shared by read_vector_group_by_id and read_set_group_by_id (Dart's _decodeGroupRows). A cell
    whose mask is 0 is None and its data slot is never read (a NULL cell's char* is NULL). DATE_TIME
    columns go through _parse_datetime. read_time_series_group keeps its own loop: it returns
    columns and parses only the dimension column.
    """
    if col_count == 0 or row_count == 0:
        return []
    try:
        columns: dict[str, list] = {}
        for c in range(col_count):
            name = ffi.string(out_names[0][c]).decode("utf-8")
            ctype = out_types[0][c]
            mask = out_has_value[0][c]
            if ctype == DataType.INTEGER:
                ints = ffi.cast("int64_t*", out_data[0][c])
                columns[name] = [ints[r] if mask[r] else None for r in range(row_count)]
            elif ctype == DataType.FLOAT:
                floats = ffi.cast("double*", out_data[0][c])
                columns[name] = [floats[r] if mask[r] else None for r in range(row_count)]
            else:  # STRING or DATE_TIME
                strs = ffi.cast("char**", out_data[0][c])
                texts = [ffi.string(strs[r]).decode("utf-8") if mask[r] else None for r in range(row_count)]
                columns[name] = (
                    [_parse_datetime(t, collection, name) for t in texts] if ctype == DataType.DATE_TIME else texts
                )
        return [{name: cells[r] for name, cells in columns.items()} for r in range(row_count)]
    finally:
        get_lib().quiver_database_free_time_series_data(
            out_names[0], out_types[0], out_data[0], out_has_value[0], col_count, row_count
        )
```

Why:
- `_parse_datetime(None, ...)` returns `None`, which its `@overload` triple already covers.
- Row keys come out in declaration order (C column order), the same order as before.
- `DataType`, `ffi`, `get_lib` and `_parse_datetime` are already in scope in this module.

Leave `read_time_series_group`, `read_vectors_by_id` and `read_sets_by_id` alone.

### 5. `bindings/js/src/loader.ts`: two symbols

In `readSymbols`, right after (currently ~L87)

```ts
  quiver_database_read_set_strings_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
```

insert:

```ts
  quiver_database_read_vector_group_by_id: {
    args: [P, BUF, BUF, I64, P, P, P, P, P, P],
    returns: I32,
  },
  quiver_database_read_set_group_by_id: {
    args: [P, BUF, BUF, I64, P, P, P, P, P, P],
    returns: I32,
  },
```

These are the same shape as `quiver_database_read_time_series_group` (~L130-133). `quiver_database_free_time_series_data` is already in `timeSeriesSymbols`.

### 6. `bindings/js/src/group-columns.ts`: the read type and the shared decoder

**6a.** Replace the imports at the top. Currently:

```ts
import { type Pointer, ptr } from "bun:ffi";
import { check, QuiverError } from "./errors.ts";
import {
  allocNativeFloat64,
  allocNativeInt64,
  allocNativePtrTable,
  allocNativeStringArray,
  toCString,
} from "./ffi-helpers.ts";
import type { NativePointer } from "./loader.ts";
import { DATA_TYPE_FLOAT, DATA_TYPE_INTEGER, DATA_TYPE_STRING, type Allocation } from "./types.ts";
```

New:

```ts
import { CString, type Pointer, ptr, read, toArrayBuffer } from "bun:ffi";
import { check, QuiverError } from "./errors.ts";
import {
  allocNativeFloat64,
  allocNativeInt64,
  allocNativePtrTable,
  allocNativeStringArray,
  allocPtrOut,
  allocUint64Out,
  decodeFloat64Array,
  decodeInt64Array,
  decodePtrArray,
  decodeStringArray,
  readPtrOut,
  readUint64Out,
  toCString,
} from "./ffi-helpers.ts";
import { getSymbols, type NativePointer } from "./loader.ts";
import {
  type Allocation,
  DATA_TYPE_DATE_TIME,
  DATA_TYPE_FLOAT,
  DATA_TYPE_INTEGER,
  DATA_TYPE_STRING,
} from "./types.ts";
```

**6b.** Right after the existing `export type GroupColumns = ...` line, add the read type, which moves here from `time-series.ts` (step 7):

```ts
/**
 * Column-oriented group read result: one array of cells per column name, `null` for SQL NULL.
 * The read-side twin of GroupColumns; no reader produces a boolean, so it admits none.
 */
export type TimeSeriesData = Record<string, (number | string | null)[]>;
```

**6c.** Append at the end of the file (after `updateGroupColumns`):

```ts
/**
 * The out-parameter signature every columnar group read C function shares
 * (quiver_database_read_time_series_group, quiver_database_read_{vector,set}_group_by_id).
 */
type ColumnReadFn = (
  db: NativePointer,
  collection: Uint8Array,
  group: Uint8Array,
  id: bigint,
  names: Uint8Array,
  types: Uint8Array,
  data: Uint8Array,
  masks: Uint8Array,
  columnCount: Uint8Array,
  rowCount: Uint8Array,
) => number;

/**
 * Call one of the columnar group read C functions, decode its typed arrays + per-cell mask into
 * columns, and free the C result. The read-side twin of updateGroupColumns, shared by
 * readTimeSeriesGroup (which returns the columns) and readVectorGroupById / readSetGroupById
 * (which transpose them into rows). A masked cell is `null`; DATE_TIME cells stay ISO 8601 strings.
 */
export function readGroupColumns(
  handle: NativePointer,
  readGroup: ColumnReadFn,
  collection: string,
  group: string,
  id: number,
): TimeSeriesData {
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const outNames = allocPtrOut();
  const outTypes = allocPtrOut();
  const outData = allocPtrOut();
  const outHasValue = allocPtrOut();
  const outColCount = allocUint64Out();
  const outRowCount = allocUint64Out();

  check(
    readGroup(
      handle,
      collBuf.buf,
      grpBuf.buf,
      BigInt(id),
      outNames.buf,
      outTypes.buf,
      outData.buf,
      outHasValue.buf,
      outColCount.buf,
      outRowCount.buf,
    ),
  );

  const colCount = readUint64Out(outColCount);
  const rowCount = readUint64Out(outRowCount);
  if (colCount === 0) return {};

  const namesPtr = readPtrOut(outNames);
  const typesPtr = readPtrOut(outTypes);
  const dataPtr = readPtrOut(outData);
  const hasValuePtr = readPtrOut(outHasValue);

  try {
    const colNames = decodeStringArray(namesPtr, colCount);
    const typesAb = toArrayBuffer(typesPtr as Pointer, 0, colCount * 4);
    const types = Array.from(new Int32Array(typesAb));
    const dataPtrs = decodePtrArray(dataPtr, colCount);
    const maskPtrs = decodePtrArray(hasValuePtr, colCount);

    // Per-cell NULL mask: mask[r] === 0 means SQL NULL, surfaced as JS null. A time series'
    // dimension column's mask is always all 1, so it stays dense.
    const result: TimeSeriesData = {};
    for (let c = 0; c < colCount; c++) {
      const colName = colNames[c];
      const maskPtr = maskPtrs[c];
      const mask = maskPtr ? new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, rowCount)) : null;
      switch (types[c]) {
        case DATA_TYPE_INTEGER: {
          const vals = decodeInt64Array(dataPtrs[c], rowCount);
          result[colName] = mask ? vals.map((v, r) => (mask[r] ? v : null)) : vals;
          break;
        }
        case DATA_TYPE_FLOAT: {
          const vals = decodeFloat64Array(dataPtrs[c], rowCount);
          result[colName] = mask ? vals.map((v, r) => (mask[r] ? v : null)) : vals;
          break;
        }
        case DATA_TYPE_STRING:
        case DATA_TYPE_DATE_TIME: {
          // Read pointer-by-pointer (not decodeStringArray): a masked-out cell is a
          // NULL char* that CString cannot construct from.
          const base = dataPtrs[c];
          const col: (string | null)[] = new Array(rowCount);
          for (let r = 0; r < rowCount; r++) {
            if (mask && !mask[r]) {
              col[r] = null;
              continue;
            }
            const strPtr = base ? read.ptr(base as Pointer, r * 8) : 0;
            col[r] = strPtr === 0 ? null : new CString(strPtr as Pointer).toString();
          }
          result[colName] = col;
          break;
        }
      }
    }
    return result;
  } finally {
    getSymbols().quiver_database_free_time_series_data(
      namesPtr,
      typesPtr,
      dataPtr,
      hasValuePtr,
      BigInt(colCount),
      BigInt(rowCount),
    );
  }
}
```

The loop body is `readTimeSeriesGroup`'s current loop (`time-series.ts` ~L78-113), moved verbatim. The only changes are the dimension-column comment wording and the `try`/`finally` around the free. The parameter is `readGroup`, not `read`, because `read` is the `bun:ffi` import this function uses for `read.ptr`.

### 7. `bindings/js/src/time-series.ts`: `readTimeSeriesGroup` becomes a call

**7a.** Delete the type line (currently ~L30):

```ts
export type TimeSeriesData = Record<string, (number | string | null)[]>;
```

**7b.** Change the group-columns import (currently ~L20)

```ts
import { type GroupColumns, updateGroupColumns } from "./group-columns.ts";
```

to

```ts
import {
  type GroupColumns,
  readGroupColumns,
  type TimeSeriesData,
  updateGroupColumns,
} from "./group-columns.ts";
```

**7c.** Replace the whole `Database.prototype.readTimeSeriesGroup = function (...) { ... };` (currently ~L32-125; it starts `const lib = getSymbols();` and ends with the `lib.quiver_database_free_time_series_data(...)` call and `return result;`) with:

```ts
Database.prototype.readTimeSeriesGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
): TimeSeriesData {
  return readGroupColumns(
    this._handle,
    getSymbols().quiver_database_read_time_series_group,
    collection,
    group,
    id,
  );
};
```

**7d.** Remove the imports this orphans. At HEAD those are `toArrayBuffer` (from `bun:ffi`), `decodePtrArray` (from `./ffi-helpers.ts`) and `DATA_TYPE_DATE_TIME` (from `./types.ts`). Plan 17 edits `readTimeSeriesRow` in this file first, so run `grep -n "toArrayBuffer\|decodePtrArray\|DATA_TYPE_DATE_TIME" bindings/js/src/time-series.ts` after 7c and remove only the names with no remaining use. Keep `CString`, `read`, `ptr`, `Pointer`, `decodeInt64Array`, `decodeFloat64Array`, `decodeStringArray`, `DATA_TYPE_{INTEGER,FLOAT,STRING}`, `allocPtrOut`, `allocUint64Out`, `readPtrOut` and `readUint64Out`: other functions in the file still use them.

### 8. `bindings/js/src/read.ts`: the two row readers

**8a.** Add this import between the `} from "./ffi-helpers.ts";` line and `import { getSymbols, type NativePointer } from "./loader.ts";` (currently ~L17-18). That is biome's alphabetical module order:

```ts
import { readGroupColumns, type TimeSeriesData } from "./group-columns.ts";
```

**8b.** Append at the end of the file (after `readSetStringsById`, currently ends ~L659):

```ts
// --- Whole-group reads ---

/** One record per row, from decoded columns that all share one length. */
function columnsToRows(columns: TimeSeriesData): Record<string, number | string | null>[] {
  const names = Object.keys(columns);
  if (names.length === 0) return [];
  return columns[names[0]].map((_, r) =>
    Object.fromEntries(names.map((name) => [name, columns[name][r]])),
  );
}

/**
 * Read an element's vector group as one record per row, in vector_index order. A SQL NULL cell
 * is `null`, so rows stay aligned across columns (the per-column readers drop NULL cells);
 * DATE_TIME cells stay ISO 8601 strings.
 */
Database.prototype.readVectorGroupById = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
): Record<string, number | string | null>[] {
  return columnsToRows(
    readGroupColumns(
      this._handle,
      getSymbols().quiver_database_read_vector_group_by_id,
      collection,
      group,
      id,
    ),
  );
};

/**
 * Set-group counterpart of readVectorGroupById. Row order is consistent across every reader of
 * the group, otherwise unspecified.
 */
Database.prototype.readSetGroupById = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
): Record<string, number | string | null>[] {
  return columnsToRows(
    readGroupColumns(
      this._handle,
      getSymbols().quiver_database_read_set_group_by_id,
      collection,
      group,
      id,
    ),
  );
};
```

There is no synthetic `vector_index`, which follows Dart (the maintainer's "homogeneity with Dart/Python/Julia"; Python's `vector_index` is its own documented quirk).

### 9. `bindings/js/src/database.ts`: declarations and the moved type

**9a.** Imports. Currently:

```ts
import type { GroupColumns } from "./group-columns.ts";
...
import type { TimeSeriesData } from "./time-series.ts";
```

Change the first to `import type { GroupColumns, TimeSeriesData } from "./group-columns.ts";` and delete the second.

**9b.** In the `// --- Reads (implemented in read.ts) ---` block, right after

```ts
  declare readSetStringsById: (collection: string, attribute: string, id: number) => string[];
```

(currently ~L156), insert:

```ts
  declare readVectorGroupById: (
    collection: string,
    group: string,
    id: number,
  ) => Record<string, number | string | null>[];
  declare readSetGroupById: (
    collection: string,
    group: string,
    id: number,
  ) => Record<string, number | string | null>[];
```

### 10. `bindings/js/src/index.ts`: export the moved type from its new home

Currently:

```ts
export type { GroupColumns } from "./group-columns.ts";
...
export type { TimeSeriesData } from "./time-series.ts";
```

Change the first to `export type { GroupColumns, TimeSeriesData } from "./group-columns.ts";` and delete the second. `mod.ts` needs no change: it re-exports `TimeSeriesData` by name from `./src/index.ts`.

## Tests

All new tests fail before the fix. In Julia/Python they fail with a wrong row count/pairing, `BoundsError` or `IndexError`; in JS with `TypeError: db.readVectorGroupById is not a function`. The existing dense tests must keep passing unchanged:
- Julia `Vector Group by ID`, `Vector Group by ID Empty` (`test_database_read_vector.jl`) and `Read Set Group By ID` (`test_database_metadata.jl`).
- Python `TestReadVectorGroupById` / `TestReadSetGroupById` (`test_read_vector_group_by_id`, `_empty`, `test_read_set_group_by_id`, `_empty`).
- JS `database-time-series-group.test.ts` and `database-time-series-nulls.test.ts`, which are the regression guard for step 7's refactor of `readTimeSeriesGroup`.

The C++ core and C API are already covered, and plan 19 owns the missing C set test. Lua and Dart have no behaviour change.

### Julia

**`bindings/julia/test/test_database_read_vector.jl`**: inside `@testset "Read Vector"`, right after `@testset "Vector Group by ID Empty" ... end` (currently ~L229-239), add:

```julia
    @testset "Vector Group by ID Keeps NULL Cells In Place" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_column_groups.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Items"; label = "Item 1")

        # NULL in the column read last: the per-column composition paired (1.5, 20.5) and lost a row.
        Quiver.update_vector_group!(db, "Items", "readings", id; amount = [1.5, 2.5], score = [nothing, 20.5])
        rows = Quiver.read_vector_group_by_id(db, "Items", "readings", id)
        @test length(rows) == 2
        @test rows[1]["amount"] == 1.5
        @test rows[1]["score"] === nothing
        @test rows[2]["amount"] == 2.5
        @test rows[2]["score"] == 20.5

        # NULL in the column read first: the per-column composition threw BoundsError.
        Quiver.update_vector_group!(db, "Items", "readings", id; amount = [nothing, 2.5], score = [10.5, 20.5])
        rows = Quiver.read_vector_group_by_id(db, "Items", "readings", id)
        @test length(rows) == 2
        @test rows[1]["amount"] === nothing
        @test rows[1]["score"] == 10.5
        @test rows[2]["amount"] == 2.5
        @test rows[2]["score"] == 20.5

        Quiver.close!(db)
    end

    @testset "Vector Group by ID Parses DateTime Columns" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_column_groups.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Items"; label = "Item 1")
        Quiver.update_vector_group!(db, "Items", "events", id;
            date_event = [DateTime(2024, 1, 15, 10, 30, 0), nothing, DateTime(2024, 3, 1)],
            note = [nothing, "second", "third"],
        )

        rows = Quiver.read_vector_group_by_id(db, "Items", "events", id)
        @test length(rows) == 3
        @test rows[1]["date_event"] == DateTime(2024, 1, 15, 10, 30, 0)
        @test rows[1]["note"] === nothing
        @test rows[2]["date_event"] === nothing
        @test rows[2]["note"] == "second"
        @test rows[3]["date_event"] == DateTime(2024, 3, 1)
        @test rows[3]["note"] == "third"

        Quiver.close!(db)
    end
```

(`using Dates` is already at the top of that file. `update_vector_group!` writes a `DateTime` cell as its full ISO string.)

**`bindings/julia/test/test_database_read_set.jl`**: inside `@testset "Read Set"`, right after `@testset "Set Group Columns Pair By Row" ... end` (currently ~L244-264), add:

```julia
    @testset "Set Group by ID Keeps NULL Cells In Place" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_column_groups.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Items"; label = "Item 1")
        Quiver.update_set_group!(db, "Items", "codes", id;
            code = ["alpha", nothing, "mu"],
            weight = [1.5, 2.5, nothing],
        )

        rows = Quiver.read_set_group_by_id(db, "Items", "codes", id)
        @test length(rows) == 3
        # A set's row order is unspecified: compare the (code, weight) pairs, not positions.
        @test Set((row["code"], row["weight"]) for row in rows) ==
              Set([("alpha", 1.5), (nothing, 2.5), ("mu", nothing)])

        Quiver.close!(db)
    end
```

**`bindings/julia/test/test_database_update.jl`**, `@testset "Vector And Set Group Writers"` (currently ~L929-933). Replace

```julia
        # `nothing` cells become SQL NULL (asserted in SQL: the per-column reader drops NULLs).
        Quiver.update_vector_group!(db, "Child", "refs", child; parent_ref = [parent_a, nothing, parent_b])
        @test Quiver.query_integer(db, "SELECT COUNT(*) FROM Child_vector_refs WHERE id = ?", [child]) == 3
        @test Quiver.query_integer(
            db, "SELECT COUNT(*) FROM Child_vector_refs WHERE id = ? AND parent_ref IS NULL", [child]) == 1
```

with

```julia
        # `nothing` cells become SQL NULL, and the whole-group reader keeps them in position.
        Quiver.update_vector_group!(db, "Child", "refs", child; parent_ref = [parent_a, nothing, parent_b])
        @test [row["parent_ref"] for row in Quiver.read_vector_group_by_id(db, "Child", "refs", child)] ==
              [parent_a, nothing, parent_b]
```

Keep the later `@test Quiver.query_integer(db, "SELECT COUNT(*) FROM Child_vector_refs WHERE id = ?", [child]) == 3` at the end of that testset. It checks that the rejected writes left the three rows intact, and it still holds.

### Python

**`bindings/python/tests/test_database_read_vector.py`**, class `TestReadVectorGroupById`: add after `test_read_vector_group_by_id_empty` (`datetime`, `timezone` are already imported there; `multi_column_groups_db` is a `conftest.py` fixture):

```python
    def test_null_in_last_column_keeps_rows_aligned(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        # The per-column composition paired (1.5, 20.5) and lost a row.
        db.update_vector_group("Items", "readings", item, {"amount": [1.5, 2.5], "score": [None, 20.5]})

        assert db.read_vector_group_by_id("Items", "readings", item) == [
            {"vector_index": 0, "amount": 1.5, "score": None},
            {"vector_index": 1, "amount": 2.5, "score": 20.5},
        ]

    def test_null_in_first_column_keeps_rows_aligned(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        # The per-column composition raised IndexError.
        db.update_vector_group("Items", "readings", item, {"amount": [None, 2.5], "score": [10.5, 20.5]})

        assert db.read_vector_group_by_id("Items", "readings", item) == [
            {"vector_index": 0, "amount": None, "score": 10.5},
            {"vector_index": 1, "amount": 2.5, "score": 20.5},
        ]

    def test_parses_date_time_columns(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        db.update_vector_group(
            "Items",
            "events",
            item,
            {"date_event": ["2024-01-15T10:30:00", None, "2024-03-01"], "note": [None, "second", "third"]},
        )

        assert db.read_vector_group_by_id("Items", "events", item) == [
            {"vector_index": 0, "date_event": datetime(2024, 1, 15, 10, 30, tzinfo=timezone.utc), "note": None},
            {"vector_index": 1, "date_event": None, "note": "second"},
            {"vector_index": 2, "date_event": datetime(2024, 3, 1, tzinfo=timezone.utc), "note": "third"},
        ]
```

**`bindings/python/tests/test_database_read_set.py`**, class `TestReadSetGroupById`: add after `test_read_set_group_by_id_empty`:

```python
    def test_null_cells_keep_rows_aligned(self, multi_column_groups_db: Database) -> None:
        db = multi_column_groups_db
        db.create_element("Configuration", label="Config")
        item = db.create_element("Items", label="item1")
        db.update_set_group("Items", "codes", item, {"code": ["alpha", None, "mu"], "weight": [1.5, 2.5, None]})

        rows = db.read_set_group_by_id("Items", "codes", item)
        assert len(rows) == 3
        # A set's row order is unspecified: compare the (code, weight) pairs, not positions.
        assert {(row["code"], row["weight"]) for row in rows} == {("alpha", 1.5), (None, 2.5), ("mu", None)}
```

**`bindings/python/tests/test_database_update.py`**, `TestUpdateVectorSetGroup.test_accepts_null_cells` (currently ~L387-403). Replace the body after the `update_vector_group` call:

```python
        # Asserted in SQL, not through read_vector_group_by_id: Python composes that from
        # per-column reads, which drop NULL cells (the documented null-dropping caveat - only
        # Dart binds the NULL-preserving native reader).
        assert (
            relations_db.query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = ?", parameters=[child]) == 3
        )
        assert (
            relations_db.query_integer(
                "SELECT COUNT(*) FROM Child_vector_refs WHERE id = ? AND parent_ref IS NULL", parameters=[child]
            )
            == 1
        )
```

with

```python
        rows = relations_db.read_vector_group_by_id("Child", "refs", child)
        assert [row["parent_ref"] for row in rows] == [1, None, 2]
```

(`_seed` creates Parent A = id 1 and Parent B = id 2, and the write is `{"parent_ref": [1, None, 2]}`.)

### JS

**`bindings/js/test/database-read-vector.test.ts`**. After `const SCHEMA_PATH = ...` (currently L8), add:

```ts
const MULTI_COLUMN_SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "multi_column_groups.sql",
);
```

Then append at the end of the file:

```ts
describe("readVectorGroupById", () => {
  function openItem(): { db: Database; id: number } {
    const db = Database.fromSchema(":memory:", MULTI_COLUMN_SCHEMA_PATH);
    db.createElement("Configuration", { label: "Config" });
    const id = db.createElement("Items", { label: "Item1" });
    return { db, id };
  }

  test("returns one row per vector_index with NULL cells in place", () => {
    const { db, id } = openItem();
    try {
      db.updateVectorGroup("Items", "readings", id, { amount: [1.5, 2.5], score: [null, 20.5] });
      expect(db.readVectorGroupById("Items", "readings", id)).toEqual([
        { amount: 1.5, score: null },
        { amount: 2.5, score: 20.5 },
      ]);

      db.updateVectorGroup("Items", "readings", id, { amount: [null, 2.5], score: [10.5, 20.5] });
      expect(db.readVectorGroupById("Items", "readings", id)).toEqual([
        { amount: null, score: 10.5 },
        { amount: 2.5, score: 20.5 },
      ]);
    } finally {
      db.close();
    }
  });

  test("keeps DATE_TIME cells as strings and NULL strings as null", () => {
    const { db, id } = openItem();
    try {
      db.updateVectorGroup("Items", "events", id, {
        date_event: ["2024-01-15T10:30:00", null, "2024-03-01"],
        note: [null, "second", "third"],
      });
      expect(db.readVectorGroupById("Items", "events", id)).toEqual([
        { date_event: "2024-01-15T10:30:00", note: null },
        { date_event: null, note: "second" },
        { date_event: "2024-03-01", note: "third" },
      ]);
    } finally {
      db.close();
    }
  });

  test("returns [] for an element with no rows", () => {
    const { db, id } = openItem();
    try {
      expect(db.readVectorGroupById("Items", "readings", id)).toEqual([]);
    } finally {
      db.close();
    }
  });

  test("throws on an unknown group", () => {
    const { db, id } = openItem();
    try {
      expect(() => db.readVectorGroupById("Items", "nope", id)).toThrow(/Vector group not found/);
    } finally {
      db.close();
    }
  });
});
```

**`bindings/js/test/database-read-set.test.ts`** (`MULTI_COLUMN_SCHEMA_PATH` already exists at L10). Append at the end of the file:

```ts
describe("readSetGroupById", () => {
  test("returns every row with NULL cells in place", () => {
    const db = Database.fromSchema(":memory:", MULTI_COLUMN_SCHEMA_PATH);
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Items", { label: "Item1" });
      db.updateSetGroup("Items", "codes", id, {
        code: ["alpha", null, "mu"],
        weight: [1.5, 2.5, null],
      });

      const rows = db.readSetGroupById("Items", "codes", id);
      // A set's row order is unspecified: check membership, not positions.
      expect(rows).toHaveLength(3);
      expect(rows).toContainEqual({ code: "alpha", weight: 1.5 });
      expect(rows).toContainEqual({ code: null, weight: 2.5 });
      expect(rows).toContainEqual({ code: "mu", weight: null });
    } finally {
      db.close();
    }
  });

  test("returns [] for an element with no rows", () => {
    const db = Database.fromSchema(":memory:", MULTI_COLUMN_SCHEMA_PATH);
    try {
      db.createElement("Configuration", { label: "Config" });
      const id = db.createElement("Items", { label: "Item1" });
      expect(db.readSetGroupById("Items", "codes", id)).toEqual([]);
    } finally {
      db.close();
    }
  });
});
```

**`bindings/js/test/database-update.test.ts`**, test `"writes null cells as SQL NULL"` (currently ~L200-217). Replace

```ts
      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA, null, parentB] });
      // Asserted in SQL: the per-column reader drops NULL cells.
      expect(
        db.queryInteger("SELECT COUNT(*) FROM Child_vector_refs WHERE id = ?", [child]),
      ).toEqual(3);
      expect(
        db.queryInteger(
          "SELECT COUNT(*) FROM Child_vector_refs WHERE id = ? AND parent_ref IS NULL",
          [child],
        ),
      ).toEqual(1);
```

with

```ts
      db.updateVectorGroup("Child", "refs", child, { parent_ref: [parentA, null, parentB] });
      expect(db.readVectorGroupById("Child", "refs", child)).toEqual([
        { parent_ref: parentA },
        { parent_ref: null },
        { parent_ref: parentB },
      ]);
```

### C++ / C API / Lua / Dart

No new tests. The new schema table has to leave every existing suite green: `tests/test_database_update.cpp` (`MultiColumnGroupFixture`), `tests/test_database_read_set.cpp`, `tests/test_c_api_database_update.cpp` (`UpdateGroupNullStringEntryIsNull`), `bindings/dart/test/database_read_set_test.dart`, and the Julia/Python/JS set-pairing tests all load `multi_column_groups.sql`.

## Docs and changelog

### Root `AGENTS.md`

1. Design decision (currently ~L67-68). Old:
   > - **The group writers are column-oriented while the group readers are row-oriented** in Dart and
   >   Python (`read_vector_group_by_id` returns rows; Python even adds a synthetic 0-based
   >   `vector_index`).

   New:
   > - **The group writers are column-oriented while the group readers are row-oriented** in every
   >   FFI binding (`read_vector_group_by_id` returns rows in Julia, Dart, Python and JS; Python even
   >   adds a synthetic 0-based `vector_index`).

   Keep the rest of that bullet ("The only asymmetric reader/writer pair in those bindings, ...").

2. Core API "Whole-group readers" bullet (currently ~L582-583). Old:
   > C API mirrors
   > `read_time_series_group`'s columnar+mask shape (freed by `free_time_series_data`); Dart binds
   > them natively; Julia/Python still compose per-column reads (null-dropping caveat applies there).

   New:
   > C API mirrors
   > `read_time_series_group`'s columnar+mask shape (freed by `free_time_series_data`). Julia, Dart,
   > Python and JS all call it and return rows: a SQL NULL is `nothing`/`null`/`None`/`null`, a
   > DATE_TIME column is parsed except in JS (string datetime surface), and Python adds a synthetic
   > 0-based `vector_index`. Lua does not bind them (design decision).

3. Representative Cross-Layer Examples table: after the row `| Set group update by label | ... |` (currently ~L696), add:
   ```
   | Vector group read | `read_vector_group_by_id()` | `quiver_database_read_vector_group_by_id()` | `read_vector_group_by_id()` | `readVectorGroupById()` | N/A (not bound — design decision) |
   | Set group read | `read_set_group_by_id()` | `quiver_database_read_set_group_by_id()` | `read_set_group_by_id()` | `readSetGroupById()` | N/A (not bound — design decision) |
   ```

4. Delete the whole "Multi-column group readers" block at the end of the file (currently ~L807-813: the blank line, `**Multi-column group readers (Julia, Dart, and Python):**`, and the 4-line table). It sits under "Binding-Only Convenience Methods ... These have no direct C++ or C API counterpart", which these readers contradict, and item 3 replaces it. Make sure the file still ends with a single newline after the "Scoped resource factories" paragraph.

### `bindings/julia/AGENTS.md`

After the "**One marshaller for every row upsert**" bullet (currently ~L79-84), add:

> - **The whole-group readers call the native C readers**: `read_vector_group_by_id` /
>   `read_set_group_by_id` are one-line wrappers over `_read_group_rows(db, read_group, ...)`
>   (`src/database_read.jl`), which takes the C entry point as `_update_group_columns` does, decodes
>   the columnar typed arrays + per-cell mask into `Vector{Dict{String, Any}}` rows (masked cell →
>   `nothing`, DATE_TIME column → `DateTime` via `string_to_date_time`, never `unsafe_string` on a
>   masked-out pointer) and frees with `quiver_database_free_time_series_data` in a `finally`. They
>   used to zip the NULL-dropping per-column `_by_id` readers with the row count taken from the last
>   column, so one NULL cell mis-paired rows or threw `BoundsError`. `read_time_series_group` keeps its
>   own decode on purpose: it returns columns and parses only the dimension column, by name.

### `bindings/python/AGENTS.md`

1. In the `_marshal_group_columns` bullet (currently ~L89-94), old last sentence:
   > Note that the group
   >   *writers* take columns while `read_vector_group_by_id` returns rows, and that reader composes
   >   per-column reads, so it **drops NULL cells** — assert a NULL-cell write in SQL, not through it.

   New:
   > Note that the group
   >   *writers* take columns while `read_vector_group_by_id` / `read_set_group_by_id` return rows (the
   >   vector form adds a synthetic 0-based `vector_index`); both read a NULL cell back as `None` in its
   >   row, so assert a NULL-cell write through them.

2. Add a bullet right after it:
   > - **`_decode_group_rows` is the one decoder for the two whole-group readers** — a module-level data
   >   codec like `_marshal_group_columns` (Dart's `_decodeGroupRows`), not the closure-parameterized FFI
   >   helper the root "Do not 'fix'" list forbids: each reader keeps its own expanded FFI call block
   >   and hands the out-params over. It maps mask 0 to `None` (never `ffi.string` a masked-out NULL
   >   `char*`), parses DATE_TIME columns with `_parse_datetime`, and frees with
   >   `quiver_database_free_time_series_data` in a `finally`. `read_time_series_group` keeps its own
   >   loop (columns, dimension-only parsing).

### `bindings/js/AGENTS.md`

1. Layout (currently L16). Old: `src/group-columns.ts # Shared columnar marshaller for the group writers (by id and by label)`. New: `src/group-columns.ts # Shared columnar marshaller (group writers) and decoder (group readers)`.
2. In the "**`src/group-columns.ts` is the one columnar marshaller**" bullet (currently ~L70-77), append:
   > Its read-side twin, `readGroupColumns(handle, readGroup, collection, group, id)`, is the one
   > decoder of the columnar + per-cell-mask read result, and frees it in a `finally`:
   > `readTimeSeriesGroup` returns its columns as they are, and `readVectorGroupById` /
   > `readSetGroupById` (`read.ts`) transpose them into `Record<string, number | string | null>[]`
   > rows — rows like the other three bindings (root design decision), with DATE_TIME left as the
   > stored string and no synthetic `vector_index`.
3. In the boolean bullet (currently ~L113-116), old:
   > **`GroupColumns` is the write type and
   >   `TimeSeriesData` the read type** — they are otherwise identical, but only the former admits
   >   `boolean`, since `readTimeSeriesGroup` never produces one and its return type should not claim
   >   it.

   New:
   > **`GroupColumns` is the write type and
   >   `TimeSeriesData` the read type** (both in `group-columns.ts`) — they are otherwise identical, but
   >   only the former admits `boolean`, since no group reader produces one and its return type should
   >   not claim it.

### `tests/AGENTS.md`

In the `multi_column_groups.sql` bullet (currently ~L171-175), old:
> `Items_vector_readings` (`amount`, `score`) and `Items_set_codes` (`code`, `weight`),
>     both nullable,

New:
> `Items_vector_readings` (`amount`, `score`) and `Items_set_codes` (`code`, `weight`),
>     both nullable, plus `Items_vector_events` (`date_event` DATE_TIME, `note` TEXT, both nullable)
>     for the whole-group readers' DATE_TIME parsing and NULL string cells,

### `bindings/js/README.md`

Under `### Read (by ID)`, after `- readSetStringsById(collection, attribute, id) -- Read string set` (currently ~L105), add:

```
- `readVectorGroupById(collection, group, id)` -- Read a whole vector group as rows (`null` for a SQL NULL cell)
- `readSetGroupById(collection, group, id)` -- Read a whole set group as rows (`null` for a SQL NULL cell)
```

### `bindings/js/src/lua-api.ts`

No edit. Its "do that read in the host binding" (~L391) and "the row-aligned whole-group readers the other bindings have" (~L856) become true for JS with this change. Plans 43/44 own that file.

### `CHANGELOG.md`, under `## [0.11.0] — unreleased`

Add an `### Added` section **above** `### Changed` if no earlier plan created one (Keep a Changelog order: Added, Changed, …, Fixed), then append:

```markdown
- **JS: `readVectorGroupById()` / `readSetGroupById()`.** The whole-group readers Julia, Dart and
  Python already had. Each returns one record per row, `Record<string, number | string | null>[]`:
  a SQL NULL cell is `null` in its row, and a DATE_TIME cell stays an ISO 8601 string, as in every
  JS reader. Use them to read a group with more than one column when one of them is nullable:
  `readVectorFloatsById` and the other per-column readers drop NULL cells, so zipping them
  mis-pairs rows.
```

Append to `### Fixed`:

```markdown
- **Julia and Python: `read_vector_group_by_id` / `read_set_group_by_id` keep NULL cells in
  place.** Both built their rows by zipping the per-column `_by_id` readers, which drop NULL
  cells, and took the row count from the last column. One NULL cell therefore paired values that
  were never written together and lost a row, or raised `BoundsError` / `IndexError`. They now
  call the native C reader: a NULL cell is `nothing` / `None` in its row, and a group without NULL
  cells reads back exactly as before. A row that used to vanish now appears with `nothing` /
  `None` in its NULL column, and Python's synthetic `vector_index` counts it.
```

## Verification

From the repo root (Git Bash paths shown; the `.bat` files also run from PowerShell/cmd):

1. `cmake --build build --config Debug`. No C++ changed, but this keeps `build/bin` DLLs current for the binding suites.
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`. Both suites must be fully green, which proves the appended table breaks nothing that loads `multi_column_groups.sql`. Quick subset: `./build/bin/quiver_tests.exe --gtest_filter="*Group*:*ReadSetByIdOrderMatchesGroupReader*"` and `./build/bin/quiver_c_tests.exe --gtest_filter="*Group*"`.
3. Optional TDD check: write the tests from "Tests" first and run them against the unchanged bindings. Expect the failures described there: Julia wrong length/pairing and a `BoundsError`, Python assertion failures and an `IndexError`, JS `readVectorGroupById is not a function`.
4. `bindings/julia/test/test.bat`. All green, including the new `Vector Group by ID Keeps NULL Cells In Place`, `Vector Group by ID Parses DateTime Columns`, `Set Group by ID Keeps NULL Cells In Place` and the rewritten `Vector And Set Group Writers`.
5. `bindings/python/tests/test.bat`. All green. If the *whole* suite errors at collection, look at the step 3 cdef first: a typo there breaks `import quiverdb`. Targeted: `bindings/python/tests/test.bat -k "group_by_id or keep_rows_aligned or parses_date_time_columns or accepts_null_cells"`.
6. `bindings/js/test/test.bat`. All green, including `database-time-series-group.test.ts` / `database-time-series-nulls.test.ts` (regression for step 7). Targeted: `bindings/js/test/test.bat -t "GroupById|writes null cells"`. Then `cd bindings/js && bun run lint`: no new diagnostics in `src/group-columns.ts`, `src/time-series.ts`, `src/read.ts`, `src/database.ts`, `src/index.ts` (an unused import in `time-series.ts` means step 7d is incomplete). Pre-existing debt in untouched files is not yours to fix.
7. `bindings/dart/test/test.bat`. All green. Dart code is unchanged; this confirms the schema change is harmless there.
8. `scripts/format.bat`, then re-run steps 4–6 if the formatter changed anything.
9. `scripts/test-all.bat`. The six suites must PASS. Its CLI smoke step (`[7/7]`) references the deleted `example/example1.lua` and fails at HEAD for reasons unrelated to this plan (plan 65 fixes it). Report it if it is still failing, but do not fix it here.

## Acceptance criteria

- [ ] `tests/schemas/valid/multi_column_groups.sql` ends with the `Items_vector_events` table. The existing two tables are byte-identical.
- [ ] Julia `read_vector_group_by_id` / `read_set_group_by_id` are one-line calls to `_read_group_rows`, which calls the C reader, maps mask 0 → `nothing`, parses DATE_TIME and frees in `finally`. No `get_vector_metadata` / `get_set_metadata` / per-column `_by_id` call remains in either.
- [ ] Julia `read_time_series_group` is unchanged. `bindings/julia/src/c_api.jl` is unchanged (no regeneration).
- [ ] `_c_api.py` declares `quiver_database_read_vector_group_by_id` and `quiver_database_read_set_group_by_id`, matching the header.
- [ ] Python readers each have their own expanded FFI call block and share `_decode_group_rows` (mask → `None`, DATE_TIME parsed, free in `finally`). The vector reader still adds a 0-based `vector_index`. `read_time_series_group` is unchanged.
- [ ] JS `loader.ts` has both symbols. `readGroupColumns` in `group-columns.ts` is the only columnar read decoder. `readTimeSeriesGroup` is a one-line call to it, and its output is unchanged. `readVectorGroupById` / `readSetGroupById` return `Record<string, number | string | null>[]`, with DATE_TIME as a string and no `vector_index`. Both are declared on `Database`.
- [ ] `TimeSeriesData` is defined in `group-columns.ts`, exported from `src/index.ts`, and still exported by name from `mod.ts`.
- [ ] New tests exist and pass in Julia (3 testsets), Python (4 methods), and JS (6 tests). The three SQL-workaround assertions (Julia `test_database_update.jl`, Python `test_accepts_null_cells`, JS "writes null cells as SQL NULL") now go through the reader.
- [ ] The C++, C API, Dart, Julia, Python and JS suites are all green. `bun run lint` is clean for the touched files. `scripts/format.bat` was applied.
- [ ] Root, Julia, Python, JS and tests AGENTS.md are updated as specified. The "Multi-column group readers" block is gone, and two rows were added to the cross-layer table. The JS README has the two lines.
- [ ] CHANGELOG has the `### Added` (JS) and `### Fixed` (Julia/Python) entries under 0.11.0. Neither is marked BREAKING.

## Pitfalls

- **Never decode a masked-out string cell.** The C API writes a NULL `char*` there. `unsafe_string(C_NULL)` in Julia, `ffi.string(ffi.NULL)` in Python and `new CString(0)` in JS all fail or misbehave. The mask test must come first, as written.
- **Free in `finally`, not after the loop.** `string_to_date_time` / `_parse_datetime` throw on a malformed stored date, which is reachable because the core's write gate only fires on `date_`-prefixed columns. A post-loop free would leak. Don't copy the non-`finally` shape of Julia's `read_time_series_group` (plan 36) or Dart's `_decodeGroupRows` (plan 40).
- **Don't fold `read_time_series_group` into the new Julia/Python decoders.** It parses by column *name* and returns other DATE_TIME columns as strings, so sharing would change its return types. Only JS shares, because JS parses nothing.
- **Python house style.** Don't turn `_decode_group_rows` into a helper that makes the FFI call through a passed-in `lib` function. That is the closure-parameterized FFI helper on the root "Do Not Fix" list. Julia is different: its own `_update_group_columns` convention takes the C function, so `_read_group_rows` does too.
- **JS name collision.** `group-columns.ts` imports `read` from `bun:ffi` for `read.ptr`. Name the C-function parameter `readGroup`, never `read`. For the same reason, don't name the Julia parameter `read`: it would shadow `Base.read`.
- **JS out-params.** Pass the `.buf` TypedArrays to the FFI call and read them with `readPtrOut` / `readUint64Out` after the call. Never precompute `ptr(...)`: that is the documented Bun relocation bug in `bindings/js/AGENTS.md`. The moved code already does this, so keep it verbatim.
- **JS `TimeSeriesData` move.** Three import sites change (`time-series.ts`, `database.ts`, `index.ts`). Miss one and Bun still runs, because types are erased and nothing type-checks this repo, but the source is wrong. Grep `TimeSeriesData` across `bindings/js` after the edit. `mod.ts` needs no edit.
- **Unused imports in `time-series.ts`.** Plan 17 edits the same file first. Remove `toArrayBuffer` / `decodePtrArray` / `DATA_TYPE_DATE_TIME` only if the grep shows no remaining use.
- **Set tests.** Never assert a literal set row order. It is "consistent, otherwise unspecified" by root design decision.
- **Shared schema.** `multi_column_groups.sql` is loaded by C++, C API, Dart, Julia, Python and JS tests. Only append; don't rename or reorder existing columns. The new table makes `Items` have two vector groups, so re-grep for any test asserting the group count on this schema.
- **JS number typing on write.** `updateGroupColumns` tags a column of integral numbers (e.g. `[null, 20.0]`) INTEGER. That is fine for a REAL column (int-for-REAL coercion), but the tests use `.5` values so they don't depend on it. Keep them that way.
- **Line endings.** Every file touched is LF (`.jl`, `.py`, `.ts`, `.sql`, `.md`). No `.bat` file is edited. If you run any unix tool over a `.bat`, restore CRLF.
- **No generators, no Dart cache clear.** The C API is unchanged, so don't run `scripts/generator.bat` and don't clear `.dart_tool`.

## Out of scope

- C API test for `quiver_database_read_set_group_by_id`: plan 19.
- `marshal_group_rows_to_c` narrowing a REAL cell into an INTEGER column: plan 23.
- Julia `read_time_series_group` free in `finally`: plan 36. Dart `_decodeGroupRows` free in `finally`: plan 40.
- Renaming `read_{vector,set}_date_time_by_id`: plan 42.
- Stale "not positionally aligned" comments elsewhere in the bindings: plan 41.
- Full JS README method/type listing: plan 74 (only the two new methods are added here).
- Python/Julia/JS `read_vectors_by_id` / `read_sets_by_id` composites. They still read per column and drop NULL cells, as documented. Lua whole-group readers: not bound, by design decision.
- The C API group readers call `get_{vector,set}_metadata` and then the C++ reader, which looks the metadata up again. That redundancy is harmless and not touched, since the C API is unchanged here.
- A Dart test over the new `Items_vector_events` table: Dart's decoder is unchanged by this plan.
