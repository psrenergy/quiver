# Phase 1: Foundation - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Python binding infrastructure: package setup (rename to "quiver", CFFI dependency, uv), DLL loading, error handling, and Database/Element lifecycle. The binding is installable, the DLL loads, errors surface as Python exceptions, and Database + Element work end-to-end. No read/write/query operations — those are Phase 2+.

</domain>

<decisions>
## Implementation Decisions

### Package layout
- Modular structure mirroring Julia: `src/quiver/` with separate files per domain (database.py, element.py, exceptions.py, c_api.py, etc.)
- Top-level `__init__.py` re-export strategy: Claude's discretion
- Build system: hatchling as build backend, uv as package manager for dependency management and running tests
- Test files mirror Julia/Dart pattern: one test file per functional area (test_database_lifecycle.py, etc.)

### Pythonic conventions
- Full type hints on every public method parameter and return type (Python 3.13+, py.typed marker)
- `__repr__` on all public types (Database, Element, ScalarMetadata, GroupMetadata, etc.)
- Element builder: fluent `.set()` only — `Element().set('label', 'x').set('value', 42)` — consistent across all bindings
- Database properties (path, current_version, is_healthy): regular methods, not @property — matches Julia/Dart pattern

### Error experience
- Single `QuiverError` exception inheriting from Exception — no hierarchy
- Message only (no structured error_code or operation attribute) — `str(e)` gives the C++ message
- Verbatim passthrough of C++ error messages — bindings never craft their own
- `check()` helper reads C API thread-local error and raises, does not call `quiver_clear_last_error()` — matches Julia/Dart behavior

### CFFI declarations
- Declaration organization: Claude's discretion (single file vs split)
- Declaration format: Claude's discretion (inline strings vs separate file)
- DLL loading: Python-conventional — use `ctypes.util.find_library()` first, fall back to known paths
- Generator script for updating declarations when C API changes (unlike Julia's hand-written approach, create a generator)

### Claude's Discretion
- Top-level `__init__.py` re-export strategy (everything vs minimal)
- CFFI declaration file organization (single vs split)
- CFFI declaration format (inline strings vs .h file)
- Internal helper structure and private module organization

</decisions>

<specifics>
## Specific Ideas

- Generator script for CFFI declarations — user wants automated maintenance when C API surface changes, despite the out-of-scope note about auto-generators (that note applied to the bounded ~200 declarations being hand-writeable; user overrode this preference)
- Methods over properties for Database accessors — explicit that these call into C API, not cached values
- Julia's c_api.jl is the reference for CFFI declarations regardless of generation approach

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-foundation*
*Context gathered: 2026-02-23*
