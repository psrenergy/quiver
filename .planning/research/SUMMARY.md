# Project Research Summary

**Project:** Quiver v0.5 — CSV Library Integration and C API Header Consolidation
**Domain:** Targeted refactor — replace hand-rolled RFC 4180 writer with a third-party C++ CSV library; merge C API header
**Researched:** 2026-02-22
**Confidence:** HIGH

## Executive Summary

Quiver v0.5 is a focused refactor with two independent changes: replace the hand-rolled CSV writer in `src/database_csv.cpp` with a proper C++ CSV library, and consolidate `include/quiver/c/csv.h` into `include/quiver/c/options.h`. Neither change alters any public API, changes any binding code logic, or affects test behavior — all existing CSV tests must pass without modification. The scope is narrow: 10 files touched total across both changes, with two static functions (`csv_escape()` and `write_csv_row()`) deleted and replaced by library calls.

The recommended approach is to execute the two changes independently in sequence. Library integration first: add the CSV library via FetchContent, link it as PRIVATE, and replace the two helper functions. Header consolidation second: merge the CSV struct into options.h, delete csv.h, update two include sites, then regenerate FFI bindings for Julia and Dart. FFI regeneration is mandatory after header consolidation — the generated file content is expected to be identical, but the generators must be re-run to establish this fact and ensure the source header is tracked correctly.

The dominant risk in this milestone is not in the C++ layer — it is in the FFI boundary. A library that injects CRLF on Windows, a missed include site when deleting csv.h, a stale Dart `.dart_tool/` cache, or a compatibility stub left behind that causes Julia's `readdir`-based generator to emit duplicate struct definitions are all plausible failure modes with concrete tests that will catch them. The "Looks Done But Isn't" checklist in PITFALLS.md should be treated as the acceptance criteria for the milestone.

**CRITICAL — Library Recommendation Conflict:** STACK.md recommends **rapidcsv (d99kris, v8.92)** while FEATURES.md recommends **vincentlaucsb/csv-parser**. ARCHITECTURE.md follows FEATURES.md and provides csv-parser code examples throughout. This disagreement must be resolved before Phase 1 begins. See the Gaps section for the resolution analysis.

## Key Findings

### Recommended Stack

The only new dependency is a C++ CSV library. All other stack components are unchanged. The library must be header-only or static (no new DLLs on Windows), integrate via CMake FetchContent, support RFC 4180 auto-quoting on write, and allow Quiver to control the `std::ofstream` in binary mode. Two libraries passed the candidate evaluation:

**Core technologies:**
- **rapidcsv (d99kris, v8.92)** (STACK.md recommendation): Header-only (`src/rapidcsv.h` only), actively maintained (v8.92 Feb 2025), provides `rapidcsv::rapidcsv` CMake imported target via `INTERFACE` library, `pAutoQuote=true` default handles RFC 4180 quoting, API matches Quiver's existing row-at-a-time write pattern, supports `doc.Save(ostream&)` for caller-controlled binary mode. Limitation: loads entire document into memory — incompatible with large CSV import (future milestone).
- **vincentlaucsb/csv-parser (v2.3.0)** (FEATURES.md + ARCHITECTURE.md recommendation): RFC 4180 auto-quoting confirmed via test suite, streaming reader suitable for large CSV import, `make_csv_writer(stream)` accepts caller-provided `ofstream`, column-by-name access on read. Compiles as a static library (not pure header-only) — no new DLL on Windows.
- **CMake FetchContent**: Pattern matches all existing dependencies (SQLite, spdlog, sol2, GoogleTest); no configuration changes outside `cmake/Dependencies.cmake` and `src/CMakeLists.txt`.

### Expected Features

The v0.5 scope is a refactor, not a feature addition. All externally visible behavior is preserved.

**Must have (table stakes — required to close v0.5):**
- RFC 4180 auto-quoting on write (comma, double-quote, newline fields) — library provides this automatically, replacing `csv_escape()`
- LF line endings (not CRLF) — Quiver controls the `std::ofstream` in binary mode; library must accept a pre-opened stream
- Minimal quoting (quote only when necessary) — existing tests assert clean fields like `Alpha` appear unquoted
- NULL as empty unquoted field — Quiver pre-converts all nulls to `""` in `value_to_csv_string()`; library only handles the string
- All 22 C++ CSV tests and all 31 C API CSV tests pass without modification
- CMake FetchContent integration with no new shared DLLs on Windows

**Should have (selection criterion — must be available in chosen library even if not used in v0.5):**
- Read path capability (streaming, column-by-name) — choosing a write-only library now would require a dependency swap when CSV import is added
- Per-row streaming read (not full-document load into memory) — required for large import files at the future milestone

**Defer (v1.0 import milestone):**
- `import_csv()` implementation — explicitly out of scope for v0.5; the library selection now makes this feasible without adding a second dependency later
- Type inference, delimiter auto-detection, header mapping — all import concerns

### Architecture Approach

The refactor has two strictly independent sub-changes with no inter-dependency. Library integration touches only `src/database_csv.cpp`, `cmake/Dependencies.cmake`, and `src/CMakeLists.txt`. Header consolidation touches only `include/` files, two include sites in `src/` and `tests/`, and triggers FFI regeneration. Neither change propagates to the C API, bindings, or test logic — only to build artifacts.

**Major components:**
1. `src/database_csv.cpp` (modified) — delete `csv_escape()` and `write_csv_row()`; replace call sites with library writer; keep `parse_iso8601()`, `format_datetime()`, `value_to_csv_string()`, `Database::export_csv()` all unchanged
2. `include/quiver/c/options.h` (modified) — absorb `quiver_csv_export_options_t` struct and `quiver_csv_export_options_default()` declaration from csv.h; add `#include "common.h"` (needed for `QUIVER_C_API`, `size_t`, `int64_t`)
3. `include/quiver/c/csv.h` (deleted) — content fully moved to options.h; no compatibility stub
4. FFI regeneration (Julia + Dart) — `c_api.jl` and `bindings.dart` content expected identical; regeneration still mandatory to track the new source header

**Key patterns:**
- Library wraps the stream, not the file: Quiver opens `std::ofstream` in binary mode first, then passes it to the library writer — preserving cross-platform LF control
- Library is PRIVATE link dependency: csv-parser or rapidcsv is an implementation detail of `database_csv.cpp`; no public header includes the library header; must use `target_link_libraries(quiver PRIVATE ...)` not PUBLIC
- Merge not deprecate: csv.h is deleted outright; no compatibility stub; project philosophy prohibits deprecation when two include sites can be updated in the same commit

### Critical Pitfalls

1. **CRLF line ending contamination on Windows** — Many CSV libraries default to CRLF (RFC 4180 recommends it). The `ExportCSV_LFLineEndings` test asserts absence of `\r\n`. Must verify library accepts a caller-opened binary-mode stream and does not inject its own line endings. This is the highest-priority selection criterion and must be confirmed before committing to any library.

2. **Float formatting divergence from `%g` semantics** — Multiple tests assert exact float strings (`"1.1"`, `"9.99"`, `"23"` not `"23.0"`). The library must never format `double` values directly. All floats are pre-formatted via `snprintf("%g")` in `value_to_csv_string()` and arrive at the library as `std::string`. This is already the correct design; the risk is accidentally refactoring it away.

3. **Over-quoting breaks substring test assertions** — Tests use `content.find("Item1,Alpha,1,9.99,...")`. If the library quotes clean fields unconditionally (`"Alpha"` instead of `Alpha`), all substring searches fail. Must use a library with a configurable "quote only when necessary" mode.

4. **Missed `csv.h` include in test file** — When deleting csv.h, exactly two include sites exist: `src/c/database_csv.cpp` and `tests/test_c_api_database_csv.cpp`. The test file is a common miss. Run `grep -r "csv.h"` across the entire repo before deleting the file.

5. **Julia generator duplicate struct from compatibility stub** — The Julia generator uses `readdir(include_dir)` and processes every `.h` file as an entry point. A compatibility stub (`csv.h` redirecting to `options.h`) causes Clang.jl to emit `quiver_csv_export_options_t` twice in `c_api.jl`, breaking Julia module load. Delete csv.h completely; do not leave a stub.

6. **Dart `bindings.dart` struct silently disappears** — `quiver_csv_export_options_t` reaches Dart's generated bindings transitively through `database.h -> csv.h`. After the merge it must flow through `database.h -> options.h`. If `database.h`'s `#include "options.h"` is accidentally removed during the merge, the struct vanishes from `bindings.dart` without a compiler error. Diff `bindings.dart` before and after ffigen regeneration.

7. **Stale Dart `.dart_tool/` cache** — After any header file deletion, the Dart hooks runner may use a cached DLL. Clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` before the first post-merge Dart test run (per CLAUDE.md).

8. **Both Dart ffigen configs must stay in sync** — `bindings/dart/pubspec.yaml` and `bindings/dart/ffigen.yaml` are duplicated. Both must be updated atomically when header paths change.

## Implications for Roadmap

The two changes are fully independent and have no shared files. Either order is correct. The architecture research recommends library integration first (no header changes; verifiable in isolation) then header consolidation (no build system changes; verifiable in isolation), followed by FFI regeneration as a mandatory third step.

### Phase 1: Library Integration

**Rationale:** Self-contained change touching only the build system and one C++ implementation file. No header changes, no FFI impact, no binding changes. Verifiable entirely with `quiver_tests.exe` C++ tests. Establishing the library here means the header consolidation phase has a stable, tested codebase to work from.

**Delivers:** rapidcsv or csv-parser integrated via FetchContent; `csv_escape()` and `write_csv_row()` deleted; library handles RFC 4180 quoting; all 22 C++ CSV tests pass; all 31 C API CSV tests pass; no new DLLs on Windows.

**Addresses features from FEATURES.md:** RFC 4180 auto-quoting (P1), LF line endings (P1), write to pre-opened ofstream (P1), minimal quoting (P1), FetchContent integration (P1), all existing tests pass (P1).

**Avoids pitfalls:** CRLF contamination (verify before committing to library), float formatting divergence (preserve `value_to_csv_string` pre-formatting), over-quoting (verify minimal quoting mode), new DLL on Windows (prefer header-only or static library).

### Phase 2: C API Header Consolidation

**Rationale:** Fully independent from Phase 1 — no build system changes, only header and include-site edits. Doing this second means the C++ layer is already verified stable, so any test failure is attributable to the header merge, not the library swap.

**Delivers:** `quiver_csv_export_options_t` and `quiver_csv_export_options_default()` moved into `include/quiver/c/options.h`; `include/quiver/c/csv.h` deleted; `include/quiver/c/database.h` has `#include "csv.h"` removed; two include sites updated in `src/c/database_csv.cpp` and `tests/test_c_api_database_csv.cpp`; all C API tests pass.

**Avoids pitfalls:** Missed test file include site (grep before deletion), compatibility stub causing Julia duplicate struct (delete outright), Dart struct disappearing (verify `database.h` still includes `options.h`), Dart stale cache (clear before first test run), Dart dual configs out of sync (update both atomically).

### Phase 3: FFI Regeneration and Binding Verification

**Rationale:** Must follow Phase 2 — the generators consume header files that changed. Content of generated files is expected to be identical (same types, same functions, just sourced from options.h instead of csv.h), but regeneration is mandatory to confirm this and update the source tracking.

**Delivers:** Regenerated `bindings/julia/src/c_api.jl` (via `generator.bat`); regenerated `bindings/dart/lib/src/ffi/bindings.dart` (via `generator.bat`); Julia and Dart CSV test suites pass; diff of both generated files confirms no structural change.

**Avoids pitfalls:** Julia duplicate struct (verify exactly one `quiver_csv_export_options_t` in `c_api.jl`), Dart struct disappears (verify all 6 fields present in `bindings.dart`), Dart stale cache (clear `.dart_tool/` before test run).

### Phase Ordering Rationale

- Phases 1 and 2 are independent and can be implemented in either order or in the same commit — the architecture research confirms no shared files between them
- Phase 3 must follow Phase 2 because the generators consume headers that changed in Phase 2
- The recommended sequence (library then headers then FFI) minimizes blast radius: each step is verifiable in isolation before proceeding
- Phases 1 and 2 can be collapsed into a single commit if desired; Phase 3 (FFI regen) is always a separate atomic step

### Research Flags

Phases with standard patterns (no additional research needed):
- **Phase 1 (library integration):** The two target functions are small and clearly bounded; FetchContent pattern is established in the codebase; library API shape is documented in both research files
- **Phase 2 (header consolidation):** All include sites are identified; merge target is specified; the resulting `options.h` structure is laid out in ARCHITECTURE.md
- **Phase 3 (FFI regeneration):** Generators are already in place; the expected outcome (identical content) is specified; this is a mechanical step with a clear verification criterion

No phase in this milestone needs `/gsd:research-phase` during planning. The research is complete and implementation-ready.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All libraries evaluated with direct source inspection; CMake integration confirmed; versions verified; one unresolved conflict between research agents (see Gaps) |
| Features | HIGH | Feature requirements are derived from existing passing tests; no speculation about user needs; all 22 C++ + 31 C API CSV tests define the exact acceptance criteria |
| Architecture | HIGH | All affected files identified by name; include chains traced; FFI generator behavior confirmed by direct inspection of `generator.jl` and `pubspec.yaml` |
| Pitfalls | HIGH | 10 pitfalls documented with direct codebase evidence; exact test names and line numbers cited; all recovery costs rated LOW except one MEDIUM (new shared DLL) |

**Overall confidence:** HIGH

### Gaps to Address

**Library recommendation conflict (must resolve before Phase 1):**

STACK.md recommends **rapidcsv (d99kris, v8.92)**. FEATURES.md and ARCHITECTURE.md recommend **vincentlaucsb/csv-parser**. This is a genuine disagreement between research agents, not a minor preference difference. The ARCHITECTURE.md provides detailed csv-parser integration code and cites it throughout — suggesting the architecture researcher used it as the assumed library. STACK.md performed the more thorough comparative evaluation (5 candidates) and concluded rapidcsv fits Quiver's current write-centric use case better.

Resolution recommendation: **use rapidcsv for v0.5**. The decisive factors are (1) rapidcsv is truly header-only (single `src/rapidcsv.h`), making it a simpler build dependency; (2) the `rapidcsv::rapidcsv` CMake INTERFACE target works cleanly with FetchContent; (3) rapidcsv's Document/Save API maps directly onto the existing row-at-a-time write pattern. The main limitation of rapidcsv (full document load, no streaming read) is irrelevant for v0.5 since CSV import is explicitly out of scope. When the import milestone arrives, the library can be revisited — but by that point a design decision about large-file streaming will be made with full context.

If the team prefers csv-parser (for forward-compatibility with the import milestone), it is a valid choice. The ARCHITECTURE.md integration steps remain accurate for csv-parser. Either library makes the existing pitfalls equally applicable. The decision is a preference, not a correctness issue.

**rapidcsv write path clarification needed during Phase 1:**

STACK.md shows rapidcsv's `Document::SetRow()` / `Save()` pattern, which builds the entire document in memory before writing. ARCHITECTURE.md shows csv-parser's streaming `make_csv_writer(file) << row` pattern. The rapidcsv path requires building a `Document` object, populating it row by row, then calling `Save(ostream&)`. This is a minor API shape difference from the current code but does not change the external behavior. The Phase 1 implementer should use `doc.Save(stream)` with a binary-mode stream to preserve LF control.

**`quiver_csv_export_options_default()` implementation location:**

After the header consolidation, `quiver_csv_export_options_default()` is declared in `options.h` but its implementation remains in `src/c/database.cpp` (where it currently lives). This is slightly odd — the function is now declared alongside CSV types in options.h but implemented in the database source file. Consider whether to move the implementation to `src/c/database_csv.cpp` during Phase 2. This is cosmetic; both locations compile correctly.

## Sources

### Primary (HIGH confidence)
- [d99kris/rapidcsv GitHub](https://github.com/d99kris/rapidcsv) — header-only, read+write, v8.92, `pAutoQuote=true`, CMake `rapidcsv::rapidcsv` target (STACK.md)
- [rapidcsv SeparatorParams documentation](https://github.com/d99kris/rapidcsv/blob/master/doc/rapidcsv_SeparatorParams.md) — confirmed `pAutoQuote` write behavior (STACK.md)
- [vincentlaucsb/csv-parser GitHub](https://github.com/vincentlaucsb/csv-parser) — RFC 4180 write via `DelimWriter`/`make_csv_writer`, streaming reader, column-by-name access (FEATURES.md, ARCHITECTURE.md)
- [csv-parser write test suite](https://github.com/vincentlaucsb/csv-parser/blob/master/tests/test_write_csv.cpp) — RFC 4180 quoting verified directly (FEATURES.md)
- [csv-parser CMake FetchContent wiki](https://github.com/vincentlaucsb/csv-parser/wiki/Example:-Using-csv%E2%80%90parser-with-CMake-and-FetchContent) — integration steps confirmed (ARCHITECTURE.md)
- [RFC 4180](https://www.rfc-editor.org/rfc/rfc4180) — CRLF default behavior, quoting rules (PITFALLS.md)
- Quiver codebase — direct inspection of all affected files: `src/database_csv.cpp`, `src/c/database_csv.cpp`, `include/quiver/c/csv.h`, `include/quiver/c/options.h`, `include/quiver/c/database.h`, `bindings/julia/generator/generator.jl`, `bindings/dart/pubspec.yaml`, `bindings/dart/ffigen.yaml`, `bindings/dart/lib/src/ffi/bindings.dart` (ARCHITECTURE.md, PITFALLS.md)

### Secondary (HIGH confidence, library alternatives evaluated and rejected)
- [p-ranav/csv2 GitHub](https://github.com/p-ranav/csv2) — rejected: writer does not auto-quote fields; stale since Dec 2021 (STACK.md, FEATURES.md)
- [ben-strasser/fast-cpp-csv-parser GitHub](https://github.com/ben-strasser/fast-cpp-csv-parser) — rejected: read-only, no write support (STACK.md)
- [ashtum/lazycsv GitHub](https://github.com/ashtum/lazycsv) — rejected: read-only, no write support (STACK.md)

---
*Research completed: 2026-02-22*
*Ready for roadmap: yes*
