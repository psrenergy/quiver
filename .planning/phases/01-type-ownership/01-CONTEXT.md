# Phase 1: Type Ownership - Context

**Gathered:** 2026-02-27
**Status:** Ready for planning

<domain>
## Phase Boundary

C++ core defines its own `DatabaseOptions` and `LogLevel` types independently of C API headers. The `#include "quiver/c/options.h"` dependency in `include/quiver/options.h` is removed. C API structs stay unchanged; bindings stay unchanged. Only the C++ core and the C API boundary layer change.

</domain>

<decisions>
## Implementation Decisions

### DatabaseOptions type
- C++ defines its own `struct DatabaseOptions` with `bool read_only` and `enum class LogLevel console_level`
- Member initializers provide defaults: `read_only = false`, `console_level = LogLevel::Info`
- No factory function — `DatabaseOptions{}` gives defaults; remove `default_database_options()`
- `enum class LogLevel` values: `Debug, Info, Warn, Error, Off`

### C API surface
- `quiver_database_options_t` stays exactly as-is (no struct changes)
- `quiver_log_level_t` enum stays exactly as-is
- `quiver_database_options_default()` stays exactly as-is
- Bindings continue using the C struct — zero changes to Julia, Dart, Python

### CSVOptions
- Already a proper C++ type — no changes needed
- Existing boundary conversion in `src/c/database_options.h` stays as-is

### Conversion boundary
- C API boundary layer (`src/c/`) converts `quiver_database_options_t` to `quiver::DatabaseOptions` before calling into C++ core
- Trivial conversion: `int` -> `bool`, `quiver_log_level_t` -> `LogLevel`

### Claude's Discretion
- Where exactly the DatabaseOptions conversion function lives (likely `src/c/database_options.h` alongside existing CSVOptions conversion)
- Whether conversion is a free function or inline helper
- LogLevel enum value assignments (implicit sequential or explicit matching C values)

</decisions>

<specifics>
## Specific Ideas

- User wanted to avoid struct duplication but accepted it after seeing the tradeoff: C++ types (`bool`, `enum class`) are genuinely different from C types (`int`, C enum), so having both is the natural cost of a C FFI boundary
- User rejected individual-params approach because it scatters defaults across every binding — violates the principle that bindings should be thin and not define their own logic
- User confirmed CSVOptions is not part of this work — it's already correct

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-type-ownership*
*Context gathered: 2026-02-27*
