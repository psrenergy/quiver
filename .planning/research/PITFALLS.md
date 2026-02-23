# Domain Pitfalls: Python CFFI Bindings for Quiver

**Domain:** Adding Python CFFI bindings to existing C API (multi-language FFI system)
**Researched:** 2026-02-23
**Confidence:** MEDIUM — web access unavailable; based on C API header analysis, existing Julia/Dart binding patterns, and training knowledge of CFFI (cutoff August 2025). Core pitfalls are well-established FFI mechanics, not rapidly-changing ecosystem.

---

## Critical Pitfalls

Mistakes that cause memory corruption, data loss, or silent failures requiring significant rework.

---

### Pitfall 1: Missing C-Owned Memory Free Calls (Memory Leak)

**What goes wrong:** Python's garbage collector has no knowledge of memory allocated by `new[]` in the C++ layer. If the Python binding reads a result (e.g., `int64_t*` from `quiver_database_read_scalar_integers`) and copies it into a Python list, but never calls `quiver_database_free_integer_array`, the C-allocated buffer leaks permanently. In long-running processes or test suites, this accumulates until OOM or the process exits.

**Why it happens:** CFFI gives you a `cdata` pointer back. The natural instinct is to iterate it and build a Python list, then let the `cdata` go out of scope — but going out of scope does NOT invoke any C free function. The memory lives in C's heap, invisible to Python's GC.

**Consequences:**
- Silent memory leak; no crash or exception — the worst kind
- Test suites with thousands of reads silently consume gigabytes
- Valgrind or process memory profiling required to detect

**This API's specific exposure (every read path):**
```
quiver_database_free_integer_array(int64_t*)
quiver_database_free_float_array(double*)
quiver_database_free_string_array(char**, size_t)
quiver_database_free_integer_vectors(int64_t**, size_t*, size_t)
quiver_database_free_float_vectors(double**, size_t*, size_t)
quiver_database_free_string_vectors(char***, size_t*, size_t)
quiver_database_free_scalar_metadata(quiver_scalar_metadata_t*)
quiver_database_free_group_metadata(quiver_group_metadata_t*)
quiver_database_free_scalar_metadata_array(quiver_scalar_metadata_t*, size_t)
quiver_database_free_group_metadata_array(quiver_group_metadata_t*, size_t)
quiver_database_free_time_series_data(char**, int*, void**, size_t, size_t)
quiver_database_free_time_series_files(char**, char**, size_t)
quiver_element_free_string(char*)
```

**Prevention:** Use `try/finally` blocks that unconditionally call the corresponding free function, regardless of whether the read succeeded or raised. Never rely on Python variable scope to trigger cleanup. Pattern:

```python
out_values = ffi.new("int64_t **")
out_count = ffi.new("size_t *")
_check(lib.quiver_database_read_scalar_integers(db, collection, attribute, out_values, out_count))
count = out_count[0]
try:
    result = [out_values[0][i] for i in range(count)]
finally:
    if out_values[0] != ffi.NULL:
        lib.quiver_database_free_integer_array(out_values[0])
return result
```

**Detection:** Write a test that calls a read function in a tight loop (10,000 iterations) and monitors process RSS memory. A leak will be visible within seconds.

**Phase:** Phase 1 (core read operations). Must be correct from the start — retrofitting is tedious across all read paths.

---

### Pitfall 2: String Encoding Mismatch at the C Boundary (UTF-8)

**What goes wrong:** CFFI passes Python strings to C as UTF-8 byte sequences only when you explicitly encode. If you pass a Python `str` directly without `.encode('utf-8')`, CFFI (ABI mode) treats it as a C string but may silently truncate or misbehave on non-ASCII characters. Conversely, when reading C strings back, `ffi.string(ptr)` returns `bytes`, not `str` — calling it and assuming a `str` causes `TypeError` downstream or, worse, using it without decoding produces bytes that look correct until a non-ASCII character appears.

**Why it happens:**
- Python 3 strings are Unicode; C strings are bytes. CFFI does not auto-convert in ABI mode.
- `ffi.string()` returns `bytes`. `ffi.string().decode('utf-8')` is required to get `str`.
- SQLite stores text as UTF-8 internally, so the C layer returns UTF-8. Round-tripping requires consistent encode/decode.

**Consequences:**
- Non-ASCII labels (e.g., element labels with accented characters) silently corrupt or raise `UnicodeDecodeError` only at read time, not write time — making bugs hard to correlate.
- Test suites using only ASCII miss this entirely.

**This API's specific exposure:** Every `const char*` parameter (`collection`, `attribute`, `label`, SQL strings, path strings) and every `char*` or `char**` return value.

**Prevention:**
- Establish a single `_encode(s: str) -> bytes` helper used for all inputs: `s.encode('utf-8')`.
- Establish a single `_decode(ptr) -> str` helper for all string outputs: `ffi.string(ptr).decode('utf-8')`.
- Write at least one test with non-ASCII label values (e.g., `"café"`, `"日本語"`) in Phase 1.

**Detection:** Add a test creating an element with a non-ASCII `label` value and reading it back; verify the round-trip is identical.

**Phase:** Phase 1. Encoding helpers should be infrastructure, not an afterthought.

---

### Pitfall 3: DLL Load Failure Due to Dependency Chain (Windows)

**What goes wrong:** On Windows, `libquiver_c.dll` (the C API library) depends on `libquiver.dll` (the C++ core library). When Python calls `ffi.dlopen("libquiver_c.dll")`, Windows attempts to resolve `libquiver.dll` via `LoadLibrary`. If `libquiver.dll` is not on the system PATH or in the same directory as `libquiver_c.dll`, the load silently fails with `OSError: [WinError 126] The specified module could not be found` — which points to `libquiver_c.dll` but the actual missing dependency is `libquiver.dll`.

**Why it happens:** Windows DLL resolution does not have an equivalent of Linux's `RPATH`. The loader searches: DLL directory, system directories, then PATH. If `libquiver.dll` is in `build/bin/` but that directory is not on PATH when Python starts, the load fails.

**The Dart binding already solved this (reference implementation):**
```dart
// On Windows, load the core library first so the C API library can find it
if (Platform.isWindows) {
    final corePath = '$libDir/$_coreLibraryName';
    if (File(corePath).existsSync()) {
        DynamicLibrary.open(corePath);  // explicit pre-load
    }
}
_cachedLibrary = DynamicLibrary.open(nativeAssetsPath);
```

**Prevention:** In the Python binding's library loader, explicitly load `libquiver.dll` (or `.so`/`.dylib` equivalents) before loading `libquiver_c.dll`:

```python
import ctypes  # for side-effect pre-load
if sys.platform == "win32":
    ctypes.CDLL(str(build_dir / "libquiver.dll"))  # pre-load dependency
lib = ffi.dlopen(str(build_dir / "libquiver_c.dll"))
```

**Detection:** Test on a clean Windows environment where the build directory is not on PATH. The error message will not name `libquiver.dll` — use `dumpbin /dependents libquiver_c.dll` to inspect.

**Phase:** Phase 1 (library loader). Windows-specific, but the Dart binding proves this is a real requirement for this codebase.

---

### Pitfall 4: CFFI Declaration Mismatch with Actual C API

**What goes wrong:** The CFFI binding declares function signatures in a string passed to `ffi.cdef()`. If any declaration diverges from the actual compiled C API — wrong parameter order, wrong type (e.g., `int` instead of `int64_t`, `unsigned int` instead of `size_t`), missing `const`, or a struct field missing — the result is undefined behavior: wrong values read, memory corruption, or a crash with no Python traceback.

**Why it happens:**
- CFFI ABI mode does not parse headers; it relies on manually maintained declaration strings.
- The C API has types like `size_t`, `int64_t`, `bool`, and complex structs (`quiver_csv_export_options_t`) with pointer-to-pointer-to-pointer returns (`char***`, `void***`). One wrong indirection level is enough to corrupt the stack.
- The API will evolve (this is a WIP project at v0.3.0, breaking changes acceptable).

**This API's specific exposure:**
- `void***` for `out_column_data` in `quiver_database_read_time_series_group` — three levels of pointer indirection, easy to declare as `void**`
- `char****` for `out_vectors` in `quiver_database_read_vector_strings` — four levels
- `quiver_csv_export_options_t` struct with parallel arrays and a `size_t` count
- `size_t` vs `int` confusion in `quiver_element_set_array_*` (`count` is `int32_t`, not `size_t`)
- `bool` (C99 `_Bool`) in `quiver_database_in_transaction` — declare as `bool` with `#include <stdbool.h>` or as `int`

**Prevention:**
- Use CFFI's API mode (compile-time parsing) if possible — it reads the actual headers and cannot mismatch. If ABI mode is required, generate declarations from headers using `cffi`'s preprocessor support.
- If declarations are written by hand, keep them in a single file that can be diffed against the headers when the C API changes.
- Write a "smoke test" for every function immediately after adding its declaration — even a deliberately wrong call that triggers an error — to confirm the function is callable at all.

**Phase:** Phase 1. Every declaration added in later phases needs the same discipline.

---

### Pitfall 5: Thread-Local Error State with Python Threading

**What goes wrong:** The C API uses thread-local storage for error messages (`quiver_get_last_error()` returns a TLS string, as documented in `common.h`: "Thread-local storage"). If a Python program uses multiple threads (e.g., a web server with a thread pool), an error from Thread A can be consumed by Thread B's error check, or the error from Thread B can overwrite Thread A's. The result is either a wrong error message being raised, or `QUIVER_ERROR` returned with an empty string from `quiver_get_last_error()` — both are confusing.

**Why it happens:** Python threads map to OS threads; TLS works correctly per-OS-thread. The problem is not TLS itself but:
1. Checking `quiver_get_last_error()` outside the try/catch that called the failing function
2. Calling `quiver_clear_last_error()` before checking (e.g., in a cleanup path)
3. Using `asyncio` with thread executors where the thread that made the call may not be the thread that reads the error

**Prevention:**
- Always read `quiver_get_last_error()` immediately after a failed call, before any other C API call that might overwrite it.
- Document that `Database` objects are not thread-safe (SQLite connections are not safe to share across threads by default).
- Never cache or defer the error string read.

**Detection:** Write a threaded test that opens two databases and performs concurrent writes; verify that exceptions from each thread contain the correct error message for that thread's operation.

**Phase:** Phase 1 (error handling infrastructure). The `check()` helper must read the error immediately.

---

## Moderate Pitfalls

### Pitfall 6: `quiver_database_close` Called via Finalizer While C++ Destructor Races

**What goes wrong:** Python's `__del__` method (or a `weakref` finalizer) may be called at any point during interpreter shutdown, including after the DLL has been unloaded. Calling `quiver_database_close()` after the DLL is unloaded causes a segfault (Windows) or `SIGSEGV` (Linux/macOS). Julia's binding avoids this by setting `ptr = C_NULL` and guarding with a null check in the finalizer.

**Prevention:** Follow Julia's pattern exactly:
```python
def _finalizer(db_ref):
    ptr = db_ref()
    if ptr is not None and ptr != ffi.NULL:
        lib.quiver_database_close(ptr)

# On explicit close:
def close(self):
    if self._ptr != ffi.NULL:
        lib.quiver_database_close(self._ptr)
        self._ptr = ffi.NULL  # prevent double-free from finalizer
```

Nullify the pointer on explicit `close()` so the finalizer is a no-op. Provide an explicit `close()` and a context manager (`__enter__`/`__exit__`) so tests do not rely on finalizer timing.

**Phase:** Phase 1 (Database lifecycle).

---

### Pitfall 7: `int32_t` Count in Element Array Setters vs `size_t` Elsewhere

**What goes wrong:** Most array counts in the C API use `size_t`. However, `quiver_element_set_array_integer`, `quiver_element_set_array_float`, and `quiver_element_set_array_string` all use `int32_t count` — signed 32-bit, not `size_t`. Declaring these in CFFI with `size_t` instead of `int32_t` passes the wrong type on platforms where `size_t` is 64-bit (all 64-bit platforms). For small lists this is harmless (value fits). For large arrays (> 2 billion elements, unrealistic) it corrupts. But the type mismatch itself can cause subtle issues on big-endian platforms or in sanitizer builds.

**Prevention:** Declare all three `set_array_*` functions with `int32_t count`, not `size_t count`. Cross-check declaration against the actual header at `include/quiver/c/element.h`.

**Phase:** Phase 1 (Element builder).

---

### Pitfall 8: `quiver_csv_export_options_t` Struct Lifetime for Parallel Arrays

**What goes wrong:** `quiver_csv_export_options_t` holds pointers to caller-owned arrays (`enum_attribute_names`, `enum_entry_counts`, `enum_values`, `enum_labels`). The docstring says "All pointers are borrowed -- caller owns the memory, function reads during call only." In Python, if you create numpy arrays or Python lists and extract CFFI pointers from them, those Python objects must remain alive for the entire duration of the `quiver_database_export_csv` call. If the Python GC collects the arrays before the call completes (possible in a complex expression), the C code reads freed memory.

**Prevention:** Assign all input arrays to named local variables before the `ffi.cast` or `ffi.from_buffer` calls, ensuring they are in scope and referenced until the function returns. Do not build the struct in a single chained expression.

**Phase:** Phase 2 (CSV export), if CSV is in a later phase.

---

### Pitfall 9: Time Series `void***` Column Data — Pointer Type Dispatch

**What goes wrong:** `quiver_database_read_time_series_group` returns `void** out_column_data` where each `out_column_data[i]` is typed by `out_column_types[i]` (INTEGER → `int64_t*`, FLOAT → `double*`, STRING/DATE_TIME → `char**`). In Python CFFI, `void**` comes back as an array of `void*`. To read the actual values, you must cast each `out_column_data[i]` to the correct pointer type using `ffi.cast("int64_t *", ptr)` etc. Getting the type wrong (e.g., casting a `char**` column as `int64_t*`) reads garbage without raising an exception.

**Prevention:** Implement a dispatch function that reads `column_types[i]` and applies the correct cast before reading values. Test with a time series schema that has at least one column of each supported type (INTEGER, FLOAT, STRING/DATE_TIME).

**Phase:** Phase covering time series read (likely Phase 2 or 3).

---

### Pitfall 10: Test Isolation — Shared SQLite State via On-Disk Databases

**What goes wrong:** Tests that use on-disk SQLite databases (file paths) instead of `:memory:` leave residual database files if a test fails mid-way before cleanup. If the next test run finds the leftover file (which has existing schema from the previous run), `quiver_database_from_schema` will fail with a schema mismatch or succeed but contain stale data from the prior run, causing false positives or false negatives.

**Why it happens:** The C++ `from_schema` path opens an existing file if present and tries to apply the schema — whether this creates a fresh database or errors depends on state. Dart and Julia tests consistently use `:memory:` for lifecycle and read tests, only using on-disk paths when testing path-sensitive features.

**Prevention:**
- Default all tests to `:memory:` databases.
- For tests that require on-disk files (e.g., migration tests, CSV export tests), use pytest's `tmp_path` fixture to get a per-test temporary directory that is automatically cleaned up.
- Never hard-code file paths in tests.

**Phase:** Phase 1 (test infrastructure). Establish the pattern before any tests are written.

---

### Pitfall 11: Parameterized Query `void* const*` Parallel Arrays — Python Pointer Stability

**What goes wrong:** `quiver_database_query_string_params` (and the integer/float variants) accept `const void* const* param_values` where each element points to a typed value (`int64_t*`, `double*`, or `const char*`). In Python, constructing this requires allocating each individual value in C memory (via `ffi.new`) and collecting pointers into a `ffi.new("void*[]", n)` array. If you use temporary Python integers or floats and take their address via `ffi.cast`, those temporaries may be GC'd before the call returns.

**Prevention:** Allocate all parameter values with `ffi.new` and hold references to each in a list until the call returns:
```python
param_refs = []
param_ptrs = ffi.new("void*[]", len(params))
for i, (ptype, pval) in enumerate(params):
    if ptype == INTEGER:
        ref = ffi.new("int64_t *", pval)
    elif ptype == FLOAT:
        ref = ffi.new("double *", pval)
    elif ptype == STRING:
        ref = ffi.new("char[]", pval.encode("utf-8"))
    param_refs.append(ref)      # keep alive
    param_ptrs[i] = ref
# call with param_refs still in scope
```

**Phase:** Phase covering query operations.

---

## Minor Pitfalls

### Pitfall 12: `ffi.string()` vs `ffi.string(ptr, maxlen)` on Null Pointer

**What goes wrong:** Calling `ffi.string(ptr)` on `ffi.NULL` raises `ValueError` with an unhelpful message. Null pointers appear legitimately when optional values are not present (e.g., `default_value` in `quiver_scalar_metadata_t` is NULL for fields with no default, `references_collection` is NULL for non-foreign-key fields).

**Prevention:** Always check `ptr != ffi.NULL` before calling `ffi.string(ptr)`. Write a `_decode_optional(ptr) -> Optional[str]` helper that handles the null check centrally.

**Phase:** Phase 1 (metadata reading).

---

### Pitfall 13: Metadata Struct Fields Are C-Owned, Not Python-Owned

**What goes wrong:** `quiver_scalar_metadata_t` and `quiver_group_metadata_t` contain `const char*` fields pointing into C-allocated strings. Reading those fields via `ffi.string(metadata.name)` is correct. But after calling `quiver_database_free_scalar_metadata()`, those pointers are dangling — any Python variable that cached the raw `cdata` (not the decoded Python string) now points to freed memory. Accessing it is undefined behavior.

**Prevention:** Decode all string fields from the struct into Python `str` objects immediately after the C call returns, before the free call. Do not store raw `cdata` struct pointers beyond their scope.

**Phase:** Phase covering metadata operations.

---

### Pitfall 14: Package Name Drift (`margaux` vs `quiver`)

**What goes wrong:** The existing `pyproject.toml` names the package `margaux`. If tests, import paths, or CI scripts reference `margaux`, renaming to `quiver` will break them. Since the stub is essentially empty, the rename is straightforward now but will require updating `pyproject.toml`, any `import margaux` statements, and documentation.

**Prevention:** Rename before writing any meaningful code. The project context confirms the name must be `quiver`.

**Phase:** Phase 1 (project setup), immediate.

---

### Pitfall 15: `bool` vs `int` for `out_active` in `in_transaction`

**What goes wrong:** `quiver_database_in_transaction` uses `bool* out_active` (C99 `_Bool`). CFFI requires an explicit `#include <stdbool.h>` directive in the cdef preamble (or definition as `_Bool`) to handle `bool` correctly. If declared as `int*`, reading the value is functionally equivalent on most platforms (0 or 1), but it is technically a type mismatch. `out_healthy` in `quiver_database_is_healthy` uses `int*` (not `bool*`) and is safe to declare as `int`.

**Prevention:** Check the actual header type (`bool` vs `int`) for each boolean output parameter. Use consistent declaration in CFFI cdef.

**Phase:** Phase 1 (lifecycle/transaction).

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Library loader | DLL dependency chain (libquiver.dll not found before libquiver_c.dll) | Pre-load core lib explicitly on Windows; reference Dart's `library_loader.dart` |
| All read operations | Missing free call → memory leak | Unconditional `try/finally` with matching free function |
| String parameters | Non-UTF-8 encoding or raw bytes passed to C | `_encode()` / `_decode()` helpers used consistently |
| Error handling | Error string consumed before check, or wrong thread reads it | Read `quiver_get_last_error()` atomically inside `check()` |
| Element builder | `int32_t` count vs `size_t` in `set_array_*` functions | Declare with correct type; verify against element.h |
| Parameterized queries | Python temporaries GC'd before call returns | Hold `ffi.new` refs in a list for call duration |
| Time series read | `void**` dispatch by column type; wrong cast reads garbage | Type-dispatch helper; test all column types |
| CSV export | Python arrays freed before struct used by C | Named locals for all arrays; no chained expressions |
| Test infrastructure | On-disk DB files leaked between test runs | pytest `tmp_path` for all file-based tests; default to `:memory:` |
| Metadata reading | Null `const char*` fields in structs cause crash on `ffi.string(NULL)` | `_decode_optional()` helper for all nullable string fields |
| Database lifecycle | Finalizer called after DLL unloaded → segfault | Nullify pointer on `close()`; context manager as primary pattern |

---

## Sources

- C API headers analyzed: `include/quiver/c/common.h`, `include/quiver/c/database.h`, `include/quiver/c/element.h`, `include/quiver/c/options.h` — HIGH confidence (direct inspection)
- Existing binding patterns: `bindings/julia/src/` and `bindings/dart/lib/src/` — HIGH confidence (direct inspection)
- CFFI memory model, string handling, void* dispatch: training knowledge (CFFI docs, cutoff August 2025) — MEDIUM confidence (established CFFI mechanics, not rapidly changing)
- Thread-local error storage: documented in `include/quiver/c/common.h` comment — HIGH confidence
- Windows DLL dependency chain: documented by Dart `library_loader.dart` comment and implementation — HIGH confidence for this codebase
- `int32_t` count in element setters: verified in `include/quiver/c/element.h` — HIGH confidence
