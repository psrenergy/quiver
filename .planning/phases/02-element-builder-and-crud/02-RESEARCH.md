# Phase 2: Element Builder and CRUD - Research

**Researched:** 2026-03-08
**Domain:** bun:ffi element lifecycle, typed value marshaling, CRUD operations
**Confidence:** HIGH

## Summary

Phase 2 adds createElement and deleteElement to the JS/TS binding. The core challenge is marshaling JavaScript plain objects (with typed scalar and array values) into the C Element API, which requires creating an opaque `quiver_element_t` handle, calling typed setter functions for each field, passing it to `quiver_database_create_element`, reading the returned int64 ID, and destroying the element handle -- all through bun:ffi pointer operations.

The Element class is internal (not exported), matching the Python binding pattern where `create_element(**kwargs)` builds and destroys the Element behind the scenes. The JavaScript API is `db.createElement('Collection', { label: 'Sword', damage: 42, tags: ['fire'] })` with a `Record<string, Value>` plain object. Array type detection uses first-element inspection (same as Python), and number-vs-integer disambiguation uses `Number.isInteger()`. The `null` value maps to `quiver_element_set_null`, `undefined` skips the field, and unsupported types throw `QuiverError`.

The FFI mechanics are well-understood from Phase 1. New challenges specific to Phase 2 are: (1) building a `char**` (array of string pointers) for `quiver_element_set_array_string` -- requires a buffer holding N pointer-sized values pointing to N null-terminated strings; (2) passing `int64_t*` and `double*` typed arrays for integer and float array setters; (3) reading the `int64_t* out_id` from `quiver_database_create_element`; (4) the prototype extension pattern with `declare module` for adding methods to Database from a separate file.

**Primary recommendation:** Use the Phase 1 ffi-helpers (`toCString`, `allocPointerOut`, `readPointerOut`, `check`) for all C API calls. For integer arrays, use `BigInt64Array` with `ptr()`. For float arrays, use `Float64Array` with `ptr()`. For string arrays, manually build a pointer table using `BigInt64Array` holding `ptr()` values of individual `Buffer` strings. The element handle lifecycle is create-before-call, destroy-in-finally.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Plain object API: `db.createElement('Items', { label: 'Sword', damage: 42, tags: ['fire'] })`
- Element class is internal (not exported) -- same approach as Python binding
- createElement accepts `Record<string, Value>` with a typed `Value` union
- Types live in `types.ts` (reusable by future phases); createElement/deleteElement in `create.ts`
- Methods added to Database via prototype extension with `declare module` type augmentation
- Auto-detect type from first element: number -> check isInteger, string -> string array, bigint -> integer array
- number[] where ALL values pass `Number.isInteger()` -> integer array; ANY decimal -> float array
- bigint[] -> integer array
- Empty arrays accepted: pass count=0 to `quiver_element_set_array_integer` with null pointer (matches Python behavior)
- `null` -> `quiver_element_set_null(name)` (explicit null)
- `undefined` -> skipped entirely (field not passed to C API)
- Scalar number: `Number.isInteger(v)` -> set_integer, else -> set_float
- bigint -> set_integer
- string -> set_string
- Unsupported types (boolean, object, Date, nested arrays) -> throw QuiverError with message: "Unsupported value type for '{field}': {typeof}"
- createElement returns `number` (plain JS number, not bigint -- SQLite AUTOINCREMENT IDs fit in MAX_SAFE_INTEGER)
- deleteElement returns `void` -- error behavior delegated to C++ layer (QuiverError propagates if C++ throws)
- deleteElement accepts `number` only (consistent with createElement return type)

### Claude's Discretion
- Internal Element class lifecycle management (create/destroy around each createElement call)
- FFI marshaling details for arrays (buffer allocation, pointer management)
- New FFI symbol definitions to add to loader.ts
- Test schema choice and test structure
- ensureOpen() guard placement

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CRUD-01 | User can build elements with integer, float, string, and null scalar values | Element handle lifecycle + typed setter FFI calls (set_integer, set_float, set_string, set_null) |
| CRUD-02 | User can build elements with integer, float, and string array values | Typed array marshaling: BigInt64Array for int64[], Float64Array for double[], pointer table for char** |
| CRUD-03 | User can create an element in a collection and receive its ID | quiver_database_create_element FFI call with int64 out-parameter, Number() conversion |
| CRUD-04 | User can delete an element from a collection by ID | quiver_database_delete_element FFI call with int64 id parameter |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:ffi | Built-in (Bun 1.3.10) | C library function calling | Established in Phase 1; only FFI option for Bun |
| bun:test | Built-in (Bun 1.3.10) | Test runner | Established in Phase 1; Jest-compatible API |
| TypeScript | Built-in (Bun 1.3.10) | Type safety | Established in Phase 1; strict mode enabled |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Phase 1 helpers | N/A (local) | `check()`, `toCString()`, `allocPointerOut()`, `readPointerOut()` | Every C API call |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Manual pointer table for char** | toArrayBuffer + DataView | More complex, no real benefit for small arrays |
| `i64_fast` for out_id return | `i64` (BigInt) | i64_fast returns number directly when value fits 53 bits; better ergonomics since createElement returns `number` |

**Installation:**
```bash
# No new dependencies -- all built-in to Bun
```

## Architecture Patterns

### Recommended Project Structure
```
bindings/js/
  src/
    index.ts          # Exports (add create.ts side-effect import)
    loader.ts         # Add new symbol definitions for element + CRUD
    errors.ts         # Existing (unchanged)
    database.ts       # Existing (unchanged)
    ffi-helpers.ts    # Existing (may add helpers for int64 out-param reading)
    types.ts          # NEW: Value type union, ElementData type alias
    create.ts         # NEW: createElement, deleteElement via prototype extension
  test/
    database.test.ts  # Existing lifecycle tests
    create.test.ts    # NEW: createElement + deleteElement tests
    test.bat          # Existing test runner
```

### Pattern 1: Prototype Extension with `declare module`
**What:** Add methods to Database class from a separate file without modifying database.ts.
**When to use:** createElement and deleteElement live in `create.ts` but must appear on Database instances.
**Example:**
```typescript
// Source: TypeScript module augmentation (official TS docs)
// create.ts
import { Database } from "./database";
import { getSymbols } from "./loader";
import { check } from "./errors";
import { toCString } from "./ffi-helpers";
import type { ElementData } from "./types";

declare module "./database" {
  interface Database {
    createElement(collection: string, data: ElementData): number;
    deleteElement(collection: string, id: number): void;
  }
}

Database.prototype.createElement = function (
  this: Database,
  collection: string,
  data: ElementData,
): number {
  // implementation
};

Database.prototype.deleteElement = function (
  this: Database,
  collection: string,
  id: number,
): void {
  // implementation
};
```

**Critical:** `index.ts` must import `create.ts` as a side-effect import (`import "./create"`) for the prototype extensions to register. Without this import, the methods will not exist at runtime.

### Pattern 2: Internal Element Lifecycle (Create-Use-Destroy)
**What:** Create a temporary `quiver_element_t` handle, populate it, pass to `quiver_database_create_element`, then destroy it in a finally block.
**When to use:** Inside every `createElement` call.
**Example:**
```typescript
// Source: Python binding element.py + database.py pattern
function createElement(this: Database, collection: string, data: ElementData): number {
  const lib = getSymbols();

  // 1. Create element handle
  const outElem = allocPointerOut();
  check(lib.quiver_element_create(ptr(outElem)));
  const elemPtr = readPointerOut(outElem);

  try {
    // 2. Set each field on the element
    for (const [name, value] of Object.entries(data)) {
      if (value === undefined) continue;  // skip undefined
      setElementField(lib, elemPtr, name, value);
    }

    // 3. Call create_element to insert into database
    const outId = new BigInt64Array(1);
    check(lib.quiver_database_create_element(
      this._handle,
      toCString(collection),
      elemPtr,
      ptr(outId),
    ));

    // 4. Read and return the ID as number
    return Number(outId[0]);
  } finally {
    // 5. Always destroy element handle
    lib.quiver_element_destroy(elemPtr);
  }
}
```

### Pattern 3: Type-Dispatched Field Setting
**What:** Route each JS value to the correct `quiver_element_set_*` function based on its type.
**When to use:** For each field in the ElementData object.
**Example:**
```typescript
// Source: Python element.py set() method logic
function setElementField(lib: Symbols, elemPtr: number, name: string, value: Value): void {
  const cName = toCString(name);

  if (value === null) {
    check(lib.quiver_element_set_null(elemPtr, cName));
  } else if (typeof value === "bigint") {
    check(lib.quiver_element_set_integer(elemPtr, cName, value));
  } else if (typeof value === "number") {
    if (Number.isInteger(value)) {
      check(lib.quiver_element_set_integer(elemPtr, cName, value));
    } else {
      check(lib.quiver_element_set_float(elemPtr, cName, value));
    }
  } else if (typeof value === "string") {
    check(lib.quiver_element_set_string(elemPtr, cName, toCString(value)));
  } else if (Array.isArray(value)) {
    setElementArray(lib, elemPtr, name, value);
  } else {
    throw new QuiverError(`Unsupported value type for '${name}': ${typeof value}`);
  }
}
```

### Pattern 4: Array Marshaling
**What:** Convert JS arrays to C typed arrays and pass to element array setters.
**When to use:** When a field value is an array.
**Example:**
```typescript
// Source: Python element.py _set_array methods + bun:ffi docs
function setElementArray(lib: Symbols, elemPtr: number, name: string, values: unknown[]): void {
  const cName = toCString(name);

  if (values.length === 0) {
    // Empty array: pass null pointer with count=0
    check(lib.quiver_element_set_array_integer(elemPtr, cName, 0, 0));
    return;
  }

  const first = values[0];

  if (typeof first === "bigint" || (typeof first === "number" && Number.isInteger(first))) {
    // Integer array
    const arr = BigInt64Array.from(values as (number | bigint)[], v => BigInt(v));
    check(lib.quiver_element_set_array_integer(elemPtr, cName, ptr(arr), values.length));
  } else if (typeof first === "number") {
    // Float array (first element is non-integer number)
    const arr = new Float64Array(values as number[]);
    check(lib.quiver_element_set_array_float(elemPtr, cName, ptr(arr), values.length));
  } else if (typeof first === "string") {
    // String array: build pointer table
    const buffers = (values as string[]).map(s => toCString(s));
    const ptrTable = new BigInt64Array(buffers.length);
    for (let i = 0; i < buffers.length; i++) {
      ptrTable[i] = BigInt(ptr(buffers[i]));
    }
    check(lib.quiver_element_set_array_string(elemPtr, cName, ptr(ptrTable), values.length));
  } else {
    throw new QuiverError(`Unsupported array element type for '${name}': ${typeof first}`);
  }
}
```

### Pattern 5: Integer Array Detection (Number.isInteger)
**What:** For `number[]`, check ALL values with `Number.isInteger()` to determine integer vs float.
**When to use:** When first element is a number (could be integer or float).
**Important:** Per CONTEXT.md decisions: "number[] where ALL values pass `Number.isInteger()` -> integer array; ANY decimal -> float array". However, the simpler approach (check first element only) is what Python does and was also discussed. The locked decision says to check first element type, then for number arrays: all must be integer for integer array. In practice, checking the first element is sufficient for the first-pass type dispatch, but a full scan may be needed for number arrays to ensure correctness.
**Example:**
```typescript
// For number arrays, first element determines dispatch path:
// If first is integer, treat as integer array
// If first is non-integer, treat as float array
// This matches the Python binding behavior (check first element)
if (typeof first === "number" && Number.isInteger(first)) {
  // Integer array path
} else if (typeof first === "number") {
  // Float array path
}
```

**Note on number[] handling:** The CONTEXT.md states "number[] where ALL values pass Number.isInteger() -> integer array; ANY decimal -> float array". This means for a `number[]` where the first element is an integer, we should still check remaining elements. If any is decimal, the whole array should be float. This is a stricter check than Python (which only checks the first element).

### Anti-Patterns to Avoid
- **Exporting the Element class:** Element is internal. Users never see it. Only `createElement` and `deleteElement` are public.
- **Forgetting to destroy the element handle:** Always use try/finally. A leaked element handle is a memory leak.
- **Passing JS number directly for int64 args:** `quiver_element_set_integer` takes `int64_t`. bun:ffi `i64` args accept both `number` and `BigInt`, so passing `number` is fine. But the `i64` FFI arg type must be declared correctly in the symbol table.
- **Using `i64` return type for out_id:** This would return BigInt. Use `BigInt64Array` as out-parameter and read with `Number(outId[0])` instead, or use `read.i64()` which also returns BigInt and then convert with `Number()`.
- **Not importing create.ts in index.ts:** Without the side-effect import, prototype extensions don't register and `createElement`/`deleteElement` will be undefined at runtime.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Null-terminated strings | Manual encoding | `toCString()` from ffi-helpers.ts | Already built in Phase 1, tested |
| Error checking | Manual error code inspection | `check()` from errors.ts | Already built in Phase 1, tested |
| Pointer out-parameter | Raw ArrayBuffer math | `allocPointerOut()` + `readPointerOut()` from ffi-helpers.ts | Already built in Phase 1, tested |
| BigInt-to-Number conversion | Manual overflow checks | `Number(bigint)` | SQLite AUTOINCREMENT IDs always fit in MAX_SAFE_INTEGER |

**Key insight:** Phase 1 already built all the FFI infrastructure. Phase 2 consumes it. The new code is type-dispatch logic (JS types -> C API calls) and array marshaling (JS arrays -> C typed arrays). No new FFI primitives needed.

## Common Pitfalls

### Pitfall 1: String Array Pointer Table (char**)
**What goes wrong:** `quiver_element_set_array_string` expects `const char* const* values` -- a pointer to an array of char pointers. Each char pointer must point to a valid null-terminated string. If the Buffer objects are garbage-collected before the FFI call, the pointers become dangling.
**Why it happens:** JavaScript GC doesn't know the pointer table references those Buffers.
**How to avoid:** Keep all Buffer references alive in local scope (as an array variable) until after the FFI call returns. Since bun:ffi calls are synchronous, local variables in the same function scope are safe from GC during the call.
**Warning signs:** Garbled string values in created elements, sporadic crashes.

### Pitfall 2: BigInt64Array.from() for Integer Arrays
**What goes wrong:** `BigInt64Array.from([1, 2, 3])` throws TypeError because BigInt64Array requires BigInt values, not numbers.
**Why it happens:** BigInt64Array only accepts BigInt values in its constructor.
**How to avoid:** Use `BigInt64Array.from(values, v => BigInt(v))` with a mapping function. Or construct with `new BigInt64Array(values.map(v => BigInt(v)))`.
**Warning signs:** TypeError: "can't convert number to BigInt".

### Pitfall 3: Empty Array Null Pointer
**What goes wrong:** Passing `ptr(emptyTypedArray)` for an empty array passes a valid pointer to zero bytes instead of NULL.
**Why it happens:** A zero-length typed array still has a valid backing buffer.
**How to avoid:** For empty arrays, pass literal `0` (null pointer) with count `0` directly: `lib.quiver_element_set_array_integer(elemPtr, cName, 0, 0)`. This matches the Python behavior of passing `ffi.NULL`.
**Warning signs:** Unexpected behavior from C API when it receives a non-null pointer with zero count.

### Pitfall 4: Prototype Extension Not Registered
**What goes wrong:** `db.createElement(...)` throws "db.createElement is not a function" at runtime.
**Why it happens:** `create.ts` extends `Database.prototype` but is never imported. TypeScript's `declare module` only affects type checking, not runtime.
**How to avoid:** Add `import "./create"` in `index.ts` (side-effect import). This ensures the prototype extension code runs when the module is loaded.
**Warning signs:** TypeScript compiles fine (types are augmented) but runtime crashes with "not a function".

### Pitfall 5: Element Handle Leak on Error
**What goes wrong:** If `quiver_element_set_*` or `quiver_database_create_element` throws, the element handle is never destroyed, leaking memory.
**Why it happens:** The element was created successfully but destruction was skipped due to an early exception.
**How to avoid:** Always wrap the element lifecycle in try/finally with `quiver_element_destroy` in the finally block.
**Warning signs:** Growing memory usage with repeated failed createElement calls.

### Pitfall 6: i64 vs i32 for count Parameter
**What goes wrong:** `quiver_element_set_array_integer` takes `int32_t count`, not `size_t`. Declaring the FFI arg as `i64` or `u64` would read 8 bytes instead of 4, corrupting the stack.
**Why it happens:** The C API uses `int32_t count` for element array setters (unlike the database read functions which use `size_t`).
**How to avoid:** Declare the count parameter as `FFIType.i32` in the symbol table for all `quiver_element_set_array_*` functions.
**Warning signs:** Crashes or wrong array sizes when setting arrays.

## Code Examples

Verified patterns from official sources and existing code:

### FFI Symbol Definitions for Element and CRUD
```typescript
// Source: include/quiver/c/element.h + include/quiver/c/database.h
// New symbols to add to loader.ts symbol table

// Element lifecycle
quiver_element_create: {
  args: [FFIType.ptr],  // quiver_element_t** out_element
  returns: FFIType.i32,
},
quiver_element_destroy: {
  args: [FFIType.ptr],  // quiver_element_t* element
  returns: FFIType.i32,
},

// Scalar setters
quiver_element_set_integer: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.i64],  // element, name, value
  returns: FFIType.i32,
},
quiver_element_set_float: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.f64],  // element, name, value
  returns: FFIType.i32,
},
quiver_element_set_string: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr],  // element, name, value (char*)
  returns: FFIType.i32,
},
quiver_element_set_null: {
  args: [FFIType.ptr, FFIType.ptr],  // element, name
  returns: FFIType.i32,
},

// Array setters
quiver_element_set_array_integer: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i32],  // element, name, values (int64_t*), count (int32_t)
  returns: FFIType.i32,
},
quiver_element_set_array_float: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i32],  // element, name, values (double*), count (int32_t)
  returns: FFIType.i32,
},
quiver_element_set_array_string: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i32],  // element, name, values (char**), count (int32_t)
  returns: FFIType.i32,
},

// Database CRUD
quiver_database_create_element: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],  // db, collection, element, out_id (int64_t*)
  returns: FFIType.i32,
},
quiver_database_delete_element: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.i64],  // db, collection, id (int64_t)
  returns: FFIType.i32,
},
```

### Types Definition (types.ts)
```typescript
// Source: CONTEXT.md locked decisions
/**
 * Scalar value types accepted by createElement.
 * - number: integer (if Number.isInteger) or float
 * - bigint: integer
 * - string: string
 * - null: explicit null
 * - undefined is handled by omission (field skipped)
 */
export type ScalarValue = number | bigint | string | null;

/**
 * Array value types accepted by createElement.
 * Type is inferred from the first element.
 */
export type ArrayValue = number[] | bigint[] | string[];

/**
 * Any value accepted in an element data object.
 */
export type Value = ScalarValue | ArrayValue;

/**
 * Plain object describing element attributes.
 * Keys are attribute names, values are typed.
 * undefined values are skipped (field not set on element).
 */
export type ElementData = Record<string, Value | undefined>;
```

### Reading int64 Out-Parameter for createElement Return ID
```typescript
// Source: Phase 1 out-parameter pattern + bun:ffi BigInt64Array
import { ptr } from "bun:ffi";

// Allocate 8-byte buffer for int64 out-param
const outId = new BigInt64Array(1);

check(lib.quiver_database_create_element(
  dbHandle,           // quiver_database_t*
  toCString(collection),  // const char*
  elemPtr,            // quiver_element_t*
  ptr(outId),         // int64_t* out_id
));

// Read the ID as JS number (safe for SQLite AUTOINCREMENT IDs)
const id = Number(outId[0]);
// outId[0] is a BigInt, Number() converts safely for values < MAX_SAFE_INTEGER
```

### Building char** for String Arrays
```typescript
// Source: bun:ffi pointer mechanics + Python element.py _set_array_string
import { ptr } from "bun:ffi";

const strings = ["fire", "ice", "lightning"];

// 1. Create null-terminated C strings
const buffers = strings.map(s => Buffer.from(s + "\0"));

// 2. Build pointer table (array of pointers)
// Each entry is a 64-bit pointer to one of the C strings
const ptrTable = new BigInt64Array(buffers.length);
for (let i = 0; i < buffers.length; i++) {
  ptrTable[i] = BigInt(ptr(buffers[i]));
}

// 3. Pass pointer table to C API
check(lib.quiver_element_set_array_string(
  elemPtr,
  toCString("tags"),
  ptr(ptrTable),        // const char* const* values
  strings.length,       // int32_t count
));
// buffers and ptrTable are alive in this scope, so pointers are valid
```

### deleteElement Implementation
```typescript
// Source: Python database.py delete_element + C API database_delete.cpp
Database.prototype.deleteElement = function (
  this: Database,
  collection: string,
  id: number,
): void {
  const lib = getSymbols();
  check(lib.quiver_database_delete_element(
    this._handle,      // triggers ensureOpen()
    toCString(collection),
    id,                // i64 arg accepts number directly
  ));
};
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Element as public class | Internal element, kwargs/plain-object API | Python binding design | Simpler public API, no Element export needed |
| Type-explicit array setters | Auto-detect from first element | All bindings | Less boilerplate for users |

**Resolved from Phase 1:**
- `usize` FFIType: CONFIRMED available in Bun 1.3.10. Maps to value 8 (same as `u64`). Can be used for `size_t` parameters in future phases. Not needed in Phase 2 since element array setters use `int32_t count`.

## Open Questions

1. **Number.isInteger() for full-array scan**
   - What we know: CONTEXT.md says "number[] where ALL values pass Number.isInteger() -> integer array; ANY decimal -> float array"
   - What's unclear: Whether the planner should implement a full scan of all values or just check the first element (Python only checks first)
   - Recommendation: Implement full scan for number arrays per the locked decision. Check all elements with `Number.isInteger()`. If all pass, integer array. If any fails, float array. This is a one-line `.every()` call and matches the user's explicit decision.

2. **BigInt mixing in arrays**
   - What we know: `bigint[]` is accepted. `number[]` is accepted. Mixed `(number | bigint)[]` is not addressed.
   - What's unclear: Whether mixed arrays should be supported
   - Recommendation: Check first element type only. If first is bigint, treat as bigint array (all must be bigint). If first is number, treat as number array (isInteger check applies). Mixed types throw an error naturally since BigInt64Array.from with a non-bigint, or Float64Array with a bigint, will throw.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in, Bun 1.3.10) |
| Config file | none |
| Quick run command | `bun test bindings/js/test/create.test.ts` |
| Full suite command | `bun test bindings/js/test/` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CRUD-01 | createElement with integer, float, string, null scalars | integration | `bun test bindings/js/test/create.test.ts` | Wave 0 |
| CRUD-02 | createElement with integer, float, string arrays | integration | `bun test bindings/js/test/create.test.ts` | Wave 0 |
| CRUD-03 | createElement returns numeric ID | integration | `bun test bindings/js/test/create.test.ts` | Wave 0 |
| CRUD-04 | deleteElement removes element by ID | integration | `bun test bindings/js/test/create.test.ts` | Wave 0 |

### Sampling Rate
- **Per task commit:** `bun test bindings/js/test/`
- **Per wave merge:** `bun test bindings/js/test/`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/create.test.ts` -- covers CRUD-01 through CRUD-04
- [ ] `bindings/js/src/types.ts` -- Value type union and ElementData type
- [ ] `bindings/js/src/create.ts` -- createElement and deleteElement implementations

## Sources

### Primary (HIGH confidence)
- [bun:ffi official docs](https://bun.sh/docs/runtime/ffi) - FFIType table, ptr(), read.*, CString, typed array passing
- C API headers: `include/quiver/c/element.h` - Element handle lifecycle and setter function signatures
- C API headers: `include/quiver/c/database.h` - create_element and delete_element signatures
- C API implementation: `src/c/element.cpp` - Array setter validation logic (null/count guard)
- C API implementation: `src/c/database_create.cpp` - create_element flow
- C API implementation: `src/c/database_delete.cpp` - delete_element flow
- Python binding: `bindings/python/src/quiverdb/element.py` - kwargs pattern, type dispatch, array handling
- Python binding: `bindings/python/src/quiverdb/database.py` - create_element/delete_element lifecycle
- Phase 1 JS code: `bindings/js/src/` - Established patterns (loader, errors, ffi-helpers, database)
- Empirical verification: `bun run` confirmed `FFIType.usize` maps to value 8 (u64) in Bun 1.3.10

### Secondary (MEDIUM confidence)
- [bun:ffi FFIType reference](https://bun.com/reference/bun/ffi) - Complete module API
- Python tests: `bindings/python/tests/test_database_create.py` - Test patterns for createElement

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All tools established in Phase 1, no new dependencies
- Architecture: HIGH - Prototype extension pattern is standard TypeScript, Element lifecycle mirrors Python binding exactly
- FFI marshaling: HIGH - BigInt64Array/Float64Array for typed arrays confirmed by bun:ffi docs, char** pointer table pattern is well-understood
- Pitfalls: HIGH - Documented from empirical bun:ffi experience and Python binding code review
- Array type detection: HIGH - Rules explicitly locked in CONTEXT.md, Python binding provides reference implementation

**Research date:** 2026-03-08
**Valid until:** 2026-04-08 (30 days -- bun:ffi API is stable, C API is stable)
