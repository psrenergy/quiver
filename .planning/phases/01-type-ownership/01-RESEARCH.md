# Phase 1: Type Ownership - Research

**Researched:** 2026-02-27
**Domain:** C++ type definitions, FFI boundary patterns
**Confidence:** HIGH

## Summary

Phase 1 eliminates a layer inversion where the C++ core header (`include/quiver/options.h`) depends on a C API header (`include/quiver/c/options.h`). Currently `quiver::DatabaseOptions` is a typedef alias for the C struct `quiver_database_options_t`, meaning the C++ core cannot compile without C API headers. The fix defines native C++ types (`enum class LogLevel`, `struct DatabaseOptions`) in the C++ header and adds a trivial conversion function in the C API boundary layer (`src/c/`).

The change is mechanically simple but has a wide blast radius in test files: 196 occurrences of `{.read_only = 0, .console_level = QUIVER_LOG_OFF}` designated initializers across 12 C++ test files reference the C enum values directly. These must be updated to use the new C++ `LogLevel::Off` enum class value. The C API tests and all bindings remain completely unchanged.

**Primary recommendation:** Define `enum class LogLevel` and `struct DatabaseOptions` in `include/quiver/options.h`, add a `convert_options` function in `src/c/database_options.h`, update `src/database.cpp` and `src/cli/main.cpp` to use the C++ types, and mechanically update C++ test initializers.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- C++ defines its own `struct DatabaseOptions` with `bool read_only` and `enum class LogLevel console_level`
- Member initializers provide defaults: `read_only = false`, `console_level = LogLevel::Info`
- No factory function -- `DatabaseOptions{}` gives defaults; remove `default_database_options()`
- `enum class LogLevel` values: `Debug, Info, Warn, Error, Off`
- `quiver_database_options_t` stays exactly as-is (no struct changes)
- `quiver_log_level_t` enum stays exactly as-is
- `quiver_database_options_default()` stays exactly as-is
- Bindings continue using the C struct -- zero changes to Julia, Dart, Python
- CSVOptions already a proper C++ type -- no changes needed
- Existing boundary conversion in `src/c/database_options.h` stays as-is (for CSV)
- C API boundary layer (`src/c/`) converts `quiver_database_options_t` to `quiver::DatabaseOptions` before calling into C++ core

### Claude's Discretion
- Where exactly the DatabaseOptions conversion function lives (likely `src/c/database_options.h` alongside existing CSVOptions conversion)
- Whether conversion is a free function or inline helper
- LogLevel enum value assignments (implicit sequential or explicit matching C values)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TYPES-01 | C++ defines `DatabaseOptions` with `enum class LogLevel` natively; C API converts at boundary | All research findings directly support this: new C++ types in `options.h`, conversion in `src/c/database_options.h`, updates to `database.cpp` and `main.cpp` |
| TYPES-02 | C++ defines `CSVOptions` natively; C API converts at boundary | Already satisfied -- `CSVOptions` is already a native C++ type in `include/quiver/options.h` and conversion already exists in `src/c/database_options.h`. No work needed. |
</phase_requirements>

## Standard Stack

No external libraries are added or changed by this phase. The relevant existing stack:

### Core
| Library | Version | Purpose | Relevance |
|---------|---------|---------|-----------|
| C++20 | Standard | `enum class`, member initializers, designated initializers | All new type definitions use C++20 features |
| spdlog | Existing | Logging levels | `to_spdlog_level()` in `database.cpp` switches on log level enum -- must update to new type |
| sqlite3 | Existing | `SQLITE_OPEN_READONLY` flag | Consumes `read_only` field -- works with both `int` and `bool` |

### Alternatives Considered

None. This is a pure refactoring of existing types with no library choices.

## Architecture Patterns

### Pattern 1: C++ Type Ownership with FFI Boundary Conversion

**What:** C++ core defines its own types with idiomatic C++ features (`bool`, `enum class`). The C API layer defines parallel C-compatible types (`int`, plain C enum). A conversion function in the C API implementation bridges them.

**When to use:** Any FFI boundary where the core language has richer type semantics than the FFI language.

**Current state (BROKEN):**
```
include/quiver/options.h  --includes-->  include/quiver/c/options.h   [LAYER INVERSION]
```
C++ public header depends on C API header. `DatabaseOptions` is a typedef, not a real C++ type.

**Target state (CORRECT):**
```
include/quiver/options.h              [standalone C++ types, no C API dependency]
include/quiver/c/options.h            [standalone C types, no C++ dependency]
src/c/database_options.h              [conversion: C -> C++, includes BOTH headers]
```
Only the `src/c/` boundary layer includes both headers. No circular dependencies possible because `src/c/` is implementation-only (never included by public headers).

**Conversion function example:**
```cpp
// src/c/database_options.h
inline quiver::DatabaseOptions convert_database_options(const quiver_database_options_t& c_opts) {
    return {
        .read_only = c_opts.read_only != 0,
        .console_level = static_cast<quiver::LogLevel>(c_opts.console_level)
    };
}
```

### Pattern 2: Existing CSVOptions Boundary Pattern (Reference Implementation)

**What:** `CSVOptions` is already correctly owned by C++. The file `src/c/database_options.h` already contains `convert_options(const quiver_csv_options_t*)` that converts C flat arrays to C++ `std::unordered_map`. This is the exact pattern to follow for `DatabaseOptions`.

**Source:** `src/c/database_options.h` lines 9-25

### Anti-Patterns to Avoid

- **Shared typedef across layers:** The current `using DatabaseOptions = quiver_database_options_t` is the anti-pattern being fixed. It couples the C++ core to C API headers.
- **Conversion in public headers:** The conversion function must live in `src/c/` (implementation), not in `include/quiver/` (public). Putting it in public headers would re-introduce the dependency.
- **Matching enum integer values implicitly:** While `static_cast<LogLevel>(c_opts.console_level)` works if both enums have identical integer assignments, this is fragile. Recommend explicit matching values: `Debug = 0, Info = 1, Warn = 2, Error = 3, Off = 4` in the C++ enum to match the C enum's values.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Enum conversion | Per-value switch statement | `static_cast` with matching integer values | Both enums are defined by this project; keeping integer values aligned is simpler and less error-prone than a switch |

**Key insight:** Since we control both the C and C++ enum definitions, we can guarantee they share the same integer mapping. A `static_cast` is then safe and correct. If the values ever diverge, the mismatch would be caught immediately by the test suite.

## Common Pitfalls

### Pitfall 1: C++ Test Files Reference C Enum Values Directly

**What goes wrong:** 196 occurrences across 12 C++ test files use `QUIVER_LOG_OFF` (the C enum value) in designated initializers: `{.read_only = 0, .console_level = QUIVER_LOG_OFF}`. After the change, `DatabaseOptions` is a C++ struct with `enum class LogLevel console_level` -- the C enum value is the wrong type.

**Why it happens:** The current typedef means C++ code and C API code share the same type, so both use the same enum constants.

**How to avoid:** Mechanically replace all occurrences in C++ test files. The new pattern is: `{.read_only = false, .console_level = quiver::LogLevel::Off}`. This is a straightforward find-and-replace.

**Warning signs:** Compiler errors about enum type mismatch will catch every instance.

**Affected files (C++ tests only -- C API tests are unchanged):**
- `test_database_create.cpp` (25)
- `test_database_read.cpp` (47)
- `test_database_update.cpp` (36)
- `test_database_errors.cpp` (29)
- `test_database_time_series.cpp` (19)
- `test_database_query.cpp` (17)
- `test_database_transaction.cpp` (8)
- `test_database_lifecycle.cpp` (6)
- `test_database_delete.cpp` (5)
- `test_database_csv_import.cpp` (2)
- `test_database_csv_export.cpp` (1)
- `test_schema_validator.cpp` (1)

Also affected:
- `tests/sandbox/sandbox.cpp` (1)
- `tests/benchmark/benchmark.cpp` (2)

### Pitfall 2: CLI Uses C API Types

**What goes wrong:** `src/cli/main.cpp` uses `quiver_log_level_t` return type in `parse_log_level()` and assigns `options.read_only = ... ? 1 : 0` (C-style int). After the change, `DatabaseOptions` fields are `bool` and `LogLevel`.

**Why it happens:** CLI currently uses `quiver::DatabaseOptions` which is the C typedef.

**How to avoid:** Update `parse_log_level()` to return `quiver::LogLevel` and use `quiver::LogLevel::Debug` etc. Change `options.read_only` assignment to plain `bool`.

**Warning signs:** Compiler errors on type mismatch.

### Pitfall 3: `quiver_database_options_default()` Cannot Return C++ Type Anymore

**What goes wrong:** The C API function `quiver_database_options_default()` currently calls `quiver::default_database_options()` which returns the C struct (since they're the same type). After removing `default_database_options()`, this function needs its own implementation.

**Why it happens:** The current code exploits the typedef -- both layers return the same struct type.

**How to avoid:** `quiver_database_options_default()` constructs and returns `quiver_database_options_t` directly: `{0, QUIVER_LOG_INFO}`. This is trivial.

**Warning signs:** Linker error if `default_database_options()` is removed but `quiver_database_options_default()` still references it.

### Pitfall 4: `src/database.cpp` Internal Functions Use C Enum Type

**What goes wrong:** `to_spdlog_level()` takes `quiver_log_level_t` and `create_database_logger()` takes `quiver_log_level_t console_level`. These are internal functions that should use the C++ type.

**Why it happens:** When `DatabaseOptions` was the C typedef, the C enum type naturally flowed through.

**How to avoid:** Change both functions to accept `quiver::LogLevel` instead. Update the switch cases from `QUIVER_LOG_DEBUG` to `LogLevel::Debug` etc.

**Warning signs:** Compiler error on switch case values.

### Pitfall 5: `quiver_database` struct Constructor Passes Options Through

**What goes wrong:** The `quiver_database` struct in `src/c/internal.h` has a constructor `quiver_database(const std::string& path, const quiver::DatabaseOptions& options)`. After the change, call sites in `src/c/database.cpp` pass `quiver_database_options_t` (the C type), which is no longer the same as `quiver::DatabaseOptions`.

**Why it happens:** The C API implementation creates `quiver_database` structs from C types.

**How to avoid:** Call sites in `src/c/database.cpp` must convert `quiver_database_options_t` to `quiver::DatabaseOptions` before constructing `quiver_database`. Use the new `convert_database_options()` function.

**Warning signs:** Compiler error on type mismatch in constructor call.

### Pitfall 6: `test_utils.h` References C Types

**What goes wrong:** `test_utils.h` defines `quiet_options()` returning `quiver_database_options_t`. This function is used by C API tests, which still need C types. But C++ tests also use this header.

**Why it happens:** Shared test utility header serves both C++ and C API tests.

**How to avoid:** Keep `quiet_options()` returning `quiver_database_options_t` for C API tests. It does not need to change since C API tests use C types. C++ tests should stop using `QUIVER_LOG_OFF` from the C header and use the C++ `LogLevel` enum directly in their designated initializers.

## Code Examples

### New `include/quiver/options.h` (complete)

```cpp
#ifndef QUIVER_OPTIONS_H
#define QUIVER_OPTIONS_H

#include "export.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Off = 4,
};

struct QUIVER_API DatabaseOptions {
    bool read_only = false;
    LogLevel console_level = LogLevel::Info;
};

struct QUIVER_API CSVOptions {
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int64_t>>>
        enum_labels;
    std::string date_time_format;
};

inline CSVOptions default_csv_options() {
    return {};
}

}  // namespace quiver

#endif  // QUIVER_OPTIONS_H
```

Key changes from current file:
1. Removed `#include "quiver/c/options.h"`
2. Replaced `using DatabaseOptions = quiver_database_options_t` with a proper struct
3. Added `enum class LogLevel` with explicit integer values matching C enum
4. Removed `default_database_options()` -- `DatabaseOptions{}` gives defaults via member initializers

### Updated `src/c/database_options.h` (add DatabaseOptions conversion)

```cpp
#ifndef QUIVER_C_DATABASE_OPTIONS_H
#define QUIVER_C_DATABASE_OPTIONS_H

#include "quiver/c/options.h"
#include "quiver/options.h"

#include <string>

inline quiver::DatabaseOptions convert_database_options(const quiver_database_options_t& c_opts) {
    return {
        .read_only = c_opts.read_only != 0,
        .console_level = static_cast<quiver::LogLevel>(c_opts.console_level),
    };
}

inline quiver::CSVOptions convert_options(const quiver_csv_options_t* options) {
    // ... existing code unchanged ...
}

inline quiver_csv_options_t csv_options_default() {
    // ... existing code unchanged ...
}

#endif
```

### Updated `src/database.cpp` (internal functions)

```cpp
// Before:
spdlog::level::level_enum to_spdlog_level(quiver_log_level_t level) {
    switch (level) {
    case QUIVER_LOG_DEBUG: return spdlog::level::debug;
    // ...
    }
}

// After:
spdlog::level::level_enum to_spdlog_level(quiver::LogLevel level) {
    switch (level) {
    case quiver::LogLevel::Debug: return spdlog::level::debug;
    case quiver::LogLevel::Info:  return spdlog::level::info;
    case quiver::LogLevel::Warn:  return spdlog::level::warn;
    case quiver::LogLevel::Error: return spdlog::level::err;
    case quiver::LogLevel::Off:   return spdlog::level::off;
    default: return spdlog::level::info;
    }
}

std::shared_ptr<spdlog::logger> create_database_logger(
    const std::string& db_path, quiver::LogLevel console_level) {
    // ... same body ...
}
```

### Updated `src/c/database.cpp` (C API functions)

```cpp
// Before:
QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    return quiver::default_database_options();
}

// After:
QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    return {0, QUIVER_LOG_INFO};
}
```

And for `quiver_database_open`:
```cpp
// Before:
*out_db = new quiver_database(path, *options);

// After:
auto cpp_options = convert_database_options(*options);
*out_db = new quiver_database(path, cpp_options);
```

### Updated `database.h` default arguments

```cpp
// Before:
explicit Database(const std::string& path, const DatabaseOptions& options = default_database_options());

// After:
explicit Database(const std::string& path, const DatabaseOptions& options = {});
```

### C++ test initializer pattern change

```cpp
// Before (196 occurrences):
{.read_only = 0, .console_level = QUIVER_LOG_OFF}

// After:
{.read_only = false, .console_level = quiver::LogLevel::Off}
```

### CLI `parse_log_level` update

```cpp
// Before:
static quiver_log_level_t parse_log_level(const std::string& level) {
    if (level == "debug") return QUIVER_LOG_DEBUG;
    // ...
}

// After:
static quiver::LogLevel parse_log_level(const std::string& level) {
    if (level == "debug") return quiver::LogLevel::Debug;
    if (level == "info")  return quiver::LogLevel::Info;
    if (level == "warn")  return quiver::LogLevel::Warn;
    if (level == "error") return quiver::LogLevel::Error;
    if (level == "off")   return quiver::LogLevel::Off;
    throw std::runtime_error("Unknown log level: " + level);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| C typedef alias for C++ type | C++ owns types, C API converts at boundary | This phase | Eliminates layer inversion, enables future C++-only fields |

## Open Questions

1. **`default` case in `to_spdlog_level` switch**
   - What we know: Current code has `default: return spdlog::level::info;` as fallback
   - What's unclear: With `enum class`, all values are accounted for -- should `default` be kept for safety or removed?
   - Recommendation: Keep `default` for defensive coding against potential future enum values, but it could also be removed since `enum class` prevents implicit conversions. Either approach is correct. The compiler will warn about unhandled cases if `-Wswitch` is enabled, which makes the `default` case unnecessary if all values are covered.

2. **CLI `main.cpp` -- should it still include `quiver/c/options.h`?**
   - What we know: CLI currently uses `quiver::DatabaseOptions` which was the C typedef. After the change, CLI uses the C++ type directly.
   - What's unclear: CLI currently does `#include <quiver/database.h>` which transitively included C API types. After the change it won't.
   - Recommendation: CLI should only include `<quiver/database.h>` (which includes `options.h`). Remove any direct C API includes from `main.cpp`. CLI is a C++ consumer, not a C API consumer.

## Sources

### Primary (HIGH confidence)
- Direct code analysis of `include/quiver/options.h` -- verified layer inversion (line 5: `#include "quiver/c/options.h"`)
- Direct code analysis of `include/quiver/c/options.h` -- verified C struct layout and enum values
- Direct code analysis of `src/c/database_options.h` -- verified existing CSVOptions conversion pattern
- Direct code analysis of `src/c/database.cpp` -- verified how `quiver_database_options_default()` calls `default_database_options()`
- Direct code analysis of `src/database.cpp` -- verified `to_spdlog_level()` and `create_database_logger()` use C enum types
- Direct code analysis of `src/cli/main.cpp` -- verified `parse_log_level()` returns C enum type
- Direct code analysis of `src/c/internal.h` -- verified `quiver_database` constructor takes `quiver::DatabaseOptions`
- Grep scan of test files -- verified 196 occurrences in 12 C++ test files, 233 in 12 C API test files (unchanged)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no external libraries involved, pure refactoring of owned types
- Architecture: HIGH -- existing CSVOptions pattern provides proven template; verified both source and target include graphs
- Pitfalls: HIGH -- all pitfalls identified by direct code analysis and grep; compiler will catch any missed instances

**Research date:** 2026-02-27
**Valid until:** Indefinite (internal refactoring, no external dependencies)
