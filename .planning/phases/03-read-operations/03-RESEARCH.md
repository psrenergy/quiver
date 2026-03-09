# Phase 3: Read Operations - Research

**Researched:** 2026-03-08
**Domain:** bun:ffi array marshaling, C pointer dereferencing, typed array construction from native memory
**Confidence:** HIGH

## Summary

Phase 3 adds scalar read operations and element ID retrieval to the JS/TS binding. The core challenge is reading C-allocated arrays (int64_t*, double*, char**) through bun:ffi, converting them to JavaScript typed arrays, and properly freeing the C-allocated memory afterward. This is the inverse of Phase 2's write operations -- Phase 2 marshaled JS values into C, Phase 3 marshals C values back into JS.

The C API read functions follow a consistent pattern: the caller passes out-parameter pointers for `values` and `count`, the C API allocates arrays internally (via `new[]`), and the caller must free them after use (via `quiver_database_free_*`). For scalar-all reads (`readScalarIntegers`, etc.), the return is a flat array. For scalar-by-id reads, it is a single value plus a `has_value` boolean flag. For element IDs, it is a flat int64 array.

The bun:ffi toolkit for reading from C pointers is: (1) `toArrayBuffer(ptr, byteOffset, byteLength)` to create an ArrayBuffer view over native memory, then wrap it in a TypedArray; (2) `read.ptr(pointer, byteOffset)` to dereference pointer-to-pointer (for char** string arrays); (3) `CString(ptr)` to read a null-terminated C string from a pointer. For numeric arrays, `toArrayBuffer` is the most efficient approach (zero-copy view). For string arrays, element-by-element `read.ptr` + `CString` is required since each string is individually allocated.

**Primary recommendation:** Use `toArrayBuffer` for bulk numeric array reads (int64, float64) and `read.ptr` + `CString` for string arrays. Follow the exact same file structure pattern as Phase 2: `read.ts` with prototype extension via `declare module`, new FFI symbols added to `loader.ts`, side-effect import in `index.ts`. Free C memory immediately after copying data to JS objects.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| READ-01 | User can read all scalar integers/floats/strings for a collection attribute | `quiver_database_read_scalar_integers/floats/strings` C API functions with `out_values` + `out_count` pattern; toArrayBuffer for numerics, read.ptr+CString for strings |
| READ-02 | User can read a scalar integer/float/string by element ID | `quiver_database_read_scalar_integer/float/string_by_id` C API with `out_value` + `out_has_value` pattern; read.i64/read.f64 for numerics, CString for string |
| READ-03 | User can read all element IDs from a collection | `quiver_database_read_element_ids` C API with `out_ids` + `out_count`; same int64 array pattern as readScalarIntegers |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:ffi | Built-in (Bun 1.3.10) | C library function calling, pointer reading, typed array creation | Established in Phase 1; toArrayBuffer and read.* are the array-reading primitives |
| bun:test | Built-in (Bun 1.3.10) | Test runner | Established in Phase 1 |
| TypeScript | Built-in (Bun 1.3.10) | Type safety | Established in Phase 1 |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Phase 1 helpers | N/A (local) | `check()`, `toCString()`, `allocPointerOut()`, `readPointerOut()` | Every C API call |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| toArrayBuffer for numeric arrays | Element-by-element read.i64/read.f64 | toArrayBuffer creates a zero-copy view; element-by-element is O(n) function calls. Use toArrayBuffer for bulk reads. |
| BigInt64Array + Number conversion for int64 | Float64Array | Float64Array loses precision for values > 2^53. Stick with BigInt64Array and convert via Number(). |

**Installation:**
```bash
# No new dependencies -- all built-in to Bun
```

## Architecture Patterns

### Recommended Project Structure
```
bindings/js/
  src/
    index.ts          # Add side-effect import for read.ts
    loader.ts         # Add new symbol definitions for read + free functions
    errors.ts         # Existing (unchanged)
    database.ts       # Existing (unchanged)
    ffi-helpers.ts    # Existing; may add readSizeT helper
    types.ts          # Existing (unchanged)
    create.ts         # Existing (unchanged)
    read.ts           # NEW: all read operations via prototype extension
  test/
    database.test.ts  # Existing lifecycle tests
    create.test.ts    # Existing CRUD tests
    read.test.ts      # NEW: read operation tests
    test.bat          # Existing test runner
```

### Pattern 1: Prototype Extension for Read Methods (Same as Phase 2)
**What:** Add read methods to Database class from `read.ts` using `declare module` augmentation.
**When to use:** All read methods (readScalarIntegers, readScalarIntegerById, readElementIds, etc.).
**Example:**
```typescript
// Source: Phase 2 create.ts pattern (established in this project)
// read.ts
import { ptr, read, toArrayBuffer, CString } from "bun:ffi";
import { Database } from "./database";
import { getSymbols } from "./loader";
import { check } from "./errors";
import { toCString, allocPointerOut, readPointerOut } from "./ffi-helpers";

declare module "./database" {
  interface Database {
    readScalarIntegers(collection: string, attribute: string): number[];
    readScalarFloats(collection: string, attribute: string): number[];
    readScalarStrings(collection: string, attribute: string): string[];
    readScalarIntegerById(collection: string, attribute: string, id: number): number | null;
    readScalarFloatById(collection: string, attribute: string, id: number): number | null;
    readScalarStringById(collection: string, attribute: string, id: number): string | null;
    readElementIds(collection: string): number[];
  }
}
```

### Pattern 2: Reading Numeric Arrays via toArrayBuffer
**What:** Use `toArrayBuffer(ptr, 0, byteLength)` to create an ArrayBuffer backed by C memory, wrap in a TypedArray, copy to a JS array, then free the C memory.
**When to use:** readScalarIntegers, readScalarFloats, readElementIds -- any function returning a flat numeric array.
**Example:**
```typescript
// Source: bun:ffi docs (https://bun.com/docs/runtime/ffi) + C API pattern
Database.prototype.readScalarIntegers = function (
  this: Database,
  collection: string,
  attribute: string,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  // Allocate out-parameters: pointer and count
  const outValues = allocPointerOut();  // BigInt64Array(1) for int64_t*
  const outCount = new BigUint64Array(1);  // for size_t

  check(lib.quiver_database_read_scalar_integers(
    handle,
    toCString(collection),
    toCString(attribute),
    ptr(outValues),      // int64_t** out_values
    ptr(outCount),       // size_t* out_count
  ));

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);

  // Create typed array view over C-allocated memory
  // int64_t is 8 bytes, so total bytes = count * 8
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const bigInts = new BigInt64Array(buffer);

  // Copy to JS number array
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }

  // Free C-allocated memory
  check(lib.quiver_database_free_integer_array(valuesPtr));

  return result;
};
```

### Pattern 3: Reading Float Arrays via toArrayBuffer
**What:** Same pattern as integers but with Float64Array (8 bytes per element, same as int64).
**When to use:** readScalarFloats.
**Example:**
```typescript
// Source: bun:ffi docs + C API pattern
Database.prototype.readScalarFloats = function (
  this: Database,
  collection: string,
  attribute: string,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(lib.quiver_database_read_scalar_floats(
    handle,
    toCString(collection),
    toCString(attribute),
    ptr(outValues),
    ptr(outCount),
  ));

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const valuesPtr = readPointerOut(outValues);

  // Float64Array maps directly to double* (8 bytes per element)
  const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
  const result = Array.from(new Float64Array(buffer));

  check(lib.quiver_database_free_float_array(valuesPtr));

  return result;
};
```

### Pattern 4: Reading String Arrays via read.ptr + CString
**What:** For `char**` (array of string pointers), dereference each pointer individually using `read.ptr` and construct a CString from each.
**When to use:** readScalarStrings.
**Example:**
```typescript
// Source: bun:ffi docs read.ptr + CString + C API char** pattern
Database.prototype.readScalarStrings = function (
  this: Database,
  collection: string,
  attribute: string,
): string[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outValues = allocPointerOut();  // for char***
  const outCount = new BigUint64Array(1);

  check(lib.quiver_database_read_scalar_strings(
    handle,
    toCString(collection),
    toCString(attribute),
    ptr(outValues),
    ptr(outCount),
  ));

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const arrPtr = readPointerOut(outValues);  // char** pointer

  // Read each char* from the array, convert to JS string
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = read.ptr(arrPtr, i * 8);  // 8 bytes per pointer on 64-bit
    result[i] = new CString(strPtr).toString();
  }

  // Free C-allocated memory (frees each string + the array)
  check(lib.quiver_database_free_string_array(arrPtr, count));

  return result;
};
```

### Pattern 5: Reading Scalar by ID (Optional Value Pattern)
**What:** The by-id functions return a value and a `has_value` boolean flag (int). When `has_value` is 0, return `null`.
**When to use:** readScalarIntegerById, readScalarFloatById, readScalarStringById.
**Example:**
```typescript
// Source: C API read_scalar_integer_by_id signature + Dart readScalarIntegerById pattern
Database.prototype.readScalarIntegerById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): number | null {
  const lib = getSymbols();
  const handle = this._handle;

  // For int64_t out_value: use BigInt64Array(1) as out-parameter
  const outValue = new BigInt64Array(1);
  // For int out_has_value: use Int32Array(1)
  const outHasValue = new Int32Array(1);

  check(lib.quiver_database_read_scalar_integer_by_id(
    handle,
    toCString(collection),
    toCString(attribute),
    id,               // int64_t id
    ptr(outValue),    // int64_t* out_value
    ptr(outHasValue), // int* out_has_value
  ));

  if (outHasValue[0] === 0) return null;
  return Number(outValue[0]);
};
```

### Pattern 6: Reading Element IDs
**What:** Same pattern as readScalarIntegers (returns int64_t* + count), but the function takes only `collection` (no `attribute`).
**When to use:** readElementIds.
**Example:**
```typescript
// Source: C API quiver_database_read_element_ids signature
Database.prototype.readElementIds = function (
  this: Database,
  collection: string,
): number[] {
  const lib = getSymbols();
  const handle = this._handle;

  const outIds = allocPointerOut();
  const outCount = new BigUint64Array(1);

  check(lib.quiver_database_read_element_ids(
    handle,
    toCString(collection),
    ptr(outIds),
    ptr(outCount),
  ));

  const count = Number(outCount[0]);
  if (count === 0) return [];

  const idsPtr = readPointerOut(outIds);
  const buffer = toArrayBuffer(idsPtr, 0, count * 8);
  const bigInts = new BigInt64Array(buffer);

  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }

  check(lib.quiver_database_free_integer_array(idsPtr));

  return result;
};
```

### Anti-Patterns to Avoid
- **Reading from toArrayBuffer after freeing the C memory:** The ArrayBuffer from `toArrayBuffer` is a *view* over C memory, not a copy. If you call `free_integer_array` before reading from the BigInt64Array, the data is gone. Always copy to a JS array first, then free.
- **Forgetting to free C-allocated arrays:** Every successful read allocates memory via `new[]` in the C layer. Forgetting to call `free_integer_array`, `free_float_array`, or `free_string_array` leaks memory.
- **Using read.i64() for all elements of a large array:** `read.i64(ptr, offset)` works but makes N function calls. `toArrayBuffer` + `BigInt64Array` is one call and creates a zero-copy view.
- **Returning BigInt from readScalarIntegers:** The Dart binding returns `List<int>`, not `List<BigInt>`. Follow the same pattern: convert BigInt64Array values to `number` via `Number()`. All SQLite integer values fit in 53-bit range.
- **Not handling the count=0/null-pointer case:** When the collection is empty, the C API sets `*out_values = nullptr` and `*out_count = 0`. Do NOT call `toArrayBuffer(nullptr, ...)` -- it will crash. Check count first.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Numeric array from C pointer | Element-by-element read.i64/read.f64 loop | `toArrayBuffer` + TypedArray | Single FFI call, zero-copy view, then bulk Array.from or loop |
| C string reading | Manual null-byte scanning | `CString(ptr)` from bun:ffi | Built-in UTF-8 to UTF-16 transcoding, null-terminator search |
| Pointer dereference | Manual buffer arithmetic | `read.ptr(base, offset)` | Direct pointer read at byte offset |
| Out-parameter allocation | Raw ArrayBuffer setup | `allocPointerOut()` + `readPointerOut()` from ffi-helpers.ts | Established pattern from Phase 1 |
| size_t out-parameter | BigInt64Array | `BigUint64Array(1)` | size_t is unsigned; BigUint64Array matches semantics. Phase 1 used BigInt64Array but BigUint64Array is more correct for size_t. |

**Key insight:** The read operation is the mirror image of the write operation. Phase 2 converted JS values to C types (BigInt64Array/Float64Array for writes). Phase 3 converts C types back to JS values (toArrayBuffer + TypedArray for reads). The symmetry means the helper functions and patterns are already well-understood.

## Common Pitfalls

### Pitfall 1: Use-After-Free on toArrayBuffer
**What goes wrong:** `toArrayBuffer` creates an ArrayBuffer that is a *view* over C-allocated memory (zero-copy). If you free the C memory before reading from the TypedArray, you get garbage data or a crash.
**Why it happens:** Unlike `Buffer.from(arrayBuffer)`, `toArrayBuffer` does NOT copy the data -- it wraps the existing memory.
**How to avoid:** Always copy data from the TypedArray into a JS array (via `Array.from()` or loop) BEFORE calling the corresponding `free_*` function.
**Warning signs:** Intermittent garbage values, especially under memory pressure or with large arrays.

### Pitfall 2: Null Pointer from Empty Collections
**What goes wrong:** Calling `toArrayBuffer(0, 0, 0)` or `read.ptr(0, ...)` with a null pointer crashes.
**Why it happens:** When the C API returns count=0, it sets `*out_values = nullptr`. Attempting to read from address 0 is undefined behavior.
**How to avoid:** Always check count before dereferencing the pointer. If count is 0, return an empty array immediately.
**Warning signs:** Segfault on read operations against empty collections.

### Pitfall 3: size_t Out-Parameter Type
**What goes wrong:** Using `Int32Array(1)` for `size_t*` reads only 4 bytes on 64-bit systems where `size_t` is 8 bytes, getting a truncated count or corrupting adjacent memory.
**Why it happens:** `size_t` is 8 bytes on 64-bit platforms, not 4. The C API's `out_count` parameter is `size_t*`.
**How to avoid:** Use `BigUint64Array(1)` for `size_t*` out-parameters. Read with `Number(outCount[0])`.
**Warning signs:** Incorrect array counts, memory corruption when C writes 8 bytes into a 4-byte buffer.

### Pitfall 4: char** String Array Memory Layout
**What goes wrong:** Reading strings from `char**` by treating it as a contiguous buffer of characters instead of an array of pointers.
**Why it happens:** `char**` is an array of pointers, where each pointer points to an independently allocated null-terminated string. It is NOT a contiguous block of all string data.
**How to avoid:** Use `read.ptr(basePtr, i * 8)` to read each pointer (8 bytes per pointer on 64-bit), then `new CString(strPtr)` to read each string.
**Warning signs:** Reading the first string works but subsequent strings are garbage.

### Pitfall 5: read_scalar_string_by_id Memory Leak
**What goes wrong:** The C API allocates a new string via `new_c_str()` for the string-by-id result. Not freeing it leaks memory.
**Why it happens:** Unlike integer/float by-id (which write to stack-allocated out-params), string by-id allocates heap memory.
**How to avoid:** After reading the string with `CString`, call `quiver_database_free_string(strPtr)` to free the C allocation.
**Warning signs:** Growing memory usage with repeated string-by-id reads.

### Pitfall 6: FFI Symbol Table Mismatch for size_t
**What goes wrong:** Declaring `size_t*` parameters as `FFIType.ptr` works but declaring the `size_t` type in the FFI symbol table differently (e.g., as `FFIType.i32`) will cause misaligned reads.
**Why it happens:** The C API uses `size_t` (8 bytes on 64-bit) for counts.
**How to avoid:** Use `FFIType.ptr` for all pointer parameters (including `size_t*`), and read the value from the TypedArray backing buffer directly.
**Warning signs:** Wrong counts, crashes on large collections.

## Code Examples

Verified patterns from official sources and existing code:

### FFI Symbol Definitions for Read Operations
```typescript
// Source: include/quiver/c/database.h read function signatures
// New symbols to add to loader.ts symbol table

// Read scalar arrays
quiver_database_read_scalar_integers: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  // (db, collection, attribute, out_values: int64_t**, out_count: size_t*)
  returns: FFIType.i32,
},
quiver_database_read_scalar_floats: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_read_scalar_strings: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  // (db, collection, attribute, out_values: char***, out_count: size_t*)
  returns: FFIType.i32,
},

// Read scalar by ID
quiver_database_read_scalar_integer_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  // (db, collection, attribute, id: int64_t, out_value: int64_t*, out_has_value: int*)
  returns: FFIType.i32,
},
quiver_database_read_scalar_float_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  // (db, collection, attribute, id: int64_t, out_value: double*, out_has_value: int*)
  returns: FFIType.i32,
},
quiver_database_read_scalar_string_by_id: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
  // (db, collection, attribute, id: int64_t, out_value: char**, out_has_value: int*)
  returns: FFIType.i32,
},

// Read element IDs
quiver_database_read_element_ids: {
  args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
  // (db, collection, out_ids: int64_t**, out_count: size_t*)
  returns: FFIType.i32,
},

// Free functions
quiver_database_free_integer_array: {
  args: [FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_free_float_array: {
  args: [FFIType.ptr],
  returns: FFIType.i32,
},
quiver_database_free_string_array: {
  args: [FFIType.ptr, FFIType.u64],
  // (values: char**, count: size_t)
  returns: FFIType.i32,
},
quiver_database_free_string: {
  args: [FFIType.ptr],
  returns: FFIType.i32,
},
```

### size_t Out-Parameter Reading
```typescript
// Source: bun:ffi docs + C API size_t* parameters
import { ptr } from "bun:ffi";

// Allocate 8-byte buffer for size_t out-parameter
const outCount = new BigUint64Array(1);

// Pass pointer to FFI function
check(lib.quiver_database_read_scalar_integers(
  handle,
  toCString(collection),
  toCString(attribute),
  ptr(outValues),
  ptr(outCount),   // size_t* out_count
));

// Read the count value
const count = Number(outCount[0]);
```

### Complete Read-Copy-Free Flow for Integer Array
```typescript
// Source: bun:ffi toArrayBuffer docs + C API alloc/free co-location pattern
import { ptr, toArrayBuffer } from "bun:ffi";

// 1. Call C API to get pointer + count
const outValues = allocPointerOut();
const outCount = new BigUint64Array(1);
check(lib.quiver_database_read_scalar_integers(handle, cCollection, cAttribute, ptr(outValues), ptr(outCount)));

const count = Number(outCount[0]);
if (count === 0) return [];

// 2. Get the raw pointer
const valuesPtr = readPointerOut(outValues);

// 3. Create zero-copy view over C memory
const buffer = toArrayBuffer(valuesPtr, 0, count * 8);
const bigInts = new BigInt64Array(buffer);

// 4. COPY to JS array BEFORE freeing C memory
const result: number[] = new Array(count);
for (let i = 0; i < count; i++) {
  result[i] = Number(bigInts[i]);
}

// 5. Free C memory AFTER copying
check(lib.quiver_database_free_integer_array(valuesPtr));

return result;
```

### String By-ID with Memory Cleanup
```typescript
// Source: C API read_scalar_string_by_id + Dart readScalarStringById pattern
Database.prototype.readScalarStringById = function (
  this: Database,
  collection: string,
  attribute: string,
  id: number,
): string | null {
  const lib = getSymbols();
  const handle = this._handle;

  const outValue = allocPointerOut();    // for char**
  const outHasValue = new Int32Array(1); // for int*

  check(lib.quiver_database_read_scalar_string_by_id(
    handle,
    toCString(collection),
    toCString(attribute),
    id,
    ptr(outValue),
    ptr(outHasValue),
  ));

  if (outHasValue[0] === 0) return null;

  const strPtr = readPointerOut(outValue);
  const result = new CString(strPtr).toString();

  // Free the C-allocated string
  check(lib.quiver_database_free_string(strPtr));

  return result;
};
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Element-by-element read.i64() | toArrayBuffer + TypedArray | Always available | Zero-copy view is faster for bulk reads |
| Signed BigInt64Array for size_t | BigUint64Array | N/A | Correct unsigned semantics for size_t |

**Established from Phase 1/2:**
- `allocPointerOut()` + `readPointerOut()` confirmed working for pointer-to-pointer out-params
- `check()` pattern handles all error propagation
- Prototype extension with `declare module` confirmed working for adding methods to Database
- Side-effect import pattern in `index.ts` confirmed working

## Open Questions

1. **toArrayBuffer lifetime vs C free**
   - What we know: `toArrayBuffer(ptr, 0, byteLength)` creates a view over C memory. Bun documentation says it does not copy.
   - What's unclear: Whether Bun's GC might interact with the ArrayBuffer after we free the underlying C memory (though since we copy first, this should be safe)
   - Recommendation: Copy first, free second. Keep references to intermediaries alive in scope. This is the safe approach used by all other bindings.

2. **BigUint64Array vs BigInt64Array for size_t**
   - What we know: size_t is unsigned. BigUint64Array represents unsigned 64-bit values. BigInt64Array represents signed.
   - What's unclear: Whether the sign difference matters in practice (size_t values from this API never exceed JS MAX_SAFE_INTEGER)
   - Recommendation: Use BigUint64Array for correctness. The signed/unsigned difference is irrelevant for small values but BigUint64Array communicates intent better.

3. **CString safety after free_string_array**
   - What we know: `new CString(ptr)` creates a cloned JS string (the docs say it "clones" from the C pointer)
   - What's unclear: Whether the clone happens eagerly or lazily
   - Recommendation: Based on the docs ("automatically search for the closing \\0 character and transcode from UTF-8 to UTF-16"), CString reads and clones eagerly on construction. So reading the string before freeing should be safe. If in doubt, use `.toString()` immediately.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in, Bun 1.3.10) |
| Config file | none |
| Quick run command | `bun test bindings/js/test/read.test.ts` |
| Full suite command | `bun test bindings/js/test/` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| READ-01 | readScalarIntegers returns number[] | integration | `bun test bindings/js/test/read.test.ts -t "readScalarIntegers"` | Wave 0 |
| READ-01 | readScalarFloats returns number[] | integration | `bun test bindings/js/test/read.test.ts -t "readScalarFloats"` | Wave 0 |
| READ-01 | readScalarStrings returns string[] | integration | `bun test bindings/js/test/read.test.ts -t "readScalarStrings"` | Wave 0 |
| READ-01 | empty collection returns empty array | integration | `bun test bindings/js/test/read.test.ts -t "empty"` | Wave 0 |
| READ-02 | readScalarIntegerById returns number or null | integration | `bun test bindings/js/test/read.test.ts -t "readScalarIntegerById"` | Wave 0 |
| READ-02 | readScalarFloatById returns number or null | integration | `bun test bindings/js/test/read.test.ts -t "readScalarFloatById"` | Wave 0 |
| READ-02 | readScalarStringById returns string or null | integration | `bun test bindings/js/test/read.test.ts -t "readScalarStringById"` | Wave 0 |
| READ-03 | readElementIds returns number[] | integration | `bun test bindings/js/test/read.test.ts -t "readElementIds"` | Wave 0 |
| READ-03 | readElementIds on empty collection returns [] | integration | `bun test bindings/js/test/read.test.ts -t "readElementIds"` | Wave 0 |

### Sampling Rate
- **Per task commit:** `bun test bindings/js/test/`
- **Per wave merge:** `bun test bindings/js/test/`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/read.test.ts` -- covers READ-01 through READ-03
- [ ] `bindings/js/src/read.ts` -- all read operation implementations

## Sources

### Primary (HIGH confidence)
- [bun:ffi official docs](https://bun.com/docs/runtime/ffi) - toArrayBuffer, read.*, CString, FFIType table
- [bun:ffi read reference](https://bun.com/reference/bun/ffi/read) - read.ptr, read.i64, read.f64 method signatures
- [bun:ffi module reference](https://bun.com/reference/bun/ffi) - Complete module exports, CString constructor
- C API header: `include/quiver/c/database.h` - All read function signatures, free function signatures
- C API implementation: `src/c/database_read.cpp` - Read implementation details, alloc/free co-location
- C API helpers: `src/c/database_helpers.h` - `read_scalars_impl`, `copy_strings_to_c` templates
- Dart binding: `bindings/dart/lib/src/database_read.dart` - Reference implementation showing the complete read pattern
- Phase 1 JS code: `bindings/js/src/` - Established FFI patterns (loader, errors, ffi-helpers, database)
- Phase 2 JS code: `bindings/js/src/create.ts` - Prototype extension pattern, declare module augmentation

### Secondary (MEDIUM confidence)
- [bun:ffi blog posts](https://dev.to/1ce/how-to-and-should-you-use-bun-ffi-4c9k) - Practical toArrayBuffer usage examples

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All tools established in Phase 1, no new dependencies
- Architecture: HIGH - Prototype extension pattern proven in Phase 2, all C API signatures verified from headers
- FFI read mechanics: HIGH - toArrayBuffer, read.ptr, CString are documented in official bun:ffi docs
- Memory management: HIGH - Copy-then-free pattern used by all three existing bindings (Julia, Dart, Python)
- Pitfalls: HIGH - Use-after-free, null pointer, size_t sizing are well-documented cross-language FFI issues

**Research date:** 2026-03-08
**Valid until:** 2026-04-08 (30 days -- bun:ffi API is stable, C API is stable)
