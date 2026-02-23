# Stack Research

**Domain:** C++ CSV library integration for SQLite wrapper (replacing hand-rolled writer)
**Researched:** 2026-02-22
**Confidence:** HIGH

## Context

This is a targeted refactor milestone (v0.5). The existing hand-rolled RFC 4180 CSV writer in
`src/database_csv.cpp` (~15 lines + escape function) works correctly. The goal is to replace it
with a proper CSV library that supports both reading and writing, enabling future CSV import without
a second dependency addition. All existing CSV export tests must continue to pass.

Existing validated stack (NOT re-researched): C++20, SQLite 3.50.2, spdlog 1.17.0, sol2 3.5.0,
GoogleTest 1.17.0. All brought in via `cmake/Dependencies.cmake` using `FetchContent_Declare` /
`FetchContent_MakeAvailable`.

## Recommended Stack

### New Dependency: CSV Library

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| rapidcsv (d99kris) | v8.92 | Replace hand-rolled CSV writer; enables future import | Only library in the candidate set that is header-only, actively maintained (v8.92 released Feb 21 2025), supports both reading AND writing, integrates with `rapidcsv::rapidcsv` CMake imported target via FetchContent, and has RFC 4180-compliant quoting via `pAutoQuote=true` (default). API matches how Quiver already writes CSV: write one field at a time, not stream row objects. |

### Retained Stack (No Changes)

| Technology | Version | Purpose | Notes |
|------------|---------|---------|-------|
| `std::ofstream` / `std::fstream` | C++20 stdlib | File I/O for CSV | rapidcsv writes to `std::ostream`; Quiver wraps in its own `std::ofstream`. No change required. |
| `strftime` / `std::get_time` | C standard library | DateTime reformatting | SQLite TEXT to user format. Retained unchanged -- rapidcsv does not handle application-level value formatting. |
| CMake FetchContent | CMake 3.21+ | Dependency management | Pattern matches existing project dependencies. |

## Why rapidcsv

### Candidate Evaluation

| Library | Read | Write | Header-Only | RFC 4180 Quoting | Maintained | Verdict |
|---------|------|-------|-------------|-------------------|------------|---------|
| **rapidcsv** (d99kris) | YES | YES | YES (single `rapidcsv.h`) | YES (`pAutoQuote=true` default) | YES (v8.92, Feb 2025) | **RECOMMENDED** |
| vincentlaucsb/csv-parser | YES | YES (`DelimWriter`) | PARTIAL (single_include available) | YES (tested in test suite) | YES (v2.5.0, Feb 2026) | See note below |
| p-ranav/csv2 | YES | YES | YES | Not documented | NO (v0.1 Dec 2021, stale) | Reject: unmaintained |
| ben-strasser/fast-cpp-csv-parser | YES | NO | YES | N/A | NO (72 commits, dormant) | Reject: no write support |
| ashtum/lazycsv | YES | NO | YES | N/A | NO (27 commits, small) | Reject: no write support |

### Why rapidcsv Over csv-parser (vincentlaucsb)

csv-parser (v2.5.0, Feb 2026) is a strong library with genuine RFC 4180 write support via
`DelimWriter`. It is more actively maintained and more feature-rich. However, rapidcsv is the
better fit for Quiver specifically:

**rapidcsv wins because:**

1. **API shape matches the existing code.** The current `write_csv_row()` function writes a
   `vector<string>` of pre-formatted field values to an `ofstream`. rapidcsv's `Document::SetRow()`
   and `Document::Save()` map directly onto this pattern. The migration replaces two small helper
   functions without restructuring the export logic.

2. **True single-header simplicity.** rapidcsv is literally one file: `src/rapidcsv.h`. Drop it
   in, include it, done. csv-parser's "single include" at `single_include/csv.hpp` is large (the
   full multi-file library concatenated) and the CMake integration requires linking
   `csv::csv` (a compiled static library), not just include paths. This complicates the build
   graph for what is a header-only use case.

3. **`rapidcsv::rapidcsv` CMake imported target works with FetchContent.** As of v8.92 (Jan 2026),
   rapidcsv added `find_package` support with an `INTERFACE` library target (`add_library(rapidcsv
   INTERFACE)`). This means `FetchContent_MakeAvailable(rapidcsv)` + `target_link_libraries(quiver
   PRIVATE rapidcsv::rapidcsv)` works cleanly. No additional configuration needed.

4. **`pAutoQuote=true` is the default.** The `SeparatorParams` default of `pAutoQuote=true`
   automatically handles RFC 4180 quoting (fields with commas, quotes, or line breaks get quoted;
   internal quotes are doubled). No manual invocation of an escape function required. Quiver's
   existing `csv_escape()` function is deleted.

5. **Smaller footprint.** rapidcsv is one ~2000-line header focused on the Document model (load,
   modify, save). csv-parser is a full streaming parser + serializer targeting high-throughput
   scenarios. Quiver's CSV export is simple structured data from SQLite -- performance is not a
   concern, correctness is.

**csv-parser would win if:** Quiver needed streaming CSV import of large files, delimiter
auto-detection, or columnar in-memory DataFrames. Those are not requirements for v0.5 or the
deferred import milestone.

### Why the Others Are Rejected

**p-ranav/csv2:** Last release v0.1 in December 2021. Stale. No version suitable for pinning in
CMake. Reject.

**fast-cpp-csv-parser:** Read-only. No write support. Reject on fundamental requirements.

**lazycsv:** Read-only. No write support. Reject on fundamental requirements.

## CMake Integration

Add one entry to `cmake/Dependencies.cmake`:

```cmake
# rapidcsv for CSV read+write
FetchContent_Declare(rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v8.92
)
FetchContent_MakeAvailable(rapidcsv)
```

Then in `src/CMakeLists.txt`, link the quiver target:

```cmake
target_link_libraries(quiver PRIVATE rapidcsv::rapidcsv)
```

No other changes to the build system. rapidcsv's CMakeLists.txt defines `add_library(rapidcsv
INTERFACE)` and sets the include path, so `target_link_libraries` propagates the include directory
automatically.

## Usage Pattern in src/database_csv.cpp

Current hand-rolled write path:

```cpp
// BEFORE (hand-rolled)
static std::string csv_escape(const std::string& field) { ... }  // ~15 lines
static void write_csv_row(std::ofstream& file, const std::vector<std::string>& fields) { ... }
```

Replacement with rapidcsv:

```cpp
// AFTER (rapidcsv)
#include "rapidcsv.h"

// No csv_escape() needed -- rapidcsv handles quoting automatically.
// Write path: build a rapidcsv::Document, populate rows, Save() to path.

rapidcsv::Document doc("", rapidcsv::LabelParams(-1, -1));
// Populate header and rows using doc.SetRow<std::string>(index, fields_vector)
doc.Save(path);
```

The `SeparatorParams` default (`pSeparator=','`, `pAutoQuote=true`, `pQuoteChar='"'`) produces
RFC 4180-compliant output with no configuration needed.

**Alternative write path (stream-based):** rapidcsv also supports `doc.Save(std::ostream&)`, which
matches the binary-mode `std::ofstream` already used to prevent Windows CRLF conversion.

## What NOT to Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| vincentlaucsb/csv-parser | Heavier dependency (compiled static lib, not pure header-only), mismatched API shape for this use case, overkill for structured SQLite export | rapidcsv |
| p-ranav/csv2 | Unmaintained since Dec 2021, no suitable version tag for CMake pinning | rapidcsv |
| fast-cpp-csv-parser | Read-only, no write support | rapidcsv |
| lazycsv | Read-only, no write support | rapidcsv |
| Any CSV library for the C API layer | The C API calls C++ methods; CSV logic stays in C++. The C API (`src/c/database_csv.cpp`) is a thin wrapper with no CSV logic of its own. | No library at C API layer |
| python csv module style "always quote all fields" output | Produces valid but ugly CSV (every field quoted). rapidcsv's minimal quoting is cleaner and matches existing test expectations. | `pAutoQuote=true` default (quote only when needed) |

## Version Compatibility

| Component | Version | Compatible With | Notes |
|-----------|---------|-----------------|-------|
| rapidcsv | v8.92 (Feb 2025) | C++11 and later, all compilers | Project uses C++20 -- fully compatible. MSVC, GCC, Clang all supported. |
| rapidcsv | v8.92 | CMake 3.11+ | Project requires CMake 3.21 -- well within range. |
| rapidcsv | v8.92 | Windows, Linux, macOS | Cross-platform `std::fstream` I/O; binary-mode `std::ofstream` already used in Quiver for CRLF control. |
| rapidcsv + existing spdlog | v8.92 + v1.17.0 | No conflict | Both are header-only with no shared symbols or link-time conflicts. |

## Sources

- [d99kris/rapidcsv GitHub](https://github.com/d99kris/rapidcsv) -- HIGH confidence. Confirmed: header-only, read+write, v8.92 Feb 2025, `pAutoQuote=true` default, CMake `rapidcsv::rapidcsv` imported target.
- [rapidcsv SeparatorParams documentation](https://github.com/d99kris/rapidcsv/blob/master/doc/rapidcsv_SeparatorParams.md) -- HIGH confidence. Confirmed `pAutoQuote` behavior: "automatically dequote data during read, and add quotes during write."
- [rapidcsv CMake find_package discussion #125](https://github.com/d99kris/rapidcsv/discussions/125) -- HIGH confidence. Confirmed `rapidcsv::rapidcsv` imported target added Jan 2026 (included in v8.92).
- [d99kris/rapidcsv Releases](https://github.com/d99kris/rapidcsv/releases) -- HIGH confidence. v8.92 is latest as of Feb 2025.
- [vincentlaucsb/csv-parser GitHub](https://github.com/vincentlaucsb/csv-parser) -- HIGH confidence. Confirmed: RFC 4180 write support via `DelimWriter`, v2.5.0 Feb 2026, FetchContent integration documented.
- [csv-parser test_write_csv.cpp](https://github.com/vincentlaucsb/csv-parser/blob/master/tests/test_write_csv.cpp) -- HIGH confidence. Confirmed: comma escaping, quote doubling, newline quoting all tested.
- [p-ranav/csv2 GitHub](https://github.com/p-ranav/csv2) -- HIGH confidence. Last release v0.1 Dec 2021. Stale.
- [ben-strasser/fast-cpp-csv-parser GitHub](https://github.com/ben-strasser/fast-cpp-csv-parser) -- HIGH confidence. Read-only confirmed by documentation.
- [ashtum/lazycsv GitHub](https://github.com/ashtum/lazycsv) -- HIGH confidence. Read-only C++17 parser, no write support.

---
*Stack research for: Quiver v0.5 -- CSV Library Integration (replacing hand-rolled writer)*
*Researched: 2026-02-22*
