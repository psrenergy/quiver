# Milestones

## v0.2 (Pre-GSD)

**Shipped:** Before GSD tracking

Full CRUD, typed scalar/vector/set/time-series operations, parameterized queries, relations, CSV, migrations, Lua scripting, C API, Julia and Dart bindings with auto-generated FFI.

**Last phase:** N/A (pre-GSD)

---

## v0.3 Explicit Transactions (Shipped: 2026-02-22)

**Phases completed:** 4 phases, 4 plans, 9 tasks
**Timeline:** 3 days (2026-02-20 to 2026-02-22)
**Execution time:** ~40 min
**Files changed:** 57 files, +7292 / -1506 lines
**Git range:** feat(01-01)..docs(v0.3)

**Key accomplishments:**
1. Public transaction API (begin/commit/rollback/in_transaction) on C++ Database with nest-aware TransactionGuard
2. C API transaction surface -- 4 flat FFI-safe functions with 6 test cases
3. Julia, Dart, and Lua bindings -- 4 raw functions + 1 convenience block wrapper per language, 27 new tests
4. Error messages propagate unchanged from C++ through every layer (no binding-crafted messages)
5. Standalone benchmark proving 20x+ write throughput from explicit transactions on file-based SQLite
6. Zero regressions across all test suites (C++ 418, C API 263, Julia 418, Dart 247)

**Key decisions:**
- Explicit begin/commit/rollback over RAII wrapper (simplest FFI surface)
- TransactionGuard becomes no-op when nested (avoids SAVEPOINT complexity)
- sqlite3_get_autocommit() for transaction detection (authoritative, no drift)
- bool* out_active for in_transaction C API (user preference, first bool* in C API)
- #include <stdbool.h> in C API header (C standard compliance for FFI generators)

**Archives:**
- milestones/v0.3-ROADMAP.md
- milestones/v0.3-REQUIREMENTS.md
- milestones/v0.3-MILESTONE-AUDIT.md

---
