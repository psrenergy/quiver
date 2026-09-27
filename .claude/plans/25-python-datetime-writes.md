# 25 — Python: accept datetime on every write path, converting aware values to UTC

**Batch** 4 · **Severity** medium · **Breaking** no (additive, plus a fix: an aware non-UTC `datetime` is now stored as its UTC instant instead of its wall clock) · **Size** S · **Layers** Python binding (`_helpers.py`, `element.py`, `database.py`, tests), `bindings/python/AGENTS.md`, `docs/time_series.md`, `CHANGELOG.md`

**Depends on** none. Plans 17 and 24 run earlier and edit the same functions (see Overlaps). This plan is written to apply on top of either state.
**Overlaps with**
- **24** (runs first) restructures `_marshal_group_columns` and `Element._set_array` to type a column from all of its cells. It may have rewritten the datetime branch this plan edits and the `_marshal_group_columns` docstring. Steps 3.3 and 2.3 say what to do in each case.
- **17** (runs first) rewrites the body of `read_time_series_row` (a mask out-param, frees in `try/finally`). This plan changes only the one `date_time` argument line in that call.
- **21** (runs first) drops a sentence from `check()`'s docstring in `_helpers.py`. This plan only adds an import and a new function at the end of that file.
- **27** (runs after) adds `/` to the `create_element` / `update_element` / `upsert_time_series_row*` signatures. Those are the same methods, but no line is shared.
- **28** (runs after) deletes the `bool` branches in `Element.set`, `Element._set_array` and `_marshal_row_columns`. The datetime lines this plan adds sit next to them and must survive that deletion.
- **29** (runs after) deletes `Element._ensure_valid` and its call at the top of `Element.set`. This plan leaves that call in place.
- **30** (runs after) rewrites the `upsert_time_series_row` docstring's type line and the first line of the `_marshal_group_columns` docstring. It must keep the `datetime -> STRING` wording this plan adds.

## Why

The Python binding converts a `datetime` to the core's DATE_TIME string in two places, and both convert it wrongly. Three other write paths do not accept a `datetime` at all.

**1. An aware value is stored as its wall clock and the offset is dropped.** Both existing conversions use `strftime`, which formats the wall-clock fields and ignores `tzinfo`:

- `bindings/python/src/quiverdb/database.py`, `read_time_series_row` (currently ~L1507):
  ```python
  date_time.strftime("%Y-%m-%dT%H:%M:%S").encode("utf-8"),
  ```
- `database.py`, `_marshal_group_columns`, datetime branch (currently ~L2207-2208). This helper serves every columnar group writer (time series, vector and set, by id and by label):
  ```python
  elif isinstance(first, datetime):
      encoded = [(v.strftime("%Y-%m-%dT%H:%M:%S").encode("utf-8") if v is not None else b"") for v in values]
  ```

Every reader stamps UTC on what it reads back. `_parse_datetime` (currently ~L2095):
```python
return datetime.fromisoformat(s).replace(tzinfo=timezone.utc)
```

Reproduction (verified with CPython 3.14: `datetime(2024,1,1,10,tzinfo=timezone(timedelta(hours=3))).strftime("%Y-%m-%dT%H:%M:%S")` returns `'2024-01-01T10:00:00'`):
```python
plus3 = timezone(timedelta(hours=3))
db.update_time_series_group("Sensor", "readings", eid, {
    "date_time": [datetime(2024, 1, 1, 10, tzinfo=plus3)],   # the instant 07:00 UTC
    "temperature": [20.5], "humidity": [65], "status": ["normal"]})
db.read_time_series_group("Sensor", "readings", eid)["date_time"]
# -> [datetime(2024, 1, 1, 10, 0, tzinfo=timezone.utc)]   three hours off, no error
db.read_time_series_row("Collection", "data", "value", datetime(2024, 1, 1, 11, tzinfo=plus3))
# looks up 11:00, not 08:00 UTC, so it returns the wrong row
```
`bindings/python/AGENTS.md` (the `_parse_datetime` bullet) records the same bug as fixed on the read side ("`"...T10:30:00+03:00"` came back as `10:30Z`, three hours off"). The write side still has it.

**2. Three write paths reject `datetime`, although every reader returns one.**
- `bindings/python/src/quiverdb/element.py`, `Element.set` (currently ~L18-40), handles None, bool, int, float, str and list. Anything else hits:
  ```python
  raise TypeError(f"Unsupported type {type(value).__name__} for Element.set('{name}')")
  ```
  `Element._set_array` (currently ~L58-75) has no datetime branch either (`"Unsupported array element type ..."`). `create_element`, `update_element` and `update_element_by_label` (database.py ~L206-268) all go through `Element.set`.
- `database.py`, `_marshal_row_columns` (currently ~L2239-2291), which serves `upsert_time_series_row` and `upsert_time_series_row_by_label`, handles only bool, int, float and str:
  ```python
  raise TypeError(
      f"Column '{name}' value has unsupported type {type(v).__name__}; expected int, float, or str"
  )
  ```

So `db.create_element("Configuration", label="b", date_attribute=db.read_scalar_date_time_by_id("Configuration", "date_attribute", a))` raises `TypeError`, and so does `db.upsert_time_series_row("Collection", "data", eid, date_time=datetime(2024, 1, 1), value=1.0)`. Yet `update_time_series_group` accepts the same `datetime` for the same column. Julia (`bindings/julia/src/element.jl` `setindex!(::DateTime)` and `::Vector{DateTime}`, `database_update.jl` `_upsert_row_columns` `v isa DateTime`) and Dart (`element.dart` `case DateTime v` and `List<DateTime>`, `database_update.dart` `_marshalGroupColumn`, which also serves `upsertTimeSeriesRow`) accept their native datetime on all three paths.

Principles violated: **Homogeneity** (the binding surface should be uniform, both within Python and across bindings) and correctness of pre-FFI marshalling, which is the binding's own job. No Design Decision or "Do Not Fix" entry makes Python's datetime writes string-only. The only string-only datetime decision is the one for JS.

## Constraints and decisions

- **Maintainer decisions for this item (binding):**
  - Add one `format_datetime` helper in `bindings/python/src/quiverdb/_helpers.py`. It cannot go in `database.py`: `database.py` imports `Element` from `element.py` (database.py ~L13), so `element.py` importing from `database.py` would create an import cycle.
  - Convert an aware value to UTC when `v.utcoffset() is not None`, because the readers return UTC.
  - Use the helper at every site, including the two existing `strftime` calls (`_marshal_group_columns` and `read_time_series_row`).
  - **Do not touch `_marshal_params`.** Query parameters stay datetime-free, as they are in Julia (`database_query.jl`) and Dart (`database_query.dart`).
- Root AGENTS.md, **Error Messages**: a pre-FFI type-marshalling error may be crafted locally and should name the offending column and type. The one message this plan changes (`_marshal_row_columns`' `TypeError`) keeps that shape.
- Root AGENTS.md, **"A DATE_TIME string is validated on write, and stored verbatim"**: the core never normalizes. The binding sends the exact grammar `YYYY-MM-DDTHH:MM:SS`, and the core's `datetime::is_valid_iso8601` still validates it, so no validation is added in Python.
- Root AGENTS.md, **"JS keeps a string-based datetime surface"**: JS is unaffected. Julia's `DateTime` has no zone and Dart writes its `DateTime`'s own fields. Neither is changed here.
- Root AGENTS.md, **Intelligence / thin bindings**: formatting a native value into the string the C API takes is marshalling, not schema-dependent coercion. The same helper is used whatever the column type.
- `bindings/python/AGENTS.md`: the `_marshal_row_columns` note ("each kwarg is a scalar wrapped in a 1-element typed array ... a `None` kwarg raises `TypeError` here") stays true. The `None` rejection does not change.
- `pyproject.toml` has `requires-python = ">=3.13"`.

Alternatives considered and rejected:
- **Keep `strftime("%Y-%m-%dT%H:%M:%S")` in the helper** (one verifier's minimal version). Rejected: it keeps the three-hour offset bug, and the maintainer chose UTC conversion.
- **Test `if v.tzinfo:`** (the original proposal). Rejected: a `tzinfo` whose `utcoffset()` returns `None` makes the value naive by Python's definition, and the maintainer specified `utcoffset() is not None`.
- **Call `astimezone(timezone.utc)` unconditionally.** Rejected: on a naive value it assumes the host's local zone, so the stored string would depend on the machine's TZ.
- **The "`%Y` does not zero-pad on glibc" justification for `isoformat`.** Dropped: CPython 3.13 already pads `%Y` to four digits (gh-120713), and on Windows CPython 3.14 `datetime(999,1,1).strftime('%Y')` gives `0999`. `isoformat(timespec="seconds")` is still used because it produces the exact grammar without depending on the platform's `strftime`.
- **Put the helper in `database.py` as `_format_datetime`.** Rejected: the import cycle described above.
- **Accept `datetime` in query parameters.** Rejected by the maintainer, and consistent with Julia and Dart.

## Changes

Before starting, run `git grep -n "strftime\|isoformat" bindings/python/src/quiverdb`. At HEAD 58dfe7a it lists exactly `database.py` `read_time_series_row` and `_marshal_group_columns` (plus a comment at ~L2066 that mentions `fromisoformat`). If plan 24 moved the group-marshaller formatting into a new helper, that new site also appears in this grep. Every `strftime` hit must become a `format_datetime` call.

### 1. `bindings/python/src/quiverdb/_helpers.py`

**1.1 Imports.** Current top of file:
```python
from __future__ import annotations

from quiverdb._c_api import ffi, get_lib
from quiverdb.exceptions import QuiverError
```
New:
```python
from __future__ import annotations

from datetime import datetime, timezone

from quiverdb._c_api import ffi, get_lib
from quiverdb.exceptions import QuiverError
```

**1.2 Append the helper** after `decode_string_or_none`, at the end of the file:
```python
def format_datetime(value: datetime) -> str:
    """Format a datetime as the core's DATE_TIME string, YYYY-MM-DDTHH:MM:SS.

    An aware value is converted to UTC first, the zone every datetime reader returns, so
    10:00+03:00 is written as 07:00. A naive value is written as given. Sub-second precision
    is truncated; the core's grammar has none.
    """
    if value.utcoffset() is not None:
        value = value.astimezone(timezone.utc).replace(tzinfo=None)
    return value.isoformat(timespec="seconds")
```
Why: this is the only datetime-to-string conversion on the write side. `.replace(tzinfo=None)` is required, because an aware value's `isoformat` would append `+00:00`, which the core's grammar rejects. Behaviour verified with CPython 3.14:

| Input | Output |
| --- | --- |
| `datetime(2024,1,1,10,tzinfo=+03:00)` | `2024-01-01T07:00:00` |
| `datetime(2024,1,1,0,30,tzinfo=+03:00)` | `2023-12-31T21:30:00` |
| `datetime(2024,1,1,10)` | `2024-01-01T10:00:00` |
| `datetime(2024,1,1,10,30,15,999999)` | `2024-01-01T10:30:15` |
| `datetime(999,1,1)` | `0999-01-01T00:00:00` |
| a `tzinfo` whose `utcoffset()` returns `None` | treated as naive |

### 2. `bindings/python/src/quiverdb/element.py`

**2.1 Imports.** Current:
```python
from __future__ import annotations

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, decode_string
from quiverdb.exceptions import QuiverError
```
New (`datetime` is needed at runtime for `isinstance`, even under `from __future__ import annotations`):
```python
from __future__ import annotations

from datetime import datetime

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, decode_string, format_datetime
from quiverdb.exceptions import QuiverError
```
If plan 29 has already removed the `QuiverError` import, keep it removed. This plan does not need it.

**2.2 `Element.set`: docstring and scalar branch.** Current:
```python
        """Set an attribute value. Returns self for fluent chaining.

        Supported types: int, float, str, None, bool (stored as int),
        list[int], list[float], list[str].
        """
        ...
        elif isinstance(value, str):
            self._set_string(name, value)
        elif isinstance(value, list):
            self._set_array(name, value)
```
New:
```python
        """Set an attribute value. Returns self for fluent chaining.

        Supported types: int, float, str, None, bool (stored as int),
        datetime (stored as a DATE_TIME string, see format_datetime),
        list[int], list[float], list[str], list[datetime].
        """
        ...
        elif isinstance(value, str):
            self._set_string(name, value)
        elif isinstance(value, datetime):
            self._set_string(name, format_datetime(value))
        elif isinstance(value, list):
            self._set_array(name, value)
```
Leave every other branch as it is: the `None`, `bool`, `int` and `float` branches, the `self._ensure_valid()` call and the final `else: raise TypeError(...)`.

**2.3 `Element._set_array`: list branch.** Current (HEAD 58dfe7a):
```python
        first = values[0]
        if isinstance(first, bool):
            self._set_array_integer(name, [int(v) for v in values])
        elif isinstance(first, int):
            self._set_array_integer(name, values)
        elif isinstance(first, float):
            self._set_array_float(name, values)
        elif isinstance(first, str):
            self._set_array_string(name, values)
        else:
            raise TypeError(f"Unsupported array element type {type(first).__name__} for Element.set('{name}')")
```
New (add one `elif` before the `else`):
```python
        elif isinstance(first, str):
            self._set_array_string(name, values)
        elif isinstance(first, datetime):
            self._set_array_string(name, [format_datetime(v) for v in values])
        else:
            raise TypeError(f"Unsupported array element type {type(first).__name__} for Element.set('{name}')")
```
**If plan 24 has already reshaped `_set_array`** (for example, typing the list from all of its cells and raising a `TypeError` that names the cell for anything non-numeric), keep its structure and add the equivalent rule ahead of the branch that raises: a list whose cells are all `datetime` goes to `self._set_array_string(name, [format_datetime(v) for v in values])`. If plan 24 already added a datetime or string-or-datetime case, replace its formatting expression with `format_datetime(v)` rather than adding a second branch. The outcome to check is step 5.4's list test.

### 3. `bindings/python/src/quiverdb/database.py`

**3.1 Import.** Current (~L10):
```python
from quiverdb._helpers import check, decode_string, decode_string_or_none
```
New:
```python
from quiverdb._helpers import check, decode_string, decode_string_or_none, format_datetime
```
Keep `from datetime import datetime, timezone` (~L6). `_parse_datetime` still uses both.

**3.2 `read_time_series_row`.** Current argument line inside the `lib.quiver_database_read_time_series_row(...)` call (~L1507):
```python
                date_time.strftime("%Y-%m-%dT%H:%M:%S").encode("utf-8"),
```
New:
```python
                format_datetime(date_time).encode("utf-8"),
```
Plan 17 may have changed the rest of this call and body (an added `out_mask` argument, `try/finally` frees). Change only this argument line.

Docstring. Current:
```python
        """Read one value per element for a time series attribute at a given date.

        Uses "last non-null value at or before date_time" lookup semantics.
```
New (add one sentence to the second paragraph and leave any text plan 17 added below it):
```python
        """Read one value per element for a time series attribute at a given date.

        Uses "last non-null value at or before date_time" lookup semantics. An aware
        date_time is converted to UTC first; a naive one is taken as UTC.
```

**3.3 `_marshal_group_columns`: datetime branch.** Current (~L2207-2208):
```python
        elif isinstance(first, datetime):
            encoded = [(v.strftime("%Y-%m-%dT%H:%M:%S").encode("utf-8") if v is not None else b"") for v in values]
```
New:
```python
        elif isinstance(first, datetime):
            encoded = [(format_datetime(v).encode("utf-8") if v is not None else b"") for v in values]
```
**If plan 24 restructured this function**, wherever it now turns a `datetime` cell into text (a separate datetime branch, or a string branch that formats datetime cells one by one), replace that `v.strftime("%Y-%m-%dT%H:%M:%S")` expression with `format_datetime(v)`. Change nothing else in its typing logic.

Docstring. The current dispatch sentence (~L2169) is:
```
    Column types are dispatched on the first non-None element: datetime/str ->
    STRING, bool/int -> INTEGER, float -> FLOAT. ...
```
Change `datetime/str -> STRING` to `datetime/str -> STRING (datetime via format_datetime, aware values converted to UTC)`. If plan 24 rewrote that sentence, append the same parenthetical to whatever phrase now says datetime becomes STRING.

**3.4 `_marshal_row_columns`: new datetime normalization.** Current loop head (~L2256-2264):
```python
    for i, (name, v) in enumerate(kwargs.items()):
        name_buf = ffi.new("char[]", name.encode("utf-8"))
        keepalive.append(name_buf)
        c_col_names[i] = name_buf

        # bool is a subclass of int; test it explicitly first so True/False
        # marshal as INTEGER 1/0 rather than being rejected by the `is int`
        # check. Mirrors `_marshal_params` policy in this same file.
        if isinstance(v, bool):
```
New (insert two lines before the dispatch chain, so the existing `str` branch marshals the formatted string and none of its code is duplicated):
```python
    for i, (name, v) in enumerate(kwargs.items()):
        name_buf = ffi.new("char[]", name.encode("utf-8"))
        keepalive.append(name_buf)
        c_col_names[i] = name_buf

        if isinstance(v, datetime):
            v = format_datetime(v)  # marshalled by the str branch below

        # bool is a subclass of int; test it explicitly first so True/False
        # marshal as INTEGER 1/0 rather than being rejected by the `is int`
        # check. Mirrors `_marshal_params` policy in this same file.
        if isinstance(v, bool):
```
(Plan 28 later deletes the bool comment and branch. The two new lines are independent of them.)

Error text. Current (~L2286-2289):
```python
            raise TypeError(
                f"Column '{name}' value has unsupported type {type(v).__name__}; expected int, float, or str"
            )
```
New:
```python
            raise TypeError(
                f"Column '{name}' value has unsupported type {type(v).__name__}; expected int, float, str, or datetime"
            )
```
No test pins the old text (`git grep -n "expected int, float, or str" bindings/python/tests` is empty).

Docstring. Current first paragraph:
```
    Each value is wrapped in a 1-element typed array. Not `_marshal_group_columns`: the
```
New:
```
    Each value is wrapped in a 1-element typed array; a datetime is formatted by
    format_datetime and sent as a string. Not `_marshal_group_columns`: the
```

**3.5 Public docstrings.**
- `update_time_series_group` (~L1542-1543). Current:
  ```
          Pass an empty dict to clear all rows. datetime values are formatted to
          ISO strings; integers are accepted for REAL columns.
  ```
  New:
  ```
          Pass an empty dict to clear all rows. datetime values are formatted to
          ISO strings (an aware value is converted to UTC); integers are accepted
          for REAL columns.
  ```
- `upsert_time_series_row` (~L1788-1791). Current:
  ```
          date_time) and all value columns must be provided. Type dispatch uses
          isinstance: bool -> INTEGER (0/1), int -> INTEGER, float -> FLOAT, str ->
          STRING. No Int->Float coercion (per D-03: Python strict typing).
  ```
  New (add datetime only. The stale "No Int->Float coercion" sentence belongs to plan 30, so leave it):
  ```
          date_time) and all value columns must be provided. Type dispatch uses
          isinstance: bool -> INTEGER (0/1), int -> INTEGER, float -> FLOAT, str ->
          STRING, datetime -> STRING (an aware value is converted to UTC).
          No Int->Float coercion (per D-03: Python strict typing).
  ```

### 4. Layers with no change (say so in the PR)

- **C++ core and C API: no change.** The core already validates and stores the string it receives. `format_datetime` output always matches `datetime::is_valid_iso8601` for years 0001-9999.
- **FFI declarations:** no C signature changes, so there is nothing to regenerate. That covers Julia `c_api.jl`, Dart `bindings.dart`, Python `_c_api.py` and JS `loader.ts`.
- **Julia, Dart, JS, Lua: no change.** Julia and Dart already accept their native datetime on these paths. JS is string-only by decision. Lua takes strings.

## Tests

All of the tests below are Python, because Python is the only layer whose behaviour changes. Add `timedelta` to the `datetime` import in each of the three test files touched, so it reads `from datetime import datetime, timedelta, timezone`.

### 5. `bindings/python/tests/test_database_create.py`

Change the import (~L5) from `from datetime import datetime, timezone` to `from datetime import datetime, timedelta, timezone`. Append a new class after `TestCreateScalarDateTime`, at the end of the file:

```python
class TestCreateWithDatetime:
    def test_aware_datetime_stored_as_utc_instant(self, db: Database) -> None:
        # Every reader returns UTC, so an aware value is converted to UTC on the way in. Writing its
        # wall clock would read back three hours off.
        written = datetime(2024, 1, 1, 10, tzinfo=timezone(timedelta(hours=3)))
        elem_id = db.create_element("Configuration", label="cfg", date_attribute=written)

        assert db.read_scalar_string_by_id("Configuration", "date_attribute", elem_id) == "2024-01-01T07:00:00"
        assert db.read_scalar_date_time_by_id("Configuration", "date_attribute", elem_id) == datetime(
            2024, 1, 1, 7, tzinfo=timezone.utc
        )

    def test_naive_datetime_stored_as_written(self, db: Database) -> None:
        elem_id = db.create_element("Configuration", label="cfg", date_attribute=datetime(2024, 1, 15, 10, 30))
        assert db.read_scalar_string_by_id("Configuration", "date_attribute", elem_id) == "2024-01-15T10:30:00"

    def test_read_back_datetime_writes_back(self, db: Database) -> None:
        # A read-modify-write round trip: the reader's UTC-aware datetime is a valid write value.
        source = db.create_element("Configuration", label="a", date_attribute="2024-01-15T10:30:00")
        value = db.read_scalar_date_time_by_id("Configuration", "date_attribute", source)

        copy = db.create_element("Configuration", label="b", date_attribute=value)
        assert db.read_scalar_string_by_id("Configuration", "date_attribute", copy) == "2024-01-15T10:30:00"

        db.update_element("Configuration", copy, date_attribute=datetime(2025, 6, 1, tzinfo=timezone.utc))
        assert db.read_scalar_string_by_id("Configuration", "date_attribute", copy) == "2025-06-01T00:00:00"

    def test_datetime_list_stored_as_strings(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element(
            "AllTypes",
            label="item1",
            label_value=[datetime(2024, 1, 15, 10, 30), datetime(2024, 1, 16, tzinfo=timezone(timedelta(hours=3)))],
        )
        assert all_types_db.read_vector_strings_by_id("AllTypes", "label_value", elem_id) == [
            "2024-01-15T10:30:00",
            "2024-01-15T21:00:00",
        ]
```
Fixtures: `db` uses `tests/schemas/valid/basic.sql`, where `Configuration.date_attribute TEXT` is DATE_TIME by its `date_` prefix, so the core's write gate runs. `all_types_db` uses `tests/schemas/valid/all_types.sql`, whose `AllTypes_vector_labels.label_value TEXT NOT NULL` is the only string vector column in the fixtures. No schema has a `date_`-prefixed vector column, and none is needed.

Before the fix, the first three tests raise `TypeError: Unsupported type datetime for Element.set('date_attribute')`, and the fourth raises `TypeError: Unsupported array element type datetime for Element.set('label_value')`.

### 6. `bindings/python/tests/test_database_time_series_group.py`

Change the import (~L5) to `from datetime import datetime, timedelta, timezone`. In `class TestUpdateTimeSeriesGroup`, add this test right after `test_update_time_series_group_round_trip`:

```python
    def test_update_time_series_group_converts_aware_datetime_to_utc(self, mixed_time_series_db: Database) -> None:
        """An aware datetime is stored as its UTC instant, the zone read_time_series_group returns."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        data = {
            "date_time": [datetime(2024, 1, 1, 10, tzinfo=timezone(timedelta(hours=3)))],
            "temperature": [20.5],
            "humidity": [65],
            "status": ["normal"],
        }
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, data)

        stored = mixed_time_series_db.query_string(
            "SELECT date_time FROM Sensor_time_series_readings WHERE id = ?", parameters=[eid]
        )
        assert stored == "2024-01-01T07:00:00"
        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["date_time"] == [datetime(2024, 1, 1, 7, tzinfo=timezone.utc)]
```
Fixture: `mixed_time_series_db` uses `tests/schemas/valid/mixed_time_series.sql`, `Sensor_time_series_readings`. Before the fix, `stored` is `"2024-01-01T10:00:00"`.

The existing `TestReadTimeSeriesGroup.test_update_time_series_group_accepts_datetime_values` writes a naive `datetime(2024, 1, 1)` and must still pass unchanged, because a naive value is formatted to the same string as before.

### 7. `bindings/python/tests/test_database_time_series_row.py`

Change the import (~L5) to `from datetime import datetime, timedelta, timezone`.

**7.1** In `class TestUpsertTimeSeriesRow`, add after `test_upsert_time_series_row_dict_unpacking`:
```python
    def test_upsert_time_series_row_accepts_datetime(self, collections_db: Database) -> None:
        """A datetime dimension value is accepted by both upserts; an aware one is stored as UTC."""
        eid = _create_collection_element(collections_db, "Item1")
        aware = datetime(2024, 1, 1, 10, tzinfo=timezone(timedelta(hours=3)))
        collections_db.upsert_time_series_row("Collection", "data", eid, date_time=aware, value=10.0)
        collections_db.upsert_time_series_row_by_label(
            "Collection", "data", "Item1", date_time=datetime(2024, 1, 2), value=20.0
        )

        result = collections_db.read_time_series_group("Collection", "data", eid)
        assert result["date_time"] == [datetime(2024, 1, 1, 7, tzinfo=timezone.utc), _utc(2024, 1, 2)]
        assert result["value"] == [10.0, 20.0]
```
Before the fix, this raises `TypeError: Column 'date_time' value has unsupported type datetime; expected int, float, or str`.

**7.2** In `class TestReadTimeSeriesRow`, add after `test_read_time_series_row_returns_one_value_per_element`:
```python
    def test_read_time_series_row_converts_aware_datetime_to_utc(self, collections_db: Database) -> None:
        """An aware date_time is looked up at its UTC instant."""
        eid = _create_collection_element(collections_db, "Item 1")
        collections_db.update_time_series_group(
            "Collection",
            "data",
            eid,
            {"date_time": ["2024-01-01T07:00:00", "2024-01-01T09:00:00"], "value": [10.5, 20.5]},
        )

        # 11:00+03:00 is 08:00 UTC, so the 07:00 row is the last at or before it. Formatting the
        # wall clock would look up 11:00 and return the 09:00 row.
        at = datetime(2024, 1, 1, 11, tzinfo=timezone(timedelta(hours=3)))
        assert collections_db.read_time_series_row("Collection", "data", "value", at) == [10.5]
```
Fixture: `collections_db` uses `tests/schemas/valid/collections.sql`, `Collection_time_series_data (id, date_time, value REAL)`. Before the fix, this returns `[20.5]`. The element has data at the requested instant, so plan 17's None-for-no-data change does not affect the expected value.

### 8. Existing tests: none change

- `tests/test_element.py::test_element_set_unsupported_type_raises` uses `{}` and still raises `Unsupported type`.
- `tests/test_database_query.py::test_marshal_params_unsupported_type_raises_typeerror` is untouched, because `_marshal_params` is out of scope.
- `tests/test_database_boolean.py::test_boolean_input` is unaffected.
- No other layer (C++, C API, Lua, Julia, Dart, JS) gets a test: none of them changes behaviour.

## Docs and changelog

**`bindings/python/AGENTS.md`**
1. Layout block. Old: `  _helpers.py     # Shared check()/decode_string helpers`. New: `  _helpers.py     # Shared check()/decode_string/format_datetime helpers`.
2. Insert a new bullet directly after the `_parse_datetime` bullet, which ends "...so that note is the only guard against a "remove the redundant overloads" cleanup.":
   ```markdown
   - **`format_datetime` (`_helpers.py`) is the write side's only datetime → string conversion.**
     `Element.set` (scalar and list), `_marshal_group_columns`, `_marshal_row_columns` and
     `read_time_series_row`'s `date_time` all call it. An aware value (`utcoffset()` not `None`) is
     converted to UTC before formatting, because every reader stamps UTC: formatting the wall clock,
     as `strftime` used to, stored `10:00+03:00` as `10:00` and read it back as `10:00Z` — the
     write-side twin of the `_parse_datetime` bug above. A naive value is written as given (never
     `astimezone` it — that would assume the host's zone). It lives in `_helpers.py`, not
     `database.py`, because `database.py` imports `element.py`. Query parameters (`_marshal_params`)
     deliberately take no datetime, as in Julia and Dart.
   ```
3. `_marshal_row_columns` bullet. Old: `` `_by_label` form — each kwarg is a scalar wrapped in a 1-element typed array. Kept separate ``. New: `` `_by_label` form — each kwarg is a scalar wrapped in a 1-element typed array (a `datetime` is formatted by `format_datetime` first). Kept separate ``. Re-wrap the paragraph to about 100 columns.

**Root `AGENTS.md`: no edit.** No passage describes Python's write-side datetime handling. The "DateTime wrappers" table lists readers only, and the JS string-datetime decision is untouched.

**`docs/time_series.md`** (~L24-26). Old:
```
text (`YYYY-MM-DDTHH:MM:SS`). The bindings convert their native datetime types to and from
this format automatically.
```
New:
```
text (`YYYY-MM-DDTHH:MM:SS`). The bindings convert their native datetime types to and from
this format automatically. Python converts a timezone-aware `datetime` to UTC first, the zone
its readers return; a naive one is written as given.
```

**`CHANGELOG.md`**: add under `## [0.11.0] — unreleased` → `### Fixed`, as the first bullet of that section. It is not BREAKING.
```markdown
- **Python: a `datetime` is accepted on every write path, and an aware one is stored as its UTC
  instant.** `create_element`, `update_element` and `update_element_by_label` (scalar and list
  attributes) and `upsert_time_series_row` / `upsert_time_series_row_by_label` raised `TypeError`
  for a `datetime`, although every reader returns one, so a value read back could not be written
  back. The group writers and `read_time_series_row` did take one but formatted its wall clock and
  dropped the offset, while the readers stamp UTC: `10:00+03:00` was stored as `10:00` and read
  back as `10:00Z`, three hours off, and `read_time_series_row` looked up the wrong instant the
  same way. An aware value is now converted to UTC (`07:00`); a naive one is written as given.
  Rows written earlier from an aware non-UTC value keep the wall-clock time they were stored with.
```

## Verification

Run from the repo root (`C:\Development\Quiver\quiver3`) in PowerShell.

1. `git grep -n "strftime" bindings/python/src/quiverdb` → no output.
2. `cmake --build build --config Debug`: no C++ changes, but the Python tests load `build/bin` DLLs, and earlier plans change C++.
3. `bindings\python\tests\test.bat -k "datetime or utc"` → all selected tests pass, including these new ones:
   - `test_database_create.py::TestCreateWithDatetime::test_aware_datetime_stored_as_utc_instant`
   - `test_database_create.py::TestCreateWithDatetime::test_naive_datetime_stored_as_written`
   - `test_database_create.py::TestCreateWithDatetime::test_read_back_datetime_writes_back`
   - `test_database_create.py::TestCreateWithDatetime::test_datetime_list_stored_as_strings`
   - `test_database_time_series_group.py::TestUpdateTimeSeriesGroup::test_update_time_series_group_converts_aware_datetime_to_utc`
   - `test_database_time_series_row.py::TestUpsertTimeSeriesRow::test_upsert_time_series_row_accepts_datetime`
   - `test_database_time_series_row.py::TestReadTimeSeriesRow::test_read_time_series_row_converts_aware_datetime_to_utc`

   To confirm the tests have teeth, stash only this plan's source edits (`git stash push -- bindings/python/src/quiverdb/_helpers.py bindings/python/src/quiverdb/element.py bindings/python/src/quiverdb/database.py`), re-run step 3, and check that exactly these seven fail with the errors listed under Tests. Then `git stash pop`. Earlier plans are expected to be committed already. If uncommitted changes from them sit in those three files, skip this check rather than stash their work.
4. `bindings\python\tests\test.bat` → the full Python suite passes.
5. `scripts\format.bat`, or at least `bindings\python\format.bat` (ruff format). Then `git diff --stat` should list only the files named in this plan.
6. `scripts\test-all.bat` → every suite and the CLI smoke test pass. This plan cannot affect the non-Python suites. Run it anyway, because it is the gate.

## Acceptance criteria

- [ ] `format_datetime(value: datetime) -> str` exists in `bindings/python/src/quiverdb/_helpers.py` with the body in step 1.2. It converts to UTC only when `value.utcoffset() is not None`.
- [ ] `Element.set` accepts a `datetime` scalar, and `Element._set_array` accepts a list of `datetime`. Both go through `format_datetime`.
- [ ] `_marshal_row_columns` accepts a `datetime`, and its `TypeError` text lists `datetime`.
- [ ] `_marshal_group_columns` and `read_time_series_row` call `format_datetime`. No `strftime` remains in `bindings/python/src/quiverdb`.
- [ ] `_marshal_params` is unchanged.
- [ ] The seven new tests pass, and each of them fails on the pre-change sources.
- [ ] The full Python suite and `scripts/test-all.bat` pass.
- [ ] `bindings/python/AGENTS.md` (layout line, new bullet, `_marshal_row_columns` note), `docs/time_series.md` and `CHANGELOG.md` (`0.11.0` → `### Fixed`) are updated as specified.
- [ ] No C++, C API, Julia, Dart, JS or Lua file is changed.

## Pitfalls

- **Import cycle.** `database.py` imports `element.py`, so the helper must live in `_helpers.py`. Putting it in `database.py` and importing it from `element.py` fails at import time.
- **`from __future__ import annotations` does not make the `datetime` import optional** in `element.py`. The class is used at runtime by `isinstance`.
- **Never `astimezone` a naive value.** Python would interpret it in the host's local zone, so the tests would pass on a UTC CI runner and fail on a developer machine (or the reverse).
- **`.replace(tzinfo=None)` after `astimezone(timezone.utc)` is required.** Without it `isoformat` emits `+00:00`, and the core rejects the value as an invalid DATE_TIME (Pattern 1).
- **Aware `datetime` equality compares instants**, so `read == datetime(2024,1,1,10,tzinfo=+03:00)` is true against a correct `07:00Z` read. That is why the tests also assert the stored string (`read_scalar_string_by_id` / `query_string`), which is where the bug shows.
- **Plan 24 may have moved code.** Anchor on `strftime` and `isinstance(..., datetime)` with `git grep`, not on line numbers. Do not re-introduce a first-cell dispatch that plan 24 removed.
- **Mixed `str`/`datetime` cells in one column or list** are not handled by this plan (see Out of scope). Do not add a per-cell fallback here.
- **`datetime.min` with a positive offset** (`datetime(1,1,1,1,tzinfo=+03:00)`) raises Python's own `OverflowError` in `astimezone`. It is out of the grammar's range anyway. Do not catch it.
- **Microseconds are truncated, not rounded.** This is the same behaviour as the old `strftime`. The core's grammar has no fractional seconds.
- **ruff isort** (`ruff.toml` `select = ["I"]`) wants the stdlib `from datetime import ...` block separated from the first-party `quiverdb` block by one blank line, as shown. Line length is 120.
- Do not edit any `.bat` file. If one is touched by accident, restore its CRLF line endings.

## Out of scope

- `_marshal_params` / query parameters accepting `datetime`: excluded by maintainer decision, and consistent with Julia and Dart.
- Typing a group column or element list from all of its cells, including a `str`/`datetime` mix in one column: **plan 24**.
- The stale `No Int->Float coercion (per D-03 ...)` sentence in the `upsert_time_series_row` docstring, and the "columnar time series API" wording in the `_marshal_group_columns` docstring: **plan 30**.
- Deleting the redundant `bool` branches next to the new datetime lines: **plan 28**.
- Deleting `Element._ensure_valid` / `Element.clear`: **plan 29**.
- Positional-only `/` on the `**kwargs` methods: **plan 27**.
- `read_time_series_row` no-data `None` via a C API mask: **plan 17**.
- Renaming `read_{vector,set}_date_time_by_id`: **plan 42**. The new tests deliberately do not call them.
- Accepting `datetime.date`, which is not a `datetime` subclass: not requested. It still raises `TypeError`.
- Time-zone handling in Dart (`dateTimeToString` writes the `DateTime`'s own fields) and Julia (`DateTime` has no zone): unchanged, since each is consistent with its own readers.
- JS: string-based datetime surface by Design Decision.
