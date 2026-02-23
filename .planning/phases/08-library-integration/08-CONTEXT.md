# Phase 8: Library Integration - Context

**Gathered:** 2026-02-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace hand-rolled CSV writing helpers (`csv_escape()`, `write_csv_row()`) in `src/database_csv.cpp` with rapidcsv, integrated via CMake FetchContent. External behavior stays identical — all existing tests must pass. Minor public API cleanup is acceptable if the library swap reveals a cleaner design.

</domain>

<decisions>
## Implementation Decisions

### CSV writing approach
- Build a rapidcsv Document in-memory with all rows, then Save() to file
- Quiver transforms data first (enum label resolution, datetime formatting), then feeds plain strings to rapidcsv — rapidcsv only handles CSV structure (escaping, quoting, file I/O)
- RFC 4180 compliance takes priority over matching current test expectations — if rapidcsv produces more correct output that breaks a test, update the test and document why
- Minor public API cleanup is acceptable across the full stack (C++ -> C API -> bindings) if the swap reveals a meaningfully cleaner interface
- If rapidcsv's write API turns out to be genuinely awkward or inadequate, pivot to an alternative library rather than forcing it — come back with a recommendation before proceeding

### Version pinning
- Pin to a specific rapidcsv release tag (not commit hash, not tracking main)
- FetchContent at configure time is acceptable (no vendoring)
- Stay on the pinned version for the duration of the milestone — no mid-milestone updates

### Cleanup scope
- Remove all dead code after the swap, not just `csv_escape()` and `write_csv_row()`
- Clean as you go: if the library swap naturally simplifies C++ headers or C API headers, do it in Phase 8
- Keep Phase 9 as a separate phase even if Phase 8 cleanup covers some of its territory — Phase 9 does the formal consolidation pass

### Claude's Discretion
- File I/O approach (let rapidcsv Save() handle it vs stringstream intermediary)
- Header row construction mapping from current metadata logic to rapidcsv API
- FetchContent placement (root CMakeLists.txt vs cmake/ module)
- Specific rapidcsv release version to pin

</decisions>

<specifics>
## Specific Ideas

- RFC 4180 is the correctness standard — if hand-rolled code had quirks that deviated from RFC, the library swap is an opportunity to fix them
- User is open to pivoting away from rapidcsv if its write capabilities are genuinely insufficient — don't force a square peg

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 08-library-integration*
*Context gathered: 2026-02-22*
