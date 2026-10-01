# Python Binding (quiverdb)

Cross-layer naming rules (same snake_case names, `@staticmethod` factories, kwargs create/update)
and the convenience-method parity tables live in the root `AGENTS.md`. Local Python runs go
through `uv` (see root Build & Test).

## Layout

```
src/quiverdb/
  __init__.py     # Public exports: Database, QuiverError, LuaRunner, CSVOptions, DataType,
                  # LogLevel, ScalarMetadata, GroupMetadata, version()
  database.py     # Database class (inherits the CSV mixins below)
  database_csv_export.py / database_csv_import.py  # export_csv / import_csv mixins
  database_options.py  # CSVOptions-to-C marshaling
  lua_runner.py   # LuaRunner class
  metadata.py     # DataType/LogLevel (IntEnums), CSVOptions, ScalarMetadata, GroupMetadata
  element.py      # Element builder - INTERNAL ONLY (users pass **kwargs)
  exceptions.py   # QuiverError
  _helpers.py     # Shared check()/decode_string/column_data_type/format_datetime helpers
  _c_api.py       # Hand-written CFFI cdef declarations (kept in sync manually)
  _loader.py      # Library loading
  py.typed        # PEP 561 marker
generator/        # generator.py prints current cdecls from headers to stdout — a diff aid
                  # for hand-updating _c_api.py (it does NOT write the file)
tests/            # Test suite (test_*.py per area) + test.bat
format.bat        # uv sync, ruff check --fix (isort), ruff format; run from bindings/python
pyproject.toml    # Version must match CMakeLists.txt; requires-python >=3.13; deps: cffi>=2.0;
                  # dev group: pytest, ruff
ruff.toml         # Lint/format config; lint is isort only (select = ["I"])
```

## Rules and gotchas

- **CFFI ABI-mode** — no compiler required at install time; `_c_api.py` declarations must match
  the C headers exactly (struct layout mismatches corrupt silently). After C API changes, run
  `generator/generator.bat` and diff its output against `_c_api.py`.
- **`_loader.py` pre-loads `libquiver.dll`** on Windows so the OS resolves `libquiver_c.dll`'s
  dependency chain. `tests/test.bat` prepends `build/bin/` to PATH for DLL discovery.
- **API shape**: `create_element`/`update_element` accept `**kwargs` (dict unpacking works:
  `db.create_element("Collection", **my_dict)`); the `Element` class is internal — created, filled
  in a `try` and destroyed in its `finally` by each writer, with no `clear` and no binding-side
  use-after-destroy guard (a destroyed `Element` holds `ffi.NULL`, which the C API rejects as
  `Null argument: element`). Properties are
  regular methods, not `@property` (design decision). `LogLevel` is an `IntEnum` exported from
  `__init__.py`; internal mixin classes are not exported.
- **Every `**kwargs` method marks its leading parameters positional-only (`/`).** Without it, a
  kwarg that shares a parameter's name binds to the parameter and raises `TypeError: got multiple
  values for argument '<name>'` before the FFI call. All five follow it: `create_element(collection,
  /, **kwargs)`, `update_element(collection, id, /, **kwargs)`, `update_element_by_label(collection,
  label, /, **kwargs)`, `upsert_time_series_row(collection, group, id, /, **kwargs)` and
  `upsert_time_series_row_by_label(collection, group, label, /, **kwargs)`. A new `**kwargs` method
  gets the `/` too. With it the kwarg reaches the core like any other attribute:
  `update_element(c, id, **read_scalars_by_id(c, id))` works (that dict holds `id`), `label=`
  renames through `update_element_by_label`, and `id=`/`label=` on an upsert gets the core's
  `column '<name>' not found in group` error. The cost is that these parameters cannot be passed by
  keyword.
- **A nullable scalar string argument passes `ffi.NULL`, never `b""`** (`update_relation` /
  `update_relation_by_label`) — the C API reads NULL as "clear the relation" and an empty string
  as a label to look up.
- **Per-method FFI boilerplate is the house style** — don't collapse it into
  closure-parameterized helpers (root "Do not 'fix'" list).
- **Bulk and per-cell NULLs**: `read_scalar_integers`/`_floats` decode a parallel `uint8_t**` mask
  into `list[T | None]` (`mask[i]` falsy → `None`); the four numeric vector/set `_by_id` readers do
  the same, while the four numeric vector/set **bulk** readers decode a nested `uint8_t***` — one
  mask per element, parallel to `out_sizes` — freed by `quiver_database_free_masks`. The string
  readers carry no mask: a NULL cell is an `ffi.NULL` entry, guarded on read. The boolean and
  datetime wrappers map those lists while preserving their `None` slots. `_c_api.py` carries the
  mask out-params and both free functions. `read_time_series_row` decodes the same kind of mask,
  which the C API returns for **every** column type (strings included): mask 0 (no data at or
  before the date) → `None`, and each type branch frees the data array and the mask in a `finally`.
- **`_parse_datetime` gates on `_DATE_TIME_PATTERN` before calling `fromisoformat`.**
  `fromisoformat` is *wider* than the core's DATE_TIME grammar — it accepts `"20240115"`, a `Z`
  suffix and a UTC offset, none of which Julia's parser reads — so without the gate the same stored
  bytes read differently per binding. The gate is also what makes the trailing
  `.replace(tzinfo=timezone.utc)` correct: it used to **overwrite** an offset rather than convert
  it, so `"...T10:30:00+03:00"` came back as `10:30Z`, three hours off, with no error. Everything
  reaching that line is now naive. An out-of-range field that clears the regex (`"2024-02-31"`)
  falls through to the same rejection so the message still names the column. Keep this parser
  accepting exactly the same set as Julia's `string_to_date_time` and Dart's `stringToDateTime`.
  Its `@overload` triple mirrors `_integer_to_boolean`'s. No declared return type depends on the
  narrow `(str) -> datetime` variant any more — the vector/set readers keep NULL cells and are
  declared `list[... datetime | None]` — so it only sharpens the two callers that pass a
  guaranteed `str` (`query_date_time`, the time-series dimension column). Nothing typechecks this
  repo (`ruff.toml` is `select = ["I"]`, isort only; no mypy/pyright in CI, `pyproject.toml`, or
  the pre-commit hooks).
- **`format_datetime` (`_helpers.py`) is the write side's only datetime → string conversion.**
  `Element.set` (scalar and list), `_marshal_group_columns`, `_marshal_row_columns` and
  `read_time_series_row`'s `date_time` all call it. An aware value (`utcoffset()` not `None`) is
  converted to UTC before formatting, because every reader stamps UTC: formatting the wall clock,
  as `strftime` used to, stored `10:00+03:00` as `10:00` and read it back as `10:00Z` — the
  write-side twin of the `_parse_datetime` bug above. A naive value is written as given (never
  `astimezone` it — that would assume the host's zone). It lives in `_helpers.py`, not
  `database.py`, because `database.py` imports `element.py`. Query parameters (`_marshal_params`)
  deliberately take no datetime, as in Julia and Dart.
- **`_integer_to_boolean` raises `ValueError`, not `QuiverError`** — the second documented
  exception to "messages come from C++", alongside `_marshal_group_columns`' jagged-column and
  cell-type checks.
  The boolean readers are a binding-only convenience with no C++ counterpart, so the core cannot
  diagnose a stray `2`; the message names the offending `collection.attribute` (nothing to name for
  `query_boolean`). The `@overload` triple mirrors `bindings/js/src/boolean.ts`; every caller now
  passes `int | None` (the vector/set readers keep NULL cells and are declared
  `list[... bool | None]`), so the narrow `(int) -> bool` variant backs no declared type.
- **`Element._set_array` refuses a `None` cell** with a `TypeError` naming the column: a
  vector/set/time-series read returns a NULL cell as `None`, and without the check it failed inside
  cffi (`an integer is required`) or on `str.encode`, naming nothing. NULL cells are written with
  `update_vector_group` / `update_set_group` / `update_time_series_group` (the element surface stays
  non-null).
- **`LuaRunner.run` owns its result**: `quiver_lua_runner_run` takes a `char** out_result` and the
  JSON string must be freed with `quiver_lua_runner_free_string` — *not*
  `quiver_database_free_string` (both are hand-declared in `_c_api.py`). The free sits in a
  `finally` so a `decode_string` failure (the JSON is rejected as non-UTF-8 in C++, but be safe)
  cannot leak the native buffer.
- **`LuaRunner.__init__` starts with `_closed = True` and sets it to `False` only after `_ptr` is
  assigned.** Python runs `__del__` even when `__init__` raised, so a runner whose construction
  failed (e.g. `LuaRunner(closed_db)`, which the C API rejects with `Null argument: db`) must
  already look closed. Otherwise `__del__` emits a spurious `ResourceWarning` and then fails on the
  missing `_ptr`. Only moving `_closed = False` below `_ptr` is not enough: `__del__` then fails on
  the missing `_closed` instead. Pinned by `test_failed_construction_is_silent_when_collected`.
- **Time-series group NULLs**: `read_time_series_group` surfaces a SQL NULL cell as `None` in the
  column list (decoded via the per-cell `uint8_t**` mask out-param); the dimension column stays
  dense datetimes. `_marshal_group_columns` types each column from all of its non-`None` cells
  (`column_data_type`, below), builds a per-column mask, and substitutes `0`/`0.0`/`ffi.NULL` placeholders for `None` cells; an all-`None`
  column is tagged FLOAT with a zeroed placeholder.
- **`_marshal_group_columns` serves every columnar group writer** (time series, vector, set, by id
  and by label) — Dart's counterpart is `_marshalGroupColumns`. It raises `ValueError` for jagged
  column lists and `TypeError` for a cell that does not fit its column (both pre-FFI marshalling
  errors, the documented exception to "messages come from C++"); everything else is validated in
  the core and surfaces as `QuiverError`. Note that the group
  *writers* take columns while `read_vector_group_by_id` / `read_set_group_by_id` return rows (the
  vector form adds a synthetic 0-based `vector_index`); both read a NULL cell back as `None` in its
  row, so a NULL-cell write can be asserted through them.
- **`column_data_type` (`_helpers.py`) types a column from every cell**, for both
  `_marshal_group_columns` and `Element._set_array`: bool/int cells are INTEGER, a float anywhere
  among them widens the column to FLOAT (JS's and Dart's rule too, so `[1, 2.5]` writes 1.0 and 2.5
  in every binding), str is STRING and datetime DATE_TIME (the group marshaller formats it and tags
  STRING). Any other cell, or one that does not fit the column (a str among numbers, a number
  among strs, a str among datetimes), raises `TypeError: Unsupported value type <T> in cell <i> of
  column '<name>'` — Dart's wording. **No cell is run through `int()`/`float()`**: the first-cell
  dispatch this replaced did, so `[1, 2.5]` reached a REAL column as `[1, 2]` and `[1, "7"]` was
  parsed to 7, both with no error. cffi converts bool/int cells into `int64_t[]` and bool/int/float
  cells into `double[]` itself. Element arrays stay dense: `_set_array` refuses a `None` cell before
  typing (the `Element._set_array` bullet above), where a group-writer column skips it via the
  mask. An all-`datetime` list is formatted by `format_datetime` and written as strings, like a
  group-writer DATE_TIME column.
- **`_decode_group_rows` is the one decoder for the two whole-group readers** — a module-level data
  codec like `_marshal_group_columns` (Dart's `_decodeGroupRows`), not the closure-parameterized FFI
  helper the root "Do not 'fix'" list forbids: each reader keeps its own expanded FFI call block
  and hands the out-params over. It maps mask 0 to `None` (never `ffi.string` a masked-out NULL
  `char*`), parses DATE_TIME columns with `_parse_datetime`, and frees with
  `quiver_database_free_time_series_data` in a `finally`. The readers used to compose one
  per-column read per column, which resolves the column *name* — a column another group of the
  same kind shares came from that group's table — and took N snapshots. `read_time_series_group`
  keeps its own loop (columns, dimension-only parsing).
- **`_marshal_row_columns` is its row-shaped sibling**, serving `upsert_time_series_row` and its
  `_by_label` form — each kwarg is a scalar wrapped in a 1-element typed array (a `datetime` is
  formatted by `format_datetime` first). Kept separate because the row-upsert C signature carries
  no per-cell mask: the group marshaller's zeroed placeholder for a `None` cell would be written as
  data instead of NULL (a `None` kwarg raises `TypeError` here).

## Packaging

- Wheels build via **scikit-build-core** (`cmake.source-dir = ../..`, Release,
  `-DQUIVER_BUILD_TESTS=OFF`; the root CMakeLists detects `SKBUILD` and forces the C API ON).
  `wheel.exclude` strips `bin`/`lib`/`include`/`share` from the wheel.
- **cibuildwheel** targets `cp313-win_amd64`, `cp313-manylinux_x86_64` and `cp313-manylinux_aarch64`
  (built natively on `ubuntu-24.04-arm`), running pytest as the wheel test. CI publish flow in `.github/AGENTS.md`.
- Local wheel checks: `scripts/test-wheel.bat`, `scripts/test-wheel-install.bat`,
  `scripts/validate_wheel.py`, `scripts/validate_wheel_install.py`.
