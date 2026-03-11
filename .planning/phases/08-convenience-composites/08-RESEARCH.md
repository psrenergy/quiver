# Phase 8: Convenience Composites - Research

**Researched:** 2026-03-11
**Domain:** JS binding convenience composite read methods (pure JS composition of existing FFI primitives)
**Confidence:** HIGH

## Summary

Phase 8 implements three convenience composite methods for the JS binding: `readScalarsById`, `readVectorsById`, and `readSetsById`. These are **binding-only** convenience methods that compose existing primitives (`listScalarAttributes` + typed `readScalar*ById` calls, `listVectorGroups` + typed `readVector*ById` calls, etc.). No new C API or C++ code is required.

All four other bindings (Julia, Dart, Python, Lua) already implement these identically. The pattern is mechanical: list the metadata for a collection, iterate attributes/columns, switch on `dataType` to call the correct typed reader, and return a key-value dictionary. The JS implementation will follow the same pattern using the already-implemented metadata and read-by-id methods from Phases 2 and 3.

**Primary recommendation:** Implement three methods in a new `composites.ts` file following the identical pattern from Dart/Julia/Python. Use the existing `ScalarMetadata.dataType` numeric field (matching C enum values 0=INTEGER, 1=FLOAT, 2=STRING, 3=DATE_TIME) to dispatch. Since JS has no native DateTime type, DATE_TIME columns should be read as strings (same as `readScalarStringById`).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| JSCONV-01 | User can read all scalars/vectors/sets for an element by ID | Identical pattern exists in Julia, Dart, Python, Lua. Pure JS composition of `listScalarAttributes` + `readScalar*ById`, `listVectorGroups` + `readVector*ById`, `listSetGroups` + `readSet*ById`. All underlying primitives already exist from Phases 2-3. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:test | built-in | Test framework | Already used by all JS binding tests |
| bun:ffi | built-in | FFI calls | Already used -- but composite methods do NOT need direct FFI calls |

### Supporting
No new dependencies needed. These methods are pure JS composition of already-implemented methods.

## Architecture Patterns

### Recommended Project Structure
```
bindings/js/src/
  composites.ts        # NEW: readScalarsById, readVectorsById, readSetsById
  index.ts             # Add: import "./composites"
bindings/js/test/
  composites.test.ts   # NEW: Tests for all three composite methods
```

### Pattern 1: Composite Method via Metadata Dispatch
**What:** List metadata, iterate attributes, switch on dataType, call typed reader, build dictionary
**When to use:** For all three composite methods
**Example:**
```typescript
// Follows identical pattern from Dart/Julia/Python/Lua
Database.prototype.readScalarsById = function (
  this: Database,
  collection: string,
  id: number,
): Record<string, number | string | null> {
  const result: Record<string, number | string | null> = {};
  for (const attr of this.listScalarAttributes(collection)) {
    switch (attr.dataType) {
      case 0: // QUIVER_DATA_TYPE_INTEGER
        result[attr.name] = this.readScalarIntegerById(collection, attr.name, id);
        break;
      case 1: // QUIVER_DATA_TYPE_FLOAT
        result[attr.name] = this.readScalarFloatById(collection, attr.name, id);
        break;
      case 2: // QUIVER_DATA_TYPE_STRING
      case 3: // QUIVER_DATA_TYPE_DATE_TIME
        result[attr.name] = this.readScalarStringById(collection, attr.name, id);
        break;
      default:
        throw new Error(`Unsupported data type: ${attr.dataType}`);
    }
  }
  return result;
};
```

### Pattern 2: Vector/Set Composites Iterate Groups Then Columns
**What:** For vectors and sets, iterate groups first, then value columns within each group
**When to use:** `readVectorsById` and `readSetsById`
**Example:**
```typescript
// Source: Dart readVectorsById, Julia read_vectors_by_id (identical pattern)
Database.prototype.readVectorsById = function (
  this: Database,
  collection: string,
  id: number,
): Record<string, number[] | string[]> {
  const result: Record<string, number[] | string[]> = {};
  for (const group of this.listVectorGroups(collection)) {
    for (const col of group.valueColumns) {
      switch (col.dataType) {
        case 0: // INTEGER
          result[col.name] = this.readVectorIntegersById(collection, col.name, id);
          break;
        case 1: // FLOAT
          result[col.name] = this.readVectorFloatsById(collection, col.name, id);
          break;
        case 2: // STRING
        case 3: // DATE_TIME
          result[col.name] = this.readVectorStringsById(collection, col.name, id);
          break;
        default:
          throw new Error(`Unsupported data type: ${col.dataType}`);
      }
    }
  }
  return result;
};
```

### Pattern 3: Data Type Constants
**What:** The C API enum values for `quiver_data_type_t`
**Values:**
- `0` = `QUIVER_DATA_TYPE_INTEGER`
- `1` = `QUIVER_DATA_TYPE_FLOAT`
- `2` = `QUIVER_DATA_TYPE_STRING`
- `3` = `QUIVER_DATA_TYPE_DATE_TIME`
- `4` = `QUIVER_DATA_TYPE_NULL`

The `ScalarMetadata.dataType` field already returns these as plain numbers from `metadata.ts`. The composite methods switch on these numeric values.

### Pattern 4: DATE_TIME Handling in JS
**What:** JS has no standard immutable DateTime type. Unlike Dart (DateTime) and Julia (DateTime), JS treats DATE_TIME columns as strings.
**Why:** All other bindings with native DateTime types (Dart, Julia, Python) parse DATE_TIME to their native type. JS/Bun lacks a standard one. Treating DATE_TIME as STRING is consistent with how `readScalarStringById` already works for date columns.
**How:** In the switch statement, `case 3` (DATE_TIME) falls through to `case 2` (STRING) to read via `readScalar/Vector/SetStringById`.

### Anti-Patterns to Avoid
- **Direct FFI calls in composites:** These methods should NOT call C API functions directly. They compose existing JS methods only.
- **Importing getSymbols/check/ptr:** Not needed -- composites call `this.listScalarAttributes()`, `this.readScalarIntegerById()` etc. which already handle FFI.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Metadata iteration | Custom SQL queries against schema | `listScalarAttributes()`, `listVectorGroups()`, `listSetGroups()` | Already implemented and tested in Phase 3 |
| Typed dispatch | Manual column type detection | `ScalarMetadata.dataType` numeric field | C API already provides type information |
| Data type enum | Custom constants | Numeric literals 0/1/2/3 matching C enum | All bindings use the same C enum values |

## Common Pitfalls

### Pitfall 1: Forgetting to Handle All Data Types
**What goes wrong:** Missing a case in the switch statement
**Why it happens:** The C enum has 5 values (INTEGER, FLOAT, STRING, DATE_TIME, NULL) but only 4 are relevant for column types
**How to avoid:** Include a `default` case that throws an error, matching the pattern in all other bindings
**Warning signs:** Missing DATE_TIME case (would leave date columns unread)

### Pitfall 2: Using Wrong Group Iteration for Vectors/Sets
**What goes wrong:** Iterating groups but not nested value columns
**Why it happens:** Scalars iterate `listScalarAttributes()` directly (flat list), but vectors/sets need two levels: group then valueColumns
**How to avoid:** Follow the Dart/Julia pattern exactly: `for group of listVectorGroups` -> `for col of group.valueColumns`
**Warning signs:** Only one column per group appearing in results

### Pitfall 3: Module Augmentation Not Imported
**What goes wrong:** Methods exist but TypeScript does not see them on the Database type
**Why it happens:** The `declare module "./database"` augmentation only takes effect when the file is imported
**How to avoid:** Add `import "./composites"` to `index.ts`, following the same pattern as all other modules
**Warning signs:** TypeScript compilation errors about missing methods

### Pitfall 4: Test Schema Selection
**What goes wrong:** Using wrong schema for tests
**Why it happens:** Different schemas have different attribute layouts
**How to avoid:** Use `basic.sql` for readScalarsById (has integer, float, string, date scalar attributes), `composite_helpers.sql` for readVectorsById and readSetsById (has integer, float, string vector/set groups), matching what Dart and Julia tests use
**Warning signs:** Tests pass but don't actually exercise all type dispatch branches

## Code Examples

### Complete readScalarsById Implementation
```typescript
// Source: Verified pattern from Julia database_read.jl:372-389, Dart database_read.dart:795-815
import { Database } from "./database";

declare module "./database" {
  interface Database {
    readScalarsById(collection: string, id: number): Record<string, number | string | null>;
    readVectorsById(collection: string, id: number): Record<string, number[] | string[]>;
    readSetsById(collection: string, id: number): Record<string, number[] | string[]>;
  }
}

Database.prototype.readScalarsById = function (
  this: Database,
  collection: string,
  id: number,
): Record<string, number | string | null> {
  const result: Record<string, number | string | null> = {};
  for (const attr of this.listScalarAttributes(collection)) {
    switch (attr.dataType) {
      case 0: result[attr.name] = this.readScalarIntegerById(collection, attr.name, id); break;
      case 1: result[attr.name] = this.readScalarFloatById(collection, attr.name, id); break;
      case 2:
      case 3: result[attr.name] = this.readScalarStringById(collection, attr.name, id); break;
      default: throw new Error(`Unsupported scalar data type: ${attr.dataType}`);
    }
  }
  return result;
};
```

### Test Pattern (readVectorsById)
```typescript
// Source: Verified pattern from Dart database_read_test.dart:1056-1084, Julia test_database_read.jl:614-641
describe("readVectorsById", () => {
  test("returns all vector groups with correct types", () => {
    const db = Database.fromSchema(":memory:", COMPOSITE_SCHEMA_PATH);
    try {
      const id = db.createElement("Items", {
        label: "Item 1",
        amount: [10, 20, 30],
        score: [1.1, 2.2],
        note: ["hello", "world"],
      });

      const result = db.readVectorsById("Items", id);

      expect(Object.keys(result).length).toBe(3);
      expect(result["amount"]).toEqual([10, 20, 30]);
      expect(result["score"]).toEqual([1.1, 2.2]);
      expect(result["note"]).toEqual(["hello", "world"]);
    } finally {
      db.close();
    }
  });
});
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| N/A | Composite methods are binding-only | Established pattern from v0.1 | No C/C++ changes ever needed |

All four existing bindings use the same pattern. This is a stable, proven approach.

## Open Questions

None. The pattern is fully established across four existing bindings. The JS implementation is a straightforward port.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in) |
| Config file | none (bun:test uses conventions) |
| Quick run command | `cd bindings/js && bun test test/composites.test.ts` |
| Full suite command | `bindings/js/test/test.bat` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| JSCONV-01a | readScalarsById returns dict of all scalar values by type | unit | `cd bindings/js && bun test test/composites.test.ts` | No -- Wave 0 |
| JSCONV-01b | readVectorsById returns dict of all vector column arrays | unit | `cd bindings/js && bun test test/composites.test.ts` | No -- Wave 0 |
| JSCONV-01c | readSetsById returns dict of all set column arrays | unit | `cd bindings/js && bun test test/composites.test.ts` | No -- Wave 0 |

### Sampling Rate
- **Per task commit:** `cd bindings/js && bun test test/composites.test.ts`
- **Per wave merge:** `bindings/js/test/test.bat`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/composites.test.ts` -- covers JSCONV-01a/b/c
- [ ] `bindings/js/src/composites.ts` -- implementation file

No framework install needed -- bun:test is built-in. No new test schemas needed -- `basic.sql` and `composite_helpers.sql` already exist in `tests/schemas/valid/`.

## Sources

### Primary (HIGH confidence)
- `bindings/julia/src/database_read.jl` lines 372-431 -- Reference implementation of read_scalars_by_id, read_vectors_by_id, read_sets_by_id
- `bindings/dart/lib/src/database_read.dart` lines 795-869 -- Dart readScalarsById, readVectorsById, readSetsById
- `bindings/python/src/quiverdb/database.py` lines 1263-1335 -- Python read_scalars_by_id, read_vectors_by_id, read_sets_by_id
- `src/lua_runner.cpp` lines 566-685 -- Lua C++ implementation of composites
- `include/quiver/c/database.h` lines 22-26 -- quiver_data_type_t enum values
- `bindings/js/src/metadata.ts` -- Existing ScalarMetadata/GroupMetadata types with dataType field
- `bindings/js/src/read.ts` -- All existing typed read-by-id methods
- `tests/schemas/valid/composite_helpers.sql` -- Test schema for vector/set composite tests
- `tests/schemas/valid/basic.sql` -- Test schema for scalar composite tests

### Secondary (MEDIUM confidence)
- `bindings/dart/test/database_read_test.dart` lines 1056-1115 -- Test patterns for composite methods
- `bindings/julia/test/test_database_read.jl` lines 614-670 -- Julia test patterns for composites

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new libraries, pure composition of existing methods
- Architecture: HIGH - identical pattern proven across 4 bindings, straightforward port
- Pitfalls: HIGH - known from existing implementations, well-documented in cross-binding patterns

**Research date:** 2026-03-11
**Valid until:** Indefinite (pattern is stable, no external dependencies)
