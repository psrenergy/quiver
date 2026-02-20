# Stack Research

**Domain:** Explicit transaction control for SQLite wrapper library (C++ core, C API, bindings)
**Researched:** 2026-02-20
**Confidence:** HIGH

## Recommended Stack

### Core Technologies (No New Dependencies Required)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| SQLite3 | 3.50.2 (already in project) | `sqlite3_get_autocommit()` for nesting detection | Already linked. The function has existed since SQLite 3.0. No upgrade needed. |
| `std::chrono::steady_clock` | C++20 standard library | Benchmark timing for transaction performance tests | Zero-dependency, monotonic clock immune to NTP/DST adjustments. Already available via C++20 standard. |
| GoogleTest | 1.17.0 (already in project) | Benchmark test assertions (correctness verification around perf tests) | Already the test framework. Benchmark tests are just regular `TEST()` cases with timing. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Google Benchmark | 1.9.5 | Formal microbenchmark framework with statistical analysis | ONLY if simple `steady_clock` timing proves insufficient. See "Alternatives Considered" below. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `sqlite3_get_autocommit()` | Detect whether a user transaction is already active | Returns non-zero when in autocommit mode (no active txn), zero when inside a transaction. Available in project's SQLite 3.50.2. |
| `sqlite3_txn_state()` | Finer-grained transaction state (NONE/READ/WRITE) | Available since SQLite 3.34.0. More detailed than `sqlite3_get_autocommit()` but not needed for this milestone -- binary detection is sufficient. |

## No Installation Required

This milestone requires **zero new dependencies**. Everything needed is already in the project:

```cmake
# Already in cmake/Dependencies.cmake:
# - SQLite 3.50.2 (provides sqlite3_get_autocommit)
# - GoogleTest 1.17.0 (provides test framework for benchmark tests)
# - spdlog 1.17.0 (provides logging for transaction lifecycle)

# Already in CMakeLists.txt:
# - C++20 standard (provides std::chrono::steady_clock)
```

If Google Benchmark is added later (not recommended for this milestone):

```cmake
# In cmake/Dependencies.cmake, inside a QUIVER_BUILD_BENCHMARKS guard:
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
)
FetchContent_MakeAvailable(googlebenchmark)
```

## Key Technical Findings

### 1. SQLite Transaction Nesting Rules

SQLite does NOT support nested `BEGIN` statements. Issuing `BEGIN` inside an existing `BEGIN...COMMIT` block produces an error ("cannot start a transaction within a transaction"). This is the core constraint driving the design.

**Nesting options:**
- **SAVEPOINT**: SQLite supports `SAVEPOINT name` / `RELEASE name` / `ROLLBACK TO name` for nested transactions. These are fully nestable and work inside `BEGIN...COMMIT` blocks.
- **No-op guard**: The simpler approach -- `TransactionGuard` checks `sqlite3_get_autocommit()` and becomes a no-op if a user transaction is already active.

**Recommendation: No-op guard, not SAVEPOINT.** Rationale:
1. The project's `TransactionGuard` exists solely to ensure atomicity of multi-statement internal operations (e.g., `create_element` inserts into collection + vector + set tables). When a user transaction wraps these, the user's transaction already provides that atomicity.
2. SAVEPOINT introduces rollback-to semantics that conflict with the simple "guard rolls back on exception" pattern. If an internal SAVEPOINT is rolled back to but the outer user transaction continues, partial state becomes confusing.
3. SAVEPOINT adds naming complexity (unique names, lifetime management) with no benefit for this use case.
4. The project's key decision in `PROJECT.md` already leans toward "TransactionGuard becomes no-op when nested."

### 2. Autocommit Detection API

`sqlite3_get_autocommit(db)` is the correct API:
- Returns **non-zero** when autocommit is ON (no active transaction).
- Returns **zero** when inside a `BEGIN...COMMIT/ROLLBACK` block.
- **Caveat**: Certain errors (`SQLITE_FULL`, `SQLITE_IOERR`, `SQLITE_NOMEM`, `SQLITE_BUSY`, `SQLITE_INTERRUPT`) can trigger automatic rollback. After such errors, `sqlite3_get_autocommit()` is the **only reliable way** to detect whether the transaction was silently rolled back.
- **Thread safety**: Return value is undefined if another thread changes the autocommit status concurrently. Not a concern here since Quiver's `Database` is single-threaded by design (one `sqlite3*` handle, no sharing across threads).

`sqlite3_txn_state(db, NULL)` (added in SQLite 3.34.0, available in 3.50.2) provides finer granularity (NONE/READ/WRITE) but is unnecessary -- we only need the binary "is a transaction active?" check.

### 3. Existing Internal Transaction Infrastructure

The codebase already has the exact primitives needed:

**In `database_impl.h`:**
- `Impl::begin_transaction()` -- executes `BEGIN TRANSACTION;`
- `Impl::commit()` -- executes `COMMIT;`
- `Impl::rollback()` -- executes `ROLLBACK;` (swallows errors since rollback is error-recovery)
- `Impl::TransactionGuard` -- RAII wrapper: begins in constructor, rollback in destructor unless `.commit()` called

**In `database.h` (private section, lines 197-199):**
- `Database::begin_transaction()` -- delegates to `impl_->begin_transaction()`
- `Database::commit()` -- delegates to `impl_->commit()`
- `Database::rollback()` -- delegates to `impl_->rollback()`

**The methods already exist as private.** The milestone requires:
1. Moving `begin_transaction()`, `commit()`, `rollback()` from private to public in `database.h`.
2. Modifying `TransactionGuard` constructor to check `sqlite3_get_autocommit(impl_.db)` -- if zero (transaction active), set a `nested_` flag and skip `BEGIN`.
3. Modifying `TransactionGuard::commit()` and destructor to skip `COMMIT`/`ROLLBACK` when `nested_`.

### 4. C++ Pattern for Exposing Through Pimpl

No architectural change needed. The existing delegation pattern is correct:

```cpp
// database.h -- move from private to public:
void begin_transaction();
void commit();
void rollback();

// database.cpp -- already implemented:
void Database::begin_transaction() { impl_->begin_transaction(); }
void Database::commit() { impl_->commit(); }
void Database::rollback() { impl_->rollback(); }
```

The Pimpl boundary is already clean. `Impl::begin_transaction/commit/rollback` call `sqlite3_exec()` directly. The public methods just delegate. No new headers, no new types needed.

### 5. Benchmarking Approach

**Use `std::chrono::steady_clock` in a regular GoogleTest test case.** Rationale:

1. The benchmark is a one-off proof that explicit transactions reduce fsync overhead. It does not need statistical rigor (warm-up iterations, standard deviation, outlier removal) that Google Benchmark provides.
2. The project already uses GoogleTest. Adding Google Benchmark would introduce a new dependency, a new build target, and a new CMake option for a single test.
3. The pattern is simple: time N `create_element` + `update_time_series_group` calls with and without an explicit wrapping transaction, then `EXPECT_LT(with_txn_ms, without_txn_ms)`.

**Recommended pattern:**
```cpp
TEST(Benchmark, ExplicitTransactionReducesFsyncs) {
    auto db = quiver::Database::from_schema(
        "benchmark_test.db", VALID_SCHEMA("collections.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    constexpr int N = 100;

    // Without explicit transaction (N separate transactions = N fsyncs)
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        // create_element + update_time_series_group (2 internal txns each)
    }
    auto without_txn = std::chrono::steady_clock::now() - start;

    // With explicit transaction (1 transaction = 1 fsync)
    start = std::chrono::steady_clock::now();
    db.begin_transaction();
    for (int i = 0; i < N; ++i) {
        // create_element + update_time_series_group (internal txns are no-ops)
    }
    db.commit();
    auto with_txn = std::chrono::steady_clock::now() - start;

    // Explicit transaction should be significantly faster
    auto ratio = std::chrono::duration<double>(without_txn).count()
               / std::chrono::duration<double>(with_txn).count();
    EXPECT_GT(ratio, 2.0);  // Expect at least 2x speedup
}
```

**Key detail:** The benchmark MUST use a file-based database (not `:memory:`) because fsync overhead only exists for file I/O. In-memory databases would show negligible difference.

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| `sqlite3_get_autocommit()` for nesting detection | `sqlite3_txn_state()` | If you need to distinguish read vs write transactions. Not needed here -- binary active/inactive is sufficient. |
| No-op `TransactionGuard` when nested | SAVEPOINT-based nesting | If internal operations need independent rollback within a user transaction. Not the case here -- user transaction provides atomicity. |
| `std::chrono::steady_clock` for benchmarks | Google Benchmark v1.9.5 | If you need statistical analysis, parameterized benchmarks, or regression tracking across CI runs. Overkill for a single proof-of-concept perf test. |
| GoogleTest `TEST()` for benchmark | Separate benchmark executable | If benchmarks become a recurring concern. For this milestone, a single test case suffices. |
| `BEGIN DEFERRED` (default) | `BEGIN IMMEDIATE` / `BEGIN EXCLUSIVE` | `IMMEDIATE` if you want to acquire the write lock eagerly (avoids `SQLITE_BUSY` on first write). Could be a future enhancement but `DEFERRED` matches existing behavior. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| SAVEPOINT for internal nesting | Adds naming complexity, rollback-to semantics, and partial-state confusion for zero benefit when the outer transaction already provides atomicity | No-op check via `sqlite3_get_autocommit()` |
| Google Benchmark (for this milestone) | Adds a new FetchContent dependency, a new CMake option, a new build target, and increased build time for a single benchmark | `std::chrono::steady_clock` in a regular GoogleTest test case |
| `std::chrono::system_clock` | Not monotonic; affected by NTP adjustments and DST changes; unreliable for measuring durations | `std::chrono::steady_clock` |
| `std::chrono::high_resolution_clock` | May alias `system_clock` on some platforms (non-monotonic). `steady_clock` is guaranteed monotonic. | `std::chrono::steady_clock` |
| Nested `BEGIN` statements | SQLite errors on `BEGIN` inside `BEGIN`. This is not a software design choice -- it is a hard SQLite constraint. | Either SAVEPOINT (not recommended) or no-op guard (recommended) |
| Manual `is_in_transaction_` flag on `Database` | Fragile. Gets out of sync if SQLite auto-rolls back due to `SQLITE_FULL`/`SQLITE_IOERR`/etc. | `sqlite3_get_autocommit()` queries the actual SQLite state, always authoritative |

## Stack Patterns by Variant

**If the benchmark needs statistical rigor (future CI regression tracking):**
- Add Google Benchmark v1.9.5 as an optional dependency behind `QUIVER_BUILD_BENCHMARKS`
- Create a separate `benchmarks/` directory with its own `CMakeLists.txt`
- Link `benchmark::benchmark benchmark::benchmark_main`
- This is a future concern, not a v0.3 concern

**If binding-level RAII wrappers are desired (Julia/Dart/Lua):**
- The C API exposes flat `quiver_database_begin_transaction()` / `quiver_database_commit()` / `quiver_database_rollback()` functions
- Julia can wrap in a `transaction(db) do ... end` block
- Dart can wrap in a `db.transaction(() { ... })` callback
- Lua can wrap in a `db:transaction(function() ... end)` pattern
- These are convenience methods in the bindings, not C++ or C API concerns

**If `BEGIN IMMEDIATE` is needed later:**
- Add an optional parameter to `begin_transaction()`: `void begin_transaction(bool immediate = false)`
- Or add a separate method: `void begin_immediate_transaction()`
- C API: `quiver_database_begin_transaction()` (deferred) and `quiver_database_begin_immediate_transaction()`
- Not needed for v0.3 since the motivating use case is batching writes that would occur sequentially anyway

## Version Compatibility

| Component | Version | Compatible With | Notes |
|-----------|---------|-----------------|-------|
| SQLite3 3.50.2 | `sqlite3_get_autocommit()` | All SQLite 3.x | Function available since SQLite 3.0 |
| SQLite3 3.50.2 | `sqlite3_txn_state()` | SQLite >= 3.34.0 | Available but not needed for this milestone |
| GoogleTest 1.17.0 | `std::chrono` benchmark tests | C++17+ | GoogleTest 1.17.x requires C++17; project uses C++20 |
| `std::chrono::steady_clock` | C++11+ | All compilers supporting C++11 or later | Project uses C++20, fully compatible |
| Google Benchmark 1.9.5 | C++17 to build | GoogleTest 1.17.0 | NOT adding this milestone, but compatible if needed later |

## SQLite Transaction API Reference

For quick reference during implementation:

| SQL Statement | C API | Behavior |
|---------------|-------|----------|
| `BEGIN [DEFERRED]` | `sqlite3_exec(db, "BEGIN TRANSACTION;", ...)` | Starts deferred transaction. Lock acquired on first access. |
| `BEGIN IMMEDIATE` | `sqlite3_exec(db, "BEGIN IMMEDIATE;", ...)` | Acquires write lock immediately. May return `SQLITE_BUSY`. |
| `COMMIT` / `END TRANSACTION` | `sqlite3_exec(db, "COMMIT;", ...)` | Commits and fsyncs. May return `SQLITE_BUSY` (retry-safe). |
| `ROLLBACK` | `sqlite3_exec(db, "ROLLBACK;", ...)` | Reverts all changes. Safe to call even if auto-rolled-back. |
| -- | `sqlite3_get_autocommit(db)` | Non-zero = no active txn; zero = inside transaction. |
| `SAVEPOINT name` | `sqlite3_exec(db, "SAVEPOINT sp;", ...)` | Nestable named transaction point. NOT RECOMMENDED for this use case. |

## Sources

- [SQLite `sqlite3_get_autocommit()` official docs](https://www.sqlite.org/c3ref/get_autocommit.html) -- HIGH confidence, official reference
- [SQLite `sqlite3_txn_state()` official docs](https://www.sqlite.org/c3ref/txn_state.html) -- HIGH confidence, official reference
- [SQLite SAVEPOINT documentation](https://www.sqlite.org/lang_savepoint.html) -- HIGH confidence, official reference
- [SQLite BEGIN TRANSACTION documentation](https://www.sqlite.org/lang_transaction.html) -- HIGH confidence, official reference
- [SQLite Release 3.34.0 changelog](https://www.sqlite.org/releaselog/3_34_0.html) -- HIGH confidence, confirms `sqlite3_txn_state()` introduction
- [Google Benchmark GitHub](https://github.com/google/benchmark) -- HIGH confidence, v1.9.5 verified
- [GoogleTest GitHub](https://github.com/google/googletest) -- HIGH confidence, v1.17.0 already in project
- [std::chrono::steady_clock cppreference](https://en.cppreference.com/w/cpp/chrono/steady_clock.html) -- HIGH confidence, standard library reference
- [Sandor Dargo's steady_clock analysis](https://www.sandordargo.com/blog/2025/12/03/clocks-part-3-steady_clock) -- MEDIUM confidence, confirms monotonic guarantees
- [Google Benchmark + CMake FetchContent integration patterns](https://mrmikejones.com/notes/20251019_GoogleBenchmarkAndGTestCMake.html) -- MEDIUM confidence, community example confirmed by multiple sources

---
*Stack research for: Quiver v0.3 -- Explicit Transaction Control*
*Researched: 2026-02-20*
