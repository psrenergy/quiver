# Quiver

## What This Is

SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python, Lua). Provides schema-driven typed database operations (CRUD, vectors, sets, time series, relations, queries, CSV export) through a layered architecture where intelligence lives in C++ and bindings are thin wrappers.

## Core Value

Consistent, type-safe database operations across multiple languages through a single C++ implementation.

## Requirements

### Validated

<!-- Shipped and confirmed valuable. -->

- C++20 core library with full CRUD, vectors, sets, time series, relations, parameterized queries, CSV export, schema validation, explicit transactions
- C API (FFI layer) wrapping all public C++ methods with binary error codes
- Julia bindings (Quiver.jl) with CFFI via Clang.jl generator
- Dart bindings (quiver_db) with FFI via ffigen generator
- Lua bindings via sol2 (embedded scripting, not standalone)
- Migration support for schema versioning
- Benchmark tooling for transaction performance
- ✓ Python bindings (quiverdb) via CFFI ABI-mode wrapping the full C API — v0.4
- ✓ Test parity across C++, C API, Julia, Dart, Python, and Lua (1,849 tests, all pass) — v0.4

### Active

<!-- Current scope. Building toward these. -->

(None — planning next milestone)

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- LuaRunner in Python binding — niche use case, Lua scripting is for embedded scenarios
- Python <3.13 support — latest only, smallest maintenance surface
- Python packaging/distribution (PyPI) — build from source for now, packaging is a separate concern
- CFFI API mode for Python — ABI mode sufficient, no C compiler at install time
- Auto-generator for Python CFFI declarations — C API is bounded (~200 declarations), hand-writing is simpler

## Context

- Current version: 0.4.0 (WIP, breaking changes acceptable)
- 5 language bindings operational: Julia, Dart, Python, Lua, plus C API for direct use
- Python binding (quiverdb) at `bindings/python/` — CFFI ABI-mode, 5,098 LOC
- Total codebase: ~53,000 LOC across C++, C API, Julia, Dart, Python, Lua
- Test suite: 1,849 tests across 6 layers (C++ 536, C API 329, Julia 506, Dart 294, Python 184)
- All test schemas shared in `tests/schemas/valid/` and `tests/schemas/invalid/`
- Shared `all_types.sql` schema for cross-layer consistency testing

## Constraints

- **Binding philosophy**: Thin wrappers only. No logic in binding layer. Error messages come from C++/C API.
- **Naming**: Python uses `snake_case` matching C++ method names exactly (same as Julia/Lua pattern).
- **Code quality**: Human-readable, clean code. Simple solutions over complex abstractions.
- **Package manager**: uv for Python dependency management and virtual environments.
- **Test schemas**: All bindings reference shared schemas from `tests/schemas/`.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| CFFI ABI-mode for Python FFI | Standard, well-maintained, no compiler at install time | ✓ Good — works reliably across all operations |
| Package name: quiverdb | Avoid PyPI name conflicts, "quiver" was taken | ✓ Good — all tests and imports use quiverdb |
| Python >=3.13 only | Smallest maintenance surface, latest features | ✓ Good — no compat issues |
| No LuaRunner in Python | Niche use case, not core to Python binding value | ✓ Good — kept scope focused |
| Properties as methods | Consistency with C++ naming, simpler implementation | ✓ Good — avoids @property magic |
| Database __del__ warns | ResourceWarning for unclosed databases (not silent) | ✓ Good — catches leaks in dev |
| Scalar relations as pure-Python | Avoids phantom C symbol; composes metadata+query+update | ✓ Good — fixed crash, clean architecture |
| void** for time series marshaling | Type-safe columnar dispatch matching C API pattern | ✓ Good — handles N typed columns |
| import_csv as NotImplementedError | C++ stub exists but no-op; Python surfaces this clearly | ⚠️ Revisit when C++ implements |

---
*Last updated: 2026-02-24 after v0.4 milestone*
