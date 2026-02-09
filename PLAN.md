# Issue 59: createElement should support time series data

## Problem Analysis

The test in `bindings/dart/test/issues_test.dart` fails with two cascading issues:

### Issue A: Test uses wrong factory method
The test calls `Database.fromSchema(...)` but `tests/schemas/issues/issue59/` contains **migrations** (`1/up.sql`), not a schema file. Error: `Failed to open schema file`.

### Issue B (the real bug): `create_element` doesn't route arrays to time series tables
The C++ `create_element` in `src/database.cpp` (lines 572-632) routes Element arrays to **vector** and **set** tables, but has **no handling for time series tables**. Any array matching a time series column will throw: `"Array 'X' does not match any vector or set table"`.

This is the quiver bug — `create_element` should support inserting time series data just like it supports vectors and sets.

### Issue C: Dart `Element.set()` has no `Map` or `List<DateTime>` support
The Dart test passes a nested Map for time series data. `Element.set()` has no `case Map` (throws `Unsupported type`) and no `case List<DateTime>`.

## Plan

### Step 1: Fix test factory method
**File:** `bindings/dart/test/issues_test.dart`
- Change `Database.fromSchema(...)` → `Database.fromMigrations(...)`

### Step 2: C++ core — add time series routing to `create_element`
**File:** `src/database.cpp`

After the vector and set routing blocks (line 628), add a third routing block for time series tables:

```
// Build a map of time_series table -> (column_name -> array values)
std::map<std::string, std::map<std::string, const std::vector<Value>*>> time_series_table_columns;
```

For each unrouted array, check if the array name matches a column in any time series table for the collection (using `is_time_series_table()`). If so, route it there.

Then insert time series rows by zipping the parallel arrays — same pattern as vectors, but WITHOUT a `vector_index` column. The columns are the dimension column + value columns directly from the arrays.

### Step 3: Dart — support `Map` values in `Element.set()`
**File:** `bindings/dart/lib/src/element.dart`

Add to the switch in `set()`:
- `case Map<String, Object?> v:` → iterate entries, call `set(key, value)` for each (flattens nested map into top-level arrays)
- `case List<DateTime> v:` → convert each DateTime to ISO string, call `setArrayString(name, strings)`

This way the test's nested map:
```dart
'some_time_series': {
  'date_time': [DateTime(1990, 1, 1)],
  'some_time_series_float': [1.0],
  'some_time_series_integer': [1],
}
```
Gets flattened to three arrays on the Element: `date_time`, `some_time_series_float`, `some_time_series_integer`.

### Step 4: Build and test
- Build C++ (`cmake --build build --config Debug`)
- Run C++ tests (`build/bin/quiver_tests.exe`)
- Run Dart tests (`bindings/dart/test/test.bat`)

## Files Changed
1. `bindings/dart/test/issues_test.dart` — fix factory method
2. `src/database.cpp` — add time series routing in `create_element`
3. `bindings/dart/lib/src/element.dart` — add Map and List<DateTime> support
