# Quiver

## What This Is

Quiver is a SQLite wrapper library with a C++ core, a C API for FFI, and language bindings (Julia, Dart, Python, Lua). It models domain data as Collections with scalars, vectors, sets, and time series, plus a binary file subsystem (`.qvr` + `.toml`) and a lazy `Expression` engine for math over binary files.

## Core Value

Give power-users one consistent, intuitive API surface for structured + time series + binary array data across Julia, Dart, Python, and Lua — with all intelligence in C++ and bindings kept thin.

## Requirements

### Validated

<!-- Shipped capabilities (inferred from existing codebase). Locked. -->

- ✓ **CORE-01**: `Database` lifecycle: `from_schema`, `from_migrations`, `open`, `close`, move semantics — v1.0
- ✓ **CORE-02**: Explicit transactions: `begin_transaction`, `commit`, `rollback`, `in_transaction`, nest-aware RAII — v1.0
- ✓ **CORE-03**: Element CRUD: `create_element`, `update_element`, `delete_element` — v1.0
- ✓ **CORE-04**: Scalar / vector / set readers (integers, floats, strings) plus `_by_id` variants — v1.0
- ✓ **CORE-05**: Time series group read/update (multi-column rows) and single-row read with previous-value semantics — v1.0
- ✓ **CORE-06**: Time series files singleton table (`has`, `list`, `read`, `update`) — v1.0
- ✓ **CORE-07**: Scalar / group / time-series metadata getters + listers — v1.0
- ✓ **CORE-08**: Parameterized SQL queries (string / integer / float / date_time) — v1.0
- ✓ **CORE-09**: CSV export / import with enum + date formatting — v1.0
- ✓ **CORE-10**: Lua scripting via `LuaRunner` — v1.0
- ✓ **BIN-01**: `BinaryFile` open/read/write/get_metadata + write-registry guard — v1.0
- ✓ **BIN-02**: `BinaryMetadata` value type + TOML round-trip + `from_element` — v1.0
- ✓ **BIN-03**: `CSVConverter` bin↔csv + free-function iteration helpers — v1.0
- ✓ **EXPR-01**: Expression DAG with binary, unary, ternary, aggregate, label-axis nodes + `save()` — v1.0
- ✓ **FFI-01**: C API mirrors C++ surface with binary error codes + `quiver_get_last_error` — v1.0
- ✓ **BIND-01**: Julia, Dart, Python, Lua bindings with mechanical naming transformation rules — v1.0

### Active

<!-- Current milestone scope. -->

- ✓ **CORE-11..14**: `Database::add_time_series_row` C++ core — validated in Phase 1 (single-row upsert via `INSERT OR REPLACE`, nest-aware `TransactionGuard`, Pattern-1 errors, 9 GTest cases including multi-dim PK schema)
- ✓ **CAPI-11..13**: C API wrapper `quiver_database_add_time_series_row` — validated in Phase 2 (columnar typed-arrays marshaling mirroring `quiver_database_update_time_series_group`, D-07 wrapper-owned unknown-column-type error, 12 GTest cases at the FFI boundary)
- **JULIA-11 / DART-11 / PY-11 / LUA-11 / DOC-11**: Binding regeneration + docs sync — Phase 3

### Out of Scope

<!-- Explicit boundaries with reasoning. -->

- Backwards compatibility shims — WIP project, breaking changes acceptable
- Indexed `vector<int64_t>` dims overloads on `BinaryFile::read/write` — dropped in `b9c9ad0`, map-based form is the single supported entry point
- Deprecation of old APIs — delete-not-deprecate policy

## Context

- Brownfield codebase initialized into GSD on 2026-05-19. Prior history is captured in git, not in `.planning/MILESTONES.md`.
- Architecture, conventions, and cross-layer naming rules are documented in `CLAUDE.md` at the project root — treat it as authoritative for any layer naming or pattern question.
- Test suites exist in every layer: `quiver_tests.exe` (C++), `quiver_c_tests.exe` (C API), and `test.bat` per binding. CI conventions captured in `scripts/build-all.bat` and `scripts/test-all.bat`.
- `docs/time_series.md` documents an aspirational `add_time_series_row!` Julia helper that does not yet exist in the C++ core — first milestone closes that gap.

## Constraints

- **Tech stack**: C++20 core, SQLite, spdlog, Lua. Bindings: Julia (Quiver.jl), Dart, Python (CFFI ABI-mode), Lua. Build: CMake.
- **Platform**: Windows-first development environment; bindings must also work on Linux/macOS.
- **API homogeneity**: Binding surfaces must follow the mechanical naming rules in `CLAUDE.md` (C++ `verb_[category_]type[_by_id]` → C `quiver_database_*` → Julia `verb!`/`verb` → Dart `verbCamel` → Python `verb_snake` → Lua `verb_snake`).
- **Logic placement**: Intelligence lives in C++. Bindings stay thin marshalers; error messages originate in C++/C API.
- **Memory model**: RAII strictly; explicit ownership; Pimpl only when hiding private deps (`Database`, `LuaRunner`, `BinaryFile`, `CSVConverter`). Other types use Rule of Zero.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| C++ holds all logic; bindings are thin | Avoid drift between language wrappers | ✓ Good |
| Error messages defined in C++/C API only | Single source of truth surfaced through `quiver_get_last_error` | ✓ Good |
| Three canonical error message patterns (`Cannot {op}: ...`, `{Entity} not found: ...`, `Failed to {op}: ...`) | Downstream layers can rely on structured prefixes | ✓ Good |
| `update_time_series_group` replaces all rows | Simpler semantics, batch friendly | ⚠️ Revisit — no single-row append/upsert; v1.1 closes this |
| Drop indexed `vector<int64_t>` dims overloads (`b9c9ad0`) | API simplicity over micro-perf; map-based form is canonical | ✓ Good |
| Plan a vertical-slice milestone for `add_time_series_row` across all layers | Symmetric with `read_time_series_row`; closes docs/impl gap | — In progress (Phase 1 ✓) |
| `INSERT OR REPLACE INTO` over `INSERT ... ON CONFLICT DO UPDATE` for `add_time_series_row` (Phase 1) | Matches CORE-11's upsert semantic (omitted value columns reset to NULL/DEFAULT on the replaced row); shorter and mirrors existing time-series SQL style | ✓ Good |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

## Current Milestone: v1.1 add_time_series_row

**Goal:** Add a vertical-slice `add_time_series_row` API that inserts or upserts a single time series row, complementing the existing `read_time_series_row` and `update_time_series_group` operations, propagated across C++ core, C API, and all language bindings (Julia, Dart, Python, Lua).

**Target features:**
- C++ `Database::add_time_series_row` with upsert semantics on the time series PK
- C API `quiver_database_add_time_series_row` using the columnar typed-arrays pattern from `update_time_series_group`
- Julia `add_time_series_row!`, Dart `addTimeSeriesRow`, Python `add_time_series_row(**kwargs)`, Lua `add_time_series_row`
- Tests at every layer (C++, C API, Julia, Dart, Python)
- `docs/time_series.md` brought back into sync with shipped API

---
*Last updated: 2026-05-19 — Phase 2 (C API quiver_database_add_time_series_row) complete*
