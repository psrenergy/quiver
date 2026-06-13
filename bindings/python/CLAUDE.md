# Python Binding (quiverdb)

Cross-layer naming rules (same snake_case names, `@staticmethod` factories, kwargs create/update)
and the convenience-method parity tables live in the root `CLAUDE.md`. Local Python runs go
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
  _helpers.py     # Shared check()/decode_string helpers
  _c_api.py       # Hand-written CFFI cdef declarations (kept in sync manually)
  _loader.py      # Library loading
  py.typed        # PEP 561 marker
generator/        # generator.py prints current cdecls from headers to stdout — a diff aid
                  # for hand-updating _c_api.py (it does NOT write the file)
tests/            # Test suite (test_*.py per area) + test.bat
pyproject.toml    # Version must match CMakeLists.txt; requires-python >=3.13; deps: cffi>=2.0
ruff.toml         # Lint/format config (format.bat runs ruff)
```

## Rules and gotchas

- **CFFI ABI-mode** — no compiler required at install time; `_c_api.py` declarations must match
  the C headers exactly (struct layout mismatches corrupt silently). After C API changes, run
  `generator/generator.bat` and diff its output against `_c_api.py`.
- **`_loader.py` pre-loads `libquiver.dll`** on Windows so the OS resolves `libquiver_c.dll`'s
  dependency chain. `tests/test.bat` prepends `build/bin/` to PATH for DLL discovery.
- **API shape**: `create_element`/`update_element` accept `**kwargs` (dict unpacking works:
  `db.create_element("Collection", **my_dict)`); the `Element` class is internal. Properties are
  regular methods, not `@property` (design decision). `LogLevel` is an `IntEnum` exported from
  `__init__.py`; internal mixin classes are not exported.
- **Per-method FFI boilerplate is the house style** — don't collapse it into
  closure-parameterized helpers (root "Do not 'fix'" list).
- **Time-series group NULLs**: `read_time_series_group` surfaces a SQL NULL cell as `None` in the
  column list (decoded via the per-cell `uint8_t**` mask out-param); the dimension column stays
  dense datetimes. `_marshal_time_series_columns` dispatches on the first non-`None` element, builds
  a per-column mask, and substitutes `0`/`0.0`/`ffi.NULL` placeholders for `None` cells; an all-`None`
  column is tagged FLOAT with a zeroed placeholder.

## Packaging

- Wheels build via **scikit-build-core** (`cmake.source-dir = ../..`, Release,
  `-DQUIVER_BUILD_TESTS=OFF`; the root CMakeLists detects `SKBUILD` and forces the C API ON).
  `wheel.exclude` strips `bin`/`lib`/`include`/`share` from the wheel.
- **cibuildwheel** targets `cp313-win_amd64` and `cp313-manylinux_x86_64`, running pytest as the
  wheel test. CI publish flow in `.github/CLAUDE.md`.
- Local wheel checks: `scripts/test-wheel.bat`, `scripts/test-wheel-install.bat`,
  `scripts/validate_wheel.py`, `scripts/validate_wheel_install.py`.
