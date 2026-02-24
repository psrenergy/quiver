# Milestones

## v1.0 Relations (Shipped: 2026-02-24)

**Phases completed:** 4 phases, 6 plans, 12 tasks
**Timeline:** 1 day (2026-02-23 -> 2026-02-24)
**Git range:** `feat(01-01)` -> `feat(04-02)`
**Source changes:** 23 files, +567/-1,063 lines

**Delivered:** FK label resolution in `create_element` and `update_element` for all column types, replacing the separate relation API with unified resolution through the standard write path.

**Key accomplishments:**
- Extracted shared `resolve_fk_label` helper on `Database::Impl` for FK label-to-ID resolution
- Built unified pre-resolve pass (`ResolvedElement` struct) resolving FK labels before SQL writes in `create_element`
- Wired same pre-resolve pass into `update_element` for all 4 column types (scalar, vector, set, time series)
- Removed redundant `update_scalar_relation` and `read_scalar_relation` from all 5 layers (C++, C API, Lua, Julia, Dart)
- Full test suite green: 442 C++ + 271 C API + 430 Julia + 248 Dart = 1,391 tests

**Test counts at ship:** C++ 442, C API 271, Julia 430, Dart 248

---


## v1.1 FK Test Coverage (Shipped: 2026-02-24)

**Phases completed:** 4 phases (5-8), 8 plans, 64 tests
**Timeline:** 1 day (2026-02-24)
**Git range:** `test(05-01)` -> `test(08-02)`
**Source changes:** 7 files, +1,980 lines (test code only)

**Delivered:** FK label resolution test coverage across all 4 binding layers (C API, Julia, Dart, Lua), mirroring the 16 C++ FK resolution tests (9 create + 7 update) in each layer.

**Key accomplishments:**
- 16 C API FK resolution tests (9 create + 7 update) verifying FK label-to-ID resolution through C API layer
- 16 Julia FK resolution tests mirroring C++ FK resolution through Julia FFI bindings
- 16 Dart FK resolution tests verifying FK resolution through Dart FFI bindings
- 16 Lua FK resolution tests verifying FK resolution through Lua scripting layer (with typed method variants for vector/time series update)
- Full binding parity: all 4 layers test identical FK scenarios with consistent error handling (type-only checks, no message inspection)
- Shared `relations.sql` schema used by all layers for FK test infrastructure

**Test counts at ship:** C++ 458, C API 287, Julia 459, Dart 248+

---

