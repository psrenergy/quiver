---
phase: 03-language-bindings-documentation
reviewed: 2026-05-20T00:00:00Z
depth: standard
files_reviewed: 14
files_reviewed_list:
  - CLAUDE.md
  - bindings/dart/lib/src/database_update.dart
  - bindings/dart/lib/src/ffi/bindings.dart
  - bindings/dart/test/database_time_series_test.dart
  - bindings/julia/src/c_api.jl
  - bindings/julia/src/database_update.jl
  - bindings/julia/test/test_database_time_series.jl
  - bindings/python/src/quiverdb/_c_api.py
  - bindings/python/src/quiverdb/database.py
  - bindings/python/tests/conftest.py
  - bindings/python/tests/test_database_time_series.py
  - docs/time_series.md
  - src/lua_runner.cpp
  - tests/test_lua_runner.cpp
findings:
  critical: 4
  warning: 10
  info: 2
  total: 16
status: issues_found
---

# Phase 03: Code Review Report

**Reviewed:** 2026-05-20T00:00:00Z
**Depth:** standard
**Files Reviewed:** 14
**Status:** issues_found

## Summary

This phase adds `add_time_series_row` to four bindings (Julia, Dart, Python, Lua), plus the Lua C++ binding and test coverage. The bindings follow the sibling `update_time_series_group` patterns and are mostly mechanical.

Key concerns:

1. **`docs/time_series.md` documents three Julia functions that do not exist in the codebase** (`read_time_series_table`, `update_time_series_row!`, `delete_time_series!`) and a wrong signature for `read_time_series_row`. The doc is stale to the point of being misleading. This is the highest-severity finding.
2. **Cross-binding inconsistency for `Int -> Float` coercion**: Julia auto-coerces, Python/Dart/Lua reject. The CLAUDE.md "Homogeneity" principle calls this out explicitly ("API surface should feel uniform across wrappers").
3. **Python `add_time_series_row` rejects `bool` values** (because `type(v) is int` is `False` for `bool`), inconsistent with Python's `_marshal_params` which uses `isinstance(p, int)` and explicitly notes `bool is subclass of int, handled here`. The strict-type behavior differs across two write paths in the same file.
4. **Julia `update_time_series_files!` has a `GC.@preserve` scope bug**: the preserve block does not include the `column_ptrs` `Vector{Cchar}` whose `pointer()` is passed to the C call. Under GC pressure between line 230 (vector construction) and line 232 (C call) the vector could be collected. The earlier `update_time_series_group!` correctly preserves `name_cstrs`/`name_ptrs`; this one omits the equivalent. Pre-existing code, but the phase doesn't fix it and it sits next to the new wrappers.
5. Lua tests for `add_time_series_row` exercise insert / upsert / multi-dim, but omit the missing-required-dimension error case that the Julia/Dart/Python suites all cover.

The new wrappers themselves (`addTimeSeriesRow`, `add_time_series_row!`, `add_time_series_row`, `add_time_series_row_lua`) look correct structurally and would pass round-trip happy-path tests.

## Critical Issues

### CR-01: docs/time_series.md references three Julia functions that do not exist

**File:** `docs/time_series.md:163-170, 215-225, 245-252`
**Issue:** The doc shows three function calls that have no implementation anywhere in `bindings/julia/src/`:

- `Quiver.read_time_series_table(db, "Resource", "some_vector1", "Resource 1")` (line 164)
- `Quiver.update_time_series_row!(db, "Resource", "some_vector3", "Resource 1", 10.0; date_time=..., block=1)` (line 216)
- `Quiver.delete_time_series!(db, "Resource", "group1", "Resource 1")` (line 246)

A repo-wide grep for these names returns hits only in `docs/time_series.md` and `.planning/`. They are vaporware in the documentation. The real Julia API uses `read_time_series_group(db, collection, group, id)` and `update_time_series_group!(db, collection, group, id; kwargs...)`.

Additionally, `read_time_series_row` is documented with the wrong signature on line 178:
```julia
values = Quiver.read_time_series_row(db, "Resource", "some_vector1", Float64; date_time = DateTime(2020))
```
The actual signature in `bindings/julia/src/database_read.jl:582` is `read_time_series_row(db::Database, collection::String, group::String, attribute::String; date_time::DateTime)` — there is no positional type argument. Users following the doc will get a `MethodError`.

A user trying to use these would hit `UndefVarError` immediately. The doc was the entry point for the phase work but was not updated alongside the API.

**Fix:** Rewrite `docs/time_series.md` against the actual API. Concretely:
1. Replace `read_time_series_table` examples with `read_time_series_group` (returns `Dict{String, Vector}` of columnar data, not a `DataFrame`).
2. Replace `update_time_series_row!` examples with `add_time_series_row!` (the new function this phase introduces) or `update_time_series_group!` for full replacement.
3. Replace `delete_time_series!` with `update_time_series_group!(db, col, grp, id)` (empty kwargs clears the rows for one element) — or document that no whole-group delete primitive exists.
4. Fix the `read_time_series_row` signature to drop the positional type argument.
5. Drop the "DataFrame on `create_element!`" example (lines 86-117) unless `create_element!` actually accepts DataFrames for time series tables — the current Element class does not.

---

### CR-02: Python `add_time_series_row` rejects `bool` values; inconsistent with `_marshal_params` in the same file

**File:** `bindings/python/src/quiverdb/database.py:1214-1232`
**Issue:** The type dispatch uses `type(v) is int`, `type(v) is float`, `type(v) is str`. Because `bool` is a subclass of `int`, `type(True) is int` returns `False`, so a row like `{date_time: "...", flag: True}` raises `TypeError("...unsupported type bool; expected int, float, or str")` from line 1232.

Compare with `_marshal_params` on line 1512 in the same file:
```python
elif isinstance(p, int):  # bool is subclass of int, handled here
```
which explicitly handles bool-as-int. Two write paths, two different rules.

This also means `numpy.int64`, `numpy.float64`, etc. are rejected even though they would be acceptable everywhere else in the binding (`create_element` via Element accepts them).

**Fix:** Mirror the `_marshal_params` pattern — use `isinstance` with explicit ordering. `bool` should be tested before `int` if you want to reject it intentionally; otherwise allow it as integer:

```python
for i, (name, v) in enumerate(kwargs.items()):
    name_buf = ffi.new("char[]", name.encode("utf-8"))
    keepalive.append(name_buf)
    c_col_names[i] = name_buf

    if isinstance(v, bool):  # explicit, before int (bool is int subclass)
        arr = ffi.new("int64_t[]", [int(v)])
        keepalive.append(arr)
        c_col_types[i] = DataType.INTEGER
        c_col_data[i] = ffi.cast("void*", arr)
    elif isinstance(v, int):
        arr = ffi.new("int64_t[]", [v])
        keepalive.append(arr)
        c_col_types[i] = DataType.INTEGER
        c_col_data[i] = ffi.cast("void*", arr)
    elif isinstance(v, float):
        ...
```

Decide deliberately whether `bool` is an error or coerces to INTEGER, and pick one — and document the choice. The current "rejects bool but the rest of the binding accepts it" is the worst of both worlds.

---

### CR-03: Cross-binding inconsistency on Int -> Float coercion (homogeneity violation)

**Files:**
- `bindings/julia/src/database_update.jl:159-171` (Julia: auto-coerces)
- `bindings/dart/lib/src/database_update.dart:178-187` (Dart: no coercion)
- `bindings/python/src/quiverdb/database.py:1214-1232` (Python: strict via `type(v) is int`)
- `src/lua_runner.cpp:809-815` (Lua: passes whatever the table contained — `val.is<int64_t>()` then `val.is<double>()` so `1` integer becomes Integer, `1.0` becomes Float)

**Issue:** Julia checks the schema metadata and promotes `Int` to `Float64` when the column type is FLOAT:

```julia
if schema_type == C.QUIVER_DATA_TYPE_FLOAT
    float_arr = Float64[Float64(v)]
    push!(col_types, Cint(C.QUIVER_DATA_TYPE_FLOAT))
```

Dart, Python, and Lua make no such check. A user writing:

```julia
add_time_series_row!(db, "Sensor", "readings", id; date_time=..., temperature=20)   # works
```

```dart
db.addTimeSeriesRow('Sensor', 'readings', id, {'date_time': '...', 'temperature': 20})   // fails: schema type mismatch
```

```python
db.add_time_series_row("Sensor", "readings", id, date_time="...", temperature=20)   # fails: schema type mismatch
```

```lua
db:add_time_series_row("Sensor", "readings", id, { date_time = "...", temperature = 20 })   -- fails
```

CLAUDE.md explicitly states under Principles: *"Homogeneity: Binding interfaces must be consistent and intuitive. API surface should feel uniform across wrappers."* This is the exact case the principle warns against.

The same divergence exists in the older `update_time_series_group!` / `updateTimeSeriesGroup` / `update_time_series_group` paths (it's pre-existing for the bulk update), so this isn't unique to the new row API — but the new row API is the moment to make a homogeneity call, not propagate the inconsistency.

**Fix:** Pick one rule and apply it everywhere:

Option A (recommended — matches Julia's user-friendly behavior): All four bindings call `get_time_series_metadata` once, build a schema-type lookup, and coerce `int -> double` for FLOAT columns. Add this to Dart's `addTimeSeriesRow`, Python's `add_time_series_row`, and Lua's `add_time_series_row_lua` (the Lua case needs C++-side handling since `lua_table_to_value_map` doesn't know the schema).

Option B (matches CLAUDE.md "Clean code over defensive"): Drop the Julia coercion. Tell users to write `20.0` not `20`. Update the Julia test `Multi-Column Auto-Coercion Int to Float` (line 293) to either be removed or assert the error.

Either way, document the choice in CLAUDE.md alongside the existing "Python's create_element uses **kwargs" note.

---

### CR-04: Julia `update_time_series_files!` has incomplete `GC.@preserve` (pre-existing, but unchanged here)

**File:** `bindings/julia/src/database_update.jl:206-238`
**Issue:** The function builds two heap-allocated vectors (`column_ptrs`, `path_ptrs`) whose `pointer()` is passed to the C call, but the `GC.@preserve` block on line 231 only protects `column_cstrings, path_cstrings`:

```julia
column_cstrings = [Base.cconvert(Cstring, col) for col in columns]
column_ptrs = [Base.unsafe_convert(Cstring, cs) for cs in column_cstrings]
# ...
path_ptrs = Vector{Ptr{Cchar}}(undef, count)
path_cstrings = []
for i in 1:count
    # ...
    path_ptrs[i] = Base.unsafe_convert(Cstring, cs)
end

GC.@preserve column_cstrings path_cstrings begin   # <-- column_ptrs, path_ptrs NOT listed
    check(C.quiver_database_update_time_series_files(
        db.ptr, collection,
        column_ptrs, path_ptrs, Csize_t(count),    # <-- passing them here
    ))
end
```

`column_ptrs` and `path_ptrs` are themselves heap-allocated `Vector` objects. The C call receives a raw pointer into their backing storage. If GC fires between vector construction and the `@ccall`, the backing array can be freed. Julia normally keeps locals alive across `@ccall` boundaries via the calling convention, but `GC.@preserve` is the documented safe pattern and the sibling `update_time_series_group!` (line 111) does include the analog `refs` array.

In `update_time_series_group!` you push every typed buffer into `refs` and preserve it. Here the equivalent vectors are unprotected.

Compare also with how `update_time_series_group!` is structured — it preserves `name_ptrs`, `col_types_arr`, `col_data_arr` via `refs` even though those are also locals.

This function isn't new to this phase, but it sits next to the new wrappers and likely served as a template — the bug should be fixed and any add_time_series_row clones inspected for the same pattern.

**Fix:** Either preserve the pointer vectors explicitly, or use a single `refs` collection in the `update_time_series_group!` style:

```julia
refs = Any[column_cstrings, path_cstrings, column_ptrs, path_ptrs]
GC.@preserve refs begin
    check(C.quiver_database_update_time_series_files(
        db.ptr, collection,
        column_ptrs, path_ptrs, Csize_t(count),
    ))
end
```

Also: line 220 `path_cstrings = []` creates a `Vector{Any}`. Use `path_cstrings = Base.RefValue[]` or `Vector{Any}()` explicitly to avoid type instability and confusion.

## Warnings

### WR-01: Julia `update_time_series_files!` silently no-ops on empty dict (inconsistent with `update_time_series_group!`)

**File:** `bindings/julia/src/database_update.jl:207-209`
**Issue:** `update_time_series_group!` treats empty kwargs as "clear all rows for this element" and calls the C API with NULL/0. `update_time_series_files!` treats empty paths as "do nothing" and returns without calling the C API. Same surface ("an empty container"), opposite semantics.

A user trying to "clear all file paths" by passing an empty dict will get silent success with no state change. There's no way to clear all paths in a single call.

**Fix:** Either:
1. Document the semantic — the docstring should say "Empty `paths` is a no-op; pass each column with `nothing` to clear." Add a comment in the body.
2. Or change to match `update_time_series_group!`: empty dict means "set every column to NULL". But that requires the C API to support that semantic.

The Dart wrapper at `bindings/dart/lib/src/database_update.dart:238-240` has the identical early-return pattern. Apply the same fix.

---

### WR-02: Lua `add_time_series_row` test coverage missing the negative cases

**File:** `tests/test_lua_runner.cpp:1214-1277`
**Issue:** The three Lua `add_time_series_row` tests cover insert, upsert, and multi-dim success. Julia (`test_database_time_series.jl:212-227`), Dart (test file in scope), and Python (`test_database_time_series.py:233-277`) all add a missing-dim or invalid-column negative test. Lua's coverage drops below the others.

The C++ layer throws `"Cannot add_time_series_row: row missing required '<col>' column"`. The Lua wrapper should surface this via `std::runtime_error` -> `lua.safe_script` failure mode -> `Lua error: ...` thrown from `LuaRunner::run`. That path isn't exercised.

**Fix:** Add at least one negative test:

```cpp
TEST_F(LuaRunnerTest, AddTimeSeriesRowMissingDimErrors) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);
    std::string script = R"(
        db:add_time_series_row("Collection", "data", )" +
                         std::to_string(id) + R"(, { value = 10.0 })  -- no date_time
    )";
    EXPECT_THROW({
        try { lua.run(script); }
        catch (const std::runtime_error& e) {
            EXPECT_THAT(e.what(), ::testing::HasSubstr("Cannot add_time_series_row"));
            throw;
        }
    }, std::runtime_error);
}
```

---

### WR-03: Lua `lua_table_to_value_map` silently drops unsupported value types

**File:** `src/lua_runner.cpp:216-232`
**Issue:** The helper used by both `update_time_series_group_lua` and the new `add_time_series_row_lua` walks the table and only inserts entries that match `nil`, `int64_t`, `double`, or `string`:

```cpp
static std::map<std::string, Value> lua_table_to_value_map(const sol::table& t) {
    std::map<std::string, Value> result;
    for (auto& pair : t) {
        auto key = pair.first.as<std::string>();
        sol::object val = pair.second;
        if (val.is<sol::lua_nil_t>()) { ... }
        else if (val.is<int64_t>()) { ... }
        else if (val.is<double>()) { ... }
        else if (val.is<std::string>()) { ... }
        // else: silently dropped
    }
    return result;
}
```

If a user accidentally passes a nested table (e.g., `{ date_time = "...", values = {1.0, 2.0} }` intending a vector), the `values` column is silently dropped. The C++ layer then raises `"Cannot add_time_series_row: row missing required 'values' column"` if `values` is a dimension, or it succeeds with the column treated as NULL — both confusing.

This was acceptable when only used by `update_time_series_group_lua` (the old bulk API which had its own validation), but `add_time_series_row` makes the silent drop more user-visible because typos in single-row calls land here.

**Fix:** Throw on unsupported value types:

```cpp
} else {
    throw std::runtime_error("Cannot lua_table_to_value_map: column '" + key +
                             "' has unsupported Lua type");
}
```

This pattern matches the existing `read_scalars_by_id_lua` default branch on line 590 which throws on unknown DataType.

---

### WR-04: Dart `addTimeSeriesRow` test coverage doesn't include int-for-float (the homogeneity case)

**File:** `bindings/dart/test/database_time_series_test.dart:585-664`
**Issue:** The three Dart tests for `addTimeSeriesRow` use the `collections.sql` and `multi_dim_time_series.sql` schemas and pass already-correctly-typed values (`10.0`, `42.5`, `1`). None of them tests the case the Julia binding's auto-coercion was designed to handle: a user passes `temperature: 20` (int) into a REAL column.

Given CR-03 above, this gap matters: whichever direction CR-03 is resolved, Dart needs a test that pins the chosen behavior. Today the wrapper silently passes `int` through and the C++ layer rejects it (or — worse — SQLite stores it as INTEGER affinity because of how STRICT vs non-STRICT works), and no test exercises it.

**Fix:** Add a test using `mixed_time_series.sql`:

```dart
test('addTimeSeriesRow with int value into REAL column', () {
  // Per current binding contract, this should either:
  //   - succeed (if Dart adds auto-coercion matching Julia)
  //   - throw DatabaseException with "type" in message
  // Pin whichever the agreed behavior is.
});
```

---

### WR-05: Python `add_time_series_row` docstring claim "No Int->Float coercion" is binding-local; CLAUDE.md doesn't document it

**File:** `bindings/python/src/quiverdb/database.py:1189-1196`
**Issue:** The docstring says: *"Strict type dispatch: int -> INTEGER, float -> FLOAT, str -> STRING. No Int->Float coercion."*

This is a deliberate-looking design choice but isn't reflected in CLAUDE.md, which currently makes no mention of type coercion policy for any binding. Combined with CR-03 (Julia DOES coerce), this is a documentation/policy gap.

**Fix:** Once CR-03 is resolved (pick one policy), update CLAUDE.md "C++ Patterns" or "Cross-Layer Naming Conventions" with an explicit policy section: e.g., "Type coercion: bindings do NOT coerce int -> float; pass `42.0` for FLOAT columns." Apply the policy in every binding, then the per-binding docstring becomes redundant and can be tightened.

---

### WR-06: Julia `add_time_series_row!` duplicates ~80 lines of `update_time_series_group!` marshaling

**File:** `bindings/julia/src/database_update.jl:123-204` vs `15-121`
**Issue:** The new `add_time_series_row!` is the bulk `update_time_series_group!` body with vector iteration collapsed to a single-element path:
- Same metadata fetch and `schema_types` build
- Same DateTime/AbstractString/Integer/Real type dispatch
- Same `refs` / `GC.@preserve` setup
- Same Cstring construction
- Only difference: `[Float64(x) for x in v]` becomes `[Float64(v)]`

If a future schema-coercion bug is fixed in one, it won't be fixed in the other. CLAUDE.md emphasizes simple solutions — this is the opposite.

**Fix:** Extract a shared helper that takes a `Vector{Pair{String, Any}}` (column name + vector-of-values), runs the marshaling, and returns `(refs, name_ptrs, col_types_arr, col_data_arr)`. Then both `update_time_series_group!` and `add_time_series_row!` build that intermediate from kwargs (one wraps each scalar in a 1-element vector). The C call line is the only difference.

---

### WR-07: Julia test `Add Row > Multi-Dim` is missing the `flag` column

**File:** `bindings/julia/test/test_database_time_series.jl:189-209`
**Issue:** The Dart equivalent test passes `flag: 0` (line 651). The Julia test omits `flag` entirely:

```julia
Quiver.add_time_series_row!(db, "Resource", "load", id;
    date_time = DateTime(2024, 1, 1),
    block = 1,
    load = 100.0,
)
```

If the schema `multi_dim_time_series.sql` declares `flag` as `INTEGER NOT NULL` without a DEFAULT, this would fail. If `flag` is nullable, the test passes but doesn't exercise the multi-value-column path. Either way the test asymmetry suggests at least one suite is wrong.

**Fix:** Read `tests/schemas/valid/multi_dim_time_series.sql` and align both tests. If `flag` is in the schema, both bindings should pass it. If `flag` is nullable and intentionally omitted to test the "omit nullable value column" path, add a comment.

---

### WR-08: Python `_marshal_time_series_columns` validates row dicts but `add_time_series_row` does not (different validation paths)

**File:** `bindings/python/src/quiverdb/database.py:1189-1238` vs `1533-1598`
**Issue:** `update_time_series_group` calls `_marshal_time_series_columns` which fetches metadata and checks every value against the schema column type, raising specific `ValueError("Row N is missing column 'name'")` and `TypeError("Column 'name' expects float, got int")` messages.

`add_time_series_row` does no metadata fetch, no schema validation. It does its own type dispatch (`int -> INTEGER, float -> FLOAT, str -> STRING`) without checking the schema. So:

- Missing required dim column: detected only by C++ ("Cannot add_time_series_row: row missing required 'date_time' column") — fine, but the Python wrapper could pre-check.
- Wrong type (e.g., `humidity=20.0` into INTEGER column): the wrapper marshals as FLOAT, the C++ layer sees a Float Value in an Integer-typed column and throws `"Cannot add_time_series_row: column 'humidity' has type integer but received float"`.
- Unknown column (typo): C++ throws `"Cannot add_time_series_row: column 'humidty' not found in group 'readings' for collection 'Sensor'"`.

These C++ messages are fine, so this works — but the asymmetry with `update_time_series_group` means error message styles vary between the two write paths in the same binding ("missing column 'status'" vs "Cannot add_time_series_row: row missing required 'date_time' column").

**Fix:** Per CLAUDE.md "Error Messages: All error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own." -- the binding-side validation in `_marshal_time_series_columns` actually violates the principle (Python crafts `ValueError("Row N is missing column 'name'")`). Either:

1. Remove the binding-side validation in `_marshal_time_series_columns` and let C++ produce the canonical error. Then both `update_time_series_group` and `add_time_series_row` surface C++ messages consistently.
2. Keep the binding-side validation but apply it symmetrically in `add_time_series_row` too.

Pick one. Don't have one path validate in Python and one validate in C++.

---

### WR-09: Dart `addTimeSeriesRow` does not validate empty row map

**File:** `bindings/dart/lib/src/database_update.dart:159-223`
**Issue:** If a caller passes `row = {}` (no columns), `columnCount = 0` and the wrapper calls the C API with `columnCount = 0`, valid pointers to zero-length arrays. The C++ layer then sees an empty row, fails the "row missing required 'date_time' column" check, and throws. The user gets a confusing error because the wrapper invited the call.

Compare with the Dart `updateTimeSeriesGroup` (line 57) which explicitly handles `data.isEmpty` as a clear operation. `addTimeSeriesRow` has no analog because there's no meaningful "empty row" semantic.

**Fix:** Either:
1. Assert/throw early in Dart: `if (row.isEmpty) throw ArgumentError('addTimeSeriesRow requires at least one column');`
2. Let the C++ message through — this is acceptable per CLAUDE.md ("All error messages are defined in the C++/C API layer"). In that case, no change needed.

The current state is fine if you pick option 2; just flagging for awareness.

---

### WR-10: Lua's `lua_table_to_value_map` uses iteration order from `sol::table` (unordered)

**File:** `src/lua_runner.cpp:216-232, 814`
**Issue:** Lua tables iterated via `for (auto& pair : t)` use hash-order traversal for non-array keys. The new `add_time_series_row_lua` calls `lua_table_to_value_map(row)` which inserts into a `std::map<std::string, Value>`, then `db.add_time_series_row` walks the map. The C++ layer doesn't rely on column order (it uses `find(dim_col)` and iterates the row for the INSERT column list), so this works.

But: the resulting SQL `INSERT OR REPLACE INTO ... (id, col1, col2, ...) VALUES (?, ?, ?, ...)` builds the column list from `std::map<std::string, Value>` ordering, which is alphabetical. So `date_time` always comes before `value`. Two callers will produce identical SQL even if they wrote the keys in different orders. Fine.

The risk is only if someone refactors `add_time_series_row` to use a different ordered container — the Lua path relies on alphabetical ordering being stable. Not a defect today; flagging as fragile.

**Fix:** No code change required. Consider adding a comment in `lua_table_to_value_map` documenting that order doesn't matter to the consumer.

## Info

### IN-01: Dart `addTimeSeriesRow` and `updateTimeSeriesGroup` duplicate the same type-dispatch ladder

**File:** `bindings/dart/lib/src/database_update.dart:88-129` and `175-205`
**Issue:** Two near-identical `if (entry.value is int) ... else if (entry.value is double) ... else if (entry.value is String) ... else if (entry.value is DateTime) ... else throw ArgumentError` blocks. The only differences are loop count (`rowCount` vs `1`) and array allocation (`arena<Int64>(rowCount)` vs `arena<Int64>(1)`).

**Fix:** Extract a helper `void _marshalColumn(Arena arena, dynamic firstValue, List values, ...)` that fills a single column's metadata. Optional cleanup; not blocking.

---

### IN-02: docs/time_series.md formatting / outdated narrative

**File:** `docs/time_series.md:23, 50-55, 187-193`
**Issue:** Minor issues beyond CR-01:
- Line 23: "format: `YYYY-MM-DD HH:MM:SS`" — but the actual format used by `date_time.jl` is `yyyy-mm-ddTHH:MM:SS` (T separator, not space). Test fixtures all use `2024-01-01T10:00:00`.
- Lines 50-55 and 187-193 duplicate the "missing value sentinel" rules (Float -> NaN, Int -> typemin, String -> "", DateTime -> typemin). Pick one location and reference it from the other.
- Line 50: claims `Int64` missing returns `typemin(Int)`. Tests in `test_database_time_series.jl:529` only verify `NaN` for Float. There's no Julia test verifying `typemin(Int64)` for integer columns — the doc claim isn't backed by tests.

**Fix:** Reconcile date format string. Deduplicate the missing-value table. Add a test for the `typemin(Int64)` claim (or remove the claim if it's not true).

---

_Reviewed: 2026-05-20T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
