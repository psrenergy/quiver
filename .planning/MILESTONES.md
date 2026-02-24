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

