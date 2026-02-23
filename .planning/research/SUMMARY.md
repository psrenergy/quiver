# Project Research Summary

**Project:** Quiver Python Bindings
**Domain:** Python FFI binding for a C database wrapper library (SQLite-backed, multi-language)
**Researched:** 2026-02-23
**Confidence:** MEDIUM — C API and existing bindings are HIGH confidence; CFFI-specific mechanics are MEDIUM (no web access during research)

## Executive Summary

Quiver Python bindings are a thin FFI wrapper over an already-complete C API, using CFFI ABI mode (`ffi.dlopen`) to load the compiled `libquiver_c` shared library at runtime. Two direct predecessors — Julia and Dart bindings — provide working, tested reference implementations for every pattern needed: DLL loading, error propagation, memory management, struct marshaling, and composite helper composition. The recommended approach is to mechanically port those patterns to idiomatic Python, following the cross-layer naming rules already defined in CLAUDE.md. No C++ or C API changes are required; Python is a pure consumer.

The recommended stack is CFFI >= 1.17 in ABI mode with uv + hatchling for packaging, pytest for tests, and ruff for linting — all of which are pre-decided in the existing `bindings/python` stub. The primary design challenge is not architectural novelty but disciplined FFI mechanics: every C-allocated array must be freed via its matching `quiver_database_free_*` function, strings must be consistently encoded/decoded as UTF-8, and on Windows both `libquiver.dll` and `libquiver_c.dll` must be pre-loaded in the correct order. These are well-understood patterns that the Julia and Dart bindings have already solved.

The main risk is memory management: Python's garbage collector is invisible to C-heap allocations, making silent leaks the primary failure mode rather than crashes. The mitigation is a mandatory `try/finally` pattern on every read path, established as infrastructure in Phase 1 before any feature work begins. Secondary risks are CFFI declaration mismatches (wrong pointer depth causes undefined behavior, not a Python exception) and the Windows DLL dependency chain (solved by pre-loading the core library, as Dart already does). All risks are well-understood and preventable with the patterns identified in research.

---

## Key Findings

### Recommended Stack

The stack is effectively pre-decided. The `bindings/python` stub already contains `pyproject.toml`, `Makefile`, `ruff.toml`, and `.python-version` (3.13) that pin the correct tools. The only new runtime dependency is `cffi >= 1.17`. CFFI ABI mode is the correct choice because it requires no C compiler at install time, loads the pre-built DLL at runtime identically to how Julia uses `Libdl.dlopen()`, and handles all C types present in the Quiver API (opaque pointers, typed structs, `void*` arrays, `int64_t*`/`double*`/`char**` output arrays).

**Core technologies:**
- **cffi >= 1.17** (ABI mode): Load and call `libquiver_c.dll/.so/.dylib` — no compiler required; handles all C types in the API; pre-decided in PROJECT.md
- **uv (latest)**: Dependency management and test runner — already in Makefile; modern, replaces pip+virtualenv
- **hatchling >= 1.25**: Build backend — already in stub's `[build-system]`; correct for a pure-Python package
- **pytest >= 8.4**: Test runner — already pinned in dev deps; `tmp_path` fixture handles temp DB files cleanly
- **ruff >= 0.12**: Linting and formatting — already in stub; replaces black + isort + flake8; configured in `ruff.toml`
- **python-dotenv >= 1.0**: Load `QUIVER_LIB_PATH` env var in tests — already in stub dev deps

**What NOT to add:** No pybind11, Cython, SWIG, cffi API mode, mypy, hypothesis, async wrappers, DateTime convenience wrappers, or PyPI packaging — all explicitly out of scope.

### Expected Features

The MVP for this milestone is complete API coverage with test parity. All table stakes features are required — there is no "defer some methods" option. The feature dependency graph shows a clear build order from infrastructure through increasing complexity.

**Must have (table stakes) — all required for parity:**
- CFFI library loader + `check()` helper + `QuiverError` exception — foundation for everything
- `Database` class with `from_schema()`, `from_migrations()`, context manager (`__enter__`/`__exit__`), `close()`
- `Element` builder class with fluent `set(name, value)` and context manager for safe destroy
- All scalar/vector/set read operations with correct memory management (`try/finally` + free)
- All scalar/vector/set write operations with Python-to-C array marshaling
- Metadata queries returning Python `@dataclass` instances (`ScalarMetadata`, `GroupMetadata`)
- Parameterized queries with parallel `param_types[]`/`param_values[]` arrays and `keepalive` list
- Explicit transactions (`begin_transaction`, `commit`, `rollback`, `in_transaction`) + `transaction()` context manager
- Time series read/write with `void**` column dispatch by type
- CSV export with `CSVExportOptions` struct marshaling
- Relations (`update_scalar_relation`, `read_scalar_relation`)
- `None` for absent optional values (consistent with Julia `nothing` / Dart `null`)
- Snake_case naming throughout (matches C++ and Julia exactly, per CLAUDE.md table)
- Test fixtures using shared `tests/schemas/valid/` schemas via pytest `conftest.py`

**Should have (Pythonic differentiators):**
- `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id` — pure Python composition, matches Julia/Dart
- `read_vector_group_by_id`, `read_set_group_by_id` — row-oriented dict output transposed from column-oriented C result
- `LogLevel` enum (`enum.IntEnum`) for database options — avoids magic integer constants in public API
- `@dataclass` for `ScalarMetadata` and `GroupMetadata` — structured, type-checkable return types
- Type hints throughout public API — Python 3.13 `|` union syntax; no runtime cost; `py.typed` already in stub

**Defer indefinitely (anti-features):**
- LuaRunner binding — explicitly out of scope in PROJECT.md
- PyPI packaging — out of scope; build from source only
- DateTime convenience wrappers — Python's `datetime.fromisoformat()` is sufficient; return raw ISO 8601 strings
- Async API — SQLite is synchronous; async wrappers mislead callers
- Custom exception subclasses per error type — couples Python to C error message format; single `QuiverError` is correct
- `import_csv` — C++ layer does not implement it; do not bind what does not exist

### Architecture Approach

The architecture is a strict layered FFI wrapper with no logic in the Python layer. A single `_lib.py` module loads the DLL once at import time and exposes raw `ffi` and `lib` objects. All wrapper modules import from `_lib.py` and convert C results to Python types immediately, freeing C memory before returning. The component structure mirrors the Julia binding's module organization (one file per functional area) and follows the C++ source file naming convention.

**Major components:**
1. `_lib.py` — DLL path resolution (env var primary, relative path fallback) + `ffi.dlopen()`; exposes `ffi`, `lib`; pre-loads `libquiver.dll` on Windows before `libquiver_c.dll`
2. `_declarations.py` — hand-written `ffi.cdef()` string with all C types and function signatures stripped of macros; single source of truth for the Python binding's view of the C API
3. `_error.py` — `QuiverError(Exception)` + `check()` helper; all C call results flow through `check()`; reads `quiver_get_last_error()` atomically
4. `database.py` — `Database` class with factory classmethods, `__del__` finalizer (pointer nullified on `close()` to prevent double-free), context manager
5. `element.py` — `Element` context manager wrapping `quiver_element_t*`; type-dispatches `set(name, value)` to correct C setter; destroys on `__exit__`
6. `database_read.py`, `database_update.py`, `database_create.py` — CRUD wrappers with mandatory `try/finally` free patterns
7. `database_metadata.py` — struct field extraction into `@dataclass` instances; decode all string fields before free
8. `database_query.py` — param marshaling with `keepalive` list; handles `int`, `float`, `str`, `None`
9. `database_transaction.py` — thin wrappers + `contextmanager`-based `transaction()` block
10. `database_time_series.py` — columnar `void**` dispatch by `column_types[i]`; most complex marshaling
11. `database_csv.py` — `CSVExportOptions` struct construction with lifetime-managed parallel arrays
12. `__init__.py` — public namespace re-exporting `Database`, `Element`, `QuiverError`, `LogLevel`, metadata dataclasses

**Key patterns:**
- Every C call: `check(lib.quiver_database_*(...))`; never inline error code checks
- Every array read: `try: build_python_result() finally: lib.quiver_database_free_*(ptr)`
- All strings: `value.encode("utf-8")` into C; `ffi.string(ptr).decode("utf-8")` back to Python
- Optional strings: `_decode_optional(ptr)` helper checks `ptr != ffi.NULL` before `ffi.string()`
- Param queries: allocate each value with `ffi.new`, hold in `keepalive` list for call duration
- Time series columns: read `column_types[i]`, then `ffi.cast("int64_t *" | "double *" | "char **", column_data[i])`

### Critical Pitfalls

1. **Missing C-owned memory free calls (silent memory leak)** — Every read operation returns a C-heap-allocated array that Python's GC cannot reclaim. The `try/finally` free pattern must be established as non-negotiable infrastructure from the first read function written. Detect with a tight-loop memory test.

2. **DLL load failure on Windows due to dependency chain** — `libquiver_c.dll` depends on `libquiver.dll`; if the core library is not pre-loaded before the C API library, `ffi.dlopen()` raises `OSError` that points to the wrong library. Fix: `ctypes.CDLL(libquiver_path)` before `ffi.dlopen(libquiver_c_path)` on Windows. Reference: Dart's `library_loader.dart` solves this identically.

3. **CFFI declaration mismatch with actual C API** — `ffi.cdef()` is a string; CFFI ABI mode does not parse headers or type-check. Wrong pointer depth (`void**` vs `void***`) or wrong integer type (`size_t` vs `int32_t`) causes undefined behavior, not a Python exception. Mitigation: write a smoke-test call for each declared function immediately after adding it; use Julia's `c_api.jl` as a complete, verified cross-reference.

4. **`keepalive` list for parameterized query values** — CFFI `ffi.new()` allocations are GC-eligible if not held in a live Python variable. Parameter buffers for `query_*_params` must be held in a list for the duration of the C call. Same issue applies to CSV options parallel arrays during `export_csv`.

5. **Finalizer called after DLL unloaded (segfault at interpreter shutdown)** — `__del__` on `Database` may fire after the DLL is unloaded. Mitigation: nullify `self._ptr = ffi.NULL` inside `close()`; guard `__del__` with `if self._ptr != ffi.NULL`. Provide `close()` and context manager as primary patterns so tests do not rely on finalizer timing.

---

## Implications for Roadmap

Based on the feature dependency graph in FEATURES.md and the phase-specific pitfall warnings in PITFALLS.md, the natural build order is from infrastructure through increasing FFI complexity. The Julia binding's module structure and the Dart binding's param marshaling approach together provide a complete implementation blueprint.

### Phase 1: Foundation and Infrastructure

**Rationale:** Everything else depends on library loading, error handling, and test scaffolding. Memory management patterns must be correct from the first function written — retrofitting `try/finally` across all read paths is tedious and error-prone. The package rename (`margaux` → `quiver`) must happen before any meaningful code is written.

**Delivers:** Working DLL loader (Windows + Linux + macOS), `QuiverError` + `check()`, `Database` factory methods + context manager + finalizer, `Element` builder, test infrastructure with shared schema fixtures, and a `test.bat` equivalent that sets PATH correctly on Windows.

**Addresses:** Package setup (rename pyproject.toml), DLL discovery, all lifecycle functions (`from_schema`, `from_migrations`, `close`, `describe`, `current_version`)

**Avoids:** Pitfall 3 (DLL dependency chain), Pitfall 5 (error TLS consumed too late), Pitfall 6 (double-free in finalizer), Pitfall 10 (on-disk test DB state leak), Pitfall 14 (package name drift), Pitfall 15 (`bool` vs `int` in `in_transaction`)

### Phase 2: Read Operations and Metadata

**Rationale:** Read operations represent the majority of test coverage and establish the `try/finally` free pattern that all subsequent phases repeat. Metadata must come before composite helpers. The memory management discipline established here is the most important quality gate of the entire binding.

**Delivers:** All `read_scalar_*`, `read_scalar_*_by_id`, `read_vector_*`, `read_set_*` operations; all metadata queries returning `@dataclass` instances; `list_scalar_attributes`, `list_vector_groups`, `list_set_groups`, `list_time_series_groups`; `read_element_ids`.

**Addresses:** Table stakes — scalar/vector/set reads, metadata queries, `None` for absent optional values

**Avoids:** Pitfall 1 (missing free), Pitfall 2 (UTF-8 encoding), Pitfall 12 (null pointer to `ffi.string()`), Pitfall 13 (metadata struct fields decoded before free)

### Phase 3: Write Operations and Transactions

**Rationale:** Write operations depend on `Element` (from Phase 1) and establish the Python-to-C array marshaling pattern for vector/set inputs. Transactions are low complexity and unblock batch write performance tests (benchmark already validates this in C++).

**Delivers:** `create_element`, `update_element`, `delete_element`; all `update_scalar_*`, `update_vector_*`, `update_set_*`; `begin_transaction`, `commit`, `rollback`, `in_transaction`; `transaction()` context manager.

**Addresses:** Table stakes — all write operations, explicit transaction control, `transaction()` context manager

**Avoids:** Pitfall 7 (`int32_t` count in `element_set_array_*` vs `size_t`)

### Phase 4: Queries and Relations

**Rationale:** Parameterized query marshaling is self-contained but introduces the `keepalive` pattern that also applies to time series updates and CSV export. Better to validate it here on a simpler surface before applying it to `void**` columns. Relations are two thin wrappers.

**Delivers:** `query_string`, `query_integer`, `query_float` (plain and `_params` variants); `update_scalar_relation`, `read_scalar_relation`.

**Addresses:** Table stakes — parameterized queries, relations

**Avoids:** Pitfall 11 (Python temporaries GC'd before parameterized call returns)

### Phase 5: Time Series

**Rationale:** Time series is the most complex marshaling task in the entire binding — `void**` column dispatch by type, per-column free with exact count tracking, and multi-column input construction for updates. It is isolated from earlier phases and should come last among core features.

**Delivers:** `read_time_series_group`, `update_time_series_group`, `has_time_series_files`, `list_time_series_files_columns`, `read_time_series_files`, `update_time_series_files`.

**Addresses:** Table stakes — time series read/write, time series files operations

**Avoids:** Pitfall 9 (`void**` column type dispatch — wrong cast reads garbage without exception)

### Phase 6: CSV Export and Composite Helpers

**Rationale:** CSV export has the second most complex struct marshaling (parallel arrays in `quiver_csv_export_options_t`) and benefits from the lifetime management lessons from time series. Composite helpers (`read_all_*`, `read_vector_group_by_id`) are pure Python composition with no new C interaction — trivial but placed last since they depend on all read and metadata operations being complete.

**Delivers:** `export_csv` with `CSVExportOptions`; `read_all_scalars_by_id`, `read_all_vectors_by_id`, `read_all_sets_by_id`, `read_vector_group_by_id`, `read_set_group_by_id`.

**Addresses:** Table stakes — CSV export; differentiators — composite read helpers

**Avoids:** Pitfall 8 (CSV options parallel array lifetime — Python arrays freed before C reads struct)

### Phase Ordering Rationale

- **Infrastructure before features:** `check()`, the free pattern, and the DLL loader are not optional safeguards — they are the correctness foundation. Any feature written without them will need rework.
- **Reads before writes:** Reads expose the memory management pattern that write marshaling also uses (allocating output arrays). Establishing it on reads first means write marshaling is incremental.
- **Simple marshaling before complex:** Scalar reads (flat `int64_t*`) → vector reads (array of arrays) → parameterized queries (typed `void*` array) → time series (`void**` dispatch by type). Each step adds one level of indirection.
- **Composite helpers last:** They are pure Python with no new FFI surface. They always depend on the lower-level operations they compose.
- **Tests with each phase:** Do not batch tests to the end. Each phase's test file should pass before the next phase begins.

### Research Flags

Phases with standard, well-documented patterns (skip research-phase):
- **Phase 1 (Foundation):** DLL loading, error handling, and context managers are standard CFFI and Python patterns. Julia/Dart provide complete reference implementations.
- **Phase 2 (Reads + Metadata):** Memory management patterns are established CFFI mechanics. Julia `database_read.jl` is a direct reference.
- **Phase 3 (Writes + Transactions):** Array marshaling and transaction context managers are well-understood. No novel patterns.
- **Phase 4 (Queries + Relations):** `keepalive` pattern for parameterized queries is documented in ARCHITECTURE.md. Dart's `_marshalParams` is a reference.
- **Phase 6 (CSV + Helpers):** CSV struct lifetime is documented in PITFALLS.md. Composite helpers are pure Python.

Phases that may benefit from deeper investigation during planning:
- **Phase 5 (Time Series):** The `void**` column dispatch pattern is the most complex FFI task in the binding. Recommend reading Julia's `read_time_series_group` implementation in full before writing the Python version. The free function signature (`quiver_database_free_time_series_data(char**, int*, void**, size_t, size_t)`) requires careful attention to which arguments are column names (freed per-element as strings) vs column data (freed per column by type).

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Pre-decided in pyproject.toml stub; cffi/pytest/ruff/uv all present; only CFFI version range needs PyPI verification |
| Features | HIGH | C API headers are the authoritative source; Julia/Dart bindings confirm all patterns work; scope boundary clear from PROJECT.md |
| Architecture | HIGH | Julia/Dart provide working reference implementations; layered FFI structure is proven; no novel architectural decisions |
| Pitfalls | MEDIUM | Core FFI pitfalls (memory, string encoding, DLL chain) are well-established; CFFI-specific mechanics based on training data through Aug 2025 without live docs verification |

**Overall confidence:** MEDIUM-HIGH — the domain is well-understood (FFI binding, not novel algorithm), the reference implementations exist and work, and the constraints are clearly documented. The MEDIUM boundary is from lack of web access to verify current CFFI documentation and PyPI version specifics.

### Gaps to Address

- **CFFI version specifics:** Verify current cffi, pytest, and ruff patch versions against PyPI before pinning in `pyproject.toml`. Research used knowledge cutoff August 2025.
- **`_declarations.py` completeness:** The hand-written cdef string must be validated against all C headers at implementation time. Use Julia's `c_api.jl` as a line-by-line cross-reference. This is implementation work, not a research gap, but it is the highest-risk mechanical task.
- **`import_csv` status:** FEATURES.md notes this exists in the C API as a stub but may not be implemented in the C++ layer. Verify before Phase 6 whether to bind it or skip it entirely.
- **Windows `os.add_dll_directory` vs PATH:** Python 3.8+ added `os.add_dll_directory()` as the preferred way to add DLL search paths on Windows (instead of PATH manipulation). Confirm whether `ctypes.CDLL(libquiver_path)` as a pre-load is sufficient or whether `os.add_dll_directory(build_bin_dir)` is also needed before `ffi.dlopen()`.

---

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h`, `element.h`, `options.h`, `common.h` — authoritative C API surface and type definitions
- `bindings/julia/src/` (`c_api.jl`, `database.jl`, `element.jl`, `exceptions.jl`) — working reference for all FFI patterns
- `bindings/dart/lib/src/` — working reference for param marshaling, struct parsing, DLL pre-loading
- `bindings/dart/lib/src/library_loader.dart` — Windows DLL dependency chain solution
- `CLAUDE.md` — cross-layer naming rules, error message patterns, transaction guard semantics
- `.planning/PROJECT.md` — constraints, scope boundaries, out-of-scope list
- `bindings/python/pyproject.toml`, `Makefile`, `ruff.toml`, `.python-version` — pre-decided tooling

### Secondary (MEDIUM confidence)
- CFFI ABI mode documentation (training knowledge, cutoff August 2025) — declaration format, `ffi.dlopen()`, `ffi.unpack()`, `void*` casting, `keepalive` requirement
- `include/quiver/c/element.h` — verified `int32_t` count type in `set_array_*` functions (vs `size_t` elsewhere)
- `include/quiver/c/common.h` — confirmed thread-local storage for error state

### Tertiary (LOW confidence)
- Current PyPI package versions (cffi, pytest, ruff) — need live verification before pinning; research used August 2025 cutoff

---
*Research completed: 2026-02-23*
*Ready for roadmap: yes*
