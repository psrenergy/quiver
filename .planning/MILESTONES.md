# Milestones

## v1.0 Quiver Refactoring (Shipped: 2026-02-11)

**Phases completed:** 10 phases, 15 plans
**Timeline:** 2026-02-09 to 2026-02-11 (3 days)
**Execution time:** ~3 hours
**Tests passing:** 1,213 across 4 suites (C++ 388, C API 247, Julia 351, Dart 227)

**Key accomplishments:**
1. Decomposed monolithic C++ database.cpp (1934 lines) and C API database.cpp (1612 lines) into 10+ focused modules each
2. Standardized naming across all 5 layers with mechanical transformation rules (C++ -> C API -> Julia -> Dart -> Lua)
3. Unified error handling: 3 C++ patterns, QUIVER_REQUIRE macro in C API, uniform error surfacing in all bindings
4. Added SQL identifier validation (require_column, is_safe_identifier) eliminating string concatenation risks
5. Integrated clang-tidy static analysis with zero project-code warnings across 30 source files
6. Documented cross-layer naming conventions in CLAUDE.md with transformation rules and representative examples

---


## v1.1 Time Series Ergonomics (Shipped: 2026-02-20)

**Phases completed:** 4 phases (11-14), 8 plans, 16 tasks
**Timeline:** 2026-02-19 to 2026-02-20 (2 days)
**Execution time:** ~34 minutes
**Files modified:** 17 (+2,056 / -417 lines)
**Tests passing:** 1,295+ across 4 suites (C++ 401, C API 257, Julia 399, Dart 238)
**Git range:** 0b4d1d9..650a59f

**Key accomplishments:**
1. Replaced single-column C API time series with multi-column typed-array signatures supporting N typed value columns (columnar parallel arrays: column_names[], column_types[], column_data[])
2. Rewrote Julia time series with idiomatic kwargs interface (`update_time_series_group!(db, col, group, id; date_time=[...], val=[...])`) and Dict{String, Vector} return type
3. Rewrote Dart time series with Map<String, List<Object>> interface, strict type enforcement, and DateTime auto-formatting/parsing
4. Added comprehensive Lua multi-column time series test coverage (6 tests) and composite read helper tests (3 tests) using mixed_time_series.sql
5. Full test suite gate: all 4 suites pass, old single-column API confirmed fully removed, CLAUDE.md updated with columnar typed-arrays documentation

---

