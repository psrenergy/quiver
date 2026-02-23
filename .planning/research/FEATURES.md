# Feature Landscape: Python CFFI Bindings for Quiver

**Domain:** Python FFI binding for a C database wrapper library
**Researched:** 2026-02-23
**Scope:** NEW milestone features only — existing C++/C API/Julia/Dart/Lua features are already built

---

## Table Stakes

Features Python users expect from a database binding. Missing = binding feels broken or unprofessional.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Context manager (`with` statement) | Python resource management idiom — every file, connection, lock uses `with`. Users reach for it automatically. | Low | `__enter__`/`__exit__` wrapping `from_schema` + `close`. Explicit `close()` should also remain available. |
| `QuiverError` exception class | C errors must surface as Python exceptions — not return codes. Users expect `try/except`, not `if err != OK`. | Low | Subclass `Exception`. Message comes from `quiver_get_last_error()` via `check()` helper. Error text already defined in C layer — Python never crafts its own. |
| `check()` helper | Internal function that converts C binary error codes to `QuiverError`. Every C call must go through it. | Low | Direct pattern from Julia/Dart. `if err != QUIVER_OK: raise QuiverError(ffi.string(lib.quiver_get_last_error()).decode())` |
| C memory freed after every read | CFFI leaves C-allocated memory as-is — Python layer must call the matching `free_*` function after copying data into Python objects. Never expose raw C pointers to callers. | Medium | Use try/finally pattern. Free must happen even if conversion raises. This is the most error-prone area. |
| Factory functions (not constructors) | `Database.from_schema(db_path, schema_path)` and `Database.from_migrations(db_path, migrations_path)` match C API factories. Python callers should never call `__init__` directly. | Low | Classmethods wrapping C lifecycle functions. Same pattern as Dart named constructors. |
| `Element` builder class | Python way to construct typed key/value payloads for `create_element`. Must accept `int`, `float`, `str`, and `list` of each. | Low | Wraps `quiver_element_*` C API. Fluent setters via `set(name, value)` dispatching by type. Auto-destroys via `__del__` or explicit `destroy()`. |
| `None` for optional values | `read_scalar_*_by_id` returns `None` (not a sentinel or exception) when element has no value for that attribute. | Low | `out_has_value == 0` → `return None`. Consistent with Julia's `nothing` and Dart's `null`. |
| Snake_case naming | Python expects `read_scalar_integers`, `create_element`, `update_vector_floats`. Matches C++ names exactly (same rule as Julia/Lua). No camelCase. | Low | Naming is mechanical, not a design decision. The cross-layer table in CLAUDE.md defines this. |
| Explicit transactions | `begin_transaction()`, `commit()`, `rollback()`, `in_transaction()` — direct wrappers, same names as C++. | Low | Thin wrappers over C API. Essential for batch write performance (benchmark already validates this). |
| `transaction()` context manager | `with db.transaction():` block that auto-commits on success and rolls back on exception. | Low | Compose `begin_transaction` + commit/rollback. Standard Python context manager protocol. |
| Read scalar/vector/set operations | All `read_scalar_integers`, `read_scalar_floats`, `read_scalar_strings`, `read_vector_*`, `read_set_*` and their `_by_id` variants. Returns Python `list` of `int`/`float`/`str`. | Medium | Medium complexity from memory management — copy data from C array into Python list, then free C array. |
| All write operations | `create_element`, `update_element`, `delete_element`, `update_scalar_*`, `update_vector_*`, `update_set_*`. | Medium | Marshaling Python lists to C arrays for vector/set writes. Use `ffi.new` + array construction. |
| Metadata queries | `get_scalar_metadata`, `get_vector_metadata`, `get_set_metadata`, `get_time_series_metadata`, `list_scalar_attributes`, `list_vector_groups`, `list_set_groups`, `list_time_series_groups`. Return Python dataclasses or named tuples. | Medium | Struct field extraction. Use `@dataclass` for `ScalarMetadata` and `GroupMetadata`. Must free C-allocated metadata structs after extraction. |
| Parameterized queries | `query_string(sql, params=[])`, `query_integer(sql, params=[])`, `query_float(sql, params=[])`. Params are a plain Python list of `int`, `float`, `str`, or `None`. | Medium | Must marshal Python list into parallel `param_types[]` / `param_values[]` C arrays. Type dispatch by `isinstance`. Pattern from Dart's `_marshalParams`. |
| CSV export | `export_csv(collection, group, path, options=None)`. `options` includes `date_time_format` and enum mappings. | Medium | CSV options struct requires building nested C arrays for enum mappings. Medium complexity from parallel-array layout of `quiver_csv_export_options_t`. |
| Relations | `update_scalar_relation`, `read_scalar_relation`. | Low | Simple thin wrappers. `read_scalar_relation` returns `list[str | None]`. |
| Time series read/write | `read_time_series_group`, `update_time_series_group`. Returns/accepts `dict[str, list]` (column name → typed list). | High | Highest complexity. Columnar typed-array marshaling with `void**` dispatch by `int*` type array. Python must handle per-column type dispatch when unpacking. Must call `free_time_series_data` with exact column/row counts. |
| Time series files | `has_time_series_files`, `list_time_series_files_columns`, `read_time_series_files`, `update_time_series_files`. | Low | Simple parallel string array operations. |
| `describe()` | Prints schema info to stdout. Direct C call with no return value. | Low | One-liner. |
| `current_version()` | Returns `int`. Direct C call. | Low | One-liner. |
| Schema-based test fixtures | Tests must use shared `tests/schemas/valid/` schemas, not private copies. Pattern already established by Julia/Dart. | Low | Test infrastructure, not a binding feature. Required for parity. |
| Shared `conftest.py` with `tmp_path` fixture | pytest already in dev dependencies. Fixtures create temp databases using `from_schema` and `tmp_path`. | Low | Standard pytest pattern. |

---

## Differentiators

Features that exceed minimum expectations. Not required, but make the binding genuinely Pythonic.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| `read_all_scalars_by_id(collection, id)` | Returns `dict[str, int | float | str | None]` for all scalars of an element in one call. Matches Julia/Dart. Avoids boilerplate `list_scalar_attributes` + per-attribute reads. | Low | Compose `list_scalar_attributes` + typed `read_scalar_*_by_id` calls. Pure Python, no new C interaction. |
| `read_all_vectors_by_id(collection, id)` | Returns `dict[str, list]` for all vector groups of an element. | Low | Same composition pattern as `read_all_scalars_by_id`. |
| `read_all_sets_by_id(collection, id)` | Returns `dict[str, list]` for all set groups of an element. | Low | Same composition pattern. |
| `transaction()` as context manager with callable form | `with db.transaction():` (context manager) OR `db.transaction(fn)` (callable). Context manager form is more Pythonic than callable. | Low | Use `contextlib.contextmanager` or implement `__enter__`/`__exit__`. Context manager is preferred for Python; callable form is optional compatibility. |
| `ScalarMetadata` and `GroupMetadata` as `@dataclass` | Structured return types instead of raw tuples. `metadata.name`, `metadata.data_type`, `metadata.not_null`. Type-checkable and readable. | Low | Use Python `@dataclass` or `dataclasses.dataclass`. Aligns with Python typing idioms. No C complexity. |
| `LogLevel` enum for database options | `quiver.LogLevel.DEBUG`, `quiver.LogLevel.INFO`, etc. Wraps `quiver_log_level_t`. Avoids magic integer constants leaking into Python API. | Low | `enum.IntEnum` subclass mapping Python names to C integer values. |
| Type hints throughout | All public methods have `-> list[int]`, `-> str | None`, `-> dict[str, list]`, etc. Python 3.13 supports `|` union syntax natively. | Low | No runtime cost. Enables mypy and IDE autocompletion. `py.typed` marker already in stub. |
| `read_vector_group_by_id` / `read_set_group_by_id` | Returns `list[dict[str, Any]]` (rows of dicts). Transposes from column-oriented C result to row-oriented Python. Matches Julia/Dart. | Low | Pure Python composition on top of existing read + metadata calls. |

---

## Anti-Features

Features to explicitly NOT build for this milestone.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| LuaRunner binding | Explicitly out of scope in PROJECT.md. Embedded scripting is niche; Lua use case not relevant to Python users. Adding it bloats the binding with complex marshaling for negligible benefit. | Skip. Document as out of scope if anyone asks. |
| PyPI packaging / `pip install` | Out of scope in PROJECT.md. Packaging requires versioning strategy, CI publishing, platform wheel builds. That is a separate milestone, not a binding milestone. | Build from source. Test with local install via `pip install -e .`. |
| ORM / ActiveRecord layer | No logic in the Python layer. Project philosophy: intelligence lives in C++. An ORM adds a Python schema model that duplicates C++ schema knowledge. | Keep thin. Users define schema in SQL; Python reflects it through metadata queries. |
| Async API | No async variant of any operation. SQLite is inherently synchronous. CFFI calls block. Adding `async def` wrappers provides no benefit and misleads callers about concurrency behavior. | Synchronous only. Document this. |
| Custom error subclasses per error type | `QuiverError` is sufficient. Parsing C error message text to create typed subclasses (e.g., `NotFoundError`, `ConstraintError`) couples Python to C error message format, which is not versioned. | Single `QuiverError` with `.message` attribute. Error text from C layer is already structured (3 patterns documented in CLAUDE.md). |
| Python-side schema validation | C++ layer already validates schema at open time. Python duplicating validation adds maintenance burden without benefit. | Rely on `QuiverError` raised from C layer if schema is invalid. |
| `import_csv` | C API has `quiver_database_import_csv` but it is not part of the C++ `Database` class API — it is a C-only stub. Check whether it is implemented before exposing it. If not implemented in C++, skip. | Do not bind until C++ layer implements it. |
| DateTime convenience wrappers | Julia and Dart have `read_scalar_date_time_by_id` etc. Python's `datetime` standard library is straightforward enough that users can convert ISO 8601 strings themselves. Only add if evidence of demand emerges. | Return raw `str` for `DATE_TIME` columns. Document format is ISO 8601. Let callers parse with `datetime.fromisoformat()`. |
| `read_only` option in `from_schema` / `from_migrations` | C API supports `read_only` in `quiver_database_options_t`. Expose as `Database.open(path, read_only=False)` if needed, but not required for `from_schema`/`from_migrations` factory methods in v1. | Add `kwargs` forwarding later if requested. Skip for now to keep factory signatures clean. |

---

## Feature Dependencies

```
# Core lifecycle (required first — everything else depends on this)
CFFI library load + check() helper
  → Database.from_schema / Database.from_migrations (factory methods)
    → Database.__enter__ / __exit__ (context manager)
    → Database.close()

# Element (required for create_element)
Element class (wraps quiver_element_t)
  → Database.create_element(collection, element)
  → Database.update_element(collection, id, element)

# Read operations (no dependencies beyond lifecycle)
Database.read_scalar_integers / floats / strings (collection, attribute)
Database.read_scalar_integer_by_id / float_by_id / string_by_id (collection, attribute, id)
Database.read_vector_* / read_set_* variants
Database.read_element_ids

# Composite reads (depend on list + typed read variants)
list_scalar_attributes → read_all_scalars_by_id
list_vector_groups → read_all_vectors_by_id / read_vector_group_by_id
list_set_groups → read_all_sets_by_id / read_set_group_by_id

# Write operations (no dependencies beyond Element + lifecycle)
Database.update_scalar_integer / float / string
Database.update_vector_integers / floats / strings
Database.update_set_integers / floats / strings
Database.delete_element

# Transactions (depend on lifecycle)
begin_transaction / commit / rollback / in_transaction
  → transaction() context manager (depends on the four above)

# Queries (depend on lifecycle + param marshaling)
param marshal helper (private)
  → query_string / query_integer / query_float
  → query_string_params / query_integer_params / query_float_params

# Metadata (depend on lifecycle, feed composite reads)
get_scalar_metadata / get_vector_metadata / get_set_metadata / get_time_series_metadata
list_scalar_attributes / list_vector_groups / list_set_groups / list_time_series_groups

# Time series (depend on lifecycle, complex marshaling, independent of metadata in C)
update_time_series_group / read_time_series_group
has_time_series_files / list_time_series_files_columns
read_time_series_files / update_time_series_files

# Relations (depend on lifecycle)
update_scalar_relation / read_scalar_relation

# CSV (depend on lifecycle, requires options struct marshaling)
export_csv(collection, group, path, options=None)

# Build infrastructure (prerequisite for all)
pyproject.toml (name: quiver, dependency: cffi)
DLL loading (platform-aware: .dll on Windows, .so on Linux, .dylib on macOS)
C header parsing (ffi.cdef from header files or inline definitions)
```

---

## MVP Recommendation

The MVP for this milestone is complete API coverage with test parity. All features in the Table Stakes section are required. There is no "defer some C API methods" option — the goal stated in PROJECT.md is test parity across all layers.

**Build order within the milestone:**

1. **Infrastructure first**: CFFI library loader, `ffi.cdef` from C headers, `check()` helper, `QuiverError`. Unblocks everything.
2. **Lifecycle + Element**: `Database` class with factory methods, context manager, `close()`. `Element` builder. Enables `create_element` which enables all subsequent tests.
3. **Reads (scalar, vector, set)**: Most test coverage lives here. Memory management patterns established once and repeated.
4. **Metadata**: Required for composite read helpers (`read_all_*`). Moderate complexity from struct field extraction.
5. **Transactions**: Low complexity. Enables batch operation tests.
6. **Write operations**: Scalar simple. Vector/set require array marshaling.
7. **Queries**: Parameterized query marshaling is moderately complex but self-contained.
8. **Relations**: Simple. Two functions.
9. **Time series**: Most complex. Build last. Columnar `void**` marshaling is the hardest FFI task.
10. **CSV export**: Second most complex from options struct. Build after time series.
11. **Composite helpers**: `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`, `read_vector_group_by_id`, `read_set_group_by_id`. Pure Python, build last.
12. **Tests**: Each feature tested as built. All tests use `tests/schemas/valid/` schemas.

**Defer indefinitely**: DateTime convenience wrappers, LuaRunner, PyPI packaging, async API.

---

## Implementation Notes (CFFI-Specific)

These are Python/CFFI-specific concerns that affect every feature and are not specific to one category.

**Library loading**: Must be platform-aware. `libquiver_c.dll` on Windows, `libquiver_c.so` on Linux, `libquiver_c.dylib` on macOS. `libquiver_c` depends on `libquiver` — both must be on the load path. Dart's `library_loader.dart` handles this by PATH manipulation; Python must do equivalent via `os.add_dll_directory` (Windows) or `RPATH` at build time (Linux/macOS).

**Header parsing**: Two options for `ffi.cdef`:
- Inline: Copy C declarations manually into Python string. Simple, no file dependency, but diverges from headers on changes.
- File-based: Read `.h` files at import time, strip non-CFFI constructs (macros, `#ifdef`, etc.). More complex loader but stays in sync.
- Recommended: Inline for initial implementation. If headers diverge, migrate to file-based. Julia uses a generated `c_api.jl` from Clang.jl; Dart uses `ffigen`. Python can use `cffi`'s own `cdef()` with a carefully cleaned header string, or `pycparser` for automation later.

**Memory management pattern**: Every function that returns a C-allocated array must free it. Pattern:
```python
out_values = ffi.new("int64_t **")
out_count = ffi.new("size_t *")
check(lib.quiver_database_read_scalar_integers(db._ptr, collection.encode(), attribute.encode(), out_values, out_count))
count = out_count[0]
result = [out_values[0][i] for i in range(count)]
lib.quiver_database_free_integer_array(out_values[0])
return result
```
Use `try/finally` for the free call. Never return before freeing.

**String encoding**: All C strings are UTF-8. Python `str` → `value.encode("utf-8")` → `const char*`. C `char*` → `ffi.string(ptr).decode("utf-8")`. Wrap in helpers to avoid repetition.

**Null pointer check**: `out_values[0] == ffi.NULL` before iterating. Count == 0 short-circuit before pointer check.

**`void**` for time series**: CFFI represents `void**` as `void **`. Per-column type dispatch requires reading `column_types[i]` and casting `column_data[i]` accordingly: `ffi.cast("int64_t *", column_data[i])` for INTEGER, `ffi.cast("double *", ...)` for FLOAT, `ffi.cast("char **", ...)` for STRING/DATE_TIME. This is the one place where CFFI type safety breaks down and careful casting is required.

---

## Sources

- C API headers: `include/quiver/c/database.h`, `include/quiver/c/element.h`, `include/quiver/c/options.h` (HIGH confidence — source of truth)
- Julia binding implementation: `bindings/julia/src/` (HIGH confidence — working reference for all memory management patterns)
- Dart binding implementation: `bindings/dart/lib/src/` (HIGH confidence — working reference for param marshaling and struct parsing)
- CLAUDE.md cross-layer naming table (HIGH confidence — authoritative for naming rules)
- PROJECT.md (HIGH confidence — defines scope, out-of-scope, constraints)
- CONCERNS.md (HIGH confidence — documents known complexity areas including time series marshaling and string vector memory risk)
- CFFI documentation knowledge (MEDIUM confidence — based on training data through Aug 2025; verify `ffi.cdef` string construction against current cffi docs)
