# 30 — Python: fix stale docstrings (upsert types, _c_api.py header, marshaller wording) and a duplicate test

**Batch** 4 · **Severity** low · **Breaking** no (comments, docstrings and tests only; no code path changes) · **Size** S · **Layers** Python binding only: `bindings/python/src/quiverdb/database.py` (two docstrings), `bindings/python/src/quiverdb/_c_api.py` (header comment), `bindings/python/tests/test_database_lifecycle.py` (delete one test), `bindings/python/tests/test_database_time_series_row.py` (add one test)

**Depends on** none. Plans 18, 21, 24, 25, 27, 28 and 29 run earlier and edit the same files. This plan anchors on quoted phrases, so it applies on top of any of their states (see Pitfalls).
**Overlaps with**
- **41** (stale "not positionally aligned" reader comments, all bindings) owns the four alignment paragraphs in `database.py` (`read_vector_booleans`, `read_vector_date_times`, `read_set_booleans`, `read_set_date_times`) that the source finding (python#5) also names. The maintainer decision for this item says: *"Only the NON-alignment parts of python#5 (the alignment comment fix across all bindings is plan 41)."* **Do not touch those four docstrings here.**
- **25** (runs first) adds `datetime -> STRING (an aware value is converted to UTC)` to the `upsert_time_series_row` type list and explicitly leaves the stale `No Int->Float coercion (per D-03 ...)` sentence to this plan. Keep 25's datetime wording. Plan 25 also adds `test_upsert_time_series_row_accepts_datetime` to the same test class this plan adds to. The names differ.
- **27** (runs first) changes the `def upsert_time_series_row(...)` line to `..., id: int, /, **kwargs: object) -> None:` and says in its own plan that it leaves the docstring below to this plan. Edit only the docstring.
- **28** (runs first) deletes the `isinstance(v, bool)` branch in `_marshal_row_columns`. A bool still reaches INTEGER through the `int` branch, so `bool -> INTEGER (0/1)` in the upsert docstring stays true. 28's `test_boolean_input` extension already pins a bool written to a REAL column through `upsert_time_series_row`.
- **24** (runs first) rewrites the type-dispatch paragraph of the `_marshal_group_columns` docstring. This plan changes only that docstring's **summary line** (`"...for the columnar time series API."`). If 24 already rewrote the summary line, check it names every group writer (Step 2) and move on.
- **67** (runs later) lists the same Python test deletion (`test_describe_runs_without_error`) among its four binding describe leftovers. This plan deletes it, so plan 67's Python step becomes a no-op.
- **18, 21, 22** (run first) add, delete or rename declarations inside the `ffi.cdef("""...""")` block of `_c_api.py`. This plan changes only the `#` comment above that block. After 18 lands the block mirrors every non-binary C function, which the new header states.
- **29** (runs first) deletes the local `collections_db` fixture in `test_database_metadata.py`. `TestDescribe.test_returns_string`, the test that makes the deleted lifecycle test redundant, survives that change.

## Why

Four comments in the Python binding state things that are false, plus one duplicate test whose comment is false. None of them changes behaviour; each one misleads the person who reads it. Root `AGENTS.md` Principles: *"Human-Centric: Codebase optimized for human readability"*. A docstring that states the wrong contract is a readability bug.

### 1. `upsert_time_series_row` docstring (`bindings/python/src/quiverdb/database.py`, currently ~L1785-1793)

Current code at HEAD (plan 25 will have inserted a `datetime -> STRING` clause, see Changes):
```python
    def upsert_time_series_row(self, collection: str, group: str, id: int, **kwargs) -> None:
        """Insert or upsert a single time series row for an element.

        Keyword arguments map column names to values. The dimension column (e.g.
        date_time) and all value columns must be provided. Type dispatch uses
        isinstance: bool -> INTEGER (0/1), int -> INTEGER, float -> FLOAT, str ->
        STRING. No Int->Float coercion (per D-03: Python strict typing).
        Dict unpacking is supported: db.upsert_time_series_row("Col", "grp", 1, **row_dict).
        """
```
Two sentences are false:

- **"No Int->Float coercion (per D-03: Python strict typing)."** The core accepts an int for a REAL column. `_marshal_row_columns` (currently ~L2239) sends a Python `int` as `DataType.INTEGER`, and `validate_time_series_row` (`src/database_time_series.cpp`, ~L22) calls `internal::value_matches_type`, which accepts an int64 for REAL (root `AGENTS.md`, *"One scalar typing policy lives in C++"*). `tests/test_database_time_series_row.cpp`, test `Database.UpsertTimeSeriesRowErrors`, case d (~L533): `// d. INTEGER passed for REAL column 'load' is accepted (converted on insert).` I ran it at HEAD: `quiver_tests.exe --gtest_filter=Database.UpsertTimeSeriesRowErrors` passes. So `db.upsert_time_series_row("Resource", "load", eid, date_time="2024-01-01", block=1, load=42)` succeeds and stores `42.0`, which is the opposite of the docstring. The sibling `update_time_series_group` docstring (~L1542) already says it correctly: *"integers are accepted for REAL columns."*
  "D-03" is not a Python decision. The finding says the ID appears nowhere else. That is wrong: `git grep -n "D-03"` also finds `src/database_describe.cpp:57-58` and `tests/test_database_ui_metadata.cpp:478-479`, where D-03 is an unrelated UI-sidecar normalization decision. That makes the Python citation point at the wrong thing, which is one more reason to drop it.
- **"The dimension column (e.g. date_time) and all value columns must be provided."** The verifiers did not flag this; I found it while re-reading the docstring. Only the dimension columns are required. `validate_time_series_row` rejects a missing dimension column (`"Cannot upsert_time_series_row: row missing required '<dim>' column"`) and nothing else. The INSERT lists only the caller's columns (`Database::upsert_time_series_row`, ~L242-256, comment: *"Any value column omitted from the caller's row is not listed in the INSERT, so SQLite leaves it as the column DEFAULT (NULL for nullable value columns)."*). C++ pins it: `Database.UpsertTimeSeriesRowPartialValueColumns` (~L451). A multi-dimension group (`multi_dim_time_series.sql`: `date_time` + `block`) also has more than one dimension column, so "The dimension column" is wrong too.

### 2. `_marshal_group_columns` summary line (`database.py`, currently ~L2166-2167)

```python
def _marshal_group_columns(data: dict[str, list]) -> tuple:
    """Marshal column lists into parallel C arrays for the columnar time series API.
```
It serves all six columnar group writers: `update_time_series_group`, `update_time_series_group_by_label`, `update_vector_group`, `update_vector_group_by_label`, `update_set_group`, `update_set_group_by_label` (`grep -n "_marshal_group_columns(" bindings/python/src/quiverdb/database.py` gives six call sites plus the `def`). `bindings/python/AGENTS.md` already says so: *"`_marshal_group_columns` serves every columnar group writer (time series, vector, set, by id and by label)"*.

### 3. `_c_api.py` header comment (`bindings/python/src/quiverdb/_c_api.py`, L5-6)

```python
# Phase 1 CFFI declarations: lifecycle subset from C API headers.
# Copied exactly from include/quiver/c/ headers with QUIVER_C_API stripped.
```
Both lines are false:
- It is not a "lifecycle subset". I compared function names at HEAD: the cdef block declares 113 `quiver_*` names; the five headers the Python generator reads (`common.h`, `options.h`, `database.h`, `element.h`, `lua_runner.h`) declare 115. The only two missing are `quiver_database_read_vector_group_by_id` and `quiver_database_read_set_group_by_id`, and plan 18 adds them. The `binary/` and `expression/` headers are excluded on purpose (root `AGENTS.md`: *"Binary + expression subsystems are exposed in Julia and Lua only"*), and `bindings/python/generator/generator.py`'s `HEADERS` list excludes them too.
- It is not "copied exactly". The block is regrouped by topic with its own section comments (`// Read scalar attributes`, `// Query methods - parameterized`, ...), and `database.h` declarations appear in several places (`// database.h`, `// database.h - element operations`, then `// element.h`, then more `database.h` content).

The real workflow is in `bindings/python/AGENTS.md`: *"After C API changes, run `generator/generator.bat` and diff its output against `_c_api.py`."* The header should say that.

### 4. Duplicate describe test (`bindings/python/tests/test_database_lifecycle.py`, currently ~L91-92)

```python
def test_describe_runs_without_error(db: Database) -> None:
    db.describe()  # Should not raise; output goes to stdout
```
`describe()` returns a `str` (`database.py` ~L169, from `quiver_database_describe`'s `out_report`); nothing goes to stdout. The comment dates from the old `describe(std::ostream&)` API, removed in #206. `test_database_metadata.py`, `TestDescribe.test_returns_string` (~L214-216), already asserts `isinstance(collections_db.describe(), str)`, which is strictly stronger. `git grep -n describe_runs_without_error` finds only this definition (no CI filter or doc refers to it).

## Constraints and decisions

- **Maintainer decision (binding):** only the non-alignment parts of python#5. The four "not positionally aligned" paragraphs belong to plan 41.
- **Root `AGENTS.md`, Design Decisions, "One scalar typing policy lives in C++":** *"an int64 is accepted for INTEGER and REAL columns (int-for-REAL coercion) ... `TypeValidator` (scalar create/update) and `value_matches_type` (time-series writes) share this rule; bindings never coerce schema-dependently."* The new docstring states that rule. It does not add any Python-side coercion.
- **Root `AGENTS.md`, Core API, Time series:** *"`upsert_time_series_row` inserts or replaces a single row by its dimension key (`INSERT OR REPLACE`)."* The docstring keeps "Insert or upsert" as its summary line.
- **`bindings/python/AGENTS.md`, Rules:** *"CFFI ABI-mode ... After C API changes, run `generator/generator.bat` and diff its output against `_c_api.py`."* The new header comment repeats this and names the excluded subsystems.
- **Root `AGENTS.md`, "Changelog":** *"user-visible changes get an entry"*. Nothing here changes what a caller gets or must do, and `CHANGELOG.md` has no entry for a docstring correction anywhere in its history (`grep -n -i docstring CHANGELOG.md` is empty). So there is **no CHANGELOG entry**.
- **Self-Updating:** `bindings/python/AGENTS.md` already states every fact the new comments state (marshaller scope, hand-maintained cdefs, generator diff). No AGENTS.md edit is needed.
- Alternatives considered and rejected:
  - *Fix the lifecycle test's comment instead of deleting the test:* rejected. `TestDescribe.test_returns_string` already asserts more, so the test adds nothing. Plan 67 reaches the same conclusion for all four bindings.
  - *Also document that a `None` kwarg raises `TypeError` in the upsert docstring:* rejected. That is true (`_marshal_row_columns` has no `None` branch) but no Python test pins it, and it is not stale text. Leave it to `bindings/python/AGENTS.md`, which already records it.
  - *Also say "an existing row is replaced, not merged" in the docstring:* rejected. It is true (INSERT OR REPLACE), but no test in any layer pins that an omitted column is reset on replace, and this plan does not add behaviour pins beyond the docstring's claims. "Insert or upsert" stays as it is.
  - *Mention the D-03 collision anywhere:* rejected. Deleting the citation is enough; the describe D-03 comments are correct in their own context.

## Changes

All edits are in the Python binding. There are **no** changes to the C++ core, the C API, Lua, Julia, Dart or JS. No C header changes, so no generator run: Julia `c_api.jl`, Dart `bindings.dart`, Python `_c_api.py` cdef block and JS `loader.ts` are untouched.

### Step 1. `bindings/python/src/quiverdb/database.py`, method `Database.upsert_time_series_row` (currently ~L1785-1793)

Find the docstring by the phrase `No Int->Float coercion (per D-03: Python strict typing).` Do not change the `def` line (plan 27 may have changed it to `..., id: int, /, **kwargs: object) -> None:`; keep whatever is there).

Current (at HEAD):
```python
        """Insert or upsert a single time series row for an element.

        Keyword arguments map column names to values. The dimension column (e.g.
        date_time) and all value columns must be provided. Type dispatch uses
        isinstance: bool -> INTEGER (0/1), int -> INTEGER, float -> FLOAT, str ->
        STRING. No Int->Float coercion (per D-03: Python strict typing).
        Dict unpacking is supported: db.upsert_time_series_row("Col", "grp", 1, **row_dict).
        """
```
Current (after plan 25, which is the expected state):
```python
        """Insert or upsert a single time series row for an element.

        Keyword arguments map column names to values. The dimension column (e.g.
        date_time) and all value columns must be provided. Type dispatch uses
        isinstance: bool -> INTEGER (0/1), int -> INTEGER, float -> FLOAT, str ->
        STRING, datetime -> STRING (an aware value is converted to UTC).
        No Int->Float coercion (per D-03: Python strict typing).
        Dict unpacking is supported: db.upsert_time_series_row("Col", "grp", 1, **row_dict).
        """
```
New (if plan 25 landed, which it should have):
```python
        """Insert or upsert a single time series row for an element.

        Keyword arguments map column names to values. Every dimension column (e.g.
        date_time) must be provided; a value column left out gets its column default
        (NULL unless the schema declares one). Type dispatch uses isinstance:
        bool -> INTEGER (0/1), int -> INTEGER, float -> FLOAT, str -> STRING,
        datetime -> STRING (an aware value is converted to UTC); integers are
        accepted for REAL columns.
        Dict unpacking is supported: db.upsert_time_series_row("Col", "grp", 1, **row_dict).
        """
```
New (only if plan 25's `datetime -> STRING` clause is absent): the same text without `datetime -> STRING (an aware value is converted to UTC); `, so the type line reads `bool -> INTEGER (0/1), int -> INTEGER, float -> FLOAT, str -> STRING;` followed by `integers are accepted for REAL columns.` on the next line. Do not add datetime support wording that the code does not back.

Why: the two false sentences (see Why §1) are replaced by what the core does. "integers are accepted for REAL columns" is the same phrase `update_time_series_group`'s docstring uses, so the two writers read alike.

Do **not** touch `upsert_time_series_row_by_label`'s one-line docstring (`"""Label-addressed counterpart of upsert_time_series_row."""`). It is accurate.

### Step 2. `bindings/python/src/quiverdb/database.py`, module function `_marshal_group_columns` (currently ~L2166-2167)

Current:
```python
def _marshal_group_columns(data: dict[str, list]) -> tuple:
    """Marshal column lists into parallel C arrays for the columnar time series API.
```
New:
```python
def _marshal_group_columns(data: dict[str, list]) -> tuple:
    """Marshal column lists into parallel C arrays for the columnar group writers (time series, vector, set).
```
That line is 108 characters, under ruff's `line-length = 120` (`bindings/python/ruff.toml`). Change only this summary line. The rest of the docstring belongs to plan 24 (type dispatch) and plan 23/25 (placeholders, datetime). Leave it as those plans left it.

If plan 24 already replaced the summary line and it no longer contains `columnar time series API`: read it. If it names the time-series, vector and set writers (or "every columnar group writer"), make no change. If it still names only time series, replace it with the line above.

Why: see Why §2.

### Step 3. `bindings/python/src/quiverdb/_c_api.py`, header comment (L5-6)

Current:
```python
ffi = FFI()

# Phase 1 CFFI declarations: lifecycle subset from C API headers.
# Copied exactly from include/quiver/c/ headers with QUIVER_C_API stripped.
ffi.cdef("""
```
New:
```python
ffi = FFI()

# Hand-maintained CFFI declarations for the C API in include/quiver/c/, minus the binary/ and
# expression/ headers (those subsystems are exposed only in Julia and Lua). Grouped by topic, not
# by header. After a C API change, run generator/generator.bat and diff its output against this block.
ffi.cdef("""
```
Keep it a `#` comment above `ffi.cdef(`, not inside the triple-quoted string. Each line is under 120 characters.

Why: see Why §3. The wording holds whether or not plans 18/21/22 have landed: they change which declarations are in the block, not how the block is maintained.

### Step 4. `bindings/python/tests/test_database_lifecycle.py`, delete `test_describe_runs_without_error` (currently ~L91-92)

Current (with its surrounding context):
```python
def test_is_healthy_returns_true(db: Database) -> None:
    assert db.is_healthy() is True


def test_describe_runs_without_error(db: Database) -> None:
    db.describe()  # Should not raise; output goes to stdout


def test_version_returns_string() -> None:
```
New:
```python
def test_is_healthy_returns_true(db: Database) -> None:
    assert db.is_healthy() is True


def test_version_returns_string() -> None:
```
Delete the two lines of the function and the two blank lines after it, so exactly two blank lines separate the neighbouring tests (ruff format enforces this). No import becomes unused: `Path`, `pytest`, `quiverdb`, `Database` and `QuiverError` are all still used by other tests in the file.

Why: see Why §4.

## Tests

Only the Python layer is touched. No other layer's behaviour or text changes, so no C++, C API, Lua, Julia, Dart or JS test is added or changed.

### New test: `bindings/python/tests/test_database_time_series_row.py`, class `TestUpsertTimeSeriesRow`

Pin the two claims the rewritten docstring now makes, since no Python test covers either (C++ covers both: `Database.UpsertTimeSeriesRowErrors` case d and `Database.UpsertTimeSeriesRowPartialValueColumns`). Add it directly after `test_upsert_time_series_row_multi_dim` (find it by name; plans 25 and 27 add other tests to this class):

```python
    def test_upsert_time_series_row_int_for_real_and_omitted_columns(self, multi_dim_ts_db: Database) -> None:
        """An int is accepted for a REAL column; a value column left out of kwargs is stored as NULL."""
        eid = multi_dim_ts_db.create_element("Resource", label="R1")
        multi_dim_ts_db.upsert_time_series_row("Resource", "load", eid, date_time="2024-01-01", block=1, load=42)
        multi_dim_ts_db.upsert_time_series_row("Resource", "load", eid, date_time="2024-01-02", block=1, flag=5)

        result = multi_dim_ts_db.read_time_series_group("Resource", "load", eid)
        assert result["date_time"] == [_utc(2024, 1, 1), _utc(2024, 1, 2)]
        assert result["load"] == [42.0, None]
        assert isinstance(result["load"][0], float)
        assert result["flag"] == [None, 5]
```
- Fixture: `multi_dim_ts_db` (`bindings/python/tests/conftest.py` ~L174) opens `tests/schemas/valid/multi_dim_time_series.sql`: `Resource_time_series_load (id, date_time TEXT NOT NULL, block INTEGER NOT NULL, load REAL, flag INTEGER, PRIMARY KEY (id, date_time, block))`. `load` and `flag` are nullable, so a left-out column is NULL. No new schema file is needed.
- `_utc` is the module helper already at the top of this file (~L24).
- The two rows use **different** `date_time` values on purpose: `read_time_series_group` orders by the date dimension only (`ORDER BY date_time`, `src/database_time_series.cpp` ~L122), so two rows with the same date and different `block` come back in an unspecified order. The C++ test sorts for that reason.
- `42 == 42.0` is true in Python, so the `isinstance(..., float)` line is what proves the value was stored in the REAL column as a real.
- A NULL cell reads back as `None` (`bindings/python/AGENTS.md`, "Time-series group NULLs").
- Before and after: this test **passes at HEAD and after the change**. The change is docstring-only, so there is no fail-before test. The test exists so the docstring's two claims are checked in the layer that states them.

### Existing tests

- `test_database_lifecycle.py::test_describe_runs_without_error` is deleted (Step 4). Its coverage lives on in `test_database_metadata.py::TestDescribe::test_returns_string` (`assert isinstance(collections_db.describe(), str)`), which is unchanged.
- No existing test asserts any of the edited docstrings or the `_c_api.py` comment (`git grep -n "D-03\|Int->Float\|Phase 1 CFFI\|columnar time series API\|all value columns must be provided" -- bindings/python/tests` is empty).

## Docs and changelog

- **AGENTS.md files:** no edit. `bindings/python/AGENTS.md` already says `_c_api.py` is *"Hand-written CFFI cdef declarations (kept in sync manually)"*, that `_marshal_group_columns` *"serves every columnar group writer (time series, vector, set, by id and by label)"*, and how to diff against the generator. Root `AGENTS.md` already states the int-for-REAL rule. `tests/AGENTS.md` lists no Python test names.
- **Other docs:** none. `bindings/python/README.md` and `docs/*.md` contain no "coercion"/"strict typing"/"D-03" claim about Python (`grep -rn -i "coercion\|strict typing" bindings/python/README.md docs/` is empty).
- **CHANGELOG.md:** no entry. No behaviour, API or error message changes (see Constraints).

## Verification

Run from the repo root (`C:\Development\Quiver\quiver1`), in PowerShell.

1. `cmake --build build --config Debug` — no C++ changes here, but earlier plans change the native libraries the Python tests load, so the build must be current. Expected: builds with no errors.
2. `bindings\python\tests\test.bat -k "TestUpsertTimeSeriesRow or TestDescribe or test_database_lifecycle"` — expected: all selected tests pass, including `test_database_time_series_row.py::TestUpsertTimeSeriesRow::test_upsert_time_series_row_int_for_real_and_omitted_columns`; `test_describe_runs_without_error` no longer appears.
3. `bindings\python\tests\test.bat` — expected: the full Python suite passes.
4. Stale-text check (Git Bash): `git grep -n "D-03\|Int->Float\|Phase 1 CFFI\|lifecycle subset\|columnar time series API\|all value columns must be provided\|describe_runs_without_error" -- bindings/python` — expected: no output. (`git grep -n "D-03"` over the whole repo still finds `src/database_describe.cpp` and `tests/test_database_ui_metadata.cpp`; those are the unrelated UI-sidecar decision. Leave them.)
5. Alignment docstrings untouched (Git Bash): `git diff -- bindings/python/src/quiverdb/database.py` — expected: hunks only in `upsert_time_series_row` and `_marshal_group_columns`, none in `read_vector_booleans`, `read_vector_date_times`, `read_set_booleans`, `read_set_date_times` (plan 41).
6. `scripts\format.bat`, then `git status --short` — expected: formatting changes nothing beyond the four files this plan edited (`database.py`, `_c_api.py`, `test_database_lifecycle.py`, `test_database_time_series_row.py`). If ruff reflows anything in them, keep its output.
7. `scripts\test-all.bat` — expected: all suites and the CLI smoke test pass, or fail only where they already failed before this plan (this plan touches no C++, Julia, Dart or JS file).

## Acceptance criteria

- [ ] `upsert_time_series_row`'s docstring no longer contains `No Int->Float coercion`, `D-03` or `all value columns must be provided`. It says every dimension column is required, a left-out value column gets its column default, and integers are accepted for REAL columns. Plan 25's `datetime -> STRING (an aware value is converted to UTC)` clause is kept if present.
- [ ] `upsert_time_series_row`'s `def` line and `upsert_time_series_row_by_label`'s docstring are unchanged by this plan.
- [ ] `_marshal_group_columns`'s summary line names the time-series, vector and set writers.
- [ ] `_c_api.py`'s header comment says the declarations are hand-maintained, excludes `binary/` and `expression/`, and points to `generator/generator.bat`. The `ffi.cdef` block itself is unchanged.
- [ ] `test_describe_runs_without_error` is gone from `test_database_lifecycle.py`; `TestDescribe.test_returns_string` still exists.
- [ ] `test_upsert_time_series_row_int_for_real_and_omitted_columns` exists and passes.
- [ ] The four alignment docstrings are untouched (plan 41).
- [ ] No CHANGELOG or AGENTS.md change.
- [ ] Full Python suite passes; `scripts/format.bat` leaves no diff outside the four files.

## Pitfalls

- **Earlier plans moved these lines.** Plans 24, 25, 27 and 28 all edit `database.py` before this one, and 18/21/22 edit `_c_api.py`. The line numbers above are HEAD values. Find each edit by its quoted phrase: `No Int->Float coercion`, `columnar time series API`, `Phase 1 CFFI declarations`, `def test_describe_runs_without_error`.
- **Keep plan 25's datetime clause.** Rewriting the upsert type line from the HEAD text would silently delete it. Start from the file as it is.
- **Do not drop `bool -> INTEGER (0/1)` after plan 28.** Plan 28 deletes the explicit bool *branch*, but a bool still marshals as INTEGER 1/0 through the `int` branch (`bool` is an `int` subclass), so the docstring stays true.
- **Row order in the new test.** Two upserts with the same `date_time` and different `block` read back in an unspecified order (the reader orders by `date_time` only). Keep the distinct dates.
- **`42 == 42.0`.** Without the `isinstance(..., float)` assertion the int-for-REAL half of the test proves nothing about storage.
- **The header is a Python comment.** Do not move it inside `ffi.cdef("""...""")`, where it would become part of the C text cffi parses.
- **Blank lines after the deleted test.** Leave exactly two blank lines between `test_is_healthy_returns_true` and `test_version_returns_string`, or `ruff format` will change the file in step 6.
- **Line endings.** `.py` files are LF (`.gitattributes`); the Edit tool keeps them. This plan touches no `.bat` file.
- **Stale `__pycache__`.** `bindings/python/src/quiverdb/__pycache__/*.pyc` matches `grep -r "D-03"`. It is not tracked; use `git grep`, not plain `grep -r`, for the stale-text check.
- **Do not "fix" the other D-03 comments.** The ones in `src/database_describe.cpp` and `tests/test_database_ui_metadata.cpp` refer to a different, valid decision.

## Out of scope

- The four "not positionally aligned with read_element_ids" paragraphs in `read_vector_booleans`, `read_vector_date_times`, `read_set_booleans` and `read_set_date_times`, and the same claims in Dart, JS and Julia: **plan 41**.
- The `tests/AGENTS.md` claim that vector/set coverage includes "omission of elements without group rows": **plan 75**.
- The type-dispatch paragraph of `_marshal_group_columns`' docstring (first non-None element vs all cells): **plan 24**.
- Datetime support on `upsert_time_series_row` and its docstring clause: **plan 25**.
- Adding `read_{vector,set}_group_by_id` to `_c_api.py`: **plan 18**. Deleting `quiver_clear_last_error` and the element counter cdefs: **plan 21**. Renaming the query cdefs: **plan 22**.
- The equivalent leftover describe tests in Julia, Dart, JS, C++ and the C API: **plan 67**.
- Documenting or pinning that a `None` kwarg to `upsert_time_series_row` raises `TypeError`, and that a replaced row resets its omitted value columns: not planned. Both are true today and neither is contradicted by any comment.
