# Phase 4: Cleanup - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Evaluate and act on the redundancy of `update_scalar_relation` and `read_scalar_relation` now that FK label resolution works through `create_element` and `update_element`. Remove both methods and all traces across every layer.

</domain>

<decisions>
## Implementation Decisions

### Fate of update_scalar_relation and read_scalar_relation
- **Remove both methods entirely** — `update_element` with FK resolution covers the important case; the label-to-label convenience of `update_scalar_relation` is not worth keeping
- `read_scalar_relation` goes too — users can query FK target labels with `query_string` if needed
- Clean sweep: remove from C++, C API, Julia, Dart, and Lua

### Source file cleanup
- Delete `src/database_relations.cpp` and `src/c/database_relations.cpp` entirely (both will be empty after removal)
- Delete `bindings/dart/lib/src/database_relations.dart` entirely
- Delete all binding relation source files in Julia
- Remove relation method registration from `LuaRunner` in `src/lua_runner.cpp`
- Remove declarations from `include/quiver/database.h` and `include/quiver/c/database.h`
- Update CMakeLists to remove deleted source files

### Test cleanup
- Delete `tests/test_database_relations.cpp` and all binding relation test files entirely
- FK resolution is already tested in create/update test files — no coverage gap
- **Keep** `tests/schemas/valid/relations.sql` — still used by create/update FK resolution tests

### Generator and FFI cleanup
- Remove relation function declarations from Julia and Dart generator templates
- **Regenerate FFI bindings** by running both `generator.bat` scripts after C API changes
- Verify regenerated bindings compile

### Documentation cleanup
- Full cleanup of CLAUDE.md: remove `update_scalar_relation` and `read_scalar_relation` from cross-layer naming tables, API descriptions, and any other references
- Remove the "Relations" row from binding convenience method tables where it refers to these methods

### Error message alignment
- **Settled automatically** — removing the methods removes their non-standard error messages ("Cannot update_scalar_relation", "Target element not found")
- Remaining FK resolution errors already follow project patterns ("Failed to resolve label", "Cannot resolve attribute")

### Validation
- Run full `scripts/build-all.bat` after all changes to confirm nothing broke across all layers (C++, C API, Julia, Dart)

</decisions>

<specifics>
## Specific Ideas

No specific requirements — the removal is mechanical and comprehensive. Every trace of both methods should be gone.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-cleanup*
*Context gathered: 2026-02-23*
