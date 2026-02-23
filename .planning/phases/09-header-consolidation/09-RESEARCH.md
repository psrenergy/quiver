# Phase 9: Header Consolidation - Research

**Researched:** 2026-02-22
**Domain:** C/C++ header reorganization with FFI binding regeneration
**Confidence:** HIGH

## Summary

This phase is a purely structural refactor: consolidate all option types into unified headers at both the C API and C++ levels, delete the standalone `csv.h` files, update all include paths, and regenerate FFI bindings. No new functionality is introduced. The phase touches 10-12 files across four layers (C++ headers, C API headers, C/C++ implementation, bindings) but every change is mechanical and independently verifiable.

The key risk is not the code changes themselves (which are straightforward) but ensuring the FFI generators produce identical output after the header reorganization. The Julia generator auto-discovers all `.h` files in the C API directory, so deletion of `csv.h` is handled automatically. Dart's ffigen uses explicit entry-point lists but discovers `quiver_csv_export_options_t` transitively through `database.h -> options.h`, so no ffigen configuration changes are needed. However, the regenerated files must be diffed to confirm no regressions.

**Primary recommendation:** Execute as a single sequential plan: C API header merge first, then C++ header creation, then include path updates, then file deletions, then FFI regeneration, then full test suite. Every step is buildable and testable.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Create new `include/quiver/options.h` as the central C++ options header
- Move `CSVExportOptions` struct and `default_csv_export_options()` from `include/quiver/csv.h` into the new `options.h`
- Move `DatabaseOptions` alias (`using DatabaseOptions = quiver_database_options_t`) and `default_database_options()` from `include/quiver/database.h` into the new `options.h`
- `DatabaseOptions` stays as a `using` alias for the C API type -- no new C++ struct
- `LogLevel` stays in the C API `options.h` -- no C++ alias needed
- Delete `include/quiver/csv.h` after migration
- `database.h` includes the new `options.h` so callers get all types from one include
- CSV types go after existing database options in C API options.h: `quiver_log_level_t` -> `quiver_database_options_t` -> `quiver_csv_export_options_t` -> `quiver_csv_export_options_default()`
- `DatabaseOptions` alias + `default_database_options()` first in C++ `options.h`, `CSVExportOptions` struct + `default_csv_export_options()` second
- Fix all 8 files that directly include either `csv.h` -- replace with `options.h` includes
- Individual `.cpp` files include `options.h` directly -- no reliance on transitive includes through `database.h`
- C API `database.h` includes `options.h` (replaces current `csv.h` include)
- C++ `database.h` includes new `options.h` (replaces current `csv.h` include)

### Claude's Discretion
- CLAUDE.md update scope: update file tree entries, plus any prose references to csv.h -- use judgment on what needs changing
- CLAUDE.md description for new `include/quiver/options.h` -- pick wording that fits existing style

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| HDR-01 | Merge `quiver_csv_export_options_t` struct and `quiver_csv_export_options_default()` declaration from `csv.h` into `options.h` | Exact file contents, type ordering, and `#include "common.h"` dependency documented in Architecture Patterns section. The struct (34 lines) and function declaration (1 line) are fully specified. |
| HDR-02 | Delete `include/quiver/c/csv.h` with no compatibility stub | All 5 files referencing `csv.h` identified and catalogued with required include path changes. No compatibility stub needed. |
| HDR-03 | Regenerate Julia and Dart FFI bindings; generated struct must be identical | Julia generator auto-discovers headers (no config change). Dart ffigen discovers struct transitively through `database.h -> options.h` (no config change). Both generators need re-run; output content is functionally identical. |
</phase_requirements>

## Standard Stack

### Core
No new libraries or tools. This phase uses only existing project infrastructure.

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| Julia Clang.jl generator | Existing | Regenerate `c_api.jl` from C headers | Already configured in `bindings/julia/generator/` |
| Dart ffigen | Existing | Regenerate `bindings.dart` from C headers | Already configured in `bindings/dart/ffigen.yaml` |
| CMake | Existing | Build system -- no changes needed | Header moves do not affect CMakeLists.txt |

### Alternatives Considered
None -- this is a file-move-and-delete operation using existing tools.

## Architecture Patterns

### Pattern 1: C API Header Merge (options.h absorbs csv.h)

**What:** Move the CSV options struct and factory function declaration from `include/quiver/c/csv.h` into `include/quiver/c/options.h`, preserving the full documentation comment block.

**Current state of `include/quiver/c/options.h`:**
```c
#ifndef QUIVER_C_OPTIONS_H
#define QUIVER_C_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { ... } quiver_log_level_t;
typedef struct { ... } quiver_database_options_t;

#ifdef __cplusplus
}
#endif
#endif
```

**Target state of `include/quiver/c/options.h`:**
```c
#ifndef QUIVER_C_OPTIONS_H
#define QUIVER_C_OPTIONS_H

#include "common.h"    // ADDED: needed for QUIVER_C_API, size_t, int64_t

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { ... } quiver_log_level_t;
typedef struct { ... } quiver_database_options_t;

// [10-line comment block from csv.h preserved verbatim]
typedef struct {
    const char* date_time_format;
    const char* const* enum_attribute_names;
    const size_t* enum_entry_counts;
    const int64_t* enum_values;
    const char* const* enum_labels;
    size_t enum_attribute_count;
} quiver_csv_export_options_t;

QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void);

#ifdef __cplusplus
}
#endif
#endif
```

**Critical detail:** Current `options.h` does NOT include `common.h`. After the merge, it needs `common.h` for `QUIVER_C_API` (used in the function declaration), `size_t`, and `int64_t` (used in the struct fields). The current `csv.h` already includes `common.h` for these types.

### Pattern 2: C++ Options Header Creation (new include/quiver/options.h)

**What:** Create a new C++ header consolidating `DatabaseOptions` alias + factory from `database.h` and `CSVExportOptions` struct + factory from `csv.h`.

**Target state of `include/quiver/options.h`:**
```cpp
#ifndef QUIVER_OPTIONS_H
#define QUIVER_OPTIONS_H

#include "export.h"
#include "quiver/c/options.h"    // for quiver_database_options_t, quiver_log_level_t

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

using DatabaseOptions = quiver_database_options_t;

inline DatabaseOptions default_database_options() {
    return {0, QUIVER_LOG_INFO};
}

struct QUIVER_API CSVExportOptions {
    std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> enum_labels;
    std::string date_time_format;
};

inline CSVExportOptions default_csv_export_options() {
    return {};
}

}  // namespace quiver

#endif  // QUIVER_OPTIONS_H
```

**Key dependencies:**
- Needs `export.h` for `QUIVER_API` macro (same as current `csv.h`)
- Needs `quiver/c/options.h` for `quiver_database_options_t` and `QUIVER_LOG_INFO` (currently in `database.h`)
- Needs `<cstdint>`, `<string>`, `<unordered_map>` (from current `csv.h`)

### Pattern 3: Include Path Updates (the 8 affected files)

**What:** Every file that directly includes either `csv.h` must be updated to include `options.h` instead.

**C API layer (include `quiver/c/csv.h` -> `quiver/c/options.h`):**

| File | Current Include | New Include |
|------|-----------------|-------------|
| `include/quiver/c/database.h` (line 5) | `#include "csv.h"` | DELETE line (already has `#include "options.h"` on line 6) |
| `src/c/database_csv.cpp` (line 2) | `#include "quiver/c/csv.h"` | `#include "quiver/c/options.h"` |
| `tests/test_c_api_database_csv.cpp` (line 6) | `#include <quiver/c/csv.h>` | `#include <quiver/c/options.h>` |

**C++ layer (include `quiver/csv.h` -> `quiver/options.h`):**

| File | Current Include | New Include |
|------|-----------------|-------------|
| `include/quiver/database.h` (line 7) | `#include "quiver/csv.h"` | `#include "quiver/options.h"` |
| `src/database_csv.cpp` (line 2) | `#include "quiver/csv.h"` | `#include "quiver/options.h"` |
| `src/lua_runner.cpp` (line 3) | `#include "quiver/csv.h"` | `#include "quiver/options.h"` |
| `tests/test_database_csv.cpp` (line 6) | `#include <quiver/csv.h>` | `#include <quiver/options.h>` |
| `src/c/database_csv.cpp` (line 4) | `#include "quiver/csv.h"` | `#include "quiver/options.h"` |

Note: `src/c/database_csv.cpp` appears twice -- it includes both the C API `csv.h` (line 2) and the C++ `csv.h` (line 4). Both must be updated.

**After include path updates, `include/quiver/database.h` include block becomes:**
```cpp
#include "export.h"
#include "quiver/attribute_metadata.h"
#include "quiver/options.h"           // was: quiver/c/options.h + quiver/csv.h
#include "quiver/element.h"
#include "quiver/result.h"
```

The `#include "quiver/c/options.h"` line (currently line 6) is removed because the new `quiver/options.h` includes `quiver/c/options.h` internally. The `#include "quiver/csv.h"` line (currently line 7) is replaced by `#include "quiver/options.h"`.

### Pattern 4: FFI Regeneration

**Julia generator** (`bindings/julia/generator/generator.jl`):
```julia
headers = [
    joinpath(include_dir, header) for
    header in readdir(include_dir) if endswith(header, ".h") && header != "platform.h"
]
```
Auto-discovers all `.h` files in `include/quiver/c/`. When `csv.h` is deleted, it disappears from the list. When `options.h` gains the CSV types, the generator picks them up. **No configuration changes needed.** The generated `c_api.jl` content will have `quiver_csv_export_options_t` and `quiver_csv_export_options_default` in the same file, just sourced from `options.h` instead of `csv.h`.

**Dart ffigen** (`bindings/dart/ffigen.yaml`):
```yaml
headers:
  entry-points:
    - '../../include/quiver/c/common.h'
    - '../../include/quiver/c/database.h'
    - '../../include/quiver/c/element.h'
    - '../../include/quiver/c/lua_runner.h'
  include-directives:
    - '../../include/quiver/c/common.h'
    - '../../include/quiver/c/database.h'
    - '../../include/quiver/c/element.h'
    - '../../include/quiver/c/lua_runner.h'
```

Neither `csv.h` nor `options.h` is listed. Types reach Dart transitively through `database.h` -> `options.h`. The struct `quiver_csv_export_options_t` is generated (confirmed at line 3105 of current `bindings.dart`). The function `quiver_csv_export_options_default()` is NOT generated (because it's declared in a header not listed in `include-directives`). **This is the existing behavior** -- Dart constructs the struct manually in `database_csv.dart` and never calls the C default factory. After the merge, this does not change. **No ffigen.yaml changes needed.**

### Anti-Patterns to Avoid
- **Compatibility stubs:** Do NOT leave a `csv.h` that `#include`s `options.h`. Project philosophy prohibits deprecation. The Julia generator would process both files and risk duplicate struct definitions.
- **Transitive include reliance:** The user decision explicitly requires `.cpp` files to include `options.h` directly, not rely on getting it transitively through `database.h`.
- **Forgetting `common.h`:** The C API `options.h` currently does NOT include `common.h`. After adding `QUIVER_C_API`, `size_t`, and `int64_t` usages, it must include `common.h` or compilation will fail.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FFI binding generation | Manual edits to `c_api.jl` or `bindings.dart` | Run the generators (`generator.bat` for Julia, `generator.bat` for Dart) | Generated files must match header definitions exactly; manual edits cause drift |

**Key insight:** The FFI bindings are fully generated. Never hand-edit them. Run the generators and diff the output.

## Common Pitfalls

### Pitfall 1: Missing `#include "common.h"` in C API options.h
**What goes wrong:** After adding `quiver_csv_export_options_t` with `size_t`, `int64_t`, and `QUIVER_C_API` to `options.h`, compilation fails because these types are undefined.
**Why it happens:** Current `options.h` is self-contained (only uses `int` and its own enum type). The developer merges the struct but forgets the include dependency.
**How to avoid:** Add `#include "common.h"` as the first include in `options.h` before adding any CSV types.
**Warning signs:** Compiler error: `unknown type name 'size_t'` or `'int64_t'` or `'QUIVER_C_API'`.

### Pitfall 2: Stale `database.h` include of `quiver/c/options.h`
**What goes wrong:** After creating `include/quiver/options.h` (which internally includes `quiver/c/options.h`), `database.h` still has its own `#include "quiver/c/options.h"` on line 6. This causes the C API types to be included twice (harmless due to include guards, but untidy and confusing).
**Why it happens:** The developer replaces line 7 (`csv.h` -> `options.h`) but forgets to remove line 6 (`quiver/c/options.h`).
**How to avoid:** Replace BOTH line 6 and line 7 in `database.h` with a single `#include "quiver/options.h"`. Also remove the `using DatabaseOptions` and `default_database_options()` lines (now in `options.h`).
**Warning signs:** Two includes of C API `options.h` visible in `database.h`.

### Pitfall 3: Julia generator produces duplicate struct definitions
**What goes wrong:** If `csv.h` still exists when the generator runs, both `csv.h` and `options.h` define `quiver_csv_export_options_t`, causing a duplicate definition in `c_api.jl`.
**Why it happens:** The generator reads ALL `.h` files in the directory.
**How to avoid:** Delete `csv.h` BEFORE running the Julia generator. Order of operations matters.
**Warning signs:** Julia compilation error about redefined struct.

### Pitfall 4: Dart struct disappears from generated bindings
**What goes wrong:** `quiver_csv_export_options_t` vanishes from `bindings.dart` after regeneration.
**Why it happens:** The struct reaches Dart transitively through `database.h -> options.h`. If the include chain breaks (e.g., `database.h` no longer includes `options.h`), the struct disappears silently -- no compiler error, just a missing type at runtime.
**How to avoid:** After updating `database.h`, verify it still includes `options.h` (or the new `quiver/options.h` which includes `quiver/c/options.h`). After regeneration, diff `bindings.dart` and confirm `quiver_csv_export_options_t` is present with all 6 fields.
**Warning signs:** `bindings.dart` diff shows removal of the struct definition around line 3105.

### Pitfall 5: C++ `database.h` still has `DatabaseOptions` definition
**What goes wrong:** Both `database.h` and the new `options.h` define `DatabaseOptions` and `default_database_options()`, causing a redefinition error.
**Why it happens:** Developer creates `options.h` with the moved definitions but forgets to remove the originals from `database.h`.
**How to avoid:** Remove `using DatabaseOptions = quiver_database_options_t;` (line 18) and the `default_database_options()` function (lines 20-22) from `database.h` after moving them to `options.h`.
**Warning signs:** Compiler error about redefinition of `DatabaseOptions`.

## Code Examples

### Complete target state of `include/quiver/c/options.h`

```c
#ifndef QUIVER_C_OPTIONS_H
#define QUIVER_C_OPTIONS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QUIVER_LOG_DEBUG = 0,
    QUIVER_LOG_INFO = 1,
    QUIVER_LOG_WARN = 2,
    QUIVER_LOG_ERROR = 3,
    QUIVER_LOG_OFF = 4,
} quiver_log_level_t;

typedef struct {
    int read_only;
    quiver_log_level_t console_level;
} quiver_database_options_t;

// CSV export options for controlling enum resolution and date formatting.
// All pointers are borrowed -- caller owns the memory, function reads during call only.
//
// Enum labels map attribute names to (integer_value -> string_label) pairs.
// Represented as grouped-by-attribute parallel arrays:
//   enum_attribute_names[i]  = attribute name for group i
//   enum_entry_counts[i]     = number of entries in group i
//   enum_values[]            = all integer values, concatenated across groups
//   enum_labels[]            = all string labels, concatenated across groups
//   enum_attribute_count     = number of attribute groups (0 = no enum mapping)
//
// Example: {"status": {1: "Active", 2: "Inactive"}, "priority": {0: "Low"}}
//   enum_attribute_names = ["status", "priority"]
//   enum_entry_counts    = [2, 1]
//   enum_values          = [1, 2, 0]
//   enum_labels          = ["Active", "Inactive", "Low"]
//   enum_attribute_count = 2
typedef struct {
    const char* date_time_format;
    const char* const* enum_attribute_names;
    const size_t* enum_entry_counts;
    const int64_t* enum_values;
    const char* const* enum_labels;
    size_t enum_attribute_count;
} quiver_csv_export_options_t;

// Returns default options (no enum mapping, no date formatting).
QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_OPTIONS_H
```

### Complete target state of `include/quiver/options.h`

```cpp
#ifndef QUIVER_OPTIONS_H
#define QUIVER_OPTIONS_H

#include "export.h"
#include "quiver/c/options.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

using DatabaseOptions = quiver_database_options_t;

inline DatabaseOptions default_database_options() {
    return {0, QUIVER_LOG_INFO};
}

struct QUIVER_API CSVExportOptions {
    std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> enum_labels;
    std::string date_time_format;  // strftime format string; empty = no formatting
};

inline CSVExportOptions default_csv_export_options() {
    return {};
}

}  // namespace quiver

#endif  // QUIVER_OPTIONS_H
```

### Target state of `include/quiver/database.h` include block

```cpp
#include "export.h"
#include "quiver/attribute_metadata.h"
#include "quiver/options.h"
#include "quiver/element.h"
#include "quiver/result.h"
```

Lines removed from `database.h`:
- `#include "quiver/c/options.h"` (line 6, now included transitively via `quiver/options.h`)
- `#include "quiver/csv.h"` (line 7, replaced by `quiver/options.h`)
- `using DatabaseOptions = quiver_database_options_t;` (line 18, moved to `options.h`)
- `inline DatabaseOptions default_database_options() { return {0, QUIVER_LOG_INFO}; }` (lines 20-22, moved to `options.h`)

### Target state of `include/quiver/c/database.h` include block

```c
#include "common.h"
#include "options.h"
```

Line removed: `#include "csv.h"` (line 5, types now in `options.h`)

## Complete File Change Manifest

### Files CREATED (1)
| File | Purpose |
|------|---------|
| `include/quiver/options.h` | New C++ consolidated options header |

### Files DELETED (2)
| File | Reason |
|------|--------|
| `include/quiver/c/csv.h` | Contents merged into `include/quiver/c/options.h` |
| `include/quiver/csv.h` | Contents merged into `include/quiver/options.h` |

### Files MODIFIED (9)
| File | Change |
|------|--------|
| `include/quiver/c/options.h` | Add `#include "common.h"`; append CSV struct + factory declaration with full comment block |
| `include/quiver/c/database.h` | Remove `#include "csv.h"` (line 5) |
| `include/quiver/database.h` | Replace includes (lines 6-7 -> `quiver/options.h`); remove `DatabaseOptions` alias + factory (lines 18-22) |
| `src/c/database_csv.cpp` | Line 2: `quiver/c/csv.h` -> `quiver/c/options.h`; Line 4: `quiver/csv.h` -> `quiver/options.h` |
| `src/database_csv.cpp` | Line 2: `quiver/csv.h` -> `quiver/options.h` |
| `src/lua_runner.cpp` | Line 3: `quiver/csv.h` -> `quiver/options.h` |
| `tests/test_database_csv.cpp` | Line 6: `<quiver/csv.h>` -> `<quiver/options.h>` |
| `tests/test_c_api_database_csv.cpp` | Line 6: `<quiver/c/csv.h>` -> `<quiver/c/options.h>` |
| `CLAUDE.md` | Update file tree entries and any prose references to csv.h |

### Files REGENERATED (2)
| File | Method |
|------|--------|
| `bindings/julia/src/c_api.jl` | Run `bindings/julia/generator/generator.bat` |
| `bindings/dart/lib/src/ffi/bindings.dart` | Run `bindings/dart/generator/generator.bat` |

Total: 1 created + 2 deleted + 9 modified + 2 regenerated = 14 file operations

## State of the Art

Not applicable -- this is a structural refactor within an existing project, not a technology adoption decision.

## Open Questions

1. **Dart `quiver_csv_export_options_default()` binding**
   - What we know: Currently NOT generated in Dart bindings because `csv.h` is not in ffigen `include-directives`. Dart constructs the struct manually. After the merge, the function moves to `options.h` which is also not in `include-directives`. Behavior unchanged.
   - What's unclear: Whether the user expects the Dart binding to gain this function after consolidation.
   - Recommendation: No action needed -- current Dart behavior is correct and unchanged. The user's success criteria (SC-3) says "exactly one definition of `quiver_csv_export_options_t` with all 6 fields" -- it refers to the struct, not the function. If the user wants the function in Dart bindings later, they can add `options.h` to ffigen `include-directives` in a future phase.

2. **CLAUDE.md update scope**
   - What we know: User left this to Claude's discretion. The file tree in CLAUDE.md lists `csv.h` entries for both C++ and C API layers. The Architecture section mentions `csv.h` and `CSVExportOptions`.
   - What's unclear: Exact prose changes needed.
   - Recommendation: Update the file tree to replace `csv.h` entries with `options.h`, add the new `include/quiver/options.h` entry, and update any prose that references `csv.h` paths. Also update the Pimpl section to list `CSVExportOptions` as now being in `options.h`.

## Sources

### Primary (HIGH confidence)
- Direct inspection of `include/quiver/c/csv.h` (43 lines) -- full struct and function declaration
- Direct inspection of `include/quiver/c/options.h` (26 lines) -- current state, no `common.h` include
- Direct inspection of `include/quiver/c/database.h` (489 lines) -- includes both `csv.h` and `options.h`
- Direct inspection of `include/quiver/csv.h` (23 lines) -- C++ CSVExportOptions struct
- Direct inspection of `include/quiver/database.h` (212 lines) -- DatabaseOptions alias and factory
- Direct inspection of `bindings/julia/generator/generator.jl` -- auto-discovers all `.h` files
- Direct inspection of `bindings/dart/ffigen.yaml` -- explicit entry-point/include-directive lists
- Direct inspection of `bindings/julia/src/c_api.jl` -- current struct at lines 47-54, function at lines 56-58
- Direct inspection of `bindings/dart/lib/src/ffi/bindings.dart` -- current struct at lines 3105-3118
- Direct inspection of `src/c/database_csv.cpp` -- includes both C and C++ csv.h
- Direct inspection of all 8 files containing `csv.h` includes (grep verified)

### Secondary (MEDIUM confidence)
- Prior research in `.planning/research/ARCHITECTURE.md` lines 180-300 -- header consolidation analysis (pre-dates CONTEXT.md decisions about C++ options.h, so only partially applicable)
- Prior research in `.planning/research/PITFALLS.md` lines 85-145 -- FFI generator pitfalls

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new tools or libraries, all existing infrastructure
- Architecture: HIGH -- all affected files directly inspected, include chains traced, FFI configurations verified
- Pitfalls: HIGH -- all five pitfalls verified against actual file contents and generator behavior

**Research date:** 2026-02-22
**Valid until:** Indefinite (structural refactor, no external dependency version sensitivity)
