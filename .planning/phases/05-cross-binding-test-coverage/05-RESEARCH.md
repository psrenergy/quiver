# Phase 5: Cross-Binding Test Coverage - Research

**Researched:** 2026-02-28
**Domain:** Test coverage for is_healthy/path across Julia, Dart, Python bindings + Python convenience helpers
**Confidence:** HIGH

## Summary

Phase 5 is a focused testing phase with two distinct work streams: (1) adding `is_healthy`/`path` wrapper methods and tests to Julia and Dart (Python already has both), and (2) confirming Python convenience helper test coverage satisfies PY-03. The Python work is already complete -- existing tests cover all 7 required methods. The Julia and Dart work requires both implementing the high-level wrapper methods (the C API FFI bindings already exist) and adding tests.

The key technical concern is the `quiver_database_path` dangling pointer: the C API returns `c_str()` from an internal `std::string`, so `path()` must be called before `close()`. Julia and Dart wrappers must copy the string immediately (Julia `unsafe_string`, Dart `Utf8.toDartString`), and tests must read path before closing.

**Primary recommendation:** Implement `is_healthy`/`path` wrappers in Julia `database.jl` and Dart `database.dart`, add tests to existing lifecycle test files, verify all 5 test suites pass.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Test depth for health/path: Claude's discretion, informed by Python's existing coverage
- Python already tests: `is_healthy` returns true on open DB, `path` returns correct value on file-based DB, both raise after close
- Julia and Dart need equivalent coverage added
- Existing Python tests already cover all 7 required methods for PY-03 -- no additional Python tests needed
- Tests located in: `test_database_read_scalar.py`, `test_database_read_vector.py`, `test_database_read_set.py`
- Add health/path tests to existing lifecycle test files:
  - Julia: `bindings/julia/test/test_database_lifecycle.jl`
  - Dart: `bindings/dart/test/database_lifecycle_test.dart`
- No new test files needed
- All five test suites (C++, C API, Julia, Dart, Python) must pass
- If a binding test fails for reasons unrelated to Phase 5, fix the issue as part of this phase
- Ship clean -- no known failures at milestone close

### Claude's Discretion
- Exact test scenarios for health/path in Julia and Dart (informed by Python's existing patterns)
- Whether to test in-memory vs file-based databases for path accessor
- Test naming and grouping within existing lifecycle files

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| TEST-01 | Julia, Dart, and Python bindings test `is_healthy()` and `path()` methods | Python already covered (lines 19, 22, 40, 44, 55-56 of `test_database_lifecycle.py`). Julia and Dart need wrapper methods added to `database.jl` / `database.dart` plus tests in lifecycle files. C API FFI bindings already exist in both. |
| PY-03 | Python tests cover `read_scalars_by_id`, `read_vectors_by_id`, `read_sets_by_id`, `read_element_by_id`, `read_vector_group_by_id`, `read_set_group_by_id`, `read_element_ids` | Already satisfied by existing tests in `test_database_read_scalar.py`, `test_database_read_vector.py`, `test_database_read_set.py`. No new work needed. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Julia Test stdlib | 1.x | Julia test framework (`@testset`, `@test`) | Built-in, used throughout Julia binding tests |
| Dart test package | 1.x | Dart test framework (`group`, `test`, `expect`) | Standard Dart testing, used throughout Dart binding tests |
| pytest | 8.x | Python test framework | Already configured and used for all Python binding tests |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| dart:ffi / package:ffi | - | Dart FFI for calling C API | Needed for `isHealthy`/`path` wrapper implementations |
| Julia ccall | - | Julia FFI for calling C API | Already used via `c_api.jl` for `quiver_database_is_healthy`/`quiver_database_path` |

### Alternatives Considered
None -- this phase uses existing test infrastructure exclusively.

## Architecture Patterns

### Pattern 1: Julia Wrapper Method (is_healthy)
**What:** Julia wrapper calling C API with out-parameter
**When to use:** For `is_healthy` and `path` in `database.jl`
**Example:**
```julia
# In bindings/julia/src/database.jl
function is_healthy(db::Database)
    out = Ref{Cint}(0)
    check(C.quiver_database_is_healthy(db.ptr, out))
    return out[] != 0
end
```
Source: Pattern from `current_version` in same file (lines 44-48).

### Pattern 2: Julia Wrapper Method (path)
**What:** Julia wrapper that reads const char* and copies to String immediately
**When to use:** For `path` in `database.jl`
**Example:**
```julia
# In bindings/julia/src/database.jl
function path(db::Database)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_database_path(db.ptr, out))
    return unsafe_string(out[])
end
```
Source: Pattern from string return functions in `database_read.jl`.

### Pattern 3: Dart Wrapper Method (isHealthy)
**What:** Dart wrapper calling C API with Arena-allocated out-parameter
**When to use:** For `isHealthy` and `path` in `database.dart`
**Example:**
```dart
// In bindings/dart/lib/src/database.dart (near describe/close)
bool isHealthy() {
  _ensureNotClosed();
  final arena = Arena();
  try {
    final outHealthy = arena<Int>();
    check(bindings.quiver_database_is_healthy(_ptr, outHealthy));
    return outHealthy.value != 0;
  } finally {
    arena.releaseAll();
  }
}
```
Source: Pattern from `currentVersion()` in `database_metadata.dart` (lines 339-352).

### Pattern 4: Dart Wrapper Method (path)
**What:** Dart wrapper that reads const char* and copies to Dart String immediately
**When to use:** For `path` in `database.dart`
**Example:**
```dart
// In bindings/dart/lib/src/database.dart
String path() {
  _ensureNotClosed();
  final arena = Arena();
  try {
    final outPath = arena<Pointer<Char>>();
    check(bindings.quiver_database_path(_ptr, outPath));
    return outPath.value.cast<Utf8>().toDartString();
  } finally {
    arena.releaseAll();
  }
}
```
Source: Pattern from string read functions in `database_read.dart`.

### Pattern 5: Julia Test (lifecycle testset)
**What:** `@testset` blocks within `test_database_lifecycle.jl`
**When to use:** For `is_healthy` and `path` tests
**Example:**
```julia
@testset "is_healthy" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = Quiver.from_schema(":memory:", path_schema)
    @test Quiver.is_healthy(db) == true
    Quiver.close!(db)
end
```
Source: Pattern from `Current Version` testset in same file (lines 42-56).

### Pattern 6: Dart Test (lifecycle group)
**What:** `group`/`test` blocks within `database_lifecycle_test.dart`
**When to use:** For `isHealthy` and `path` tests
**Example:**
```dart
group('Database isHealthy', () {
  test('returns true for open database', () {
    final db = Database.fromSchema(':memory:', schemaPath);
    try {
      expect(db.isHealthy(), isTrue);
    } finally {
      db.close();
    }
  });
});
```
Source: Pattern from `Database currentVersion` group in same file (lines 103-128).

### Anti-Patterns to Avoid
- **Reading path after close:** The C API `quiver_database_path` returns a `const char*` into internal `std::string`. After `quiver_database_close`, this pointer is dangling. Always read and copy path before closing.
- **Testing path on in-memory databases:** `:memory:` databases return `:memory:` as path, which is valid but less interesting. File-based tests verify actual file path resolution.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| String memory management | Manual string lifetime tracking | `unsafe_string` (Julia), `toDartString` (Dart) | Both copy immediately, avoiding dangling pointer |
| Test infrastructure | Custom test setup | Existing lifecycle test files + patterns | Already established patterns in all 3 bindings |

**Key insight:** The C API FFI layer already exists in both Julia (`c_api.jl`) and Dart (`bindings.dart`). Only the high-level wrapper methods and tests are missing.

## Common Pitfalls

### Pitfall 1: Dangling Pointer from quiver_database_path
**What goes wrong:** Test reads `path()` after `close()`, gets garbage or crash
**Why it happens:** C API returns `const char*` pointing into internal `std::string` that is destroyed on close
**How to avoid:** Julia `unsafe_string()` and Dart `toDartString()` both copy the string immediately. Tests must call `path()` before `close()`.
**Warning signs:** Segfault or garbage string in path tests after close

### Pitfall 2: Missing Wrapper Methods
**What goes wrong:** Tests call `Quiver.is_healthy(db)` or `db.isHealthy()` but the method doesn't exist
**Why it happens:** The C API FFI bindings exist but the high-level wrapper methods in `database.jl`/`database.dart` were never added
**How to avoid:** Implement wrapper methods BEFORE writing tests
**Warning signs:** `MethodError` (Julia) or `NoSuchMethodError` (Dart)

### Pitfall 3: Dart path() Missing Import
**What goes wrong:** `Utf8` extension methods not available
**Why it happens:** `database.dart` already imports `package:ffi/ffi.dart` (line 3), so this should not be an issue
**How to avoid:** Verify import exists at top of `database.dart`
**Warning signs:** Compilation error on `cast<Utf8>().toDartString()`

### Pitfall 4: Pre-existing Test Failures
**What goes wrong:** test-all.bat fails due to issues unrelated to Phase 5
**Why it happens:** Other phases may have introduced regressions or environment issues
**How to avoid:** Run `scripts/test-all.bat` early to establish baseline. Fix any failures before adding new tests.
**Warning signs:** Test failures in suites that Phase 5 did not modify

### Pitfall 5: Julia path() with :memory: Database
**What goes wrong:** `path()` returns `:memory:` which may not be useful for assertions
**Why it happens:** In-memory databases have no file path
**How to avoid:** Use file-based database (with `mktempdir`) for path tests, or assert `:memory:` if testing in-memory
**Warning signs:** Assertion failure on path content

## Code Examples

### Julia: Complete is_healthy and path Wrappers
```julia
# Add to bindings/julia/src/database.jl (after describe function)

function is_healthy(db::Database)
    out = Ref{Cint}(0)
    check(C.quiver_database_is_healthy(db.ptr, out))
    return out[] != 0
end

function path(db::Database)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_database_path(db.ptr, out))
    return unsafe_string(out[])
end
```

### Julia: Complete Test Block
```julia
# Add to bindings/julia/test/test_database_lifecycle.jl (before final `end`)

@testset "is_healthy" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = Quiver.from_schema(":memory:", path_schema)
    @test Quiver.is_healthy(db) == true
    Quiver.close!(db)
end

@testset "path" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db_path = joinpath(mktempdir(), "test.db")
    db = Quiver.from_schema(db_path, path_schema)
    result = Quiver.path(db)
    @test result isa String
    @test occursin("test.db", result)
    Quiver.close!(db)
end
```

### Dart: Complete isHealthy and path Wrappers
```dart
// Add to bindings/dart/lib/src/database.dart (near describe method)

/// Returns true if the database passes integrity checks.
bool isHealthy() {
  _ensureNotClosed();
  final arena = Arena();
  try {
    final outHealthy = arena<Int>();
    check(bindings.quiver_database_is_healthy(_ptr, outHealthy));
    return outHealthy.value != 0;
  } finally {
    arena.releaseAll();
  }
}

/// Returns the database file path.
String path() {
  _ensureNotClosed();
  final arena = Arena();
  try {
    final outPath = arena<Pointer<Char>>();
    check(bindings.quiver_database_path(_ptr, outPath));
    return outPath.value.cast<Utf8>().toDartString();
  } finally {
    arena.releaseAll();
  }
}
```

### Dart: Complete Test Block
```dart
// Add to bindings/dart/test/database_lifecycle_test.dart (before closing of main)

group('Database isHealthy', () {
  test('returns true for open database', () {
    final db = Database.fromSchema(':memory:', schemaPath);
    try {
      expect(db.isHealthy(), isTrue);
    } finally {
      db.close();
    }
  });
});

group('Database path', () {
  test('returns path string for file-based database', () {
    final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
    final dbPath = path.join(tempDir.path, 'test.db');
    try {
      final db = Database.fromSchema(dbPath, schemaPath);
      try {
        final result = db.path();
        expect(result, contains('test.db'));
      } finally {
        db.close();
      }
    } finally {
      tempDir.deleteSync(recursive: true);
    }
  });
});
```

## Existing File Inventory

### Files That Need Modification
| File | Change | Purpose |
|------|--------|---------|
| `bindings/julia/src/database.jl` | Add `is_healthy` and `path` functions | Wrapper methods for TEST-01 |
| `bindings/dart/lib/src/database.dart` | Add `isHealthy` and `path` methods | Wrapper methods for TEST-01 |
| `bindings/julia/test/test_database_lifecycle.jl` | Add test `@testset` blocks | Test coverage for TEST-01 |
| `bindings/dart/test/database_lifecycle_test.dart` | Add test `group` blocks | Test coverage for TEST-01 |

### Files That Are Already Complete (No Changes)
| File | Status | Requirement |
|------|--------|-------------|
| `bindings/python/src/quiverdb/database.py` | `is_healthy` (line 101) and `path` (line 85) already implemented | TEST-01 |
| `bindings/python/tests/test_database_lifecycle.py` | `test_is_healthy_returns_true` (line 55), `test_path_returns_string` (line 43), `test_operations_after_close_raise` (line 36) already exist | TEST-01 |
| `bindings/python/tests/test_database_read_scalar.py` | `test_read_scalars_by_id`, `test_read_element_ids` exist | PY-03 |
| `bindings/python/tests/test_database_read_vector.py` | `test_read_vectors_by_id`, `test_read_vector_group_by_id` exist | PY-03 |
| `bindings/python/tests/test_database_read_set.py` | `test_read_sets_by_id`, `test_read_set_group_by_id`, `test_read_element_by_id` exist | PY-03 |

### C API Layer (Already Complete)
| File | Function | Status |
|------|----------|--------|
| `src/c/database.cpp` | `quiver_database_is_healthy` (line 37) | Implemented |
| `src/c/database.cpp` | `quiver_database_path` (line 44) | Implemented (dangling pointer concern acknowledged) |
| `include/quiver/c/database.h` | Declarations (lines 45-46) | Declared |

### FFI Bindings (Already Generated)
| File | Status |
|------|--------|
| `bindings/julia/src/c_api.jl` | `quiver_database_is_healthy` (line 112), `quiver_database_path` (line 116) |
| `bindings/dart/lib/src/ffi/bindings.dart` | `quiver_database_is_healthy` (line 178), `quiver_database_path` (line 195) |
| `bindings/python/src/quiverdb/_c_api.py` | Both declared (lines 49-50) |

## Open Questions

1. **Julia `mktempdir` cleanup**
   - What we know: Julia's `mktempdir()` creates a temp directory. The standard pattern is `mktempdir() do dir ... end` for auto-cleanup, or manual `rm` after.
   - What's unclear: Whether the lifecycle test file already uses `mktempdir` pattern
   - Recommendation: Use `mktempdir() do dir ... end` block for automatic cleanup in the path test

## Sources

### Primary (HIGH confidence)
- Direct code inspection of all binding source files, test files, and C API implementation
- `bindings/julia/src/database.jl` -- confirms `is_healthy`/`path` wrappers missing
- `bindings/dart/lib/src/database.dart` -- confirms `isHealthy`/`path` wrappers missing
- `bindings/python/tests/test_database_lifecycle.py` -- confirms Python coverage exists
- `bindings/python/tests/test_database_read_*.py` -- confirms PY-03 coverage exists
- `src/c/database.cpp` lines 37-49 -- C API implementation confirms dangling pointer in `path`

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - Using existing test frameworks already in place
- Architecture: HIGH - All patterns derived from existing code in the same files
- Pitfalls: HIGH - Dangling pointer issue documented in STATE.md and verified in C API source

**Research date:** 2026-02-28
**Valid until:** 2026-03-30 (stable -- no external dependencies or fast-moving libraries)
