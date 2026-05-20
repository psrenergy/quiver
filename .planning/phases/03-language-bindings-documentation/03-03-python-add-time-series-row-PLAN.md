---
id: 03-03-python-add-time-series-row
phase: 03
plan: 03
type: execute
wave: 1
depends_on: []
files_modified:
  - bindings/python/src/quiverdb/_c_api.py
  - bindings/python/src/quiverdb/database.py
  - bindings/python/tests/test_database_time_series.py
requirements: [PY-11]
autonomous: true

must_haves:
  truths:
    - "Python users can call db.add_time_series_row(collection, group, id, **kwargs)"
    - "Calling add_time_series_row twice with the same dimension PK upserts (value columns overwritten)"
    - "Dict unpacking pattern db.add_time_series_row('Col', 'grp', 1, **row_dict) works without ceremony"
    - "Multi-dimension PK schemas (date_time + block) round-trip through the Python wrapper"
    - "bindings/python/test/test.bat exits 0 with the new add_time_series_row test functions present"
  artifacts:
    - path: "bindings/python/src/quiverdb/_c_api.py"
      provides: "Hand-edited cdef block containing the quiver_database_add_time_series_row declaration"
      contains: "quiver_database_add_time_series_row"
    - path: "bindings/python/src/quiverdb/database.py"
      provides: "Python idiomatic wrapper add_time_series_row using **kwargs"
      contains: "def add_time_series_row"
    - path: "bindings/python/tests/test_database_time_series.py"
      provides: "Insert/upsert/dict-unpacking/multi-dim coverage"
      contains: "def test_add_time_series_row"
  key_links:
    - from: "bindings/python/src/quiverdb/database.py"
      to: "lib.quiver_database_add_time_series_row"
      via: "CFFI call via the hand-edited _c_api.py cdef"
      pattern: "lib\\.quiver_database_add_time_series_row"
    - from: "bindings/python/src/quiverdb/database.py"
      to: "check()"
      via: "Error relay from C API last_error -> Python exception"
      pattern: "check\\(lib\\.quiver_database_add_time_series_row"
---

<objective>
Ship the Python idiomatic wrapper `Database.add_time_series_row(self, collection: str, group: str, id: int, **kwargs)` that mirrors the strict-typing marshaling of the sibling `update_time_series_group` (no Int→Float auto-coercion, per D-03), structurally simplified to single-element CFFI arrays per column with no `row_count` parameter. Hand-edit `_c_api.py` to add the new cdef. Add insert + upsert + dict-unpacking + multi-dim test coverage and run `bindings/python/test/test.bat`.

Purpose: Closes PY-11. Per D-03 / D-05 / D-06 / D-07 / D-08 / D-10 in `.planning/phases/03-language-bindings-documentation/03-CONTEXT.md`.
Output: Hand-edited `_c_api.py` (per CLAUDE.md Python Notes: "_c_api.py contains hand-written CFFI cdef declarations"), new `add_time_series_row` method in `database.py` alongside `update_time_series_group`, new test functions in `test_database_time_series.py`.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/03-language-bindings-documentation/03-CONTEXT.md
@CLAUDE.md
@include/quiver/c/database.h
@bindings/python/src/quiverdb/_c_api.py
@bindings/python/src/quiverdb/database.py
@bindings/python/tests/test_database_time_series.py

<interfaces>
<!-- C API symbol the Python wrapper marshals against, per CONTEXT.md canonical_refs -->

From include/quiver/c/database.h (lines 308-315):
```c
QUIVER_C_API quiver_error_t quiver_database_add_time_series_row(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group,
                                                                int64_t id,
                                                                const char* const* column_names,
                                                                const int* column_types,
                                                                const void* const* column_data,
                                                                size_t column_count);
```

Sibling wrapper template (the marshaling style to mirror), from bindings/python/src/quiverdb/database.py:1156-1187:
- Method signature: `def update_time_series_group(self, collection: str, group: str, id: int, rows: list[dict]) -> None`
- Calls `self._ensure_open()`, gets `lib = get_lib()`
- Empty-rows branch passes `ffi.NULL` for the three array pointers and `0, 0` for counts
- Calls `_marshal_time_series_columns(rows, metadata)` (module helper at lines 1482-1547) to build `(keepalive, c_col_names, c_col_types, c_col_data, col_count, row_count)`
- Final `check(lib.quiver_database_update_time_series_group(self._ptr, c_collection, c_group, id, c_col_names, c_col_types, c_col_data, col_count, row_count))`

For `add_time_series_row`, the row is one dict (`**kwargs`); `col_count == len(kwargs)`; no `row_count`. Strict typing per D-03: int → INTEGER, float → FLOAT, str → STRING; **no** Int→Float coercion (Python's update wrapper does not coerce — preserve homogeneity per D-03). DO NOT reuse `_marshal_time_series_columns` — different row shape; inline marshaling is shorter and clearer per "simple over abstract" in CONTEXT.md `<code_context>`.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Hand-add cdef block to _c_api.py</name>
  <files>bindings/python/src/quiverdb/_c_api.py</files>
  <read_first>
    bindings/python/src/quiverdb/_c_api.py (lines 267-281 — the existing time-series cdef block where the new declaration belongs)
    include/quiver/c/database.h (lines 295-315 — both update and add declarations as a side-by-side reference)
    CLAUDE.md "Python Notes" — confirms `_c_api.py` is hand-edited and `generator/generator.py` is a stdout diff aid only
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-05)
  </read_first>
  <action>
    In `bindings/python/src/quiverdb/_c_api.py`, add a new cdef declaration for `quiver_database_add_time_series_row` immediately after the existing `quiver_database_update_time_series_group` cdef (currently at lines 273-276) and before the `quiver_database_free_time_series_data` cdef. The declaration must match the C header exactly:
    ```
    quiver_error_t quiver_database_add_time_series_row(quiver_database_t* db,
        const char* collection, const char* group, int64_t id,
        const char* const* column_names, const int* column_types,
        const void* const* column_data, size_t column_count);
    ```
    Keep the surrounding comment-section banner ("// Time series group read/update/free") intact. Do not edit the generator script. Optionally run `python bindings/python/generator/generator.py` once to cross-check the hand-added line matches the generator's stdout output; commit only the hand-edited `_c_api.py`.
  </action>
  <verify>
    <automated>findstr /C:"quiver_database_add_time_series_row" bindings\python\src\quiverdb\_c_api.py</automated>
  </verify>
  <done>`bindings/python/src/quiverdb/_c_api.py` contains the 8-parameter cdef declaration for `quiver_database_add_time_series_row` inside the `ffi.cdef("""...""")` block, placed in the time-series section. No `row_count` parameter.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: Add Python add_time_series_row method</name>
  <files>bindings/python/src/quiverdb/database.py</files>
  <read_first>
    bindings/python/src/quiverdb/database.py (lines 1156-1187 — the sibling `update_time_series_group` method)
    bindings/python/src/quiverdb/database.py (lines 1482-1547 — the `_marshal_time_series_columns` helper; reference for typing/marshaling conventions, do NOT reuse it)
    bindings/python/src/quiverdb/_c_api.py (verify Task 1 added the cdef)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-03)
  </read_first>
  <behavior>
    - Method `def add_time_series_row(self, collection: str, group: str, id: int, **kwargs) -> None` exists on the `Database` class in `bindings/python/src/quiverdb/database.py`
    - Calls `self._ensure_open()`, gets `lib = get_lib()`
    - Strict typing dispatch per `**kwargs` value: `isinstance(v, int) and not isinstance(v, bool)` → `int64_t[]`; `isinstance(v, float)` → `double[]`; `isinstance(v, str)` → `char*[]`; other → `TypeError` (mirrors `_marshal_time_series_columns` error messages but inline)
    - For each kwarg builds a length-1 CFFI array (`ffi.new("int64_t[]", [v])` / `ffi.new("double[]", [v])` / `ffi.new("char*[]", [ffi.new("char[]", v.encode("utf-8"))])`)
    - Maintains a `keepalive: list = []` for ownership across the CFFI call
    - Final `check(lib.quiver_database_add_time_series_row(self._ptr, c_collection, c_group, id, c_col_names, c_col_types, c_col_data, col_count))` — 8 arguments, no `row_count`
    - Dict unpacking via `**kwargs` works naturally: `db.add_time_series_row("Col", "grp", 1, **row_dict)` is supported
    - No binding-layer "Cannot add_time_series_row:" string introduced
  </behavior>
  <action>
    In `bindings/python/src/quiverdb/database.py`, alongside `update_time_series_group` (insert immediately after the `update_time_series_group` method ends, before the "Time series files" section banner at line ~1189 — the natural sibling location per D-03), add a new `def add_time_series_row(self, collection: str, group: str, id: int, **kwargs) -> None` method on the `Database` class.
    
    Implement inline marshaling (do NOT call `_marshal_time_series_columns` — different row shape per CONTEXT.md `<code_context>` Reusable Assets). Build `c_col_names = ffi.new("const char*[]", col_count)`, `c_col_types = ffi.new("int[]", [...])`, `c_col_data = ffi.new("void*[]", col_count)`. For each kwarg value, dispatch strictly on `type(v)` (use `type(v) is int` for INTEGER, `type(v) is float` for FLOAT, `type(v) is str` for STRING — matches the `_marshal_time_series_columns` strictness per D-03). Build a length-1 typed array per column. Append every CFFI allocation to a `keepalive` list. Call `check(lib.quiver_database_add_time_series_row(...))` with 8 arguments. The `**kwargs` shape automatically supports dict unpacking.
    
    No new helpers; no factoring with `update_time_series_group`. The wrapper is self-contained.
  </action>
  <verify>
    <automated>findstr /C:"def add_time_series_row" bindings\python\src\quiverdb\database.py &amp;&amp; findstr /C:"lib.quiver_database_add_time_series_row" bindings\python\src\quiverdb\database.py</automated>
  </verify>
  <done>`bindings/python/src/quiverdb/database.py` contains the new `def add_time_series_row(self, collection: str, group: str, id: int, **kwargs)` method with strict-typing inline marshaling, keepalive list, and the 8-argument CFFI call. No `row_count` argument. No new "Cannot add_time_series_row:" string.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 3: Add Python add_time_series_row test functions</name>
  <files>bindings/python/tests/test_database_time_series.py</files>
  <read_first>
    bindings/python/tests/test_database_time_series.py (lines 1-30 — fixtures; lines 120-154 — existing TestTimeSeriesValidation / TestTimeSeriesSingleColumn class structure)
    tests/schemas/valid/collections.sql (primary single-dim schema)
    tests/schemas/valid/multi_dim_time_series.sql (composite-PK schema for multi-dim test)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-07 Python scenario list)
  </read_first>
  <behavior>
    - Four new test functions exist in `bindings/python/tests/test_database_time_series.py` (or grouped under a new `TestAddTimeSeriesRow` class — pick whichever matches the file's existing style, per D-08)
    - Test 1 — `test_add_time_series_row_insert`: insert one row via kwargs (`db.add_time_series_row("Collection", "data", id, date_time="2024-01-01", value=10.0)`); read back and assert presence
    - Test 2 — `test_add_time_series_row_upsert`: insert, then call again with the same `date_time` and a new `value`; assert read returns one row with the new value
    - Test 3 — `test_add_time_series_row_dict_unpacking`: build `row_dict = {"date_time": "2024-02-01", "value": 7.5}`, call `db.add_time_series_row("Collection", "data", id, **row_dict)`; assert round-trip — this proves dict unpacking works per PY-11
    - Test 4 — `test_add_time_series_row_multi_dim`: load `tests/schemas/valid/multi_dim_time_series.sql`; insert with both `date_time` and `block` populated plus a value column; assert round-trip
  </behavior>
  <action>
    In `bindings/python/tests/test_database_time_series.py`, append four new test functions (or a single `TestAddTimeSeriesRow` class containing four methods — match existing class-vs-function convention). Place after the existing time-series test classes. Use the existing `_create_collection_element` / temp-DB fixture pattern. Per D-07 Python and Claude's Discretion in CONTEXT.md, do not add a missing-dimension error-path test (default policy — skip per the test-coverage decision). For Test 4, load `tests/schemas/valid/multi_dim_time_series.sql` via the same `Database.from_schema` pattern; sample dimension values are in that schema file.
  </action>
  <verify>
    <automated>findstr /C:"def test_add_time_series_row_insert" bindings\python\tests\test_database_time_series.py &amp;&amp; findstr /C:"def test_add_time_series_row_upsert" bindings\python\tests\test_database_time_series.py &amp;&amp; findstr /C:"def test_add_time_series_row_dict_unpacking" bindings\python\tests\test_database_time_series.py &amp;&amp; findstr /C:"def test_add_time_series_row_multi_dim" bindings\python\tests\test_database_time_series.py</automated>
  </verify>
  <done>`bindings/python/tests/test_database_time_series.py` contains the four new test functions (insert, upsert, dict_unpacking, multi_dim). Existing tests unmodified.</done>
</task>

<task type="auto">
  <name>Task 4: Run Python test suite</name>
  <files>bindings/python/test/test.bat</files>
  <read_first>
    bindings/python/test/test.bat
    CLAUDE.md (section "Build & Test" + "Python Notes" — confirms `bindings/python/test/test.bat` is the command and notes the PATH prepending procedure)
  </read_first>
  <action>
    Run `bindings/python/test/test.bat` from the repository root. All tests including the four new `test_add_time_series_row_*` functions must pass. `test.bat` already prepends `build/bin/` to PATH for DLL discovery (per CLAUDE.md Python Notes) — no manual PATH wiring needed.
  </action>
  <verify>
    <automated>bindings\python\test\test.bat</automated>
  </verify>
  <done>`bindings/python/test/test.bat` exits with code 0. Test output shows the four new `test_add_time_series_row_*` cases as passing alongside the existing time-series tests.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Python kwargs → C ABI | Caller-supplied scalar values cross the FFI boundary as CFFI-allocated typed arrays; the Python `keepalive` list owns ownership across the call. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-07 | Tampering | `add_time_series_row` marshaling | mitigate | All CFFI allocations are appended to a per-call `keepalive: list` whose lifetime spans the `check(lib....)` call. Mirrors `_marshal_time_series_columns` ownership policy. |
| T-03-08 | Information Disclosure | Error messages | accept | Error strings come from C++/C API only (per CLAUDE.md). No PII / no sensitive paths leaked by binding layer. |
| T-03-09 | Denial of Service | Misuse via wrong Python type | mitigate | Strict `type(v) is ...` dispatch raises `TypeError` for unsupported types — fails fast in Python before crossing FFI. |
</threat_model>

<verification>
- `bindings/python/src/quiverdb/_c_api.py` contains the new cdef (Task 1 acceptance)
- `bindings/python/src/quiverdb/database.py` exposes `add_time_series_row` as a method on the `Database` class — reachable as `db.add_time_series_row(...)` because `database.py` is the public module entry
- All Python tests pass via `bindings/python/test/test.bat`
- No new "Cannot add_time_series_row:" string introduced inside `bindings/python/` (grep for that prefix should match zero lines under `bindings/python/`)
</verification>

<success_criteria>
- All four tasks' acceptance criteria pass
- `db.add_time_series_row` round-trips through Python ↔ C ABI ↔ SQLite for insert, upsert, dict-unpacking, and multi-dim paths
- PY-11 is satisfied: signature matches roadmap success criterion #3 verbatim, including `**kwargs` convention and dict-unpacking support
</success_criteria>

<output>
Create `.planning/phases/03-language-bindings-documentation/03-03-SUMMARY.md` when done.
</output>
