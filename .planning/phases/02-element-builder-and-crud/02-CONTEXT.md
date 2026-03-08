# Phase 2: Element Builder and CRUD - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Create and delete elements with typed scalar and array attributes via the JS/TS binding. Covers requirements CRUD-01 (scalar element building), CRUD-02 (array element building), CRUD-03 (create element and return ID), CRUD-04 (delete element by ID). The Element class is internal — users pass plain objects.

</domain>

<decisions>
## Implementation Decisions

### Element builder API
- Plain object API: `db.createElement('Items', { label: 'Sword', damage: 42, tags: ['fire'] })`
- Element class is internal (not exported) — same approach as Python binding
- createElement accepts `Record<string, Value>` with a typed `Value` union
- Types live in `types.ts` (reusable by future phases); createElement/deleteElement in `create.ts`
- Methods added to Database via prototype extension with `declare module` type augmentation

### Array value handling
- Auto-detect type from first element: number → check isInteger, string → string array, bigint → integer array
- number[] where ALL values pass `Number.isInteger()` → integer array; ANY decimal → float array
- bigint[] → integer array
- Empty arrays accepted: pass count=0 to `quiver_element_set_array_integer` with null pointer (matches Python behavior)

### Null and type semantics
- `null` → `quiver_element_set_null(name)` (explicit null)
- `undefined` → skipped entirely (field not passed to C API)
- Scalar number: `Number.isInteger(v)` → set_integer, else → set_float
- bigint → set_integer
- string → set_string
- Unsupported types (boolean, object, Date, nested arrays) → throw QuiverError with message: "Unsupported value type for '{field}': {typeof}"

### ID return and delete API
- createElement returns `number` (plain JS number, not bigint — SQLite AUTOINCREMENT IDs fit in MAX_SAFE_INTEGER)
- deleteElement returns `void` — error behavior delegated to C++ layer (QuiverError propagates if C++ throws)
- deleteElement accepts `number` only (consistent with createElement return type)

### Claude's Discretion
- Internal Element class lifecycle management (create/destroy around each createElement call)
- FFI marshaling details for arrays (buffer allocation, pointer management)
- New FFI symbol definitions to add to loader.ts
- Test schema choice and test structure
- ensureOpen() guard placement

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `ffi-helpers.ts`: `toCString()`, `allocPointerOut()`, `readPointerOut()`, `makeDefaultOptions()` — all reusable for createElement/deleteElement calls
- `errors.ts`: `check()` function and `QuiverError` class — wraps all C API calls
- `loader.ts`: `getSymbols()` for lazy library access — needs new symbol definitions for element and CRUD functions

### Established Patterns
- Database class with `ensureOpen()` guard (throws "Database is closed")
- `_handle` getter for internal pointer access
- Prototype extension pattern decided for adding methods across files
- bun:ffi patterns: `ptr()` for typed arrays, `BigInt64Array(1)` for out-params, `Buffer.from(str + "\0")` for C strings

### Integration Points
- `include/quiver/c/element.h`: quiver_element_create, set_integer/float/string/null, set_array_integer/float/string, destroy
- `include/quiver/c/database.h`: quiver_database_create_element (db, collection, element, out_id), quiver_database_delete_element (db, collection, id)
- `loader.ts` symbol table needs expansion for element lifecycle and CRUD functions
- `index.ts` needs to import `create.ts` to register prototype extensions

</code_context>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. The binding should follow established patterns from existing Julia/Dart/Python wrappers, with the plain-object API being the JS-idiomatic choice (like Python's `**kwargs` approach).

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-element-builder-and-crud*
*Context gathered: 2026-03-08*
