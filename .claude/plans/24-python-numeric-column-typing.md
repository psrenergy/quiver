# 24 — Python (+Dart): type numeric columns from all cells, never coerce with int()/float()

**Batch** 4 · **Severity** high (silent data loss) · **Breaking** yes, narrowly: Python callers that
relied on a `str` cell among numbers being parsed (`[1, "7"]`), or on a float among ints being
truncated into an INTEGER column, now get an error; Dart callers that caught `ArgumentError` for
`[1, 2.5]` now get success (REAL column) or `DatabaseException` (INTEGER column) · **Size** M ·
**Layers** Python binding, Dart binding, JS (one comment + one parity test), docs

**Depends on** none · **Overlaps with** 25, 28, 29, 30 (same Python functions — see "Out of
scope"), 31, 33 (`bindings/js/src/group-columns.ts`, the comment edited here), 38, 39 (same Dart
files), 18, 20 (run earlier and touch the same files, so line numbers below will have shifted)

## Why

Python's group-column marshaller picks a column's type from its **first** non-`None` cell and then
forces every other cell into that type with Python's own converters.
`bindings/python/src/quiverdb/database.py`, `_marshal_group_columns` (currently ~L2200-2234):

```python
        first = next((v for v in values if v is not None), None)
        ...
        elif isinstance(first, bool) or isinstance(first, int):
            arr = ffi.new("int64_t[]", [int(v) if v is not None else 0 for v in values])
        ...
        elif isinstance(first, float):
            arr = ffi.new("double[]", [float(v) if v is not None else 0.0 for v in values])
```

Reproduction (`tests/schemas/valid/all_types.sql`, `AllTypes_vector_scores.score REAL`):

```python
eid = db.create_element("AllTypes", label="x")
db.update_vector_group("AllTypes", "scores", eid, {"score": [1, 2.5]})
db.read_vector_floats_by_id("AllTypes", "score", eid)   # -> [1.0, 2.0]   (2.5 lost, no error)
```

The first cell is an `int`, so the column goes out tagged INTEGER as `[1, 2]` (`int(2.5) == 2`), and
the core's int-for-REAL rule accepts it. All six columnar writers share this helper
(`update_time_series_group`, `update_vector_group`, `update_set_group` and each `_by_label` form).
Related inputs go wrong the same way:

| Input column | Today | Should be |
| --- | --- | --- |
| `[1, 2.5]` into REAL | stores `1.0, 2.0` silently | `1.0, 2.5` |
| `[1, 2.5]` into INTEGER | stores `1, 2` silently (the core rejects floats in INTEGER, but it never sees the float) | rejected by the core |
| `[True, 2.5]` | `1, 2` | `1.0, 2.5` |
| `[1, "7"]` | `int("7")`, stores 7 | `TypeError` naming cell 1 |
| `[1.5, "2"]` | `float("2")`, stores 2.0 | `TypeError` naming cell 1 |
| `["a", 1]` | `AttributeError: 'int' object has no attribute 'encode'` | `TypeError` naming cell 1 and the column |
| `[1.5, "x"]` | `ValueError: could not convert string to float` | `TypeError` naming cell 1 |

The Element path (`create_element` / `update_element` arrays) dispatches on `values[0]` the same way.
`bindings/python/src/quiverdb/element.py`, `Element._set_array` (currently ~L65-75):

```python
        first = values[0]
        if isinstance(first, bool):
            self._set_array_integer(name, [int(v) for v in values])
        elif isinstance(first, int):
            self._set_array_integer(name, values)
```

So `create_element(..., score=[True, 2.5])` silently writes `[1, 2]`, `[True, "7"]` writes `[1, 7]`,
and `[1, 2.5]` fails inside cffi with `TypeError: an integer is required`, a message that names
neither the attribute nor the cell.

Dart does not lose data, but it disagrees on the same literal.
`bindings/dart/lib/src/database_update.dart`, `_marshalGroupColumn` (currently ~L758) and
`bindings/dart/lib/src/element.dart`, `_setMixedList` (currently ~L111) take the INTEGER branch
from a leading int, so `[1, 2.5]` throws `ArgumentError: Unsupported value type double in cell 1 of
column 'score'`. JS (`group-columns.ts`, `nonNull.every(Number.isInteger)`) and Julia (the
`[1, 2.5]` literal is already a `Vector{Float64}`) widen the column to FLOAT. The same data therefore
means three different things across the bindings.

Principles this violates:
- Root `CLAUDE.md`, Design Decisions, "One scalar typing policy lives in C++": the core rejects a
  float in an INTEGER column, but Python truncates the float before the core sees it. `int("7")`
  parses text before the core's TEXT/FK typing runs.
- Root **Error Messages**: a pre-FFI marshalling error "should name the offending column and type".
  `AttributeError` and cffi's `an integer is required` name neither.
- Root **Homogeneity**: `[1, 2.5]` should mean the same thing in every binding.

I checked the facts: `ffi.new('int64_t[]', [True, 2, False])` gives `[1, 2, 0]`, and
`ffi.new('double[]', [1, 2.5, True, 0.0])` gives `[1.0, 2.5, 1.0, 0.0]`, both run in
`bindings/python/.venv`. So once every cell's type has been checked, cffi does the int/bool →
int64 and int/bool/float → double conversion itself, and no `int()`/`float()` call is needed.

## Constraints and decisions

- **Maintainer decision (binding):** widen to FLOAT if any non-`None` cell is a `float`; bool/int
  otherwise give INTEGER, with a bool as 1/0. A non-numeric cell in a numeric column raises
  `TypeError` naming the cell and the column. Never `int()`/`float()` a `str`. Apply the rule to
  both `_marshal_group_columns` and `Element._set_array`. Give Dart `_marshalGroupColumn` the same
  whole-column rule. Fix the Dart and JS comments that cite Python's `int(v)`. Update
  `bindings/python/CLAUDE.md` and `bindings/dart/CLAUDE.md`. Plan 38 touches the same Dart file
  (see ordering below).
- **Dart `Element._setMixedList` also changes.** This goes one step past the literal note. The note's
  stated goal is that `[1, 2.5]` "means the same in every binding", and `bindings/dart/CLAUDE.md`
  documents `_marshalGroupColumn` and `_setMixedList` as one rule ("Both ... dispatch on the first
  non-null cell and then convert every cell"). Changing only the group writer would make
  `createElement({'score': [1, 2.5]})` throw while `updateVectorGroup` accepted the same list.
- Root `CLAUDE.md`, "One scalar typing policy lives in C++ ... bindings never coerce
  schema-dependently": the new rule looks at the cells only, never the schema. The core still owns
  every schema decision, including rejecting the widened FLOAT column in an INTEGER column.
- Root `CLAUDE.md`, Error Messages: the new `TypeError` falls under the documented exception for
  pre-FFI marshalling errors, crafted locally. It reuses Dart's wording exactly:
  `Unsupported value type <T> in cell <i> of column '<name>'`.
- Root `CLAUDE.md`, "Element arrays accept NULL cells": "Julia/Python/JS pass a dense (NULL) mask and
  keep their non-null surfaces." A Python element array therefore still **rejects** a `None` cell.
  The group writers skip it, because they have a mask.
- `bindings/dart/CLAUDE.md`, Marshaling idiom: convert every cell individually, never `as`/`cast`,
  and find the dispatch cell with a plain loop, not `firstWhere(..., orElse:)`. Both still hold. The
  change adds one `values.any((v) => v is double)` scan.
- `bindings/python/CLAUDE.md`: per-method FFI boilerplate is the house style. This change does not
  touch the six writer methods, only the shared marshaller and the Element helper.
- `bindings/js/CLAUDE.md`: "fix only what your change orphans". JS changes by one comment sentence
  and gains one test.

Alternatives considered and rejected:
- **Reject `[1, 2.5]` (Dart's current behaviour) instead of widening.** The maintainer chose
  widening. It loses nothing, because a REAL column stores the int as REAL anyway, and it matches JS
  and Julia.
- **Accept a mix of `str` and `datetime` cells in one string column** (one verifier suggested it).
  Rejected because Dart (`first is String` / `first is DateTime`) and Julia (`Vector{Any}` →
  `ArgumentError`) both refuse it. Today it raises `AttributeError`, so no caller depends on it
  working. With one rule it now raises a named `TypeError`.
- **Keep `int(v)` / `float(v)` behind an `isinstance` check.** Unnecessary: cffi converts
  bool/int/float cells itself (verified above).
- **Leave `Element._set_array` alone** (one verifier's suggestion). Rejected by the maintainer
  decision, and its bool-first branch has the same silent truncation (`[True, 2.5]` → `[1, 2]`).
- **Support `None` cells in Python element arrays through the mask.** That widens Python's element
  surface, which the root decision keeps non-null. Out of scope.
- **Accept `datetime` in element arrays now.** That is plan 25 ("accept datetime on every write
  path"). This plan keeps them rejected, now with the shared message.
- **Two helpers (numeric and string).** One ~20-line function covers both callers.

## Changes

No change in the C++ core, the C API, `_c_api.py`, Julia (`c_api.jl`, no regeneration), Dart
`bindings.dart`, JS `loader.ts`, or Lua. No signature changes. The core already rejects a FLOAT
cell in an INTEGER column: `TypeValidator::validate_value` for vector/set groups (via
`insert_rows_into_group_table`, before the DELETE) and `internal::value_matches_type` for time
series.

### 1. `bindings/python/src/quiverdb/_helpers.py` — add `column_data_type`

Current imports (top of file):

```python
from __future__ import annotations

from quiverdb._c_api import ffi, get_lib
from quiverdb.exceptions import QuiverError
```

New imports:

```python
from __future__ import annotations

from datetime import datetime

from quiverdb._c_api import ffi, get_lib
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import DataType
```

(`metadata.py` imports nothing from `quiverdb`, so this adds no import cycle.)

Append at the end of the file, after `decode_string_or_none`:

```python
_NUMERIC = (DataType.INTEGER, DataType.FLOAT)


def column_data_type(name: str, values: list) -> DataType | None:
    """Type one column of cells -- a group-writer column or an element array -- from every cell.

    bool/int cells are INTEGER (a bool is 1/0), and a float anywhere among them widens the column
    to FLOAT: an int loses nothing by it, since the core's int-for-REAL rule stores it as REAL
    anyway and still rejects a float in an INTEGER column. str cells are STRING and datetime cells
    DATE_TIME. None cells are skipped; a column with no other cell is typed None.

    A cell of any other type, or one that does not fit the column (a str among numbers, a number
    among strs, a str among datetimes), raises TypeError naming the cell and the column. No cell
    is run through int()/float() to make it fit: that turned 2.5 into 2 and "7" into 7 with no
    error. cffi converts the bool/int/float cells of a typed column itself.
    """
    column_type = None
    for i, v in enumerate(values):
        if v is None:
            continue
        if isinstance(v, datetime):
            cell_type = DataType.DATE_TIME
        elif isinstance(v, str):
            cell_type = DataType.STRING
        elif isinstance(v, float):
            cell_type = DataType.FLOAT
        elif isinstance(v, int):  # bool is an int subclass
            cell_type = DataType.INTEGER
        else:
            cell_type = None
        if cell_type is not None and column_type in (None, cell_type):
            column_type = cell_type
        elif column_type in _NUMERIC and cell_type in _NUMERIC:
            column_type = DataType.FLOAT
        else:
            raise TypeError(f"Unsupported value type {type(v).__name__} in cell {i} of column '{name}'")
    return column_type
```

How it decides (check these by hand): `[1, 2.5]` gives INTEGER then FLOAT, so FLOAT. `[2.5, 1]` stays
FLOAT. `[True, False]` is INTEGER. `[1, "7"]` raises at cell 1 (str). `["a", 1]` raises at cell 1
(int). `[None, object()]` raises at cell 1 (object). `[datetime, "2024-01-02"]` raises at cell 1
(str). `[None, None]` gives `None`. The index `i` counts `None` cells, so it is the caller's own
list index.

### 2. `bindings/python/src/quiverdb/database.py` — `_marshal_group_columns` uses it

Import line (top of file, currently ~L10), current:

```python
from quiverdb._helpers import check, decode_string, decode_string_or_none
```

new:

```python
from quiverdb._helpers import check, column_data_type, decode_string, decode_string_or_none
```

Replace the whole `_marshal_group_columns` function (currently ~L2166-2236). It starts
`def _marshal_group_columns(data: dict[str, list]) -> tuple:` and ends just before
`def _marshal_row_columns(kwargs: dict) -> tuple:`. The new version:

```python
def _marshal_group_columns(data: dict[str, list]) -> tuple:
    """Marshal column lists into parallel C arrays for the columnar group writers.

    Each column is typed from all of its non-None cells by `column_data_type`: bool/int ->
    INTEGER, and a float anywhere widens the column to FLOAT; str -> STRING; datetime ->
    STRING in the core's ISO format. A cell that does not fit its column raises TypeError
    naming the cell and the column. The C++ layer validates against the schema and accepts
    integers for REAL columns. A None entry becomes a per-cell NULL via the mask (with a
    placeholder in the data array); an all-None column is tagged FLOAT with a zero-filled
    placeholder.

    Returns (keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value,
    col_count, row_count) where keepalive must remain referenced until the C API
    call completes.
    """
    col_count = len(data)
    row_count = len(next(iter(data.values())))
    for name, values in data.items():
        if len(values) != row_count:
            raise ValueError(f"All column lists must have the same length, got {len(values)} for '{name}'")

    keepalive: list = []
    c_col_names = ffi.new("const char*[]", col_count)
    c_col_types = ffi.new("int[]", col_count)
    c_col_data = ffi.new("void*[]", col_count)
    c_col_has_value = ffi.new("uint8_t*[]", col_count)

    for c, (name, values) in enumerate(data.items()):
        name_buf = ffi.new("char[]", name.encode("utf-8"))
        keepalive.append(name_buf)
        c_col_names[c] = name_buf

        mask = ffi.new("uint8_t[]", [0 if v is None else 1 for v in values])
        keepalive.append(mask)
        c_col_has_value[c] = mask

        column_type = column_data_type(name, values)
        if column_type is None:
            # All-null column: tag FLOAT with zeroed placeholder data; the mask is all zero.
            arr = ffi.new("double[]", [0.0] * row_count)
            keepalive.append(arr)
            c_col_types[c] = DataType.FLOAT
            c_col_data[c] = ffi.cast("void*", arr)
        elif column_type == DataType.DATE_TIME:
            encoded = [(v.strftime("%Y-%m-%dT%H:%M:%S").encode("utf-8") if v is not None else b"") for v in values]
            c_strs = [ffi.new("char[]", e) for e in encoded]
            keepalive.extend(c_strs)
            c_arr = ffi.new("char*[]", [(s if v is not None else ffi.NULL) for s, v in zip(c_strs, values)])
            keepalive.append(c_arr)
            c_col_types[c] = DataType.STRING
            c_col_data[c] = ffi.cast("void*", c_arr)
        elif column_type == DataType.STRING:
            encoded = [(v.encode("utf-8") if v is not None else b"") for v in values]
            c_strs = [ffi.new("char[]", e) for e in encoded]
            keepalive.extend(c_strs)
            c_arr = ffi.new("char*[]", [(s if v is not None else ffi.NULL) for s, v in zip(c_strs, values)])
            keepalive.append(c_arr)
            c_col_types[c] = DataType.STRING
            c_col_data[c] = ffi.cast("void*", c_arr)
        elif column_type == DataType.INTEGER:
            # Every cell is a bool or an int; cffi stores True/False as 1/0.
            arr = ffi.new("int64_t[]", [v if v is not None else 0 for v in values])
            keepalive.append(arr)
            c_col_types[c] = DataType.INTEGER
            c_col_data[c] = ffi.cast("void*", arr)
        else:
            # DataType.FLOAT: float cells, plus any bool/int cells, which cffi widens to double.
            arr = ffi.new("double[]", [v if v is not None else 0.0 for v in values])
            keepalive.append(arr)
            c_col_types[c] = DataType.FLOAT
            c_col_data[c] = ffi.cast("void*", arr)

    return keepalive, c_col_names, c_col_types, c_col_data, c_col_has_value, col_count, row_count
```

What changed against the current body:
- `first = next(...)` became `column_type = column_data_type(name, values)`.
- The `isinstance(first, ...)` tests became `column_type == DataType.X`.
- `int(v)` and `float(v)` were dropped.
- The trailing `else: raise TypeError(f"Unsupported value type for column '{name}': ...")` was
  deleted, because the helper raises now.
- The docstring was rewritten. It used to say "for the columnar time series API", which is stale,
  since the helper also serves vectors and sets.

The DATE_TIME and STRING bodies are unchanged, and are now safe: every non-`None` cell is
guaranteed to be a `datetime` (or `str`) respectively. Leave the six callers (currently ~L1559,
1599, 1644, 1684, 1727, 1767) and `_marshal_row_columns` untouched.

### 3. `bindings/python/src/quiverdb/element.py` — `Element._set_array` uses it

Current imports:

```python
from __future__ import annotations

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, decode_string
from quiverdb.exceptions import QuiverError
```

New imports:

```python
from __future__ import annotations

from datetime import datetime

from quiverdb._c_api import ffi, get_lib
from quiverdb._helpers import check, column_data_type, decode_string
from quiverdb.exceptions import QuiverError
from quiverdb.metadata import DataType
```

`Element.set` docstring (currently ~L19-23), current:

```python
        """Set an attribute value. Returns self for fluent chaining.

        Supported types: int, float, str, None, bool (stored as int),
        list[int], list[float], list[str].
        """
```

New:

```python
        """Set an attribute value. Returns self for fluent chaining.

        Supported types: int, float, str, None, bool (stored as int), and lists of
        int/bool, float or str -- a float anywhere in a numeric list makes it a float array.
        """
```

Replace `_set_array` (currently ~L58-75). The current body:

```python
    def _set_array(self, name: str, values: list) -> None:
        lib = get_lib()
        if len(values) == 0:
            # Empty array -- type doesn't matter
            check(lib.quiver_element_set_array_integer(self._ptr, name.encode("utf-8"), ffi.NULL, 0, ffi.NULL))
            return

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

The new body:

```python
    def _set_array(self, name: str, values: list) -> None:
        lib = get_lib()
        if len(values) == 0:
            # Empty array -- type doesn't matter
            check(lib.quiver_element_set_array_integer(self._ptr, name.encode("utf-8"), ffi.NULL, 0, ffi.NULL))
            return

        # Typed from every cell like a group-writer column (column_data_type), except that an
        # element array is dense here -- its setters pass a NULL mask -- so a None cell is refused
        # rather than skipped, and a datetime cell is not accepted.
        for i, v in enumerate(values):
            if v is None or isinstance(v, datetime):
                raise TypeError(f"Unsupported value type {type(v).__name__} in cell {i} of column '{name}'")

        array_type = column_data_type(name, values)
        if array_type == DataType.INTEGER:
            self._set_array_integer(name, values)
        elif array_type == DataType.FLOAT:
            self._set_array_float(name, values)
        else:
            self._set_array_string(name, values)
```

After the pre-check, the list is non-empty and holds no `None` or `datetime`, so the result is
INTEGER, FLOAT or STRING (or the helper has already raised). `_set_array_integer`,
`_set_array_float` and `_set_array_string` stay as they are. cffi accepts bool cells in
`int64_t[]` and bool/int cells in `double[]`.

### 4. `bindings/dart/lib/src/database_update.dart` — `_marshalGroupColumn`

Doc comment (currently ~L718-723), current:

```dart
  /// Marshals one vector/set/time-series column into arena-allocated typed + mask arrays,
  /// returning its quiver_data_type_t tag, data pointer, and per-cell NULL mask.
  /// Supported value types: int, bool (INTEGER 1/0), double, String, DateTime; a `null` entry becomes
  /// a SQL NULL (mask 0) with a placeholder in the data array. An all-null (or
  /// empty) column is tagged FLOAT with a zeroed placeholder — the C API ignores
  /// the type tag and data for masked-out cells.
```

New:

```dart
  /// Marshals one vector/set/time-series column into arena-allocated typed + mask arrays,
  /// returning its quiver_data_type_t tag, data pointer, and per-cell NULL mask.
  /// Supported value types: int, bool (INTEGER 1/0), double, String, DateTime; a `null` entry becomes
  /// a SQL NULL (mask 0) with a placeholder in the data array. A numeric column is FLOAT if any cell
  /// is a double and INTEGER otherwise. An all-null (or empty) column is tagged FLOAT with a zeroed
  /// placeholder — the C API ignores the type tag and data for masked-out cells.
```

Numeric dispatch (currently ~L754-779), current:

```dart
    // SQLite has no boolean type: a bool is INTEGER 1/0, the same as on Element.set and the query
    // parameters. Folded into the int branch (checked first — a Dart bool is not an int) and
    // converted per cell, so a mixed [true, 1] column writes 1 and 1 rather than throwing a raw
    // TypeError that names no column. Matches Python's per-cell `int(v)` and JS's normalization.
    if (first is bool || first is int) {
```

and, after the int branch's closing `}`:

```dart
    // An int in a REAL column is the documented int-for-REAL coercion, and a bool reaches REAL
    // through it; converted per cell so a mixed column reports the cell rather than raw-casting.
    if (first is double) {
```

New. Replace the first excerpt with:

```dart
    // SQLite has no boolean type: a bool is INTEGER 1/0, the same as on Element.set and the query
    // parameters (a Dart bool is not an int, so it is named here). The first non-null cell picks the
    // family and every cell picks the numeric type: one double anywhere widens the column to FLOAT,
    // so [1, 2.5] writes 1.0 and 2.5 — the whole-column rule Python and JS apply — where choosing
    // INTEGER from the first cell threw on the 2.5. Both branches still convert per cell, so a mixed
    // [true, 1] column writes 1 and 1 and a stray String reports its cell and column instead of
    // throwing a raw TypeError.
    final isNumeric = first is bool || first is int || first is double;
    if (isNumeric && !values.any((v) => v is double)) {
```

and replace the second excerpt with:

```dart
    // An int in a REAL column is the documented int-for-REAL coercion, and a bool reaches REAL
    // through it; converted per cell so a mixed column reports the cell rather than raw-casting.
    if (isNumeric) {
```

The bodies of both branches (the `switch (v)` per-cell conversions and the returned records) stay
as they are. The int branch's `_ => throw ArgumentError(...)` still catches a String (or any
non-numeric) cell, and the float branch already converts `int` (`i.toDouble()`) and `bool`
(`1.0/0.0`). The String, DateTime and final-`throw` parts stay as they are.

### 5. `bindings/dart/lib/src/element.dart` — `Element._setMixedList`

Current (currently ~L96-123, first half of the method):

```dart
  void _setMixedList(String name, List<dynamic> values) {
    // Dispatch on the first non-null element; an empty or all-null list is
    // tagged integer (the type is irrelevant when every cell is NULL). Each branch then converts
    // per cell rather than `cast`ing the list, which defers the check to iteration and throws a
    // raw TypeError naming neither the attribute nor the cell. A bool is INTEGER 1/0 and an int
    // reaches a REAL column by the int-for-REAL coercion — the rules `_marshalGroupColumn` applies.
    Object? first;
    for (final v in values) {
      if (v != null) {
        first = v;
        break;
      }
    }
    if (first == null) {
      setArrayInteger(name, List<int?>.filled(values.length, null));
    } else if (first is bool || first is int) {
```

and further down:

```dart
    } else if (first is double) {
      setArrayFloat(name, [
```

New. Replace the first excerpt with:

```dart
  void _setMixedList(String name, List<dynamic> values) {
    // The first non-null element picks the family and every element the numeric type: a numeric
    // list is an integer array unless some cell is a double, which widens it to a float array (so
    // [1, 2.5] stores 1.0 and 2.5, as in the other bindings). An empty or all-null list is tagged
    // integer (the type is irrelevant when every cell is NULL). Each branch then converts per cell
    // rather than `cast`ing the list, which defers the check to iteration and throws a raw
    // TypeError naming neither the attribute nor the cell. A bool is INTEGER 1/0 and an int
    // reaches a REAL column by the int-for-REAL coercion — the rules `_marshalGroupColumn` applies.
    Object? first;
    for (final v in values) {
      if (v != null) {
        first = v;
        break;
      }
    }
    final isNumeric = first is bool || first is int || first is double;
    if (first == null) {
      setArrayInteger(name, List<int?>.filled(values.length, null));
    } else if (isNumeric && !values.any((v) => v is double)) {
```

and the second with:

```dart
    } else if (isNumeric) {
      setArrayFloat(name, [
```

Everything else in the method stays unchanged: the per-cell `switch` bodies, the String and
DateTime branches, and the final `throw`.

### 6. `bindings/js/src/group-columns.ts` — comment only

In `updateGroupColumns`, inside the `for (let c = 0; c < columnCount; c++)` loop (currently ~L98-102),
current:

```ts
    // SQLite has no boolean type: a boolean is INTEGER 1/0, as in setElementField and
    // marshalParams. Normalizing per cell before the dispatch (rather than adding a boolean
    // branch after it) is what makes a mixed [true, 5] column write 1 and 5 instead of
    // truthiness-mapping every cell, and matches Python's per-cell `int(v)`. A string column is
    // left alone so normalizing cannot change what a mixed ['a', true] column already wrote.
```

New (keep every line at 100 columns or less, the biome `lineWidth`):

```ts
    // SQLite has no boolean type: a boolean is INTEGER 1/0, as in setElementField and
    // marshalParams. Normalizing per cell before the dispatch (rather than adding a boolean
    // branch after it) is what makes a mixed [true, 5] column write 1 and 5 instead of
    // truthiness-mapping every cell -- the same per-cell 1/0 Python and Dart apply. A string
    // column is left alone so normalizing cannot change what a mixed ['a', true] column already
    // wrote.
```

No code change in JS. The numeric branch below (`nonNull.every((v) => Number.isInteger(v))`)
already is the whole-column rule.

## Tests

No C++, C API or Lua test changes: the core's behaviour is unchanged. The core rejecting a float in
an INTEGER column is already pinned for scalars (`DatabaseCApi.ScalarTypeCoercionPolicy`,
`LuaRunnerTest.ScalarTypeCoercionPolicy`), and the binding tests below pin it for groups end to end.
Julia gets no test either. No Julia code makes this decision: `[1, 2.5]` is a `Vector{Float64}`
before the binding sees it, so a test would test the language's promotion, not Quiver.

### Python — `bindings/python/tests/test_database_update.py`

The current import block (after the module docstring) is:

```python
from __future__ import annotations

import pytest

from quiverdb import Database, QuiverError
```

Replace it with the following, so that the stdlib import sits in its own isort group before
`import pytest`:

```python
from __future__ import annotations

from datetime import datetime

import pytest

from quiverdb import Database, QuiverError
```

Append this class at the end of the file, after `class TestUpdateRelation`. It uses the existing
`all_types_db` fixture (`conftest.py`, `tests/schemas/valid/all_types.sql`):

```python
class TestColumnTyping:
    """A group column or element array is typed from every cell, not the first one. The first-cell
    dispatch ran the rest through int()/float(), so [1, 2.5] stored [1.0, 2.0] in a REAL column and
    [1, "7"] stored [1, 7], with no error."""

    def test_float_among_ints_widens_a_group_column(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element("AllTypes", label="Widened")
        all_types_db.update_vector_group("AllTypes", "scores", elem_id, {"score": [1, 2.5, True]})
        assert all_types_db.read_vector_floats_by_id("AllTypes", "score", elem_id) == [1.0, 2.5, 1.0]

    def test_float_among_ints_widens_an_element_array(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element("AllTypes", label="Widened", score=[True, 2.5])
        assert all_types_db.read_vector_floats_by_id("AllTypes", "score", elem_id) == [1.0, 2.5]

    def test_widened_column_is_rejected_by_an_integer_column(self, all_types_db: Database) -> None:
        elem_id = all_types_db.create_element("AllTypes", label="Counts", count_value=[7])
        # The 2.5 now reaches the core, which refuses a float in an INTEGER column instead of
        # storing the 2 the old marshaller truncated it to. Validation precedes the DELETE.
        with pytest.raises(QuiverError, match="count_value"):
            all_types_db.update_vector_group("AllTypes", "counts", elem_id, {"count_value": [1, 2.5]})
        assert all_types_db.read_vector_integers_by_id("AllTypes", "count_value", elem_id) == [7]

    @pytest.mark.parametrize(
        ("cells", "message"),
        [
            ([1, "7"], "Unsupported value type str in cell 1 of column 'score'"),
            ([1.5, "2"], "Unsupported value type str in cell 1 of column 'score'"),
            (["a", 1], "Unsupported value type int in cell 1 of column 'score'"),
            ([None, object()], "Unsupported value type object in cell 1 of column 'score'"),
            ([datetime(2024, 1, 1), "2024-01-02"], "Unsupported value type str in cell 1 of column 'score'"),
        ],
    )
    def test_group_cell_of_the_wrong_kind_raises_naming_it(
        self, all_types_db: Database, cells: list, message: str
    ) -> None:
        elem_id = all_types_db.create_element("AllTypes", label="Bad")
        with pytest.raises(TypeError, match=message):
            all_types_db.update_vector_group("AllTypes", "scores", elem_id, {"score": cells})

    @pytest.mark.parametrize(
        ("cells", "message"),
        [
            ([True, "7"], "Unsupported value type str in cell 1 of column 'score'"),
            ([1, None], "Unsupported value type NoneType in cell 1 of column 'score'"),
        ],
    )
    def test_element_array_cell_of_the_wrong_kind_raises_naming_it(
        self, all_types_db: Database, cells: list, message: str
    ) -> None:
        with pytest.raises(TypeError, match=message):
            all_types_db.create_element("AllTypes", label="Bad", score=cells)
```

How each test fails before the fix:
- `test_float_among_ints_widens_a_group_column` reads back `[1.0, 2.0, 1.0]`, because the first cell
  typed the column INTEGER and `int(2.5)` gave 2.
- `test_float_among_ints_widens_an_element_array` reads back `[1.0, 2.0]` (the bool-first branch
  ran `int(v)`).
- `test_widened_column_is_rejected_by_an_integer_column` raises nothing, and `[1, 2]` is stored.
- In the group parametrization: `[1, "7"]` and `[1.5, "2"]` raise nothing (the strings are parsed).
  `["a", 1]` raises `AttributeError`, not `TypeError`. The datetime/str case raises
  `AttributeError`. `[None, object()]` raises with the old wording
  `Unsupported value type for column 'score': object`, which does not match.
- In the element parametrization: `[True, "7"]` stores `[1.0, 7.0]`. `[1, None]` raises cffi's
  `an integer is required`, which does not match.

Existing Python tests keep passing. Every current group or array write in `bindings/python/tests`
is single-type per column (`grep` for int-then-float lists finds none), and
`test_update_time_series_group_wrong_type_str_for_int` passes an all-str column, which is still
STRING, so the core still reports `column 'humidity' has type INTEGER but received TEXT`.

### Python — `bindings/python/tests/test_database_time_series_group.py`

In `class TestTimeSeriesValidation`, directly after
`test_update_time_series_group_int_for_float_column` (currently ~L169-181), add:

```python
    def test_update_time_series_group_float_among_ints_widens(self, mixed_time_series_db: Database) -> None:
        """A float anywhere makes the column FLOAT; the first-cell dispatch int()-ed 20.5 to 20."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        data = {
            "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
            "temperature": [20, 20.5],
            "humidity": [65, 70],
            "status": ["normal", "normal"],
        }
        mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, data)

        result = mixed_time_series_db.read_time_series_group("Sensor", "readings", eid)
        assert result["temperature"] == [20.0, 20.5]

    def test_update_time_series_group_float_among_ints_rejected_for_int_column(
        self, mixed_time_series_db: Database
    ) -> None:
        """The widened column reaches the core, which rejects a float in an INTEGER column."""
        eid = _create_sensor(mixed_time_series_db, "S1")
        bad_data = {
            "date_time": ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
            "temperature": [20.5, 21.0],
            "humidity": [65, 70.5],
            "status": ["normal", "normal"],
        }
        with pytest.raises(QuiverError, match="column 'humidity' has type INTEGER but received REAL"):
            mixed_time_series_db.update_time_series_group("Sensor", "readings", eid, bad_data)
```

(`tests/schemas/valid/mixed_time_series.sql`: `temperature REAL NOT NULL`, `humidity INTEGER NOT
NULL`.) Before the fix, the first test reads back `[20.0, 20.0]`, and the second raises nothing
(humidity is stored as `[65, 70]`). The expected message follows `validate_time_series_row`'s format
in `src/database_time_series.cpp` (`"column '<c>' has type " + INTEGER + " but received " + REAL`),
the same shape the neighbouring `..._wrong_type_str_for_int` test pins.

### Python — `bindings/python/tests/test_database_boolean.py` (docstring only)

The `test_boolean_input` docstring (currently ~L84-88) describes the old dispatch. Current:

```python
    """A native bool on the write side.

    Python needs no special handling in most places (`bool` is an `int` subclass), but
    `Element.set` and the group/row marshallers each test `bool` explicitly and before `int`,
    so a stray reordering would send a bool down the float or the unsupported-type path.
    """
```

New:

```python
    """A native bool on the write side.

    Python needs no special handling in most places (`bool` is an `int` subclass): `Element.set`
    and the row marshaller test `bool` before `int`, and `column_data_type` (group columns and
    element arrays) lets it fall into its `int` check, so this pins that a bool arrives as 1/0
    whichever way it is dispatched.
    """
```

The test body is unchanged, and it still passes: `[True, False]` and `[True, False, True]` are
INTEGER columns.

### Dart — `bindings/dart/test/database_update_test.dart`

Insert a new group immediately **before** the banner that precedes `group('Update Relation', ...)`
(currently ~L1827-1833):

```dart
  // ==========================================================================
  // Update relation
  // ==========================================================================
```

The new group:

```dart
  // ==========================================================================
  // Mixed numeric cells
  // ==========================================================================

  group('Mixed numeric cells', () {
    // A numeric column (or element array) is INTEGER only while no cell is a double; one double
    // widens it to FLOAT, so [1, 2.5] writes 1.0 and 2.5 here as in Python, JS and Julia. Choosing
    // INTEGER from the first cell used to throw ArgumentError on the 2.5.
    Database openAllTypes() => Database.fromSchema(
      ':memory:',
      path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
    );

    test('a double among ints widens a group column to FLOAT', () {
      final db = openAllTypes();
      try {
        final id = db.createElement('AllTypes', {'label': 'Widened'});
        db.updateVectorGroup('AllTypes', 'scores', id, {
          'score': [1, 2.5, true],
        });
        expect(db.readVectorFloatsById('AllTypes', 'score', id), equals([1.0, 2.5, 1.0]));
      } finally {
        db.close();
      }
    });

    test('a double among ints widens an element array to FLOAT', () {
      final db = openAllTypes();
      try {
        final id = db.createElement('AllTypes', {
          'label': 'Widened',
          'score': [1, 2.5],
        });
        expect(db.readVectorFloatsById('AllTypes', 'score', id), equals([1.0, 2.5]));
      } finally {
        db.close();
      }
    });

    test('an INTEGER column still rejects a widened column', () {
      final db = openAllTypes();
      try {
        final id = db.createElement('AllTypes', {
          'label': 'Counts',
          'count_value': [7],
        });
        expect(
          () => db.updateVectorGroup('AllTypes', 'counts', id, {
            'count_value': [1, 2.5],
          }),
          throwsA(isA<DatabaseException>().having((e) => e.message, 'message', contains('count_value'))),
        );
        // Validation runs before the DELETE, so the group is intact.
        expect(db.readVectorIntegersById('AllTypes', 'count_value', id), equals([7]));
      } finally {
        db.close();
      }
    });

    test('a String among numbers names its own cell', () {
      final db = openAllTypes();
      try {
        final id = db.createElement('AllTypes', {'label': 'Bad'});
        expect(
          () => db.updateVectorGroup('AllTypes', 'scores', id, {
            'score': [1, 2.5, 'x'],
          }),
          throwsA(
            isA<ArgumentError>().having(
              (e) => e.message,
              'message',
              allOf(contains('score'), contains('cell 2')),
            ),
          ),
        );
      } finally {
        db.close();
      }
    });
  });

```

How each test fails before the fix:
- The first two throw `ArgumentError ... double in cell 1` (the int branch was chosen from cell 0).
- The third throws `ArgumentError`, not `DatabaseException`.
- The fourth reports `cell 1` (the 2.5) instead of `cell 2` (the `'x'`), which proves the scan now
  reaches the String.

Existing Dart tests that pin neighbouring behaviour and must still pass unchanged:
- `database_boolean_test.dart`: 'a mixed boolean/integer group column keeps its integer cells'
  (`[true, 5, false, 7]` has no double, so it stays INTEGER); 'a REAL group column takes ints and
  bools by the int-for-REAL rule' (`[1.5, 2, true]` → FLOAT, as before); 'an element array takes the
  same mixed cells' (`[1.5, 2, true]` → FLOAT; `[1.5, <int>[1]]` still throws naming `score` and
  `cell 1`); 'group writer rejection names the offending column' (`[<int>[1]]` still reaches the final
  `throw`, whose message contains `count_value` and `Unsupported value type`).

### JS — `bindings/js/test/database-update.test.ts` (parity pin, passes before and after)

Append at the end of the file, after the `describe("updateRelation", ...)` block. `SCHEMA_PATH` in
this file is already `all_types.sql`.

```ts
// A numeric column is typed from every cell -- one decimal widens it to FLOAT -- the rule Python and
// Dart now share, so [1, 2.5] writes 1 and 2.5 in every binding.
describe("group writer column typing", () => {
  test("a decimal among integers widens the column to FLOAT", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      const id = db.createElement("AllTypes", { label: "Widened" });
      db.updateVectorGroup("AllTypes", "scores", id, { score: [1, 2.5] });
      expect(db.readVectorFloatsById("AllTypes", "score", id)).toEqual([1, 2.5]);
    } finally {
      db.close();
    }
  });
});
```

This test does not fail before the fix: JS already behaves this way. It is there so that plans 31
and 33, which rework `group-columns.ts` next, cannot silently regress the shared contract.

## Docs and changelog

### `bindings/python/CLAUDE.md`

1. Layout block (currently ~L20). Old: `  _helpers.py     # Shared check()/decode_string helpers`. New:
   `  _helpers.py     # Shared check()/decode_string/column_data_type helpers`.
2. `_integer_to_boolean` bullet (currently ~L72-73). Old: `exception to "messages come from C++",
   alongside `_marshal_group_columns`' jagged-column check.`. New: `exception to "messages come from
   C++", alongside `_marshal_group_columns`' jagged-column and cell-type checks.`.
3. "Time-series group NULLs" bullet (currently ~L86-88). Old: `` `_marshal_group_columns` dispatches
   on the first non-`None` element, builds a per-column mask, `` New: `` `_marshal_group_columns`
   types each column from all of its non-`None` cells (`column_data_type`, below), builds a
   per-column mask, `` (the rest of the sentence stays).
4. "`_marshal_group_columns` serves every columnar group writer" bullet (currently ~L90-92). Old:
   `It raises `ValueError` for jagged column lists (a pre-FFI marshalling error, the documented
   exception to "messages come from C++"); everything else is validated in the core and surfaces as
   `QuiverError`.` New: `It raises `ValueError` for jagged column lists and `TypeError` for a cell
   that does not fit its column (both pre-FFI marshalling errors, the documented exception to
   "messages come from C++"); everything else is validated in the core and surfaces as
   `QuiverError`.` Leave the rest of that bullet alone, since plan 18 may have rewritten its
   `read_vector_group_by_id` sentence.
5. Add a new bullet directly after the `_marshal_group_columns` bullet:

   ```markdown
   - **`column_data_type` (`_helpers.py`) types a column from every cell**, for both
     `_marshal_group_columns` and `Element._set_array`: bool/int cells are INTEGER, a float anywhere
     among them widens the column to FLOAT (JS's and Dart's rule too, so `[1, 2.5]` writes 1.0 and 2.5
     in every binding), str is STRING and datetime DATE_TIME (the group marshaller formats it and tags
     STRING). Any other cell, or one that does not fit the column (a str among numbers, a number
     among strs, a str among datetimes), raises `TypeError: Unsupported value type <T> in cell <i> of
     column '<name>'` — Dart's wording. **No cell is run through `int()`/`float()`**: the first-cell
     dispatch this replaced did, so `[1, 2.5]` reached a REAL column as `[1, 2]` and `[1, "7"]` was
     parsed to 7, both with no error. cffi converts bool/int cells into `int64_t[]` and bool/int/float
     cells into `double[]` itself. Element arrays stay dense: `_set_array` refuses a `None` cell (and,
     for now, a `datetime`) before typing, where a group-writer column skips it via the mask.
   ```

### `bindings/dart/CLAUDE.md`

1. Marshaling idiom bullet (currently ~L81-85). Old: `Both `_marshalGroupColumn` and
   `Element._setMixedList` dispatch on the first non-null cell and then convert **every** cell
   individually — never `as`/`cast` the rest to the dispatched type, which defers the check to
   iteration and throws a raw `TypeError` naming neither the column nor the cell. An `int` (and a
   `bool`) is accepted into a REAL column by the int-for-REAL coercion.` New: `Both
   `_marshalGroupColumn` and `Element._setMixedList` take the family (numeric, String, DateTime) from
   the first non-null cell and the numeric type from all of them: a numeric column is INTEGER unless
   some cell is a `double`, which widens it to FLOAT (the rule Python and JS share, so `[1, 2.5]`
   writes 1.0 and 2.5 in every binding; it used to throw here). They then convert **every** cell
   individually — never `as`/`cast` the rest to the dispatched type, which defers the check to
   iteration and throws a raw `TypeError` naming neither the column nor the cell. An `int` (and a
   `bool`) is accepted into a REAL column by the int-for-REAL coercion.` (The following "Find the
   dispatch cell with a plain loop ..." sentence stays.)
2. "Time-series group NULLs" bullet (currently ~L102-103). Old: `dispatches on the first non-null
   element, and tags an all-null/empty column FLOAT with a zeroed placeholder.` New: `types the column
   by the rule above, and tags an all-null/empty column FLOAT with a zeroed placeholder.`
3. "Booleans are INTEGER 0/1 in both directions" bullet (currently ~L133-138). Old:
   `` `_marshalGroupColumn` (`database_update.dart`) dispatches on `first is bool || first is int` — a
   Dart `bool` is not an `int`, so without the `bool` half the group writers threw. The two are **one
   branch converting per cell**, not two branches each casting `as` their own type: dispatch reads
   only the first non-null cell, so a mixed `[true, 1]` column would otherwise throw a raw
   `TypeError` naming nothing.`` New: `` `_marshalGroupColumn` (`database_update.dart`) counts `bool`
   as numeric (`first is bool || first is int || first is double`) — a Dart `bool` is not an `int`,
   so without the `bool` half the group writers threw. bool and int are **one branch converting per
   cell**, not two branches each casting `as` their own type: the family comes from the first
   non-null cell, so a mixed `[true, 1]` column would otherwise throw a raw `TypeError` naming
   nothing.`` (The "That branch covers all eight call sites ..." remainder stays.)
4. "Element array NULLs" bullet (currently ~L143-144). Old: `` `Element.set` dispatches mixed lists
   on the first non-null element, `` New: `` `Element.set` types mixed lists like
   `_marshalGroupColumn` (family from the first non-null element, a `double` anywhere widens to
   float), ``.

### `src/CLAUDE.md`

In the `lua_table_to_vector<T>` bullet (currently ~L595-597), old: `comes from cell 1, so `{1, 2.5}`
into a REAL column is rejected rather than widened (JS scans the whole column and accepts it).` New:
`comes from cell 1, so `{1, 2.5}` into a REAL column is rejected rather than widened (JS, Python and
Dart type the whole column and widen it to FLOAT, and a Lua group-writer column converts each cell to
its own `Value`, so a Lua element array is the one path that refuses it).` That is the only Lua claim
this plan changes. The claim itself stays true: Lua is not changed here.

### Root `CLAUDE.md`, READMEs, `docs/*.md`, `bindings/js/src/lua-api.ts`

No change. Nothing there describes the bindings' per-cell dispatch (checked with `grep` for
`dispatch`, `first non-`, `int(v)`, `mixed`).

### `CHANGELOG.md`

Under `## [0.11.0] — unreleased`, append this as the **last bullet of `### Changed`**, immediately
before the `### Fixed` heading (earlier plans may have added bullets, so anchor on the heading, not a
line number):

```markdown
- **BREAKING — Python and Dart type a numeric group column or element array from every cell.**
  Python took a column's type from its first non-`None` cell and ran every other cell through
  `int()` or `float()`, so `update_vector_group(..., {"score": [1, 2.5]})` stored `[1.0, 2.0]` with
  no error. The same happened in `update_set_group`, `update_time_series_group` and their
  `_by_label` forms, and in `create_element`/`update_element` arrays that start with a `bool`. A
  `str` cell among numbers was parsed (`[1, "7"]` stored `7`). A float anywhere in a numeric column
  now makes it FLOAT, so `[1, 2.5]` stores `1.0` and `2.5`; bool/int columns stay INTEGER. A cell
  that does not fit its column — a `str` among numbers, a number among strings, a `str` among
  `datetime`s, `None` in an element array, any unsupported type — raises
  `TypeError: Unsupported value type <T> in cell <i> of column '<name>'` before the call. Dart
  applies the same whole-column rule in the group writers and `Element.set`, where `[1, 2.5]` used
  to throw `ArgumentError`. JS and Julia already behaved this way.

  *Adapt:* pass numbers, not numeric strings. A float list written to an INTEGER column is now
  rejected by the core (`type mismatch`) instead of being truncated; round it yourself if you
  intended truncation.
```

## Verification

From the repo root (`C:\Development\Quiver\quiver1`), in order:

1. `cmake --build build --config Debug`. No C++ change, so this is a no-op. It only ensures that
   `build/bin` holds the DLLs the binding suites load.
2. Python, targeted:
   `bindings/python/tests/test.bat -k "TestColumnTyping or float_among_ints or test_boolean_input"`.
   Expected to pass: `TestColumnTyping::test_float_among_ints_widens_a_group_column`,
   `::test_float_among_ints_widens_an_element_array`,
   `::test_widened_column_is_rejected_by_an_integer_column`, 5 ×
   `::test_group_cell_of_the_wrong_kind_raises_naming_it[...]`, 2 ×
   `::test_element_array_cell_of_the_wrong_kind_raises_naming_it[...]`,
   `TestTimeSeriesValidation::test_update_time_series_group_float_among_ints_widens`,
   `::test_update_time_series_group_float_among_ints_rejected_for_int_column`, and
   `test_boolean_input`. To prove the tests bite, stash only the source edits with
   `git stash push -- bindings/python/src/quiverdb/_helpers.py bindings/python/src/quiverdb/database.py bindings/python/src/quiverdb/element.py`
   and rerun the same command. Every new test except `test_boolean_input` should fail as described
   in Tests. Then run `git stash pop`.
3. Python, full: `bindings/python/tests/test.bat`. Expect all green.
4. Dart, targeted: `bindings/dart/test/test.bat test/database_update_test.dart --name "Mixed numeric cells"`
   (four tests pass). Then `bindings/dart/test/test.bat test/database_boolean_test.dart`, where the
   existing mixed-cell tests stay green. Then the full `bindings/dart/test/test.bat`. The C API did
   not change, so there is no need to clear `.dart_tool`.
5. JS: `bindings/js/test/test.bat -t "column typing"`, then the full `bindings/js/test/test.bat`.
   Also `cd bindings/js && bun run lint`: `group-columns.ts` must not gain lint findings.
6. Julia (unchanged, run for completeness): `bindings/julia/test/test.bat`.
7. `scripts/format.bat`. This runs ruff (Python), `dart format` (page width 120) and biome, which
   may re-wrap the new code. Re-run steps 2-5 if anything was reformatted.
8. `scripts/test-all.bat`. All six suites plus the CLI smoke test pass. Note that plan 65 owns the
   CLI smoke test, so if it has not landed yet, that one step's failure predates this change.

## Acceptance criteria

- [ ] `bindings/python/src/quiverdb/_helpers.py` defines `column_data_type(name, values)` as given.
      Neither it nor any other Python marshaller calls `int(v)`/`float(v)` on a cell.
- [ ] `_marshal_group_columns` types every column through `column_data_type`, and its old trailing
      `TypeError` branch is gone.
- [ ] `Element._set_array` rejects `None`/`datetime` cells with the shared message, then types the
      list through `column_data_type`. Its `isinstance(first, bool)` / `[int(v) for v in values]`
      branch is gone.
- [ ] Dart `_marshalGroupColumn` and `Element._setMixedList` take the INTEGER branch only when no
      cell is a `double`.
- [ ] The Dart comment citing "Python's per-cell `int(v)`" and the JS comment citing the same are
      rewritten.
- [ ] The new Python tests (`TestColumnTyping`, two time-series tests), the Dart group 'Mixed numeric
      cells' and the JS 'group writer column typing' test all pass, and the Python and Dart ones
      fail on the pre-change code.
- [ ] `bindings/python/CLAUDE.md` (5 edits), `bindings/dart/CLAUDE.md` (4 edits) and
      `src/CLAUDE.md` (1 edit) are updated as specified.
- [ ] The CHANGELOG bullet is under `## [0.11.0] — unreleased` → `### Changed`, prefixed
      **BREAKING**, with an *Adapt:* line.
- [ ] `scripts/format.bat` leaves no diff, and `scripts/test-all.bat` is green.

## Pitfalls

- **Test order in the helper.** Test `datetime`, then `str`, then `float`, then `int`. `bool` must
  land in the `int` check (it is an `int` subclass, and not a `float`). Do not use `type(v) is int`:
  that rejects `bool` and `IntEnum`. numpy is not a dependency, but note that `numpy.float64` is a
  `float` subclass (so FLOAT), while `numpy.int64` is not an `int` (so `TypeError`, as today).
- **`IntEnum` membership.** `column_type in (None, cell_type)` and `cell_type in _NUMERIC` compare
  `DataType` members, or `None`, by `==`. That is correct here, because the only values involved
  are `DataType` members or `None`. Do not compare them against raw ints elsewhere.
- **Python element arrays have no mask.** If the `None` pre-check in `_set_array` is dropped, `[None]`
  types as `None` and falls into `_set_array_string`, raising `AttributeError`. `[1, None]` then
  raises cffi's unnamed `TypeError`.
- **The core's message for a widened column into INTEGER names `index 0`.** The whole column is
  FLOAT-tagged, so cell 0 (the caller's `1`) is the first rejected. Tests match only the column
  name, for vector groups, so that plan 56's later rewording of `TypeValidator` messages does not
  break them. The time-series test pins the same full message its neighbour already pins, so plan
  56 must update both together.
- **"Widening is exact" holds up to 2^53.** A larger int in a FLOAT-widened column rounds when cffi
  converts it to double. A REAL column would round it on insert anyway, so nothing a REAL column
  keeps is lost. An int ≥ 2^63 still raises cffi's `OverflowError` for an INTEGER column, as before.
  That is pre-existing and not this plan's concern.
- **Dart VM vs web.** `1 is double` is `false` on the Dart VM, which is the only target of this FFI
  binding. On the web it would be `true`, but the web is not a target.
- **Line endings and formatting.** `.py` and `.dart` are LF (`.gitattributes`). Keep JS comment lines
  within 100 columns (biome) and Python within 120 (ruff). No `.bat` file is touched.
- **Line numbers have moved.** Plan 18 (whole-group readers in Python/JS) and plan 20
  (`update_time_series_files` in Python and Dart) edit `database.py`, `database_update.dart` and
  `bindings/python/CLAUDE.md` before this plan runs. Anchor every edit on the quoted excerpt and
  function name.
- **Dart pattern match on list types.** `Element.set` matches `List<int>` / `List<double>` before
  the generic `List v` case. A literal `[1, 2.5]` infers `List<num>`, so it reaches `_setMixedList`,
  which the new element test relies on. Do not type the literal `<double>[...]` in that test.

## Out of scope

- **Accepting `datetime` in Python element arrays, and converting aware datetimes to UTC.** Plan 25.
  When it lands, it should drop `or isinstance(v, datetime)` from `_set_array`'s pre-check and add a
  `DataType.DATE_TIME` branch that formats the cells and calls `_set_array_string`. It will find
  `_marshal_group_columns`' datetime branch keyed on `column_type == DataType.DATE_TIME`.
- **The remaining bool branches** in `Element.set` (scalar) and `_marshal_row_columns`. Plan 28. The
  `_set_array` bool branch and the `isinstance(first, bool) or isinstance(first, int)` test in
  `_marshal_group_columns` are already deleted here, so plan 28 should skip those two sites.
- **`Element.clear` / `_ensure_valid`.** Plan 29 (same file, untouched here).
- **Other stale Python docstrings.** Plan 30. `_marshal_group_columns`' docstring is rewritten here,
  so plan 30 should re-read it rather than apply an edit written against the old text.
- **JS per-cell numeric checks and boolean normalization in the array marshallers.** Plan 31. **JS
  `bigint` in the group writers.** Plan 33. Both rework `group-columns.ts` after this plan's comment
  edit, and must keep the new parity test green.
- **Collapsing the six Dart group writers onto one `_marshalGroupColumns` helper.** Plan 38. It must
  carry the new `isNumeric` dispatch inside `_marshalGroupColumn` unchanged. **The `Map` case of
  Dart `Element.set`.** Plan 39 (same file, different function).
- **Lua element arrays** (`table_to_element` → `lua_table_to_vector`) still reject `{1, 2.5}`
  rather than widen. Lua's group writers already convert per cell, so `{1, 2.5}` works there. The
  rejection is loud and loses no data. No plan in the current list owns widening it, and it is
  recorded in `src/CLAUDE.md` (edit above).
- **Julia `Vector{Any}` columns** (`Any[1, 2.5]`) still raise `ArgumentError: Unsupported column
  type: Any`. Literals promote, so ordinary callers never hit it. Not owned by any plan.
- **`None` cells in Python element arrays** (the C API mask supports them, but the root decision
  keeps Python's element surface dense). Not planned.
