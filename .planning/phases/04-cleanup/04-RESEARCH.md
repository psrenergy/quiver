# Phase 4: Cleanup - Research

**Researched:** 2026-02-23
**Domain:** API surface removal across C++/C/Julia/Dart/Lua layers
**Confidence:** HIGH

## Summary

Phase 4 removes two methods (`update_scalar_relation` and `read_scalar_relation`) and all their traces across every layer of the codebase. The FK resolution feature in `create_element` and `update_element` makes these methods redundant. This is a mechanical deletion task with one critical subtlety: the file `test_database_relations.cpp` contains both the old relation method tests (lines 1-186, to be deleted) AND FK resolution tests (lines 187-421, to be preserved). The FK resolution tests must be relocated before the file is deleted.

The scope spans 6 layers: C++ core, C API, Julia bindings (source + generated FFI + tests), Dart bindings (source + generated FFI + tests), Lua binding (registration + helper function), and C++ tests. Additionally, CLAUDE.md documentation references must be cleaned up.

**Primary recommendation:** Delete all relation source files, remove declarations from headers, remove from CMakeLists, relocate FK resolution tests to `test_database_create.cpp`, delete relation tests across all layers, remove Lua registrations, regenerate FFI bindings, clean CLAUDE.md, and verify with `scripts/build-all.bat`.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Remove both methods entirely** -- `update_element` with FK resolution covers the important case; the label-to-label convenience of `update_scalar_relation` is not worth keeping
- `read_scalar_relation` goes too -- users can query FK target labels with `query_string` if needed
- Clean sweep: remove from C++, C API, Julia, Dart, and Lua
- Delete `src/database_relations.cpp` and `src/c/database_relations.cpp` entirely (both will be empty after removal)
- Delete `bindings/dart/lib/src/database_relations.dart` entirely
- Delete all binding relation source files in Julia
- Remove relation method registration from `LuaRunner` in `src/lua_runner.cpp`
- Remove declarations from `include/quiver/database.h` and `include/quiver/c/database.h`
- Update CMakeLists to remove deleted source files
- Delete `tests/test_database_relations.cpp` and all binding relation test files entirely
- FK resolution is already tested in create/update test files -- no coverage gap
- **Keep** `tests/schemas/valid/relations.sql` -- still used by create/update FK resolution tests
- Remove relation function declarations from Julia and Dart generator templates
- **Regenerate FFI bindings** by running both `generator.bat` scripts after C API changes
- Verify regenerated bindings compile
- Full cleanup of CLAUDE.md: remove `update_scalar_relation` and `read_scalar_relation` from cross-layer naming tables, API descriptions, and any other references
- Remove the "Relations" row from binding convenience method tables where it refers to these methods
- Error message alignment settled automatically -- removing the methods removes their non-standard error messages
- Run full `scripts/build-all.bat` after all changes to confirm nothing broke across all layers

### Claude's Discretion
No areas marked for Claude's discretion.

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CLN-01 | Evaluate whether `update_scalar_relation` is redundant and decide keep/remove | Decision locked: REMOVE both `update_scalar_relation` and `read_scalar_relation`. Research identifies all 16+ files containing references that must be modified or deleted. |
</phase_requirements>

## Architecture Patterns

### Pattern 1: Complete Removal Across All Layers

**What:** When removing a C++ method from this project, you must trace and remove it from exactly 6 layers in a specific order.

**Layer removal order (dependency-safe):**
1. **C++ core** -- Remove declarations from `include/quiver/database.h`, delete implementation file `src/database_relations.cpp`
2. **C API** -- Remove declarations from `include/quiver/c/database.h`, delete implementation file `src/c/database_relations.cpp`
3. **CMakeLists** -- Remove deleted source files from `src/CMakeLists.txt` (both `database_relations.cpp` and `c/database_relations.cpp`)
4. **Lua binding** -- Remove registration block and helper function from `src/lua_runner.cpp`
5. **Julia binding** -- Remove wrapper functions from `bindings/julia/src/database_create.jl` and `bindings/julia/src/database_read.jl`
6. **Dart binding** -- Delete `bindings/dart/lib/src/database_relations.dart`, remove `part` directive from `bindings/dart/lib/src/database.dart`
7. **FFI regeneration** -- Run `bindings/julia/generator/generator.bat` and `bindings/dart/generator/generator.bat` to regenerate from updated C headers
8. **Tests** -- Delete/modify test files across all layers
9. **Documentation** -- Update `CLAUDE.md`

### Pattern 2: FK Resolution Test Relocation

**What:** `tests/test_database_relations.cpp` contains two distinct groups of tests. Only the first group (lines 1-186) tests the relation methods being removed. The second group (lines 187-421) tests FK label resolution in `create_element` and must be preserved.

**Tests to relocate to `test_database_create.cpp`:**
- `ResolveFkLabelInSetCreate` (line 191)
- `ResolveFkLabelMissingTarget` (line 222)
- `RejectStringForNonFkIntegerColumn` (line 239)
- `CreateElementScalarFkLabel` (line 255)
- `CreateElementVectorFkLabels` (line 277)
- `CreateElementTimeSeriesFkLabels` (line 304)
- `CreateElementAllFkTypesInOneCall` (line 334)
- `CreateElementNoFkColumnsUnchanged` (line 382)
- `ScalarFkResolutionFailureCausesNoPartialWrites` (line 405)

These 9 tests use `create_element` and belong logically in `test_database_create.cpp`.

### Anti-Patterns to Avoid
- **Deleting test_database_relations.cpp without relocating FK tests:** This would lose 9 tests covering create_element FK resolution -- the core feature of v1.0.
- **Forgetting to regenerate FFI bindings:** The Julia and Dart FFI bindings are auto-generated from the C headers. Simply removing the manual wrapper code is insufficient; the generated `c_api.jl` and `bindings.dart` files will still reference the deleted C functions.
- **Editing generated files manually:** The Dart `bindings/dart/lib/src/ffi/bindings.dart` and Julia `bindings/julia/src/c_api.jl` are generated by ffigen/Clang.jl respectively. They should be regenerated, not hand-edited.

## Complete File Inventory

### Files to DELETE entirely

| File | Layer | Content |
|------|-------|---------|
| `src/database_relations.cpp` | C++ core | `update_scalar_relation` + `read_scalar_relation` implementation |
| `src/c/database_relations.cpp` | C API | C wrapper for both methods |
| `bindings/dart/lib/src/database_relations.dart` | Dart | `DatabaseRelations` extension with both methods |
| `bindings/dart/test/database_relations_test.dart` | Dart test | 4 relation test groups |
| `bindings/julia/test/test_database_relations.jl` | Julia test | 4 relation test sets |
| `tests/test_database_relations.cpp` | C++ test | After relocating FK tests (lines 187-421) to `test_database_create.cpp` |

### Files to EDIT

| File | Layer | What to Remove |
|------|-------|----------------|
| `include/quiver/database.h` | C++ header | Lines 47-53: `update_scalar_relation` and `read_scalar_relation` declarations (under "Relation operations" comment) |
| `include/quiver/c/database.h` | C API header | Lines 72-84: `quiver_database_update_scalar_relation` and `quiver_database_read_scalar_relation` declarations (under "Relation operations" comment) |
| `src/CMakeLists.txt` | Build | Line 11: `database_relations.cpp` from QUIVER_SOURCES; Line 105: `c/database_relations.cpp` from quiver_c sources |
| `tests/CMakeLists.txt` | Build | Line 12: `test_database_relations.cpp` from quiver_tests sources |
| `src/lua_runner.cpp` | Lua binding | Lines 256-268: relation registration block ("Group 6: Relations"); Lines 1036-1051: `read_scalar_relation_to_lua` helper function and "Relations" section comment |
| `bindings/julia/src/database_create.jl` | Julia | Lines 19-28: `update_scalar_relation!` function |
| `bindings/julia/src/database_read.jl` | Julia | Lines 1-24: `read_scalar_relation` function |
| `bindings/dart/lib/src/database.dart` | Dart | Line 18: `part 'database_relations.dart';` directive |
| `tests/test_database_errors.cpp` | C++ test | Lines 181-243: 7 relation error tests (`SetScalarRelationNoSchema` through `ReadScalarRelationNotForeignKey`) |
| `tests/test_c_api_database_lifecycle.cpp` | C API test | Lines 259-442: All relation operation tests (null checks, valid operations for both update and read) |
| `tests/test_lua_runner.cpp` | C++ test | Lines 1305-1346: `LuaRunnerRelationsTest` fixture class and 2 test methods |
| `tests/test_database_create.cpp` | C++ test | ADD relocated FK resolution tests from `test_database_relations.cpp` |
| `CLAUDE.md` | Docs | Multiple references (see Documentation section below) |

### Files to REGENERATE (not hand-edit)

| File | Generator | Trigger |
|------|-----------|---------|
| `bindings/julia/src/c_api.jl` | `bindings/julia/generator/generator.bat` | Reads `include/quiver/c/database.h` |
| `bindings/dart/lib/src/ffi/bindings.dart` | `bindings/dart/generator/generator.bat` (dart run ffigen) | Reads `include/quiver/c/database.h` via `ffigen.yaml` |

### Files to KEEP unchanged

| File | Reason |
|------|--------|
| `tests/schemas/valid/relations.sql` | Used by FK resolution tests in `test_database_create.cpp`, `test_database_update.cpp`, `test_lua_runner.cpp`, `test_schema_validator.cpp`, `test_database_errors.cpp`, `test_c_api_database_read.cpp`, `test_c_api_database_lifecycle.cpp` |

## Documentation Cleanup (CLAUDE.md)

Specific lines/sections to modify in `CLAUDE.md`:

1. **Architecture > Source file list:** Remove `database_relations.cpp  # Relation operations` (line 32 area)
2. **Test Organization:** Remove `- \`test_database_relations.cpp\` - relation operations` (line 93 area)
3. **Naming Convention examples:** Remove `update_scalar_relation` from examples (line 147 area)
4. **Error Handling examples:** Remove `"Cannot update_scalar_relation: attribute 'x' is not a foreign key in collection 'Y'"` (line 155 area)
5. **Core API > Database Class:** Remove `- Relations: \`update_scalar_relation()\`, \`read_scalar_relation()\`` (line 365 area)
6. **Cross-Layer Naming table:** Remove the `Relations` row with `update_scalar_relation()` across all 5 columns (line 420 area)

## Common Pitfalls

### Pitfall 1: Losing FK Resolution Test Coverage
**What goes wrong:** Deleting `test_database_relations.cpp` without relocating the 9 FK resolution tests loses coverage for the core v1.0 feature.
**Why it happens:** The file name says "relations" so it seems safe to delete entirely, but it contains create_element FK tests added during Phase 1 and Phase 2.
**How to avoid:** Relocate lines 187-421 to `test_database_create.cpp` BEFORE deleting the file.
**Warning signs:** After deletion, `test_database_create.cpp` has no tests with "Fk" or "FK" in the name.

### Pitfall 2: Stale Generated Bindings
**What goes wrong:** Build fails in Julia or Dart because generated FFI files still reference deleted C functions.
**Why it happens:** Deleting the C header declarations without regenerating the generated bindings leaves dangling references.
**How to avoid:** Run both `generator.bat` scripts after modifying `include/quiver/c/database.h`.
**Warning signs:** Link errors mentioning `quiver_database_update_scalar_relation` or `quiver_database_read_scalar_relation`.

### Pitfall 3: Orphaned Dart Part Directive
**What goes wrong:** Dart compilation fails because `database.dart` still has `part 'database_relations.dart';` but the file is deleted.
**Why it happens:** The `part` directive in `database.dart` is manually maintained, not generated.
**How to avoid:** Remove the `part 'database_relations.dart';` line from `bindings/dart/lib/src/database.dart`.
**Warning signs:** Dart analyzer error about missing part file.

### Pitfall 4: Lua Registration Block Removal
**What goes wrong:** Compilation error in `lua_runner.cpp` if the relation registration is partially removed, leaving a dangling comma or mismatched group comment.
**Why it happens:** The sol2 registration uses a long variadic list. Removing entries from the middle requires careful comma handling.
**How to avoid:** Remove the entire "Group 6: Relations" block (lines 256-268) including the group comment, and also remove the `read_scalar_relation_to_lua` helper function and its section comment (lines 1036-1051).
**Warning signs:** C++ compilation errors in `lua_runner.cpp` mentioning unexpected tokens.

### Pitfall 5: Incomplete Error Test Removal
**What goes wrong:** Tests reference deleted methods, causing compilation failures.
**Why it happens:** Relation error tests exist in `test_database_errors.cpp` AND `test_c_api_database_lifecycle.cpp`, not just `test_database_relations.cpp`.
**How to avoid:** Remove relation error tests from all three test files: `test_database_relations.cpp` (delete entire file after relocating FK tests), `test_database_errors.cpp` (lines 181-243), and `test_c_api_database_lifecycle.cpp` (lines 259-442).
**Warning signs:** Compilation errors in test files referencing `update_scalar_relation` or `read_scalar_relation`.

## Code Examples

### Relocating FK Tests to test_database_create.cpp

The 9 FK resolution tests should be appended to `test_database_create.cpp` with a section header:

```cpp
// ============================================================================
// FK label resolution in create_element
// ============================================================================

TEST(Database, ResolveFkLabelInSetCreate) {
    // ... (exact copy from test_database_relations.cpp lines 191-220)
}

// ... remaining 8 tests
```

### Removing Lua Registration Block

Before (in `src/lua_runner.cpp`):
```cpp
            // Group 5 last entry...
            },
            // Group 6: Relations
            "update_scalar_relation",
            [](Database& self, ...) { ... },
            "read_scalar_relation",
            [](Database& self, ...) { ... },
            // Group 7: Time series metadata
```

After:
```cpp
            // Group 5 last entry...
            },
            // Group 7: Time series metadata
```

Also remove the helper function:
```cpp
    // ========================================================================
    // Relations
    // ========================================================================

    static sol::table read_scalar_relation_to_lua(...) { ... }
```

### Removing Dart Part Directive

In `bindings/dart/lib/src/database.dart`, remove:
```dart
part 'database_relations.dart';
```

## Validation

The final validation step runs `scripts/build-all.bat` which:
1. Builds C++ core library (`quiver`)
2. Builds C API library (`quiver_c`)
3. Runs C++ tests (`quiver_tests.exe`)
4. Runs C API tests (`quiver_c_tests.exe`)
5. Runs Julia tests (`bindings/julia/test/test.bat`)
6. Runs Dart tests (`bindings/dart/test/test.bat`)

All must pass green. Any reference to `update_scalar_relation` or `read_scalar_relation` in the codebase (outside `.planning/` and `tests/schemas/`) indicates an incomplete cleanup.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection of all 16+ affected files
- `include/quiver/database.h` -- C++ method declarations (lines 47-53)
- `include/quiver/c/database.h` -- C API function declarations (lines 72-84)
- `src/database_relations.cpp` -- Full C++ implementation (78 lines)
- `src/c/database_relations.cpp` -- Full C API wrapper (39 lines)
- `src/lua_runner.cpp` -- Lua registration block (lines 256-268) and helper (lines 1036-1051)
- `bindings/julia/src/database_create.jl` -- Julia update wrapper (lines 19-28)
- `bindings/julia/src/database_read.jl` -- Julia read wrapper (lines 1-24)
- `bindings/dart/lib/src/database_relations.dart` -- Full Dart extension (66 lines)
- `tests/test_database_relations.cpp` -- Mixed content: relation tests (1-186) + FK resolution tests (187-421)
- `CLAUDE.md` -- 6 reference locations identified

## Metadata

**Confidence breakdown:**
- File inventory: HIGH -- Every affected file verified by grep across entire codebase
- Removal scope: HIGH -- Each reference traced and line numbers confirmed
- FK test relocation: HIGH -- Verified FK tests exist ONLY in `test_database_relations.cpp`, not duplicated elsewhere
- FFI regeneration: HIGH -- Generator configs (`ffigen.yaml`, `generator.toml`) read from C headers confirmed

**Research date:** 2026-02-23
**Valid until:** Indefinite (internal codebase facts, not external library versions)
