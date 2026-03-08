# Domain Pitfalls

**Domain:** Bun-native JS/TS FFI binding for existing C library (libquiver_c)
**Researched:** 2026-03-08

## Critical Pitfalls

Mistakes that cause crashes, memory corruption, or require rewrites.

### Pitfall 1: Memory Leaks from Missing Free Calls

**What goes wrong:** Every C API read operation allocates memory that the caller must free. `bun:ffi` does not manage native memory -- there is no automatic cleanup. If the binding calls `quiver_database_read_scalar_strings` but fails to call `quiver_database_free_string_array` on the result, the memory leaks permanently. This is the single most likely source of bugs in the binding.

**Why it happens:** JavaScript developers expect garbage collection to handle everything. The C API has ~15 distinct free functions, each paired with specific allocation patterns. Missing even one creates a silent leak. The problem is compounded by error paths -- if an exception is thrown between the C API call and the free call, the free never executes.

**Consequences:** Memory grows unbounded over time. In long-running processes, this leads to OOM crashes. Silent -- no error message, no warning.

**Prevention:**
- Adopt a strict `try/finally` pattern for every C API call that allocates memory. The `finally` block must call the appropriate free function.
- Create a helper function pattern (e.g., `withCResult`) that wraps every alloc/free pair in a single abstraction, similar to how the Dart binding uses `Arena` with `try/finally { arena.releaseAll() }`.
- Map every C API function to its corresponding free function in a reference table before writing any binding code. The Quiver C API has these alloc/free pairs:

| Allocator | Free Function |
|-----------|---------------|
| `read_scalar_integers` | `free_integer_array` |
| `read_scalar_floats` | `free_float_array` |
| `read_scalar_strings` | `free_string_array(ptr, count)` |
| `read_scalar_string_by_id` | `free_string(ptr)` |
| `read_vector_integers` | `free_integer_vectors(vectors, sizes, count)` |
| `read_vector_floats` | `free_float_vectors(vectors, sizes, count)` |
| `read_vector_strings` | `free_string_vectors(vectors, sizes, count)` |
| `query_string` / `query_string_params` | `free_string(ptr)` |
| `read_time_series_group` | `free_time_series_data(names, types, data, cols, rows)` |
| `element_create` | `element_destroy` |
| `database_open/from_schema/from_migrations` | `database_close` |

- Write tests that specifically check for leaks by running read operations in a loop and monitoring memory.

**Detection:** Memory usage growing over time in tests. Run read-heavy test loops and compare RSS before/after.

**Confidence:** HIGH -- this is a universal FFI pitfall confirmed by Bun's own documentation: "bun:ffi does not manage memory for you."

---

### Pitfall 2: Out-Parameter Pattern with Pointer-to-Pointer in bun:ffi

**What goes wrong:** Nearly every Quiver C API function uses out-parameters. Simple cases like `int64_t* out_value` need a buffer to write into. Complex cases like `quiver_database_t** out_db` (pointer-to-pointer) require allocating a buffer to hold a pointer, calling the function, then reading the pointer back from the buffer. `bun:ffi` has no built-in struct or pointer-to-pointer abstraction -- you must manually manage this with TypedArrays and `read.ptr()`.

**Why it happens:** `bun:ffi` operates at a lower level than Dart's `dart:ffi` (which has `Pointer<Pointer<T>>` and Arena allocators). In Bun, you must:
1. Allocate a typed array large enough to hold a pointer (8 bytes on 64-bit).
2. Use `ptr(typedArray)` to get a pointer to that buffer.
3. Pass it to the C function.
4. Use `read.ptr(ptr(typedArray), 0)` to read the pointer value back.

Getting the buffer size wrong (e.g., using 4 bytes for a pointer on 64-bit) causes memory corruption. Using the wrong `read.*` function causes incorrect values.

**Consequences:** Memory corruption, crashes, silent data corruption.

**Prevention:**
- Create typed helper functions for each out-parameter pattern:
  ```typescript
  // For ptr-to-ptr (e.g., quiver_database_t** out_db)
  function allocPtrOut(): { buf: BigInt64Array; ptr: number } { ... }
  function readPtrOut(buf: BigInt64Array): number { ... }

  // For int64_t* out_value
  function allocI64Out(): { buf: BigInt64Array; ptr: number } { ... }
  function readI64Out(buf: BigInt64Array): bigint | number { ... }

  // For size_t* out_count
  function allocSizeOut(): { buf: BigUint64Array; ptr: number } { ... }

  // For double* out_value
  function allocF64Out(): { buf: Float64Array; ptr: number } { ... }

  // For int* out_has_value (C int = 32-bit)
  function allocI32Out(): { buf: Int32Array; ptr: number } { ... }
  ```
- NEVER inline the buffer allocation -- always use the helpers to guarantee correct sizes.
- Write a dedicated test for each out-parameter pattern before writing the binding functions.

**Detection:** Crashes on specific platforms (32-bit vs 64-bit mismatch). Wrong values read back from out-parameters.

**Confidence:** HIGH -- confirmed by bun:ffi documentation and the C API's heavy use of out-parameters (every single function uses them).

---

### Pitfall 3: int64_t / BigInt Type Mismatch

**What goes wrong:** The Quiver C API uses `int64_t` extensively (element IDs, integer values, query results). In `bun:ffi`, `i64` returns `BigInt`, not `number`. JavaScript's `number` type only safely represents integers up to 2^53-1. If the binding declares return types or parameter types as `i64`, it gets `BigInt` values that don't work with standard arithmetic or comparison operators (`===`, `<`, `+`).

**Why it happens:** The `i64` FFI type maps to JavaScript's `BigInt`. But most Quiver use cases involve small IDs (1, 2, 3...) where `number` is fine. The `i64_fast` variant attempts to coerce to `number` if the value fits, but this creates an inconsistent return type (`BigInt | number`), which is worse for TypeScript typing.

**Consequences:** TypeScript type errors throughout the binding. Runtime failures when users try `id + 1` on a BigInt. Confusing API where sometimes you get a number and sometimes a BigInt.

**Prevention:**
- Use `i64_fast` for parameters and return values where IDs are involved (element IDs will always fit in `number`).
- OR consistently use `i64` and convert to `number` in the binding layer with `Number(bigintValue)`.
- Pick ONE strategy and apply it uniformly. The recommended approach: use `i64` at the FFI boundary, then immediately convert to `number` in the TypeScript wrapper. This matches user expectations (IDs are numbers) and provides consistent types.
- For `size_t` out-parameters, use `u64` and convert to `number` since counts will never exceed safe integer range in practice.
- Document the decision and integer range limitation clearly.

**Detection:** TypeScript compilation errors. Runtime `TypeError: Cannot mix BigInt and other types`.

**Confidence:** HIGH -- confirmed by Bun's FFI type reference: `i64` returns BigInt, `i64_fast` returns `BigInt | number`.

---

### Pitfall 4: String Lifetime and Encoding Bugs

**What goes wrong:** Two distinct string issues:

1. **Passing strings TO C:** JavaScript strings are UTF-16 internally. The C API expects null-terminated UTF-8 `const char*`. Bun's `cstring` FFI type handles this for simple parameters, but for arrays of strings (e.g., `quiver_element_set_array_string` takes `const char* const* values`), you must manually create an array of C string pointers. These must remain valid for the duration of the C call -- if the JS GC moves or collects the underlying buffer mid-call, you get use-after-free.

2. **Reading strings FROM C:** The C API returns `char*` that the caller must free with `quiver_database_free_string`. Using `new CString(ptr)` clones the string (safe), but you must still call the free function on the original C pointer. Forgetting to free is a leak; freeing before cloning is use-after-free.

**Why it happens:** bun:ffi's `CString` clones on construction, which is good for safety but makes developers think they don't need to call free. The clone handles the JS side, but the C-allocated memory still exists.

**Consequences:** Memory leaks (not freeing returned strings), crashes (use-after-free), garbled text (encoding mismatch).

**Prevention:**
- For strings passed to C: Use `Buffer.from(str + '\0')` or rely on `cstring` parameter type which handles null-termination automatically.
- For string arrays passed to C: Allocate all string buffers first, create an array of pointers to them, then make the C call. Keep all buffers referenced until after the call returns.
- For strings returned from C: Always follow the pattern: (1) read the pointer, (2) create CString/read the data, (3) call the appropriate `free_string` function, (4) return the JS string.
- Quiver's C API uses UTF-8 exclusively (SQLite stores UTF-8). Bun's CString automatically transcodes UTF-8 to UTF-16. No manual encoding conversion needed -- but verify this works correctly with non-ASCII characters in tests.

**Detection:** Garbled text with non-ASCII characters. Memory leaks with string-heavy operations. Crashes in string-free functions.

**Confidence:** HIGH -- string handling is the most commonly reported issue with bun:ffi (see GitHub issue #6709).

---

### Pitfall 5: Windows DLL Dependency Chain Loading

**What goes wrong:** `libquiver_c.dll` depends on `libquiver.dll` (the C++ core). On Windows, when `bun:ffi`'s `dlopen` loads `libquiver_c.dll`, Windows must also find `libquiver.dll`. If it's not in the DLL search path, loading fails with a cryptic "cannot load library" error that doesn't mention the actual missing dependency.

**Why it happens:** Windows DLL search order is: (1) the directory containing the loading executable (Bun's install dir), (2) system directories, (3) PATH. The Quiver DLLs are in `build/bin/` or a package directory -- neither is searched by default. Both existing bindings (Dart and Python) solve this by explicitly pre-loading `libquiver.dll` before loading `libquiver_c.dll`.

**Consequences:** The binding fails to load on Windows with an unhelpful error message. Users have no idea what went wrong.

**Prevention:**
- Follow the exact pattern used by Dart and Python bindings: explicitly load `libquiver.dll` first using `dlopen` before loading `libquiver_c.dll`.
  ```typescript
  // Load core library first (dependency of C API library)
  import { dlopen } from "bun:ffi";
  if (process.platform === "win32") {
    dlopen(path.join(libDir, "libquiver.dll"), {});  // just load, no symbols needed
  }
  const lib = dlopen(path.join(libDir, "libquiver_c.dll"), symbols);
  ```
- On Linux/macOS, `rpath` or `LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH` may be needed similarly.
- The library loader module should be one of the first things implemented and thoroughly tested on all target platforms.

**Detection:** `Error: Cannot load library` on Windows. Works on Linux/macOS but fails on Windows.

**Confidence:** HIGH -- this exact issue is documented and solved in both the Dart (`library_loader.dart`) and Python (`_loader.py`) bindings.

---

### Pitfall 6: Struct Passing -- bun:ffi Has No Native Struct Support

**What goes wrong:** The Quiver C API uses two struct types as function parameters: `quiver_database_options_t` (passed by pointer) and `quiver_csv_options_t` (passed by pointer). `bun:ffi` has no built-in struct type. You cannot declare a struct layout and pass it naturally. You must manually serialize struct fields into a `TypedArray`/`DataView` with correct sizes, offsets, and alignment matching the C struct layout.

**Why it happens:** `bun:ffi` is intentionally minimal -- it handles scalar types and pointers, not compound types. This is a known limitation noted in the documentation.

**Consequences:** If struct field alignment is wrong, the C function reads garbage values. This can cause crashes, silent data corruption, or options being ignored.

**Prevention:**
- For `quiver_database_options_t` (simple: `int read_only` + `uint32_t console_level`):
  - Determine the exact struct layout using C sizeof/offsetof or by examining the build output.
  - On 64-bit platforms with typical alignment: `read_only` at offset 0 (4 bytes), `console_level` at offset 4 (4 bytes), total 8 bytes.
  - Use `DataView` or a `Uint8Array` with manual field writes at correct offsets.
  - Or use `quiver_database_options_default()` which returns the struct by value -- but this requires `bun:ffi` to support struct return by value, which it does NOT. Workaround: call the default function and capture to a buffer, or hardcode the defaults in JS and construct the struct manually.
- For v0.5 (core subset), `quiver_csv_options_t` is out of scope, reducing risk.
- Write a C test helper that prints `sizeof` and `offsetof` for all struct types, and use those values in the JS binding.
- Test struct passing with `quiver_database_options_default()` round-trip: construct options in JS, pass to `quiver_database_open`, verify behavior (e.g., `read_only = 1` should prevent writes).

**Detection:** Options silently ignored (wrong alignment means C reads zeros). Crashes on platforms with strict alignment requirements.

**Confidence:** HIGH -- bun:ffi's lack of struct support is documented. The Quiver C API requires struct parameters for all database lifecycle functions.

## Moderate Pitfalls

### Pitfall 7: Use-After-Close on Database Handle

**What goes wrong:** After calling `quiver_database_close(db)`, the pointer becomes invalid. If any subsequent call uses that pointer (because the JS wrapper didn't track closed state), the C layer either crashes (dereferencing freed memory) or returns confusing errors.

**Prevention:**
- Track closed state in the TypeScript Database class (boolean `_isClosed` flag).
- Check the flag before every C API call. Throw a clear JS error: `"Database has been closed"`.
- The Dart binding implements exactly this pattern (`_ensureNotClosed()`).
- Consider using `Symbol.dispose` / `using` syntax for automatic cleanup (TypeScript 5.2+ / Bun supports this).

**Confidence:** HIGH -- same pattern implemented in Dart and Julia bindings.

---

### Pitfall 8: Element Builder Lifecycle Management

**What goes wrong:** The Element builder (`quiver_element_t`) is an opaque handle that must be explicitly destroyed with `quiver_element_destroy`. The typical pattern is: create element, set fields, pass to `create_element`, then destroy. If the destroy call is missed (e.g., exception during field setting), the element leaks.

**Prevention:**
- Wrap the entire create-set-use-destroy cycle in a helper function with `try/finally`.
- Alternatively, consider a pattern where the JS binding creates the element, populates it from a plain JS object, calls create_element, and destroys it -- all within a single internal function that the user never sees.
  ```typescript
  // User-facing API (clean)
  db.createElement("Items", { label: "Item 1", value: 42 });

  // Internal implementation (handles lifecycle)
  function createElement(collection: string, fields: Record<string, unknown>) {
    const elemBuf = allocPtrOut();
    quiver_element_create(elemBuf.ptr);
    const elemPtr = readPtrOut(elemBuf);
    try {
      for (const [key, val] of Object.entries(fields)) {
        // call appropriate set_* based on type
      }
      const idBuf = allocI64Out();
      quiver_database_create_element(dbPtr, collection, elemPtr, idBuf.ptr);
      return readI64Out(idBuf);
    } finally {
      quiver_element_destroy(elemPtr);
    }
  }
  ```
- Python's binding uses exactly this kwargs-based approach -- Element is internal, users pass `**kwargs`.

**Confidence:** HIGH -- confirmed pattern from Python binding. The Element lifecycle is a known complexity in all existing bindings.

---

### Pitfall 9: Error Message Retrieval Race Condition (Thread-Local Storage)

**What goes wrong:** `quiver_get_last_error()` returns a pointer to thread-local storage. The message is overwritten on the next error. If the binding doesn't read the error message immediately after a failed call, the message may be lost or corrupted.

**Prevention:**
- Always read `quiver_get_last_error()` immediately after checking `QUIVER_ERROR` return code.
- Copy the string to a JS string before any other C API call.
- Create a centralized `checkError(result: number)` function that checks the return code and throws with the error message in one step -- identical to the Dart `check()` function.
  ```typescript
  function checkError(result: number): void {
    if (result !== 0) { // QUIVER_ERROR
      const msgPtr = lib.symbols.quiver_get_last_error();
      const msg = msgPtr ? new CString(msgPtr) : "Unknown error";
      throw new QuiverError(String(msg));
    }
  }
  ```
- Since Bun is single-threaded for JS execution, the thread-local issue is less dangerous than in multi-threaded runtimes. But the "read immediately" discipline is still essential.

**Confidence:** HIGH -- same pattern in all existing bindings (Dart's `check()`, Julia's exception handling).

---

### Pitfall 10: npm Package Structure for Native Binaries

**What goes wrong:** Publishing an npm package that includes prebuilt native DLLs/shared libraries is not straightforward. Common mistakes:
- Bundling binaries for all platforms in one package (bloated install size).
- Not handling platform-specific binary selection.
- Forgetting that `bun:ffi` is Bun-specific, so the package is useless in Node.js.
- Using postinstall scripts that fail in CI environments or behind corporate proxies.
- Not including the correct files in the npm tarball (`files` field in package.json).

**Prevention:**
- Use the `optionalDependencies` pattern with platform-specific packages (e.g., `@quiver/win32-x64`, `@quiver/linux-x64`, `@quiver/darwin-arm64`). npm/bun automatically installs only the correct one based on `os` and `cpu` fields.
- OR for a simpler first release: bundle all platform binaries in subdirectories and select at runtime. Start simple, optimize later.
- Mark the package as Bun-only in the README and package.json `engines` field.
- List all included files explicitly in `package.json` `files` array to avoid accidentally excluding DLLs or including build artifacts.
- Test the package installation flow: `bun pack`, inspect the tarball, `bun add ./path-to-tarball`, verify it works.

**Confidence:** MEDIUM -- based on community practices (esbuild, turbo, prisma). Specific Bun+native binary npm patterns are less established.

---

### Pitfall 11: bun:ffi Stability Warnings

**What goes wrong:** Bun's own documentation states: "bun:ffi has known bugs and limitations, and should not be relied on in production." There are documented crashes with specific pointer patterns, callback interactions, and edge cases. Issues continue to be reported on GitHub through 2025-2026.

**Prevention:**
- Acknowledge this is a Bun-only binding and accept the stability trade-off.
- Keep the FFI surface area minimal -- only call the functions actually needed for the v0.5 scope.
- Avoid bun:ffi callbacks (JSCallback) entirely -- they are a known crash source. The Quiver C API does not require callbacks, so this is naturally avoided.
- Pin the minimum Bun version in `package.json` `engines` field to a version known to work with the binding.
- Write a comprehensive test suite that catches regressions when Bun updates.
- Consider Node-API (napi) as a fallback path if bun:ffi proves too unstable. This is a future concern, not a v0.5 blocker.

**Confidence:** HIGH -- confirmed by Bun's official documentation and active GitHub issue tracker.

## Minor Pitfalls

### Pitfall 12: Boolean/Int Mismatch Between C and bun:ffi

**What goes wrong:** The Quiver C API uses `int` (not `bool`) for boolean-like out-parameters (e.g., `int* out_has_value`, `int* out_healthy`). In bun:ffi, `bool` FFI type maps to C `_Bool` (1 byte), not `int` (4 bytes). Using `FFIType.bool` for a C `int` parameter reads only 1 byte instead of 4.

**Prevention:**
- Use `FFIType.i32` for all C `int` parameters, including boolean-like ones.
- Convert to JavaScript boolean in the wrapper: `return outHasValue !== 0`.
- Never use `FFIType.bool` unless the C function actually uses C99 `_Bool` or C++ `bool`.

**Confidence:** HIGH -- the Quiver C API consistently uses `int` for booleans (verified in `database.h`: `int* out_has_value`, `int* out_healthy`).

---

### Pitfall 13: size_t Platform Width

**What goes wrong:** `size_t` is 8 bytes on 64-bit and 4 bytes on 32-bit. If the binding hardcodes `FFIType.u64` for `size_t`, it will break on 32-bit platforms. If it uses `FFIType.u32`, it will break on 64-bit platforms.

**Prevention:**
- Use `FFIType.usize` (which bun:ffi provides as the platform-correct size_t equivalent) -- this was mentioned in Bun documentation search results.
- If `usize` is not available as an FFI type, use pointer-sized types and detect platform word size.
- Since the primary target is 64-bit (Windows 11, modern Linux/macOS), `u64` works in practice, but `usize` is the correct choice for portability.

**Confidence:** MEDIUM -- `usize` type mentioned in search results but not verified in latest bun:ffi FFIType enum docs.

---

### Pitfall 14: Return-by-Value Struct from quiver_database_options_default

**What goes wrong:** `quiver_database_options_default()` and `quiver_csv_options_default()` return structs by value (not by pointer). `bun:ffi` does not support struct return values. Calling these functions directly will crash or return garbage.

**Prevention:**
- Do NOT call `quiver_database_options_default()` via FFI.
- Instead, hardcode the default values in the TypeScript layer. The defaults are simple and stable:
  ```typescript
  // From options.cpp: read_only = 0, console_level = QUIVER_LOG_OFF (4)
  const DEFAULT_OPTIONS = { readOnly: false, consoleLevel: 4 /* LOG_OFF */ };
  ```
- Construct the struct buffer manually with the correct layout.
- Alternatively, add a thin C wrapper function that takes a pointer and fills in defaults (modify the C API if acceptable).

**Confidence:** HIGH -- bun:ffi docs confirm no struct return-by-value support. The Quiver C API returns structs by value from these two functions.

---

### Pitfall 15: CString Null Pointer Dereference

**What goes wrong:** If a C function returns a null pointer for an optional string (e.g., `quiver_database_read_scalar_string_by_id` with `out_has_value == 0`), creating `new CString(nullPtr)` or reading from a null pointer crashes.

**Prevention:**
- Always check `out_has_value` before reading the string pointer.
- Guard every CString creation with a null check on the pointer value.
- `bun:ffi`'s `read.ptr()` returns `0` for null pointers -- compare against `0` or `null` before using.

**Confidence:** HIGH -- standard null-pointer guard, confirmed by C API patterns that use `out_has_value` sentinel.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Library loading / setup | DLL dependency chain (#5), struct defaults (#14) | Pre-load libquiver.dll, hardcode option defaults |
| FFI declarations | int64/BigInt type mismatch (#3), bool/int mismatch (#12), size_t width (#13) | Use i64+convert-to-number, i32 for C int, usize for size_t |
| Out-parameter helpers | Pointer-to-pointer patterns (#2) | Build typed helper functions first, test in isolation |
| Database lifecycle | Use-after-close (#7), struct passing (#6) | _isClosed flag, manual struct serialization |
| Element/CRUD operations | Element lifecycle leaks (#8), string arrays (#4) | Internal-only Element, try/finally everywhere |
| Read operations | Memory leaks from missing frees (#1), null strings (#15) | try/finally for every alloc/free pair, null guards |
| Error handling | Error message race (#9) | Centralized checkError() reads message immediately |
| Query operations | String parameter encoding (#4), BigInt results (#3) | CString for params, Number() conversion for results |
| npm packaging | Binary distribution (#10), Bun stability (#11) | Platform-specific packages, pinned Bun version |
| Testing | All pitfalls compound | Test each pattern in isolation before integration |

## Sources

- [Bun FFI Documentation](https://bun.com/docs/runtime/ffi) -- official docs on dlopen, types, CString, memory management
- [Bun FFI API Reference](https://bun.com/reference/bun/ffi) -- FFIType enum, read functions, module exports
- [Bun FFI FFIType enum](https://bun.com/reference/bun/ffi/FFIType) -- type mapping details including i64, i64_fast, u64_fast
- [Bun FFI CString class](https://bun.com/reference/bun/ffi/CString) -- CString null-terminated string handling, UTF-8 to UTF-16 transcoding
- [Bun FFI read object](https://bun.com/reference/bun/ffi/read) -- read.ptr, read.i32, read.f64 memory access
- [GitHub issue #6709: FFI string handling docs](https://github.com/oven-sh/bun/issues/6709) -- community reports on string passing difficulties
- [GitHub issue #7582: Memory leak with JSCallback](https://github.com/oven-sh/bun/issues/7582) -- documented memory leak in FFI callbacks
- [GitHub issue #11598: Dlopen error](https://github.com/oven-sh/bun/issues/11598) -- DLL loading issues
- [GitHub issue #16131: Bun crash with read.ptr](https://github.com/oven-sh/bun/issues/16131) -- pointer reading crash
- [GitHub issue #27664: Bun quality issues](https://github.com/oven-sh/bun/issues/27664) -- stability concerns discussion
- [Sentry: Publishing binaries on npm](https://sentry.engineering/blog/publishing-binaries-on-npm) -- optionalDependencies pattern for native binaries
- [Cloudflare: FinalizationRegistry](https://blog.cloudflare.com/we-shipped-finalizationregistry-in-workers-why-you-should-never-use-it/) -- why FinalizationRegistry is not reliable for resource cleanup
- Existing Quiver bindings: `bindings/dart/lib/src/ffi/library_loader.dart`, `bindings/python/src/quiverdb/_loader.py`, `bindings/dart/lib/src/database_read.dart`, `bindings/dart/lib/src/exceptions.dart` -- established patterns for DLL loading, memory management, and error handling
