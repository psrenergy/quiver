# Project Research Summary

**Project:** Quiver — FK Label Resolution in create_element / update_element
**Domain:** SQLite wrapper library internal pattern extension (C++20, multi-layer)
**Researched:** 2026-02-23
**Confidence:** HIGH

## Executive Summary

This milestone adds foreign key label resolution to `create_element` and `update_element`. Instead of callers looking up integer IDs before writing, they pass string labels (e.g., `element.set("parent_id", "Parent 1")`) and the C++ layer resolves them transparently. No new libraries, no new external dependencies, and no changes to the C API or any binding are required. Every building block exists: schema FK metadata via `TableDefinition::foreign_keys`, the resolution SQL pattern proven in `database_create.cpp:151-167` (set path) and `database_relations.cpp:30-36`, and the TypeValidator already permitting strings for INTEGER columns with an explicit "FK label resolution" comment. The work is purely about applying the proven set FK pattern to the scalar and vector paths in both `create_element` and `update_element`, then extracting a shared `resolve_fk_label()` helper to eliminate duplication.

The recommended approach is a strict three-step sequence inside each write method: (1) validate types with the existing TypeValidator, (2) resolve ALL FK labels before any SQL execution, (3) build and execute SQL with fully-resolved values. This ordering is critical: resolving before any writes ensures the operation is atomic within user-controlled transactions (where `TransactionGuard` is a no-op), and resolving after validation keeps TypeValidator independent of FK metadata. A single `resolve_fk_label()` helper on `Database::Impl` should be the sole resolution entry point, called from all five affected sites (scalar create, vector create, set create refactor, scalar update, vector update, set update).

Key risks are contained. The most critical: the TypeValidator currently allows strings for ALL INTEGER columns (not just FK columns), so the resolution step must also reject strings for non-FK INTEGER columns with a clean Quiver error rather than letting a raw SQLite STRICT mode error surface. Self-referential FK creation is an expected limitation (documented, not a bug). The `update_scalar_relation` method should not be removed during this milestone because its label-to-label access pattern is genuinely different from `update_element`'s ID-based approach.

## Key Findings

### Recommended Stack

No new dependencies are needed. The entire implementation uses existing infrastructure: SQLite3's `PRAGMA foreign_key_list` (already loaded at schema init via `Schema::query_foreign_keys()`), C++20's `std::variant` and `std::holds_alternative` (already used in the existing set FK path), and spdlog for debug logging (already integrated). The `ForeignKey` struct on `TableDefinition` already provides `from_column`, `to_table`, and `to_column` for every table.

**Core technologies (no version changes):**
- `TableDefinition::foreign_keys` (`include/quiver/schema.h:43`) — provides FK metadata for every table, populated at open; no changes needed
- `TypeValidator::validate_value()` (`src/type_validator.cpp:42-48`) — already allows `std::string` for `DataType::Integer`; no changes needed
- `Database::execute()` (private method) — parameterized SQL execution already used by the existing set FK resolution path

### Expected Features

**Must have (table stakes):**
- Scalar FK resolution in `create_element` — core use case; `element.set("parent_id", "Parent 1")` without manual ID lookup
- Scalar FK resolution in `update_element` — symmetric with create; currently zero FK resolution exists in the entire update path
- Vector FK resolution in `create_element` — consistency with sets (create set path already works today)
- Vector FK resolution in `update_element` — closes the update gap for vector FK columns
- Set FK resolution in `update_element` — the only missing piece in the create/update matrix (create set works, update set does not)
- Error on missing target label: `"Failed to resolve label 'X' to ID in table 'Y'"` — fail-fast with clean Quiver error
- NULL FK passthrough: `nullptr_t` variant skips resolution naturally via `holds_alternative<string>` guard

**Should have (differentiators):**
- Extract shared `resolve_fk_label()` helper on `Database::Impl` — eliminates 5x code duplication; makes all future maintenance safe
- Reject string-for-non-FK-INTEGER with clean error — prevents raw SQLite STRICT mode errors from surfacing to callers

**Defer (v2+):**
- Remove `update_scalar_relation` — label-to-label access pattern is different from `update_element`'s ID-based approach; defer until verified redundant
- FK resolution in time series columns — no current schema uses FK in time series tables; out of scope per PROJECT.md
- Batch label resolution optimization (single `WHERE label IN (...)` query per vector) — useful for large arrays; not required for correct behavior

### Architecture Approach

The implementation modifies two translation units (`database_create.cpp`, `database_update.cpp`) with one new shared helper method added to `Database::Impl` in `database_impl.h`. The helper comes first (no behavioral change, existing tests still pass), then the existing set FK inline block is refactored to use it, then scalar and vector paths are added. All of this is then mirrored in `update_element`. Zero changes to the C API, bindings, `Element`, `Schema`, or `TypeValidator`.

**Major components:**
1. `Database::Impl::resolve_fk_label()` (new, `database_impl.h`) — single resolution entry point; iterates `table_def.foreign_keys`, executes `SELECT id FROM {table} WHERE label = ?`, returns resolved `int64_t` or original value unchanged for non-FK or non-string values
2. `database_create.cpp` (modify) — add scalar and vector FK resolution loops using helper; refactor existing set FK inline block to use helper
3. `database_update.cpp` (modify) — add scalar, vector, and set FK resolution to `update_element`; exact mirror of `create_element` approach

### Critical Pitfalls

1. **TypeValidator allows strings for ALL INTEGER columns, not just FK columns** — the resolution step must explicitly throw `"Cannot create_element: attribute 'X' is INTEGER but received string and is not a foreign key"` for non-FK INTEGER columns. Never let this fall through to a raw SQLite STRICT mode error.

2. **Resolution must happen BEFORE any SQL execution (pre-resolve pass, not inline)** — within a user-controlled `begin_transaction()`, `TransactionGuard` is a no-op. If resolution fails after scalars are written but before vector rows are written, the scalar row persists. Pre-resolve all FK labels before any writes to ensure zero writes happen on any resolution failure.

3. **update_element has zero FK resolution today** — the entire scalar/vector/set path in `database_update.cpp` writes values directly to SQLite. Strings for FK INTEGER columns produce raw STRICT mode errors. All three paths need resolution added.

4. **Self-referential FK creation is a documented limitation, not a bug** — when creating a new element with a self-reference (e.g., `sibling_id = "Child 1"` in the `Child` table), resolution runs BEFORE INSERT, so the target does not yet exist. The correct behavior is to throw. The two-step pattern (create first, then update) is the answer. Document this explicitly.

5. **Inconsistent error messages between `update_scalar_relation` and `create_element` set FK path** — `update_scalar_relation` throws `"Target element not found: ..."` while the set path throws `"Failed to resolve label 'X' to ID in table 'Y'"`. The shared helper enforces the canonical message; `update_scalar_relation` should be updated to match.

## Implications for Roadmap

Based on research, the natural phase structure follows the shared-helper-first dependency.

### Phase 1: Foundation — Shared Resolution Helper
**Rationale:** The helper is the dependency for all subsequent phases. Implementing and validating it first (with the existing set FK refactor as proof) ensures no behavioral change lands while establishing the shared pattern. This is the lowest-risk change in the milestone.
**Delivers:** `Database::Impl::resolve_fk_label()` in `database_impl.h`; refactored set FK resolution in `create_element` using the helper (existing tests must still pass with no behavioral change)
**Addresses:** Extract shared helper (differentiator feature); consistent error messages (Pitfall 11)
**Avoids:** Anti-pattern of duplicating the 17-line inline FK block five more times across the codebase

### Phase 2: create_element FK Resolution — Scalar and Vector
**Rationale:** `create_element` already has the set FK path working. Scalar and vector are independent additions now that the shared helper exists. Completing the create_element matrix before touching update_element provides a clean reference implementation.
**Delivers:** `create_element` resolves FK labels for scalar and vector columns in addition to the existing set column support; integration tests for scalar FK (including self-ref failure case) and vector FK creation
**Addresses:** Scalar FK in create_element, vector FK in create_element (both table stakes)
**Avoids:** Pitfall 2 (resolution after param building); Pitfall 4 (pre-resolve before SQL); Pitfall 1 (string-for-non-FK-INTEGER handled in resolver)

### Phase 3: update_element FK Resolution — All Column Types
**Rationale:** `update_element` currently has zero FK resolution. Mirror the create_element approach for all three column types in one phase since the pattern is identical and the tests are symmetric with Phase 2.
**Delivers:** `update_element` resolves FK labels for scalar, vector, and set columns; integration tests mirroring Phase 2 tests
**Addresses:** Scalar FK in update_element, vector FK in update_element, set FK in update_element (all table stakes)
**Avoids:** Pitfall 3 (entire update path missing FK resolution); Pitfall 4 (pre-resolve before UPDATE SQL)

### Phase 4: Validation and Edge Case Hardening
**Rationale:** After the happy paths work, add the guardrails that make the feature production-grade. All edge cases are fully enumerated in PITFALLS.md, so this is a focused test-and-assert phase, not an exploratory one.
**Delivers:** Clean Quiver errors for all FK failure modes; test coverage for NULL passthrough, integer passthrough, missing label, string-for-non-FK-INTEGER, self-ref creation failure; standardized error messages across all FK resolution sites
**Addresses:** NULL FK passthrough (table stakes), error on missing target label (table stakes), reject string-for-non-FK-INTEGER (should have)
**Avoids:** Pitfall 1 (TypeValidator bypass for non-FK INTEGER); Pitfall 5 (self-ref must fail clearly); Pitfall 9 (NULL handling); Pitfall 11 (error message inconsistency)

### Phase 5: Evaluate update_scalar_relation Redundancy
**Rationale:** Only after all FK resolution paths are complete and tested does the question of redundancy become answerable. The label-to-label access pattern in `update_scalar_relation` is different from `update_element`'s id-based approach, so removal is not automatic.
**Delivers:** A clear decision: keep, remove, or keep with documentation noting the preferred alternative
**Addresses:** Differentiator #2 (API simplification) — conditionally
**Avoids:** Pitfall 6 (premature removal leaving users without a label-to-label update path)

### Phase Ordering Rationale

- Phase 1 before all others: the shared helper is the prerequisite for all downstream phases; the set FK refactor confirms it works before new call sites are added
- Phase 2 before Phase 3: completing create_element first provides a working reference for update_element; both phases use identical patterns so the second is faster to write
- Phase 4 after Phases 2-3: edge case testing is cleanest once the happy path has established test baseline behavior
- Phase 5 last: redundancy evaluation only makes sense when all paths are complete and tested

### Research Flags

Phases with standard patterns (no research-phase needed):
- **Phase 1:** Direct extraction of existing inline code; all design decisions resolved; no unknowns
- **Phase 2:** Mechanical application of the set FK pattern to scalar and vector paths; exact line numbers and code snippets provided in ARCHITECTURE.md
- **Phase 3:** Mirror of Phase 2 in a different file; same pattern, same tests
- **Phase 4:** All edge cases enumerated in PITFALLS.md; no discovery required

Phases that may benefit from a brief review before execution:
- **Phase 5:** Evaluate whether any callers depend on `update_scalar_relation`'s label-to-label access pattern before removing; a quick grep of binding tests and docs suffices

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All verified by direct source reading; zero external dependencies; all infrastructure confirmed to exist and work |
| Features | HIGH | Derived from PROJECT.md active items + ORM ecosystem survey for context; all five table-stakes features have fully specified implementation paths |
| Architecture | HIGH | Exact file locations, line numbers, and code snippets provided for every integration point; component boundaries confirmed; no design ambiguity remains |
| Pitfalls | HIGH | All 14 pitfalls derived from direct code reading; TypeValidator bypass, transaction atomicity, and update_element gap verified against actual source |

**Overall confidence:** HIGH

### Gaps to Address

- **Double values for FK INTEGER columns:** `TypeValidator` currently allows `double` for `DataType::Integer` (general numeric coercion). If a user passes `element.set("parent_id", 3.14)`, it passes validation but arrives at the resolution step as a `double` variant. The resolution helper must decide: pass through (treating as a raw numeric ID via truncation) or reject. Recommendation: pass through via `static_cast<int64_t>` since `double` is an accepted INTEGER type per the existing TypeValidator rule. Confirm this during Phase 1 implementation.
- **Batch resolution performance:** Per-row label lookups (N SELECT queries for N-element FK vector) noted in Pitfall 8. Not required for correctness. Re-evaluate after Phase 3 if any benchmark shows degradation on large vector FK arrays.

## Sources

### Primary (HIGH confidence — direct source code analysis)
- `src/database_create.cpp` — existing set FK resolution pattern (lines 151-167), scalar insert (lines 24-41), vector insert (lines 83-118)
- `src/database_update.cpp` — update_element with no FK resolution; typed update methods (lines 166-353)
- `src/type_validator.cpp` — string-to-INTEGER allowance (lines 42-48) with "FK label resolution" comment
- `include/quiver/schema.h` — ForeignKey struct, TableDefinition with foreign_keys vector
- `src/database_impl.h` — Impl struct layout, TransactionGuard nest-aware behavior
- `src/database_relations.cpp` — update_scalar_relation label-to-label pattern; inconsistent error message
- `src/schema.cpp` — PRAGMA foreign_key_list loading (lines 366-396)
- `tests/schemas/valid/relations.sql` — FK schema covering scalar, vector, set, and self-referential cases
- `bindings/julia/src/element.jl` — Julia type dispatch for string values
- `bindings/dart/lib/src/element.dart` — Dart type dispatch for string values
- `.planning/PROJECT.md` — milestone scope, out-of-scope items, key decisions

### Secondary (MEDIUM confidence — ecosystem context)
- [Basic Relationship Patterns — SQLAlchemy 2.0](https://docs.sqlalchemy.org/en/20/orm/basic_relationships.html) — ORM FK assignment patterns for context
- [Active Record Associations — Ruby on Rails Guides](https://guides.rubyonrails.org/association_basics.html) — object assignment pattern; contrast with Quiver's label approach
- [How to Use a Foreign Key in Django](https://www.freecodecamp.org/news/how-to-use-a-foreign-key-in-django/) — Django FK assignment patterns for context

---
*Research completed: 2026-02-23*
*Ready for roadmap: yes*
