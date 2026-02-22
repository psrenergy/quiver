# Phase 4: Performance Benchmark - Research

**Researched:** 2026-02-21
**Domain:** C++ benchmarking, SQLite transaction throughput
**Confidence:** HIGH

## Summary

This phase creates a standalone C++ benchmark executable that demonstrates the performance benefit of wrapping multiple write operations in a single explicit transaction. The workload is well-defined: 5000 elements, each with scalars + vectors + time series data, run in two variants (individual implicit transactions vs. single explicit transaction), with 3-5 iterations and statistics printed to stdout.

The technical domain is straightforward. The project already uses GoogleTest and CMake; the benchmark will be a separate executable linked against `quiver` (the C++ library). Timing uses `std::chrono::high_resolution_clock` from the C++20 standard library. No external benchmarking framework is needed. The `collections.sql` schema provides all required attribute types (scalars, vectors, time series). File-based database paths use `std::filesystem::temp_directory_path()`, matching the established project pattern.

**Primary recommendation:** Use a plain `main()` with `std::chrono` timing. The benchmark is a simple two-variant comparison, not a microbenchmark suite. No framework needed.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- 5000 elements per run
- Each element has label + 2-3 scalar attributes + vector data + time series data
- 10 time series rows per element with realistic-looking timestamps (plausible date_time values, e.g., hourly intervals)
- Two variants only: all-individual (no explicit transaction) vs all-batched (single wrapping transaction)
- Reuse an existing test schema from tests/schemas/valid/
- Temporary file-based database (created and deleted per benchmark run)
- Clean ASCII table with columns: variant name, total time (ms), per-element time, ops/sec, speedup ratio
- Header banner before results showing benchmark parameters (element count, time series rows per element, schema used, iteration count)
- All four metrics shown: total time (ms), per-element time, ops/sec, speedup ratio
- 3-5 iterations per variant
- Report statistics across iterations (not just single run)
- Separate executable (e.g., quiver_benchmark.exe) -- not part of the normal test suite
- Source lives in tests/benchmark/
- build-all.bat builds the benchmark but test-all.bat does NOT run it -- manual invocation only

### Claude's Discretion
- Baseline variant structure (how individual operations are grouped without explicit transaction)
- Primary statistic (median vs mean) -- pick most appropriate for benchmark reporting
- Whether to include a warm-up run discarded from results
- Whether each iteration uses a fresh DB or reuses with cleanup
- Whether to use Catch2 BENCHMARK or plain main() -- pick simplest approach
- Progress output during execution (e.g., "Running: individual..." before results)
- Exact number of iterations within the 3-5 range

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PERF-01 | C++ benchmark demonstrates measurable speedup from batching `create_element` + `update_time_series_group` in a single transaction vs separate transactions | All findings below: schema choice, timing approach, two-variant structure, file-based DB for realistic I/O, statistics across iterations |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| `std::chrono` | C++20 standard | High-resolution timing | Standard library, no dependency needed |
| `std::filesystem` | C++20 standard | Temp file paths, cleanup | Already used in project test fixtures |
| GoogleTest (link only) | v1.17.0 | Not used at runtime -- benchmark is plain main() | Already in project Dependencies.cmake |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<algorithm>` | C++20 | Sorting for median calculation | Statistics computation |
| `<numeric>` | C++20 | `std::accumulate` for mean | Statistics computation |
| `<iomanip>` | C++20 | Formatted ASCII table output | Results presentation |
| `<cstdio>` / `<iostream>` | C++20 | stdout printing | Results and progress output |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Plain main() | Catch2 BENCHMARK | Catch2 not in project, adds dependency for a single benchmark. Plain main() is simpler. |
| Plain main() | Google Benchmark | Heavyweight framework for microbenchmarks. This is a macro benchmark (seconds, not nanoseconds). Overkill. |
| std::chrono | OS-specific timers | Non-portable, unnecessary. std::chrono is sufficient for millisecond-scale measurements. |

**Installation:** No new dependencies required. Everything is in the C++20 standard library and existing project targets.

## Architecture Patterns

### Recommended Project Structure
```
tests/
  benchmark/
    benchmark.cpp          # Single file: main(), workload, timing, output
```

### Pattern 1: Two-Variant Benchmark with Fresh Database Per Iteration
**What:** Each iteration creates a fresh file-based database, runs the full workload, records elapsed time, then deletes the file. Individual variant: each `create_element` + `update_time_series_group` runs in its own implicit transaction (no wrapping `begin_transaction`). Batched variant: all operations wrapped in a single `begin_transaction()` ... `commit()`.
**When to use:** Always -- this is the only pattern for this phase.
**Example:**
```cpp
#include <chrono>
#include <filesystem>
#include <quiver/database.h>
#include <quiver/element.h>

namespace fs = std::filesystem;

// Generate a unique temp DB path
std::string temp_db_path(const std::string& suffix) {
    return (fs::temp_directory_path() / ("quiver_bench_" + suffix + ".db")).string();
}

// Build one element with realistic data
quiver::Element make_element(int index) {
    quiver::Element e;
    e.set("label", std::string("Item ") + std::to_string(index));
    e.set("some_integer", static_cast<int64_t>(index * 10));
    e.set("some_float", static_cast<double>(index) * 1.1);
    return e;
}

// Build 10 time series rows with hourly timestamps
std::vector<std::map<std::string, quiver::Value>> make_time_series_rows(int element_index) {
    std::vector<std::map<std::string, quiver::Value>> rows;
    for (int r = 0; r < 10; ++r) {
        char ts[32];
        std::snprintf(ts, sizeof(ts), "2024-01-01T%02d:00:00", r);
        rows.push_back({
            {"date_time", std::string(ts)},
            {"value", static_cast<double>(element_index * 10 + r) * 0.5}
        });
    }
    return rows;
}

double run_individual(const std::string& db_path, const std::string& schema, int count) {
    auto db = quiver::Database::from_schema(db_path, schema,
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Configuration element required
    quiver::Element config;
    config.set("label", std::string("Benchmark Config"));
    db.create_element("Configuration", config);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < count; ++i) {
        auto elem = make_element(i);
        int64_t id = db.create_element("Collection", elem);
        db.update_time_series_group("Collection", "data", id, make_time_series_rows(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

double run_batched(const std::string& db_path, const std::string& schema, int count) {
    auto db = quiver::Database::from_schema(db_path, schema,
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Benchmark Config"));
    db.create_element("Configuration", config);

    auto start = std::chrono::high_resolution_clock::now();

    db.begin_transaction();
    for (int i = 0; i < count; ++i) {
        auto elem = make_element(i);
        int64_t id = db.create_element("Collection", elem);
        db.update_time_series_group("Collection", "data", id, make_time_series_rows(i));
    }
    db.commit();

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}
```

### Pattern 2: Statistics Computation (Median)
**What:** Collect times from N iterations, compute median (robust to outliers) and mean.
**When to use:** Reporting summary statistics from multiple runs.
**Example:**
```cpp
#include <algorithm>
#include <numeric>
#include <vector>

struct Stats {
    double median_ms;
    double mean_ms;
    double per_element_ms;
    double ops_per_sec;
};

Stats compute_stats(std::vector<double>& times_ms, int element_count) {
    std::sort(times_ms.begin(), times_ms.end());
    double median = times_ms[times_ms.size() / 2];
    double mean = std::accumulate(times_ms.begin(), times_ms.end(), 0.0) / times_ms.size();
    // Use median for per-element and ops/sec
    double per_elem = median / element_count;
    double ops_sec = element_count / (median / 1000.0);
    return {median, mean, per_elem, ops_sec};
}
```

### Pattern 3: ASCII Table Output
**What:** Printf-style formatted table with aligned columns, matching user specification.
**When to use:** Final results output.
**Example:**
```cpp
void print_results(const Stats& individual, const Stats& batched,
                   int element_count, int ts_rows, int iterations,
                   const std::string& schema) {
    printf("\n");
    printf("========================================\n");
    printf("  Quiver Transaction Benchmark\n");
    printf("========================================\n");
    printf("  Elements:     %d\n", element_count);
    printf("  TS rows/elem: %d\n", ts_rows);
    printf("  Schema:       %s\n", schema.c_str());
    printf("  Iterations:   %d\n", iterations);
    printf("========================================\n\n");

    printf("%-20s %12s %12s %12s %10s\n",
           "Variant", "Total (ms)", "Per-elem", "Ops/sec", "Speedup");
    printf("%-20s %12s %12s %12s %10s\n",
           "--------------------", "------------", "------------", "------------", "----------");
    printf("%-20s %12.1f %12.3f %12.1f %10s\n",
           "Individual", individual.median_ms, individual.per_element_ms,
           individual.ops_per_sec, "1.00x");
    double speedup = individual.median_ms / batched.median_ms;
    printf("%-20s %12.1f %12.3f %12.1f %9.2fx\n",
           "Batched", batched.median_ms, batched.per_element_ms,
           batched.ops_per_sec, speedup);
    printf("\n");
}
```

### Pattern 4: Fresh Database Per Iteration
**What:** Each iteration creates a fresh temp file DB, runs the workload, then deletes the file.
**When to use:** For all iterations -- ensures no state carryover, consistent file-system conditions.
**Why:** Reusing with cleanup risks leftover WAL/SHM files, partially vacuumed pages, and OS page cache warming giving inconsistent results. Fresh DB is simpler and more reliable.

### Pattern 5: CMake Integration
**What:** Add benchmark as a separate executable target in `tests/CMakeLists.txt`, not added to `gtest_discover_tests`.
**When to use:** Build setup.
**Example:**
```cmake
# Benchmark executable (not part of test suite)
add_executable(quiver_benchmark
    benchmark/benchmark.cpp
)

target_link_libraries(quiver_benchmark
    PRIVATE
        quiver
        quiver_compiler_options
)

# Copy DLLs for benchmark
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET quiver_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:quiver>
            $<TARGET_FILE_DIR:quiver_benchmark>
    )
endif()
```

Note: No `gtest_discover_tests` call. No GTest link. This is a plain `main()` executable.

### Anti-Patterns to Avoid
- **Using `:memory:` for benchmarking:** The whole point is measuring file I/O transaction overhead. `:memory:` eliminates disk syncs, making individual vs. batched nearly identical. Must use file-based databases.
- **Measuring too little:** Timing only `create_element` without `update_time_series_group` would show a smaller differential. The user explicitly wants both operations included per element.
- **Linking GoogleTest for benchmark:** The benchmark is not a test. It is a standalone executable with `main()`. Linking GTest adds unnecessary overhead and framework noise.
- **Running in CI/automated tests:** The benchmark is noisy and slow. `test-all.bat` must NOT execute it. `build-all.bat` builds it so the binary is always available but never run automatically.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Timing | OS-specific timing APIs | `std::chrono::high_resolution_clock` | Standard, portable, sufficient precision for ms-scale measurements |
| Temp file management | Manual path strings | `std::filesystem::temp_directory_path()` | Already used in project, handles cross-platform temp paths |
| Statistics | Complex stats library | Inline median/mean with `std::sort`/`std::accumulate` | Only 3-5 values; trivial computation, no library needed |

**Key insight:** This benchmark has no complex requirements. The standard library provides everything needed. The project's existing APIs (`from_schema`, `create_element`, `update_time_series_group`, `begin_transaction`/`commit`) are the only non-standard components.

## Common Pitfalls

### Pitfall 1: In-Memory Database Hides Transaction Benefit
**What goes wrong:** Using `:memory:` shows negligible difference between variants because there are no disk syncs to amortize.
**Why it happens:** In-memory SQLite never calls `fsync()`. The entire transaction overhead for individual operations is just the SQL bookkeeping, not I/O.
**How to avoid:** Use file-based database with `std::filesystem::temp_directory_path()`. Delete after each iteration.
**Warning signs:** Speedup ratio < 2x on file-based DB suggests something is wrong with the measurement.

### Pitfall 2: WAL Mode Reduces Observable Difference
**What goes wrong:** SQLite in WAL mode has lower per-transaction sync overhead, which reduces the speedup ratio.
**Why it happens:** WAL mode uses append-only writes to a WAL file, amortizing some sync cost. DELETE journal mode (SQLite default) requires full journal sync per transaction.
**How to avoid:** Use the default journal mode (DELETE). SQLite defaults to this unless explicitly changed with `PRAGMA journal_mode=WAL`. Quiver does not set WAL mode, so default behavior is correct.
**Warning signs:** If speedup is unexpectedly low, check `PRAGMA journal_mode;` to verify DELETE mode.

### Pitfall 3: Clock Resolution for Fast Operations
**What goes wrong:** Measuring individual element creation (sub-millisecond) with wall-clock timing gives noisy results.
**Why it happens:** OS timer resolution may be ~1ms on Windows.
**How to avoid:** Time the entire loop of 5000 elements, not individual elements. Per-element time is computed by dividing total time. This is already the planned approach.
**Warning signs:** Total times < 10ms would indicate insufficient workload.

### Pitfall 4: Forgetting Configuration Element
**What goes wrong:** Schema requires a `Configuration` element. Without it, the first `create_element("Collection", ...)` may fail or behave unexpectedly.
**Why it happens:** `collections.sql` has a `Configuration` table that must exist.
**How to avoid:** Always create a Configuration element before the timed loop. Create it outside the timing window so it does not affect results.
**Warning signs:** Runtime errors about missing Configuration.

### Pitfall 5: Element Label Uniqueness
**What goes wrong:** `Collection` table has `label TEXT UNIQUE NOT NULL`. Reusing labels across iterations on the same DB causes constraint violations.
**Why it happens:** Each element needs a unique label.
**How to avoid:** Use fresh DB per iteration (eliminates the problem). If reusing, include iteration number in labels.
**Warning signs:** UNIQUE constraint errors at runtime.

### Pitfall 6: Schema Path Resolution
**What goes wrong:** Benchmark executable lives in `build/bin/`, not in `tests/`. The `VALID_SCHEMA()` macro uses `__FILE__` to resolve paths relative to the source file.
**Why it happens:** The macro computes paths relative to the source file's directory. Since benchmark source is in `tests/benchmark/`, the relative path needs to go up one level: `"../schemas/valid/collections.sql"`.
**How to avoid:** Use the same `VALID_SCHEMA()` macro pattern but with the correct relative path, or use a `SCHEMA_PATH()` macro call from `test_utils.h` with adjusted path: `SCHEMA_PATH("../schemas/valid/collections.sql")` -- actually simpler to just define a local path resolution or use the same `test_utils.h` header with `SCHEMA_PATH("schemas/valid/collections.sql")` and adjust include paths.

Better approach: include `test_utils.h` and use `quiver::test::path_from(__FILE__, "../schemas/valid/collections.sql")`. This is exact and works regardless of build directory.
**Warning signs:** "Schema file not found" errors at runtime.

## Code Examples

### Complete Benchmark Flow (Verified Against Project Patterns)

The `collections.sql` schema (at `tests/schemas/valid/collections.sql`) provides:
- `Configuration` table (required)
- `Collection` table with `label TEXT UNIQUE NOT NULL`, `some_integer INTEGER`, `some_float REAL`
- `Collection_vector_values` with `value_int INTEGER`, `value_float REAL`
- `Collection_time_series_data` with `date_time TEXT`, `value REAL`

This schema supports all required workload components:
- **Scalars:** `label`, `some_integer`, `some_float` (3 scalar attributes)
- **Vectors:** `value_int`, `value_float` in `Collection_vector_values`
- **Time series:** `date_time` + `value` in `Collection_time_series_data`

### Element Creation with All Attribute Types
```cpp
// From existing test patterns in test_database_create.cpp and test_database_transaction.cpp
quiver::Element elem;
elem.set("label", std::string("Item 0"))
    .set("some_integer", int64_t{100})
    .set("some_float", 3.14)
    .set("value_int", std::vector<int64_t>{1, 2, 3, 4, 5})
    .set("value_float", std::vector<double>{1.1, 2.2, 3.3, 4.4, 5.5});

int64_t id = db.create_element("Collection", elem);
```

### Time Series Update (from test_database_transaction.cpp WriteMethodsInsideTransaction)
```cpp
std::vector<std::map<std::string, quiver::Value>> ts_rows = {
    {{"date_time", std::string("2024-01-01T00:00:00")}, {"value", 1.5}},
    {{"date_time", std::string("2024-01-01T01:00:00")}, {"value", 2.5}},
    // ... 10 rows total with hourly intervals
};
db.update_time_series_group("Collection", "data", id, ts_rows);
```

### Transaction Wrapping (from test_database_transaction.cpp)
```cpp
db.begin_transaction();
for (int i = 0; i < count; ++i) {
    int64_t id = db.create_element("Collection", make_element(i));
    db.update_time_series_group("Collection", "data", id, make_time_series_rows(i));
}
db.commit();
```

### Database Options with Logging Off (from all existing tests)
```cpp
auto db = quiver::Database::from_schema(
    db_path, schema_path,
    {.read_only = 0, .console_level = QUIVER_LOG_OFF});
```

### Temp File Pattern (from test_database_lifecycle.cpp)
```cpp
namespace fs = std::filesystem;
std::string db_path = (fs::temp_directory_path() / "quiver_bench_individual_0.db").string();
// ... use db_path ...
fs::remove(db_path);
```

## Discretion Recommendations

Based on research findings, here are recommendations for the areas left to Claude's discretion:

### Use Median as Primary Statistic
**Recommendation:** Median. Benchmarks often have occasional outliers from OS scheduling, disk cache flushes, or background processes. Median is robust to these. Show both median and mean in the table for transparency.
**Confidence:** HIGH -- standard practice in benchmarking literature.

### Use 5 Iterations
**Recommendation:** 5 iterations (top of the 3-5 range). With file-based databases and 5000 elements, each iteration takes seconds, so 5 iterations is practical. More data points make the median more reliable.
**Confidence:** HIGH -- minimal time cost, better statistical reliability.

### Include a Warm-Up Run
**Recommendation:** Yes, include 1 warm-up run per variant, discarded from results. The first run may be slower due to DLL loading, JIT compilation of SQLite prepared statements, and OS page cache cold start. Discarding it gives cleaner numbers.
**Confidence:** MEDIUM -- the effect may be small with 5000 elements, but it costs almost nothing to include.

### Fresh Database Per Iteration
**Recommendation:** Fresh database per iteration. Simpler code (no cleanup logic), no state carryover risks, consistent starting conditions. File creation overhead is negligible compared to 5000-element workload.
**Confidence:** HIGH -- already justified in anti-patterns section.

### Use Plain main() (Not Catch2 BENCHMARK)
**Recommendation:** Plain `main()`. Catch2 is not in the project's dependencies. Adding a framework for a single benchmark file is unnecessary complexity. `std::chrono` provides everything needed.
**Confidence:** HIGH -- Catch2 not present in Dependencies.cmake, would require FetchContent addition.

### Progress Output During Execution
**Recommendation:** Yes, print progress lines before each variant/iteration. Example: `"Running: individual [1/5]..."`. With 5000 elements per iteration, the benchmark takes several seconds per run; progress output prevents the user from thinking it hung.
**Confidence:** HIGH -- minimal code, significant UX improvement.

### Baseline Variant Structure (Individual)
**Recommendation:** Each `create_element` + `update_time_series_group` pair runs without any explicit wrapping transaction. Since each method internally uses `TransactionGuard` (which starts its own transaction when none is active), each call gets its own implicit transaction. This means each element involves ~2 separate transactions (one for create, one for time series update), each with its own `fsync`. This maximizes the observable difference vs. the batched variant.
**Confidence:** HIGH -- directly follows from the TransactionGuard design documented in CLAUDE.md.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Google Benchmark for all benchmarks | Plain timing for macro benchmarks | N/A (convention) | Google Benchmark is for nanosecond microbenchmarks; macro benchmarks (seconds) use simple chrono timing |
| `clock()`/`clock_gettime()` | `std::chrono::high_resolution_clock` | C++11 | Portable, type-safe, no platform ifdefs |
| `tmpnam()`/`_tempnam()` | `std::filesystem::temp_directory_path()` | C++17 | Safe, portable, no deprecation warnings |

**Deprecated/outdated:**
- `clock()`: Measures CPU time, not wall time. Wrong metric for I/O-bound benchmarks.
- `tmpnam()`: Deprecated, security warnings on all platforms.

## Open Questions

None. The technical domain is well-understood and all decisions are either locked by the user or have clear recommendations.

## Sources

### Primary (HIGH confidence)
- Project source code: `tests/test_database_transaction.cpp` -- verified transaction API patterns
- Project source code: `tests/test_database_create.cpp` -- verified element creation with vectors and time series
- Project source code: `tests/schemas/valid/collections.sql` -- verified schema has all required attribute types
- Project source code: `tests/CMakeLists.txt` -- verified test executable build pattern
- Project source code: `tests/test_utils.h` -- verified schema path resolution pattern
- Project source code: `tests/test_database_lifecycle.cpp` -- verified temp file fixture pattern
- Project source code: `cmake/Dependencies.cmake` -- verified no Catch2 in project
- Project source code: `include/quiver/database.h` -- verified full API signatures
- Project source code: `include/quiver/element.h` -- verified Element builder API
- Project source code: `scripts/build-all.bat` and `scripts/test-all.bat` -- verified build/test integration points

### Secondary (MEDIUM confidence)
- SQLite documentation on transaction performance (well-known: individual transactions require per-transaction fsync, batching amortizes to single fsync)

### Tertiary (LOW confidence)
None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all C++20 standard library, no external deps needed
- Architecture: HIGH -- single-file benchmark, verified API patterns from existing tests
- Pitfalls: HIGH -- all derived from direct codebase inspection and SQLite fundamentals

**Research date:** 2026-02-21
**Valid until:** 2026-06-21 (stable domain, no moving parts)
