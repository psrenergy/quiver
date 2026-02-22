# Phase 2: C API Transaction Surface - Research

**Researched:** 2026-02-21
**Domain:** C API wrapper layer for C++ transaction methods (FFI surface)
**Confidence:** HIGH

## Summary

Phase 2 is a mechanical wrapping exercise. The C++ transaction API (`begin_transaction`, `commit`, `rollback`, `in_transaction`) is fully implemented and tested in Phase 1. The C API layer has a well-established pattern: every function takes `quiver_database_t*`, validates with `QUIVER_REQUIRE`, delegates to `db->db.method_name()` inside a try-catch, and returns `quiver_error_t`. The four new functions follow this pattern exactly, with zero novel design decisions.

The codebase has 9 existing C API implementation files in `src/c/`, 9 existing C API test files in `tests/`, a shared `internal.h` with the `QUIVER_REQUIRE` macro, and a `database_helpers.h` with marshaling utilities (not needed here -- transaction functions have no complex data to marshal). All patterns, conventions, file structure, and CMake registration are well-documented and consistent.

**Primary recommendation:** Create one new source file (`src/c/database_transaction.cpp`) with 4 functions, declare them in the existing `include/quiver/c/database.h` header, create one new test file (`tests/test_c_api_database_transaction.cpp`), and register both in their respective CMakeLists.txt files. This is a single-plan phase.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Include `in_transaction` in Phase 2 scope (the C++ method exists, it is a trivial one-liner wrapper)
- TQRY-01 requirement moves from "Future Requirements" into CAPI-01 / Phase 2 scope
- Update REQUIREMENTS.md traceability to reflect this
- Update ROADMAP.md Phase 2 success criteria to add a 4th criterion for `in_transaction`
- `in_transaction` should also be added to Phase 3 scope for Julia/Dart/Lua bindings
- `in_transaction` uses out-param pattern: `quiver_error_t quiver_database_in_transaction(quiver_database_t* db, bool* out_active)`
- Consistent with all other C API functions (no special-casing as a direct-return utility)
- Out-param named `out_active` (descriptive, not generic `out_result`)
- NULL db handle returns `QUIVER_ERROR` via `QUIVER_REQUIRE(db)` -- uniform with all other functions
- Lean tests that mirror success criteria -- C++ tests already cover transaction logic
- New test file: `test_c_api_database_transaction.cpp` (follows existing split pattern)
- Reuse the same schema from `tests/schemas/` that C++ transaction tests use
- Batched-writes test must verify data integrity: commit, then read back values through C API to confirm persistence

### Claude's Discretion
- Source file placement for C API implementation (new `database_transaction.cpp` or existing file)
- Header declaration location (existing `include/quiver/c/database.h` or new header)
- Exact test case names and organization within the test file

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CAPI-01 | C API exposes `quiver_database_begin_transaction` / `quiver_database_commit` / `quiver_database_rollback` as flat functions returning `quiver_error_t` | Exact function signatures derived from established C API pattern (QUIVER_REQUIRE + try-catch + binary return). Code examples below show verbatim implementation. |
| TQRY-01 (moved to CAPI-01 scope) | Public `in_transaction()` const query method exposed via C API | User decision: `quiver_database_in_transaction(db, &out_active)` with `bool* out_active` out-param. Implementation is a one-liner delegating to `db->db.in_transaction()`. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Quiver C++ core | current | Transaction logic (`begin_transaction`, `commit`, `rollback`, `in_transaction`) | All logic resides in C++ layer; C API is a thin wrapper |
| GoogleTest | current | C API test framework | Already used for all 9 existing C API test files |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| None | - | - | No additional dependencies needed |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| New source file `database_transaction.cpp` | Adding to existing `database.cpp` | New file follows the established split pattern (database_create.cpp, database_read.cpp, etc.) and keeps transaction operations co-located. Recommendation: new file. |
| New header file | Existing `include/quiver/c/database.h` | All other C API database functions are in the existing header. No reason to create a separate header for 4 functions. Recommendation: existing header. |

## Architecture Patterns

### Recommended File Structure
```
include/quiver/c/
  database.h             # ADD: 4 transaction function declarations (after lifecycle, before element ops)
src/c/
  database_transaction.cpp   # NEW: 4 transaction function implementations
tests/
  test_c_api_database_transaction.cpp  # NEW: transaction C API tests
```

### Pattern 1: Void C++ Method to C API Function (begin_transaction, commit, rollback)
**What:** Wraps a void C++ method that may throw, into a `quiver_error_t`-returning C function.
**When to use:** For `begin_transaction`, `commit`, `rollback` -- all are void methods on `Database` that throw `std::runtime_error` on precondition failure.
**Example:**
```cpp
// Source: src/c/database.cpp (existing pattern, e.g., quiver_database_describe)
QUIVER_C_API quiver_error_t quiver_database_begin_transaction(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.begin_transaction();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
```
**Confidence:** HIGH -- identical to `quiver_database_describe`, `quiver_database_export_csv`, `quiver_database_delete_element`.

### Pattern 2: Bool C++ Method to C API Function (in_transaction)
**What:** Wraps a `bool`-returning const C++ method into a `quiver_error_t`-returning C function with a `bool*` out-param.
**When to use:** For `in_transaction` -- returns a bool, cannot throw (it is a pure sqlite3 API query).
**Example:**
```cpp
// Source: derived from quiver_database_is_healthy pattern
QUIVER_C_API quiver_error_t quiver_database_in_transaction(quiver_database_t* db, bool* out_active) {
    QUIVER_REQUIRE(db, out_active);

    *out_active = db->db.in_transaction();
    return QUIVER_OK;
}
```
**Confidence:** HIGH -- `quiver_database_is_healthy` uses the same pattern (bool output via `int*` out-param). Note: the user decided on `bool*` not `int*`. The C++ `in_transaction()` is a one-liner wrapping `sqlite3_get_autocommit()` and cannot throw, so no try-catch is needed.

**Important note on `bool` in C API:** The existing C API uses `int*` for boolean out-params (see `quiver_database_is_healthy` with `int* out_healthy` and `quiver_database_read_scalar_integer_by_id` with `int* out_has_value`). The user explicitly decided on `bool* out_active`. Since `<stdbool.h>` provides `bool` in C99+ and `bool` is a built-in in C++, this is valid for the project's target environments. However, the planner should note that this is the first C API function using `bool*` instead of `int*` for a boolean result, which is a minor inconsistency with existing API surface. The user made this decision explicitly, so it should be followed as-is.

### Pattern 3: C API Test Structure
**What:** GoogleTest test using C API functions, with setup/teardown via `quiver_database_from_schema`/`quiver_database_close`.
**When to use:** For all C API transaction tests.
**Example:**
```cpp
// Source: tests/test_c_api_database_create.cpp (existing pattern)
TEST(DatabaseCApi, BeginMultipleWritesCommit) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // ... test body ...

    quiver_database_close(db);
}
```
**Confidence:** HIGH -- all 9 existing C API test files follow this exact pattern.

### Anti-Patterns to Avoid
- **Wrapping in new header:** Do not create `include/quiver/c/transaction.h`. All database C API functions live in `database.h`.
- **Adding try-catch to `in_transaction`:** The underlying `in_transaction()` calls `sqlite3_get_autocommit()` which cannot throw. Adding try-catch would be unnecessary defensive code, which violates the "clean code over defensive code" principle.
- **Testing transaction logic in C API tests:** The C++ tests already comprehensively test transaction semantics (nesting, mixed write types, etc.). C API tests should only verify the FFI surface: that calls pass through correctly, errors propagate, and the batched-writes success criterion is met.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Error propagation | Custom error forwarding | `quiver_set_last_error(e.what())` | Established pattern; C++ errors flow unchanged through `what()` |
| Null checking | Manual null checks | `QUIVER_REQUIRE(db)` macro | Auto-generates "Null argument: db" messages; consistent with all other functions |
| Boolean out-param | Complex conversion | Direct assignment `*out_active = db->db.in_transaction()` | `bool` converts naturally; no marshaling needed |

**Key insight:** This phase adds zero new abstractions. Every line of code follows an existing pattern that appears in at least 3 other C API functions.

## Common Pitfalls

### Pitfall 1: Forgetting to Register in CMakeLists.txt
**What goes wrong:** New source file compiles but is not linked into `quiver_c` library; new test file is not compiled into `quiver_c_tests` executable.
**Why it happens:** CMakeLists.txt uses explicit file lists (not globs), so new files must be manually added.
**How to avoid:** Add `c/database_transaction.cpp` to the `quiver_c` target in `src/CMakeLists.txt` (line ~93-105). Add `test_c_api_database_transaction.cpp` to the `quiver_c_tests` target in `tests/CMakeLists.txt` (line ~45-55).
**Warning signs:** Linker errors ("undefined reference to quiver_database_begin_transaction") or test not discovered.

### Pitfall 2: Missing Header Declarations
**What goes wrong:** Functions are implemented in the `.cpp` but not declared in the header, causing linker errors from bindings or test code.
**Why it happens:** Implementation and header edits are separate steps.
**How to avoid:** Declare all 4 functions in `include/quiver/c/database.h` before implementing them.

### Pitfall 3: Inconsistent `bool` vs `int` Out-Param
**What goes wrong:** Using `int*` for `in_transaction` when the user decided `bool*`, or vice versa.
**Why it happens:** Existing C API uses `int*` for boolean values (e.g., `out_healthy`, `out_has_value`).
**How to avoid:** User explicitly decided `bool* out_active`. Follow the decision. Note that `<stdbool.h>` must be included (or `common.h` already provides it via C++ `bool`). Since the header is wrapped in `extern "C"` and includes `<stdint.h>` via `common.h`, `bool` is available in C++ context. For pure C consumers, `<stdbool.h>` may need to be included in `common.h` -- verify this.

### Pitfall 4: Forgetting REQUIREMENTS.md and ROADMAP.md Updates
**What goes wrong:** Phase completes but traceability docs are stale.
**Why it happens:** User explicitly requested updating REQUIREMENTS.md (move TQRY-01) and ROADMAP.md (add 4th success criterion for `in_transaction`).
**How to avoid:** Include doc updates as explicit tasks in the plan.

## Code Examples

Verified patterns from the existing codebase:

### Complete Implementation File: `src/c/database_transaction.cpp`
```cpp
// Source: derived from src/c/database.cpp and src/c/database_create.cpp patterns
#include "internal.h"
#include "quiver/c/database.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_begin_transaction(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.begin_transaction();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_commit(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.commit();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_rollback(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.rollback();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_in_transaction(quiver_database_t* db, bool* out_active) {
    QUIVER_REQUIRE(db, out_active);

    *out_active = db->db.in_transaction();
    return QUIVER_OK;
}

}  // extern "C"
```

### Header Declarations (to add to `include/quiver/c/database.h`)
```c
// Transaction control
QUIVER_C_API quiver_error_t quiver_database_begin_transaction(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_commit(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_rollback(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_in_transaction(quiver_database_t* db, bool* out_active);
```

### Test: Batched Writes with Commit and Readback
```cpp
// Source: derived from tests/test_c_api_database_create.cpp pattern + C++ test_database_transaction.cpp
TEST(DatabaseCApi, TransactionBeginCommit) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    quiver_element_destroy(config);

    // Begin transaction
    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);

    // Create two elements inside transaction
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e2, &id2), QUIVER_OK);
    quiver_element_destroy(e2);

    // Commit
    ASSERT_EQ(quiver_database_commit(db), QUIVER_OK);

    // Read back to verify persistence
    char** labels = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Collection", "label", &labels, &count), QUIVER_OK);
    EXPECT_EQ(count, 2);
    EXPECT_STREQ(labels[0], "Item 1");
    EXPECT_STREQ(labels[1], "Item 2");
    quiver_database_free_string_array(labels, count);

    quiver_database_close(db);
}
```

### Test: Error Propagation (Double Begin)
```cpp
TEST(DatabaseCApi, TransactionDoubleBeginReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    EXPECT_EQ(quiver_database_begin_transaction(db), QUIVER_ERROR);

    const char* err = quiver_get_last_error();
    EXPECT_STREQ(err, "Cannot begin_transaction: transaction already active");

    // Clean up
    quiver_database_rollback(db);
    quiver_database_close(db);
}
```

### Test: in_transaction State Query
```cpp
TEST(DatabaseCApi, InTransactionReflectsState) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);

    bool active = true;
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_FALSE(active);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_TRUE(active);

    ASSERT_EQ(quiver_database_commit(db), QUIVER_OK);
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_FALSE(active);

    quiver_database_close(db);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| No transaction control in C API | Phase 2 adds it | Now (v0.3) | Enables language bindings (Phase 3) to expose transactions |

**Deprecated/outdated:**
- None applicable. This is new API surface, not a replacement.

## Open Questions

1. **`bool` vs `int` for `out_active` parameter in C header**
   - What we know: User explicitly decided `bool* out_active`. Existing C API uses `int*` for booleans (`out_healthy`, `out_has_value`).
   - What's unclear: Whether `bool` is available in the C compilation context. The header uses `extern "C"` and includes `common.h` which includes `<stdint.h>` but not `<stdbool.h>`.
   - Recommendation: Use `bool*` as decided. In C++ compilation (which is what this project uses), `bool` is built-in. If a pure-C consumer ever needs this, `<stdbool.h>` provides it. Since this is the first `bool*` out-param in the C API, the planner should consider whether to add `#include <stdbool.h>` to `common.h` for forward compatibility, or document it as a C++ requirement. Given that the project already compiles all C API files as C++ (the source files are `.cpp`), this is a non-issue in practice.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- Existing C API function declarations (all patterns derived from here)
- `src/c/database.cpp` -- Lifecycle function implementations (pattern template for `begin_transaction`, `commit`, `rollback`)
- `src/c/database_create.cpp` -- Element CRUD implementations (pattern template)
- `src/c/database_update.cpp` -- Update implementations (pattern template for void-returning wrappers)
- `src/c/internal.h` -- `QUIVER_REQUIRE` macro, `quiver_database` struct with `db` member
- `src/c/common.cpp` -- Error storage (`quiver_set_last_error`, `quiver_get_last_error`)
- `include/quiver/database.h` -- C++ `Database` class with `begin_transaction`, `commit`, `rollback`, `in_transaction`
- `src/database.cpp` -- C++ transaction implementations with precondition checks
- `src/database_impl.h` -- `TransactionGuard`, `Impl::begin_transaction/commit/rollback`
- `tests/test_database_transaction.cpp` -- C++ transaction tests (8 tests covering all success criteria)
- `tests/test_c_api_database_create.cpp` -- C API test patterns (setup, assertions, cleanup)
- `tests/CMakeLists.txt` -- Test registration pattern for `quiver_c_tests` target
- `src/CMakeLists.txt` -- Source registration pattern for `quiver_c` target

### Secondary (MEDIUM confidence)
- None needed -- all findings are from direct codebase inspection.

### Tertiary (LOW confidence)
- None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- No external dependencies; everything is in-tree and verified by reading source
- Architecture: HIGH -- Pattern is repeated 50+ times across 9 existing C API source files
- Pitfalls: HIGH -- Known from direct CMakeLists.txt inspection and user decision review

**Research date:** 2026-02-21
**Valid until:** Indefinite (codebase patterns, not external dependencies)
