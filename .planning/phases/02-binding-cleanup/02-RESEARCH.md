# Phase 2: Binding Cleanup - Research

**Researched:** 2026-03-01
**Domain:** Dead code removal across Julia, Dart, and Python bindings
**Confidence:** HIGH

## Summary

Phase 2 is a targeted cleanup phase with a well-defined primary target (Julia `quiver_database_sqlite_error`) and a light cross-binding audit. The codebase investigation reveals that the bindings are already very clean. The Julia dead code is confirmed (one line, no references anywhere). The cross-binding audit found exactly one additional dead code item: `encode_string` in Python's `_helpers.py`. Dart and Python have no equivalent `sqlite_error` patterns.

The implementation is mechanical: delete identified dead lines, verify tests still pass. No architectural changes, no dependency updates, no risk of regression beyond obvious syntax errors (e.g., accidentally deleting the wrong line).

**Primary recommendation:** Delete the two confirmed dead code items (Julia `quiver_database_sqlite_error`, Python `encode_string`), then run all binding test suites to verify.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Delete `quiver_database_sqlite_error` from Julia `exceptions.jl` (dead code -- defined but never called)
- **Keep** `helper_maps.jl` and `test_helper_maps.jl` -- user actively calls `scalar_relation_map`/`set_relation_map` from application code
- Do NOT export `scalar_relation_map`/`set_relation_map` -- keep current access pattern via `Quiver.scalar_relation_map`
- Light audit across ALL bindings (Julia, Dart, Python) for additional dead code
- Any dead code found in any binding gets cleaned up in this phase
- Delete `quiver_database_sqlite_error` one-liner (line 5)
- Review `check()` function for potential improvements during implementation
- Keep the `@warn` fallback guard for empty error messages -- user wants the defensive behavior
- Check Dart and Python exception modules for similar dead `sqlite_error` patterns
- Leave `helper_maps.jl` completely as-is -- no changes to functions, docstrings, or exports
- Julia-only convenience -- no cross-binding parity needed for Dart/Python

### Claude's Discretion
- Specific improvements to `check()` function if warranted during review
- Which dead code items found during the cross-binding audit to include vs defer
- Order of operations for cleanup across bindings

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| JUL-01 | Delete quiver_database_sqlite_error from exceptions.jl | Confirmed dead: defined on line 5, zero references in all source and test files |
| JUL-05 | All existing tests pass across all bindings (Julia, Dart, Python) | Test commands documented; no structural changes needed, just line deletions |
| BIND-01 | Light audit of all bindings for dead code -- clean up any findings | Audit complete: found `encode_string` in Python `_helpers.py` as only additional dead code |
</phase_requirements>

## Audit Findings

### Confirmed Dead Code

| Binding | File | Item | Evidence |
|---------|------|------|----------|
| Julia | `bindings/julia/src/exceptions.jl:5` | `quiver_database_sqlite_error(msg::String) = throw(DatabaseException(msg))` | Defined but never called anywhere in source or tests. Grep across entire repo returns zero usage. |
| Python | `bindings/python/src/quiverdb/_helpers.py:20-22` | `def encode_string(s: str) -> bytes:` | Defined but never imported or called. All Python files use `.encode("utf-8")` inline instead. |

### Verified Clean (No Dead Code Found)

| Binding | Area Audited | Result |
|---------|-------------|--------|
| Julia | All 18 source files in `bindings/julia/src/` | All functions are called from tests or other modules. `helper_maps.jl` actively used per user. |
| Julia | Exports in `Quiver.jl` | All exported symbols (`Element`, `Database`, `LuaRunner`, `DatabaseException`, `ScalarMetadata`, `GroupMetadata`, data type constants) are used. |
| Dart | `exceptions.dart` | No `sqlite_error` equivalent. `LuaException` is actively used by `lua_runner.dart` and tested. |
| Dart | All 12 source files in `bindings/dart/lib/src/` | All public methods are tested. No unused functions found. |
| Dart | `quiverdb.dart` exports | All exports correspond to actively used classes/extensions. |
| Python | `exceptions.py` | No `sqlite_error` equivalent. `QuiverError` is used throughout. |
| Python | `_helpers.py` (excluding `encode_string`) | `check`, `decode_string`, `decode_string_or_none` all have callers. |
| Python | `__init__.py` exports | All `__all__` entries correspond to real classes/functions. |
| Python | `_c_api.py` CFFI declarations | Contains `quiver_database_open` and element introspection functions not called by Python code, but these are CFFI cdef declarations (not executable code). CFFI ABI-mode declares all available symbols regardless of usage. Not dead code. |
| Python | `_declarations.py` | Auto-generated reference file from generator. Not dead code. |
| Python | `_loader.py` | All functions actively used. `__main__` block is diagnostic. |

### check() Function Review

Per user's discretion area, the `check()` functions across bindings were reviewed:

| Binding | File | Assessment |
|---------|------|------------|
| Julia | `exceptions.jl:13-23` | Clean. Has `@warn` guard for empty errors (user wants to keep this). Type annotation `check(err)` is loose but idiomatic Julia. No improvement warranted. |
| Dart | `exceptions.dart:6-17` | Clean. Has `print(WARNING...)` for empty errors matching Julia's defensive pattern. No improvement warranted. |
| Python | `_helpers.py:7-17` | Clean. Uses `detail or "Unknown error"` for empty fallback. Structurally consistent with Julia/Dart. No improvement warranted. |

**Conclusion:** The `check()` functions are consistent across all three bindings and follow the same defensive pattern. No changes recommended.

## Architecture Patterns

### Deletion Pattern
Both deletions are single-function removals with no downstream dependencies:

**Julia `quiver_database_sqlite_error`:**
- Location: `bindings/julia/src/exceptions.jl`, line 5
- One-liner function: `quiver_database_sqlite_error(msg::String) = throw(DatabaseException(msg))`
- Not exported from `Quiver.jl` (not in the `export` statement)
- Not referenced by any test file
- Not referenced by any other source file
- Adjacent code (`DatabaseException` struct on lines 1-3, `Base.showerror` on line 6, `check()` on lines 8-23) must remain untouched

**Python `encode_string`:**
- Location: `bindings/python/src/quiverdb/_helpers.py`, lines 20-22
- Standalone helper: `def encode_string(s: str) -> bytes: return s.encode("utf-8")`
- Not imported by any Python module (confirmed by grep for `encode_string` across `bindings/python/src/`)
- All callers use `.encode("utf-8")` inline instead
- Adjacent code (`decode_string` on lines 25-27, `decode_string_or_none` on lines 30-34) must remain untouched

### File Organization (Reference)
```
bindings/julia/src/
  exceptions.jl          # DatabaseException + check() -- DELETE line 5 only
  helper_maps.jl         # KEEP AS-IS (user uses it)
  Quiver.jl              # Module entry -- no changes needed
  [14 other files]       # All clean

bindings/dart/lib/src/
  exceptions.dart        # DatabaseException + LuaException + check() -- all clean
  [11 other files]       # All clean

bindings/python/src/quiverdb/
  _helpers.py            # check + encode_string (DEAD) + decode_string + decode_string_or_none
  exceptions.py          # QuiverError -- clean
  [8 other files]        # All clean
```

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| N/A | N/A | N/A | This phase is pure deletion -- no new code needed |

## Common Pitfalls

### Pitfall 1: Deleting the Wrong Line
**What goes wrong:** Off-by-one error when deleting by line number, especially if prior edits shift lines.
**Why it happens:** Line numbers change after edits. Blindly targeting "line 5" can hit the wrong content.
**How to avoid:** Use content-based matching (search for the exact string to delete) rather than line-number targeting.
**Warning signs:** The deleted line doesn't contain `quiver_database_sqlite_error` or `encode_string`.

### Pitfall 2: Accidentally Breaking Adjacent Code
**What goes wrong:** Removing a line leaves a blank line gap or breaks logical grouping.
**Why it happens:** The deleted line sits between `Base.showerror` (line 6) and the `struct` block (lines 1-3).
**How to avoid:** After deletion, verify the file reads naturally with no double-blank-line artifacts. The file should go: `struct DatabaseException ... end` -> blank line -> `Base.showerror` -> blank line -> `check()` docstring.
**Warning signs:** Two consecutive blank lines where the deleted code was.

### Pitfall 3: Forgetting to Run All Three Binding Test Suites
**What goes wrong:** A test failure in one binding is missed because only Julia tests were run.
**Why it happens:** The Python dead code item might be overlooked, or test infrastructure might have issues.
**How to avoid:** Run all three test suites in sequence: Julia, Dart, Python.
**Warning signs:** Skipping a test suite "because no changes were made" -- the audit should be verified end-to-end.

## Code Examples

### Julia: Before and After (exceptions.jl)

**Before:**
```julia
struct DatabaseException <: Exception
    msg::String
end

quiver_database_sqlite_error(msg::String) = throw(DatabaseException(msg))
Base.showerror(io::IO, e::DatabaseException) = print(io, e.msg)

"""
    check(err::C.quiver_error_t)
...
```

**After:**
```julia
struct DatabaseException <: Exception
    msg::String
end

Base.showerror(io::IO, e::DatabaseException) = print(io, e.msg)

"""
    check(err::C.quiver_error_t)
...
```

### Python: Before and After (_helpers.py)

**Before:**
```python
def check(err: int) -> None:
    ...

def encode_string(s: str) -> bytes:
    """Encode a Python string to UTF-8 bytes for passing to the C API."""
    return s.encode("utf-8")


def decode_string(ptr) -> str:
    ...
```

**After:**
```python
def check(err: int) -> None:
    ...


def decode_string(ptr) -> str:
    ...
```

## Test Commands

| Binding | Command | Notes |
|---------|---------|-------|
| Julia | `bindings/julia/test/test.bat` | Requires Julia 1.12.5 |
| Dart | `bindings/dart/test/test.bat` | Requires both DLLs on PATH |
| Python | `bindings/python/tests/test.bat` | Uses `uv run pytest`, prepends build/bin to PATH |
| All | `scripts/test-all.bat` | Runs all binding tests + C++/C tests |

## Open Questions

None. The scope is fully determined:
1. Two dead code items confirmed with high confidence.
2. No other dead code found across all three bindings.
3. `check()` functions are clean -- no improvements warranted.
4. All test commands documented and ready.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection: grep for `quiver_database_sqlite_error` across entire repo (zero usage)
- Direct codebase inspection: grep for `encode_string` across `bindings/python/src/` (zero imports)
- Direct codebase inspection: read all 18 Julia source files, 12 Dart source files, 13 Python source files
- Direct codebase inspection: read all export/module entry files (`Quiver.jl`, `quiverdb.dart`, `__init__.py`)

## Metadata

**Confidence breakdown:**
- Audit findings: HIGH -- complete codebase inspection, grep-verified, every binding file read
- Architecture: HIGH -- no architecture changes, just line deletions
- Pitfalls: HIGH -- failure modes are mechanical and well-understood

**Research date:** 2026-03-01
**Valid until:** indefinite (codebase-specific findings, not library-dependent)
