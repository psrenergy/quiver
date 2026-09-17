# Codebase Concerns

**Analysis Date:** 2026-09-17

## Overview

This document tracks technical debt, known limitations, performance considerations, and unresolved design trade-offs in the Quiver codebase. Where the root CLAUDE.md or nested CLAUDE.md files explicitly document decisions, they are listed separately at the end to prevent misreading debt as settled design.

---

## Array Fan-Out Issue (Shared Column Names)

**Area:** Element creation and update  
**Files:** `src/database_impl.h` (lines 310–325), test pinned by `tests/test_database_create.cpp::UpdateElementSharedColumnNameWritesEveryMatchingGroup`  
**Problem:**  
When two groups of a collection share a column name (legal under `validate_no_duplicate_attributes`, which exempts FK columns), an array routed by column name during `create_element()` or `update_element()` is written to **every** matching group table, not just one. A caller naming a column cannot disambiguate which group was meant.

**Example:**
```sql
CREATE TABLE Items_vector_parent_ref (...);
CREATE TABLE Items_set_parent_ref (...);
```
Passing `parent_ref` in an array rewrites both groups silently.

**Current state:**  
- Documented and warned: at runtime, a logger.warn message names all matching tables and recommends using `update_vector_group()` / `update_set_group()` instead (names exactly one group)
- Pinned by test: `UpdateElementSharedColumnNameWritesEveryMatchingGroup`
- Only `bindings/julia/test/test_helper_maps.jl` relies on the fan-out (via `create_element!`, the non-destructive half)

**Impact:**  
Any caller using `create_element()` / `update_element()` with shared column names silently rewrites all matching groups. Severity is low if the recommended group writers are used; moderate if array updates are the primary interface.

**Fix approach:**  
Reject `matches.size() > 1` when `delete_existing` is true (deep fix — a breaking behavior change). Requires rewriting `bindings/julia/test/test_helper_maps.jl` to stop relying on the fan-out. **Nothing blocks this fix except the breaking-change decision.**

---

## Julia Type-Stability Follow-Up

**File:** `bindings/julia/type_stability_followup.md`  
**Status:** Documented roadmap with three action items (two candidates for conversion, one real instability to fix, two to leave alone).

**Incomplete conversions to nullability-aware return types:**

### `read_time_series_group` (conversion candidate)
- **File:** `bindings/julia/src/database_read.jl` (~line 531)
- **Current:** Always returns `Vector{Optional{T}}` for value columns
- **Should be:** Concrete `Vector{T}` when `not_null`, `Vector{Optional{T}}` otherwise (metadata available)
- **Impact:** Allows downstream code to assume concrete arrays when schema guarantees no NULLs
- **Blocked by:** None; ready to implement

### `read_time_series_row` (real instability, different bug)
- **File:** `bindings/julia/src/database_read.jl` (~line 601)
- **Current:** Returns `Vector{Int64}`, `Vector{Float64}`, `Vector{Optional{String}}`, or `Vector{Any}` depending on runtime `data_type` and presence
- **Issue:** Unstable eltype; the `Optional` is inherent (semantics: "last non-null value or nothing"), not cell nullability alone
- **Should be:** Always `Vector{Optional{T}}` with `T` keyed on group column's data type from metadata (never `Vector{Any}`, never bare concrete)
- **Impact:** Enables type-stable downstream code; currently forces `Vector{Any}` workarounds
- **Blocked by:** None; pure consistency fix

### `read_scalar_*_by_id` and `query_*` (leave as optional)
- **Reason:** Return `nothing` for missing id OR NULL value; contract change to split them
- **Decision:** Leave optional

---

## Build System Issues

### lua-cmake Builds Unused Binaries
**File:** `cmake/Dependencies.cmake` (lines 24–44)  
**Problem:**  
lua-cmake v5.4.8.0 unconditionally includes `lua_bin` and `luac_bin` in the `all` target (~550 KB each), even though Quiver never uses them. A plain `cmake --build build` still builds these executables.

**Why it exists:**  
- No `LUA_BUILD_INTERPRETER` or `LUA_BUILD_COMPILER` option exists in upstream lua-cmake
- `set_target_properties(... EXCLUDE_FROM_ALL YES)` doesn't work — lua-cmake installs them unconditionally
- `cmake >= 3.28` `EXCLUDE_FROM_ALL` at FetchContent level would help, but project floor is 3.26

**Current workaround:**  
- Dart native-assets hook explicitly requests only `quiver`/`quiver_c` targets, avoiding the binaries
- Scikit-build-core wheel build's `pyproject.toml` exclusion (`wheel.exclude = ["bin"]`) prevents them from being packaged

**Impact:** Local builds slightly larger; CI builds unaffected (targets specified explicitly)  
**Fix approach:** Wait for CMake 3.28 floor or lua-cmake to add an opt-in switch (future decision)

### QUIVER_UNVERSIONED_SHARED Configuration Not Exercised
**File:** `cmake/Platform.cmake` (documented in root CLAUDE.md)  
**Problem:**  
`QUIVER_UNVERSIONED_SHARED=ON` (omits VERSION/SOVERSION) is used **only** by the Dart native-assets hook (`bindings/dart/hook/build.dart`) because `findAndAddCodeAssets` with `followLinks: false` matches unversioned names. No CI job exercises `ON` — it is never tested in isolation.

**Consequence:**  
If a future change breaks versioned builds, the Dart hook failure will surface it, but the feature itself (unversioned shared libraries) is untested at CI.

**Impact:** Low; intentional single-consumer feature  
**Observation:** This is a design trade-off (Julia hardcodes `libquiver.0.dylib`, so versioning is the standard), not a bug.

### macOS Deployment Target Floor 13.3
**File:** `cmake/Platform.cmake` (lines 312–319)  
**Reason:**  
libc++ marks floating-point `std::to_chars` (used in `database_csv_export.cpp` and `lua_runner.cpp`) unavailable below 13.3. This is the **core's floor**, not one binding's.

**Why removal breaks published natives:**  
Without the floor, clang stamps the builder's own OS version into every dylib. Previously published Julia/JS/S3 natives ended up requiring whatever macOS the CI runner image was at build time. Removing it would break installation on older macOS versions.

**Documented load-bearing use:**  
Dart hook repeats the floor via `DEPLOYMENT_TARGET` argument, and the iOS toolchain file derives `CMAKE_OSX_DEPLOYMENT_TARGET` from it (checked before `Platform.cmake` runs).

**Impact:** None if left alone; high if removed  
**Decision:** Intentional constraint, documented as such

---

## File Format Fragility

### .bat Files in Working Tree Are CRLF
**Files:** All `*.bat` files throughout the repo  
**Problem:**  
Windows `.bat` files are checked in as CRLF. Unix tools (sed, awk, grep) silently convert them to LF while processing, breaking them on subsequent execution.

**Example:**  
`scripts/format.bat` run through a sed-based preprocessor becomes LF and loses its carriage returns, causing shell errors on next execution.

**Mitigation:**  
`.gitattributes` enforces LF for source files (`.cpp`, `.h`, `.dart`, `.jl`, `.py`) but not for `.bat`. If a `.bat` is touched by a Unix-style tool, restore CRLF manually.

**Impact:** Local development on Windows with cross-platform tooling; scripts may break silently  
**Fix approach:** Either enforce `.bat` to always be CRLF in `.gitattributes` and document the rule, or migrate build scripts to a `.sh` + Windows-launcher pattern.

---

## Lua Sandbox Boundaries

**File:** `src/lua_runner.cpp` (function `resolve_sandboxed_path`)  
**Design:** Lua file operations (`db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`, `db:export_csv`, `db:import_csv`, `db:validate_migrations`, `expr:save`) are sandboxed to the database file's directory.

**What IS protected:**
- Absolute paths rejected
- Paths escaping the database directory rejected (symlink-aware `weakly_canonical`)
- In-memory databases (`:memory:`) reject all file I/O

**What IS NOT protected:**
- The `load` standard library function (string-form Lua code) is available — scripts can still dynamically load Lua code from strings
- `dofile`/`loadfile` are removed (only the `os`/`io` libraries are unloaded, so those are extra hardening)
- Symlinks inside the database directory are followed (subdirectories OK)

**Consequence:**  
A script can build arbitrary Lua code as a string and execute it via `load()`. File I/O is sandboxed, but code injection is not a threat model assumed here.

**Impact:** File operations are genuinely sandboxed; code execution is not  
**Note:** Julia's standalone `open_file` is unsandboxed (by design — LuaRunner policy only)

---

## Scoped Resource Factory Divergence

**Status:** Three different shapes across five bindings; unresolved  
**Files:** `bindings/julia/`, `bindings/dart/`, `bindings/python/`, `bindings/js/`

**The divergence:**
- **Julia:** Callback-first overloads on `Database.open()`, `from_schema()`, `from_migrations()` and `Binary.File.open_file()` for `do` syntax
- **Python:** `with` context managers on `Database` and `LuaRunner`
- **Dart and JS:** Neither (must call close manually)

All wrap `open + fn + close`. Caveats:
- A `LuaRunner` borrows its `Database` and must not outlive the block
- An uncommitted transaction at block exit is rolled back by `close()`

**Impact:** Inconsistent API surface across bindings; Dart/JS users must remember to close  
**Decision pending:** Unresolved; document the difference in per-binding CLAUDE.md

---

## Asymmetric Group Reader/Writer Shapes in Dart and Python

**Design decision (documented, deliberate):** In Dart and Python only, group **writers** take **column-oriented** data (`{column: [values]}`) while group **readers** return **row-oriented** data (`[{column1: val, column2: val}, ...]`).

**Files:**  
- `bindings/dart/lib/src/database_update.dart` (writers)
- `bindings/dart/lib/src/database_read.dart` (readers)
- `bindings/python/src/database_update.py` (writers)
- `bindings/python/src/database_read.py` (readers)

**Why:**
- Columnar shape is the canonical cross-binding one for **writes** (C API takes it)
- Changing either side is a breaking API change
- Julia and Lua readers still return composed column reads (dense, null-dropping caveat applies)

**Impact:** Users must transpose when round-tripping group data  
**Rationale:** Documented in root design decisions; intentional  
**Not a bug.**

---

## Lua Composite Helper Limitation

**Files:** `tests/test_lua_runner_read.cpp` (lines 328, 353 comments)  
**Functions affected:** `db:read_vectors_by_id()` and `db:read_sets_by_id()` (Lua only)

**Problem:**  
The composite helpers that read entire groups only work correctly when `group_name == column_name`. When a group has multiple columns with different names than the group itself, the helpers fail to assemble rows correctly.

**Example:**  
A vector group named `"values"` with columns `"x"` and `"y"` will not be read correctly.

**Current workaround:**  
Use per-column readers (`db:read_vector_floats_by_id()`, etc.) and manually assemble when `group_name != column_name`.

**Impact:** Lua users with multi-column groups must use lower-level readers  
**Scope:** Lua only; other bindings handle this correctly  
**Status:** Documented as known limitation, tested against basic schemas only

---

## Performance Bottlenecks (Binary Subsystem)

**Analysis:** Profiled with 480×500×31 dimensions (~7.3M read/write operations)  
**File:** `src/CLAUDE.md` (lines 458–465)

### 1. Hash Map Dims Parameter (~40% of time)
**Issue:** Every `BinaryFile::read(dims)` constructs an `unordered_map<string, int64_t>` with string keys.  
**Bottleneck:** Hashing, heap allocation, and `find()` operations.

**Prototype considered:** Indexed `vector<int64_t>` overloads for hot-path performance.  
**Decision:** Dropped in favor of API simplicity — single entry point.

**Current mitigation:** Consumers like `ExpressionFile::compute_row()` cache the map across calls.

**Fix approach:** Only reconsider if profiling shows this remains the critical path after other optimizations. Do not reintroduce indexed overloads lightly.

### 2. Dimension Validation (~19% of time)
**Issue:** `validate_dimension_values()` called on every read/write.  
**Check cost:** Bounds, name existence, time-dimension date arithmetic.

**Fix approach:** Could be made optional or skipped when callers guarantee correctness (e.g., when iterating via `next_dimensions()`). Not yet benchmarked against the cost of branching.

### 3. Vector Allocation (~3%)
**Issue:** Each `BinaryFile::read()` allocates a new `vector<double>`.  
**Fix approach:** Add `read_into(buffer, dims)` overload writing into caller-provided storage. Low priority (3% impact).

---

## Deliberate Design Decisions and "Do Not Fix" Items

**Note:** The following are reviewed as features or intentional constraints, not improvements. From root `CLAUDE.md`:

### Settled Design Decisions (Do Not Relitigate)

1. **Time-series column-oriented shape**: All bindings expose group data as `{column: [values]}` for writes. Row-shaped internally in C++.

2. **Lazy schema loading**: `Database(path, options)` opens without reading schema; first metadata/CRUD call triggers load. Solves the "half-migrated database" problem.

3. **Binary + expression subsystems Julia/Lua only**: Deliberately not exposed in Dart/Python/JS (no FFI consumer).

4. **Lua has no row-aligned whole-group readers**: `read_vector_group_by_id` / `read_set_group_by_id` not bound; composite readers read columns independently, not positionally aligned on nullable groups.

5. **Boolean wrappers Julia/Dart/Python/JS only; Lua excluded**: Read-side only (Lua has no boolean readers, only `== 1` workaround). Write-side accepts booleans everywhere in every layer.

6. **One scalar typing policy in C++**: int64 → INTEGER or REAL; double → REAL only; string → TEXT/INTEGER-FK/DATE_TIME. Shared by `TypeValidator` and `value_matches_type`.

7. **DATE_TIME validation predicate `datetime::is_valid_iso8601`**: Checks both write and read gates. The intersection of Python `fromisoformat`, Julia `DateTime`, and Dart `DateTime.parse`.

8. **Dry-run consequences**: Nested rollback not partial; `in_transaction()` still true; `import_csv` throws precondition; raw `COMMIT` via `query_*` can escape. All documented, not being "fixed" because fixing requires design changes.

9. **Scalar bulk readers preserve NULLs positionally**: Vector/set/datetime/boolean readers drop NULLs and empty-id rows (not aligned with `read_element_ids`). Scalar reads are.

10. **Binary dims parameter map-based only**: Indexed overloads prototyped and dropped (perf rationale in `src/CLAUDE.md`).

11. **Element arrays accept NULL cells**: Stored directly in C++, marshaled through C API with per-cell presence mask.

12. **Scoped resource factories different shapes**: Julia callback-first, Python `with`, Dart/JS manual. Not standardized; documented per binding.

### "Do Not Fix" Anti-Patterns

From root `CLAUDE.md` "Do Not Fix" section — these were reviewed adversarially and rejected as improvements:

1. **Collapsing FFI boilerplate in Dart/Python**: The per-method expanded style is the de facto convention; helper parameterization adds pointer-type indirection for marginal gain.

2. **Deleting `tests/sandbox`**: Intentional scratch target.

3. **"Simplifying" documented Bun FFI workarounds** (`bindings/js/CLAUDE.md`): Load-bearing.

4. **"Simplifying" binary hot-path decisions** (`src/CLAUDE.md`): Load-bearing.

5. **Drive-by fixing pre-existing lint debt in untouched JS files**: Out of scope.

6. **Relocating the agent-facing Lua reference** (`bindings/js/src/lua-api.ts`): Moving to C++ or `.md` would turn build-time constant into runtime FFI call + require republishing across npm/PyPI/Julia/S3. The actual fix is the sync test (`lua-api-sync.test.ts`).

---

## Security Considerations

### Lua Sandbox Limits
**Risk:** Lua scripts can load arbitrary code via `load()`, escaping the file-system sandbox via code execution.  
**Mitigation:** File I/O genuinely sandboxed; code execution is not a threat model. Scripts are trusted (not run against untrusted input).

### Null Pointer Dereferences in C API
**Risk:** C API consumers must free returned pointers correctly (strings, masks, time-series data).  
**Mitigation:** Every FFI binding follows the pairing rules (string freed with `quiver_database_free_string`, LuaRunner JSON with `quiver_lua_runner_free_string`, masks with `quiver_database_free_mask`). The C++ layer does not expose unsafe pointer APIs to bindings.

---

## Fragile Areas Requiring Careful Modification

### Type Validator and Value Matching Rules
**File:** `src/type_validator.cpp`, `src/database_internal.h`  
**Fragility:** One policy, two entry points (`TypeValidator::validate_value` for create/update, `value_matches_type` for time-series writes). Both must stay in sync.

**Safe modification:** Any change to one must be mirrored in the other. Add a test exercising both paths.

### Date/Time Grammar Enforcement
**Files:** `src/utils/datetime.h` (`is_valid_iso8601`), `src/type_validator.cpp`, `src/database_time_series.cpp`, `src/database_csv_import.cpp`  
**Fragility:** Three independent gates (scalar write, time-series write, CSV import) check the same grammar. One gate looses, all bets off — a date that passes write validation will later fail in a binding's parser.

**Safe modification:** Any grammar change requires updating all three. Add a unit test for the edge case, then verify all three gates accept/reject it identically. Verify against Python `fromisoformat`, Julia `DateTime`, and Dart `DateTime.parse` (the intersection is the spec).

### Group Insert and Update Validation Order
**File:** `src/database_impl.h` (`insert_rows_into_group_table`, `update_group_rows`)  
**Fragility:** Validation **must** run before DELETE. If validation is moved after DELETE, a failed write leaves the group cleared with no transaction to roll back (when `TransactionGuard` is a no-op in a dry run or caller-owned transaction).

**Safe modification:** Validation stays before delete, period. Test with dry runs.

### Lua Table-to-Vector Conversion
**File:** `src/lua_runner.cpp` (`lua_table_to_vector<T>`)  
**Fragility:** Converts **every** cell, not just the dispatch cell. Disabled type coercion in release builds (see `SOL_SAFE_NUMERICS` comment). Mixed-type arrays are caught cell-by-cell.

**Safe modification:** The loop bound and element-type dispatch are load-bearing. Do not refactor to early-exit on dispatch-cell type.

### Binary Subsystem Dimension Iteration
**Files:** `src/binary/iteration.cpp`, `include/quiver/binary/iteration.h`  
**Fragility:** `next_dimensions()` handles variable-length time dimensions (Feb 28/29, Jan 31, etc.) via `dimension_sizes_at_values()`. The iterator is the single source of truth.

**Safe modification:** Any change to time-dimension arithmetic must be verified against the aggregation code (`ExpressionAggregate`) and CSV converters, which also call `dimension_sizes_at_values()`.

---

## Testing Gaps

### Platform-Specific Coverage
**Gap:** QUIVER_UNVERSIONED_SHARED=ON exercised by Dart hook only; no CI job builds and tests it in isolation.  
**Impact:** Low (design is sound, just untested independently)

### Binary Subsystem Dimension Edge Cases
**Gap:** 480×500×31 profile data; boundary cases at month/year transitions in time dimensions may not be covered.  
**Impact:** Low (CSV converters have integration tests against real date sequences)

### Lua Sandbox Escape Scenarios
**Gap:** No fuzz testing of Lua paths against symlink edge cases or deeply nested directory structures.  
**Impact:** Low (the `weakly_canonical` check is standard; edge case testing would improve confidence but is not critical)

---

## Unresolved Open Items

### Array Fan-Out Deep Fix
**Blocker:** Breaking API change (rejects `matches.size() > 1`)  
**Scope:** One line guard in `src/database_impl.h`, plus rewrite of `bindings/julia/test/test_helper_maps.jl`  
**Ready to execute:** Yes

### Julia Type-Stability Conversions
**Items:**
- `read_time_series_group` → concrete for NOT NULL columns (ready)
- `read_time_series_row` → stable `Vector{Optional{T}}` (ready)

**Blocker:** None (pure improvements)  
**Scope:** Two separate PRs recommended

### Release Ritual for CHANGELOG.md
**Issue:** Version bump is automated (`scripts/assert_version.py`), but CHANGELOG editing is manual and unguarded.  
**Scope:** Documentation + process, not code  
**Status:** Documented as "not settled" in root CLAUDE.md

---

*Concerns audit: 2026-09-17*
