# Phase 3: C API String Consistency - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

All C API functions that copy a C++ string to a returned `char*` use `strdup_safe` — no inline `new char[]` followed by `memcpy` or `std::copy` exists in `src/c/` files. Pure internal refactoring — no public API changes, no behavior changes.

</domain>

<decisions>
## Implementation Decisions

### Refactoring depth
- Minimal swap only — replace inline `new char[]` + copy patterns with `strdup_safe()` calls
- No 2D helper extraction — the vector/set nested loops stay structurally the same, only the inner allocation changes
- Both `memcpy` and `std::copy` variants are in scope (same intent, different function call)

### element.cpp scope
- `element.cpp` is included — `element_to_string` (line 147) gets converted to `strdup_safe`
- Covers ALL inline patterns in `src/c/`, nothing left behind

### Utility extraction
- `strdup_safe` and `copy_strings_to_c` move from `database_helpers.h` to existing `src/utils/string.h`
- `database_helpers.h` gets `#include "utils/string.h"` — no further cleanup of that file
- Functions added to `src/utils/string.h` alongside existing `quiver::string::trim()`

### Documentation
- CLAUDE.md Architecture tree updated to show `src/utils/`
- CLAUDE.md String Handling section updated to reference `src/utils/string.h` instead of `database_helpers.h`

### Success criteria wording
- ROADMAP.md success criteria updated to cover both `memcpy` and `std::copy` patterns (original wording only mentioned `memcpy`)
- No additional success criteria added for utility extraction or doc updates — keep minimal

### Claude's Discretion
- `element_to_string` bad_alloc catch: Claude decides whether to remove the dead `std::bad_alloc` catch or keep it
- Build system changes: Claude determines if CMakeLists.txt needs any updates for the utility header

</decisions>

<specifics>
## Specific Ideas

No specific requirements — the refactoring targets are well-defined by the codebase scan.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/utils/string.h`: Existing utility header with `quiver::string::trim()` — new functions go here
- `strdup_safe` (database_helpers.h:95): Already defined, already used by `database_query.cpp`, `database_time_series.cpp`, and metadata converters
- `copy_strings_to_c` (database_helpers.h:64): 1D string array marshaling helper — needs internal conversion to use `strdup_safe`

### Established Patterns
- Consistent `try/catch` + `QUIVER_REQUIRE` pattern across all C API functions
- `database_helpers.h` already uses `strdup_safe` for metadata converters — proven pattern
- `src/utils/` uses traditional `#ifndef` include guards and `quiver::string` namespace

### Integration Points
- 5 inline allocation sites across 3 files: `database_read.cpp` (3), `database_helpers.h` (1), `element.cpp` (1)
- `database_helpers.h` will include `src/utils/string.h` after extraction
- `element.cpp` will include `src/utils/string.h` for `strdup_safe` access
- All existing C API tests verify behavior is preserved — no new tests needed

### Target Sites
| File | Line | Pattern | Context |
|------|------|---------|---------|
| `database_helpers.h:72` | `copy_strings_to_c` | `new char[] + std::copy` | 1D string array helper |
| `database_read.cpp:144` | string vector read | `new char[] + std::copy` | 2D nested loop |
| `database_read.cpp:246` | string set read | `new char[] + std::copy` | 2D nested loop |
| `database_read.cpp:318` | `read_scalar_string_by_id` | `new char[] + std::copy` | Single string |
| `element.cpp:147` | `element_to_string` | `new char[] + memcpy` | Single string |

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-c-api-string-consistency*
*Context gathered: 2026-03-01*
