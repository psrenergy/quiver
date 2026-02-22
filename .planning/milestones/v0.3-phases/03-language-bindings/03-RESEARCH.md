# Phase 3: Language Bindings - Research

**Researched:** 2026-02-21
**Domain:** FFI binding implementation (Julia, Dart, Lua) for transaction control
**Confidence:** HIGH

## Summary

Phase 3 binds the four C API transaction functions (`begin_transaction`, `commit`, `rollback`, `in_transaction`) to Julia, Dart, and Lua, plus adds a convenience `transaction` wrapper in each language. The C API surface is complete (Phase 2) and the functions follow the standard `quiver_error_t` return pattern already used by all other C API functions.

The primary technical finding is that **both Julia and Dart FFI binding files must be regenerated** before any high-level wrapper code can be written -- neither `c_api.jl` nor `bindings.dart` currently contains the transaction function declarations. The Lua binding is different: it binds directly to C++ via sol2 in `lua_runner.cpp`, so no FFI generation is needed.

**Primary recommendation:** Regenerate Julia and Dart FFI bindings first, then implement the four raw wrapper functions per binding, then implement the convenience `transaction` block in each language.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Block return value passes through: `result = transaction(db) do ... return 42 end` gives `result = 42`
- After rollback on exception, the original exception re-raises unchanged -- bindings do not wrap or modify error messages
- Rollback failure is silently ignored (best-effort rollback) -- original exception still re-raises
- No nesting guard in bindings -- let C++ handle double-begin errors naturally
- All three bindings pass `db` to the block function for uniformity:
  - Julia: `transaction(db) do db ... end`
  - Dart: `db.transaction((db) { ... })`
  - Lua: `db:transaction(function(db) ... end)`
- Lua returns single value only from transaction block (no multi-return passthrough)
- Full mirror: each binding re-tests all C API transaction scenarios (begin/commit/rollback, error cases, multi-op batches) plus binding-specific features
- Use shared schemas from `tests/schemas/valid/` -- no binding-specific schemas
- Error propagation tests verify that errors are raised, but do not assert exact message strings
- Include a multi-operation batch test per binding: create + update inside transaction block, verify all data persists after commit
- Single plan (03-01-PLAN.md) covering all three bindings
- Sequential implementation: Julia first (reference), then Dart, then Lua
- FFI binding regeneration (Julia/Dart generators) is an explicit tracked task in the plan
- Include a task to verify/update CLAUDE.md cross-layer table after implementation
- Lua pcall design: `db:transaction(function(db) ... end)` -- fn receives db as argument
- On Lua error: pcall catches, rollback executes, then `error()` re-raises original message (not pcall-style return tuple)
- Consistent with Julia/Dart: all three bindings re-raise on failure rather than returning error codes

### Claude's Discretion
- Internal implementation details of each binding's transaction wrapper
- Test file organization within each binding's test suite
- Generator script modifications needed for FFI regeneration

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BIND-01 | Julia binding exposes `begin_transaction!` / `commit!` / `rollback!` | Julia pattern: mutating functions use `!` suffix, call `check(C.quiver_database_*)` on `db.ptr`. Requires FFI regen first. |
| BIND-02 | Dart binding exposes `beginTransaction` / `commit` / `rollback` | Dart pattern: extension methods on Database, call `check(bindings.quiver_database_*)` on `_ptr`. Requires FFI regen first. |
| BIND-03 | Lua binding exposes `begin_transaction` / `commit` / `rollback` | Lua pattern: add lambda bindings in `lua_runner.cpp` `bind_database()`. Calls C++ methods directly on `Database&`. |
| BIND-04 | Julia `transaction(db) do...end` with auto commit/rollback | Julia `do` blocks are anonymous functions. Pattern: `begin_transaction!`, try/catch/finally with rollback+rethrow. Return value passthrough via `result = fn(db)`. |
| BIND-05 | Dart `db.transaction(() {...})` with auto commit/rollback | Dart closures. Pattern: `beginTransaction()`, try/catch/finally. Generic return type `T transaction<T>(T Function(Database) fn)`. |
| BIND-06 | Lua `db:transaction(fn)` with pcall-based auto commit/rollback | sol2 `sol::protected_function`. Pattern: `begin_transaction`, pcall fn, commit or rollback+error(). Single return value. |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Clang.jl (Generators) | Current | Julia FFI binding generation from C headers | Already used by project; `generator.toml` + `generator.jl` in place |
| dart ffigen | Current | Dart FFI binding generation from C headers | Already used by project; `ffigen.yaml` in place |
| sol2 (sol/sol.hpp) | Current | Lua-C++ binding library | Already used by project; all Lua bindings go through `lua_runner.cpp` |

### Supporting
No additional libraries needed. All three bindings use the project's existing FFI infrastructure.

## Architecture Patterns

### Recommended File Structure

**Julia additions:**
```
bindings/julia/
├── src/
│   ├── c_api.jl                    # REGENERATED (adds 4 transaction ccall declarations)
│   ├── database.jl                 # ADD: begin_transaction!, commit!, rollback!, in_transaction, transaction
│   └── ...
├── test/
│   └── test_database_transaction.jl  # NEW: transaction tests
```

**Dart additions:**
```
bindings/dart/
├── lib/src/
│   ├── ffi/bindings.dart           # REGENERATED (adds 4 transaction FFI bindings)
│   ├── database.dart               # ADD: part 'database_transaction.dart'
│   └── database_transaction.dart   # NEW: extension DatabaseTransaction on Database
├── test/
│   └── database_transaction_test.dart  # NEW: transaction tests
```

**Lua additions:**
```
src/
└── lua_runner.cpp                  # MODIFY: add 4 transaction bindings + transaction wrapper to bind_database()
tests/
└── test_lua_runner.cpp             # ADD: transaction test cases (or new test_lua_transaction.cpp)
```

### Pattern 1: Julia Raw FFI Wrapper (Mutating)
**What:** Thin wrapper calling C API through `check()`, returning nothing for void operations.
**When to use:** All transaction control functions (begin, commit, rollback).
**Example:**
```julia
# Source: Existing pattern from bindings/julia/src/database_delete.jl
function begin_transaction!(db::Database)
    check(C.quiver_database_begin_transaction(db.ptr))
    return nothing
end

function commit!(db::Database)
    check(C.quiver_database_commit(db.ptr))
    return nothing
end

function rollback!(db::Database)
    check(C.quiver_database_rollback(db.ptr))
    return nothing
end
```

### Pattern 2: Julia Non-Mutating Query (in_transaction)
**What:** Wrapper for bool* out-parameter pattern, returns Julia Bool.
**When to use:** `in_transaction` query.
**Example:**
```julia
# Note: generator config has use_julia_bool = true
# After regen, C.quiver_database_in_transaction will have Bool* out param
function in_transaction(db::Database)
    out_active = Ref{Bool}(false)
    check(C.quiver_database_in_transaction(db.ptr, out_active))
    return out_active[]
end
```

### Pattern 3: Julia Convenience Transaction Block
**What:** `do...end` block with auto commit/rollback, passthrough return value.
**When to use:** BIND-04.
**Example:**
```julia
function transaction(fn, db::Database)
    begin_transaction!(db)
    try
        result = fn(db)
        commit!(db)
        return result
    catch
        try
            rollback!(db)
        catch
            # Best-effort rollback; ignore failure
        end
        rethrow()
    end
end
```
Note: Julia's `do` block syntax requires `fn` as the first positional argument: `transaction(db) do db ... end` desugars to `transaction(db -> ..., db)`.

### Pattern 4: Dart Raw FFI Wrapper (Extension Method)
**What:** Extension method on Database calling through `check(bindings.*)`.
**When to use:** All Dart transaction functions.
**Example:**
```dart
// Source: Existing pattern from bindings/dart/lib/src/database_delete.dart
part of 'database.dart';

extension DatabaseTransaction on Database {
  void beginTransaction() {
    _ensureNotClosed();
    check(bindings.quiver_database_begin_transaction(_ptr));
  }

  void commit() {
    _ensureNotClosed();
    check(bindings.quiver_database_commit(_ptr));
  }

  void rollback() {
    _ensureNotClosed();
    check(bindings.quiver_database_rollback(_ptr));
  }

  bool inTransaction() {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final outActive = arena<Bool>();
      check(bindings.quiver_database_in_transaction(_ptr, outActive));
      return outActive.value;
    } finally {
      arena.releaseAll();
    }
  }
}
```

### Pattern 5: Dart Convenience Transaction Block
**What:** Generic method with auto commit/rollback and return value passthrough.
**When to use:** BIND-05.
**Example:**
```dart
T transaction<T>(T Function(Database) fn) {
  _ensureNotClosed();
  beginTransaction();
  try {
    final result = fn(this);
    commit();
    return result;
  } catch (e) {
    try {
      rollback();
    } catch (_) {
      // Best-effort rollback
    }
    rethrow;
  }
}
```

### Pattern 6: Lua Raw Binding (sol2 lambda)
**What:** Lambda binding in `bind_database()` calling C++ methods directly.
**When to use:** All Lua transaction functions.
**Example:**
```cpp
// Source: Existing pattern in src/lua_runner.cpp
"begin_transaction",
[](Database& self) { self.begin_transaction(); },
"commit",
[](Database& self) { self.commit(); },
"rollback",
[](Database& self) { self.rollback(); },
"in_transaction",
[](Database& self) { return self.in_transaction(); },
```

### Pattern 7: Lua Convenience Transaction Block (pcall)
**What:** Lua function that wraps fn in pcall for auto commit/rollback.
**When to use:** BIND-06.
**Example:**
```cpp
"transaction",
[](Database& self, sol::protected_function fn) {
    self.begin_transaction();
    auto result = fn(std::ref(self));
    if (!result.valid()) {
        sol::error err = result;
        try { self.rollback(); } catch (...) {}
        throw std::runtime_error(err.what());
    }
    self.commit();
    if (result.return_count() > 0) {
        return result.get<sol::object>(0);
    }
    return sol::make_object(result.lua_state(), sol::lua_nil);
},
```
Note: Using `sol::protected_function` (not `sol::function`) gives pcall-like behavior where Lua errors are captured rather than thrown as C++ exceptions. The wrapper catches the error, rolls back, then re-throws via `std::runtime_error` which sol2 converts back to a Lua error. Since the user decision says "error() re-raises original message", the wrapper must extract the error message string and re-raise it.

### Anti-Patterns to Avoid
- **Crafting error messages in bindings:** Per CLAUDE.md principle, all error messages originate in C++. Bindings only propagate. The `check()` function in each binding already handles this correctly.
- **Adding nesting guards in bindings:** Per user decision, let C++ handle double-begin errors. The binding just calls through and lets the error propagate.
- **Modifying caught exceptions:** On rollback after failure, re-raise the original exception unchanged. Do not wrap it in a new exception type or append rollback status.
- **Allocating arena/native memory for parameter-less calls:** `begin_transaction`, `commit`, `rollback` take only `db` pointer -- no arena allocation needed in Dart (unlike most other methods). Only `in_transaction` needs an arena for the `bool*` output parameter.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Julia FFI declarations | Manual `@ccall` wrappers | `generator.bat` (Clang.jl) | Generator reads all headers in `include/quiver/c/`, produces correct type mappings for `bool*` |
| Dart FFI declarations | Manual `late final _ptr` wrappers | `generator.bat` (dart ffigen) | Generator reads `ffigen.yaml` headers list, produces correct Dart FFI bindings |
| Lua error capture in transaction | Manual pcall/xpcall from C | `sol::protected_function` | sol2 handles pcall semantics, error capture, and stack cleanup |

**Key insight:** The FFI generators already know how to handle every C type in the project. Manually writing the 4 transaction FFI declarations would be faster but creates maintenance burden -- the next time any header changes, regeneration would overwrite manual additions or miss them.

## Common Pitfalls

### Pitfall 1: FFI Bindings Not Regenerated
**What goes wrong:** High-level wrapper calls `C.quiver_database_begin_transaction()` but that symbol doesn't exist in `c_api.jl` / `bindings.dart`. Compilation fails.
**Why it happens:** Phase 2 added the C API functions to the C header but the FFI generators were not re-run.
**How to avoid:** Regenerate FFI bindings as the very first task. Verify the new functions appear in the generated output before writing wrapper code.
**Warning signs:** `undefined method` (Julia) or `NoSuchMethodError` (Dart) at compile time.

### Pitfall 2: bool* Type Mapping in Julia
**What goes wrong:** Julia generator might map C `bool*` differently than `int*`. The `in_transaction` function uses `bool*` output parameter -- this is the first `bool*` in the entire C API.
**Why it happens:** The generator config has `use_julia_bool = true`, which should map C `bool` to Julia `Bool`. But since no existing function uses `bool*`, the generated code hasn't been tested for this case.
**How to avoid:** After regeneration, inspect the generated `c_api.jl` for the `quiver_database_in_transaction` declaration. Verify it uses `Ptr{Bool}` (not `Ptr{Cint}`). If the generator maps it incorrectly, manually adjust the declaration.
**Warning signs:** `in_transaction` returns unexpected values or crashes.

### Pitfall 3: Dart bool FFI Type
**What goes wrong:** Dart `ffigen` maps C `bool` to `ffi.Bool` which requires Dart 3.1+. The `Arena` allocator pattern for `bool*` uses `arena<Bool>()`.
**Why it happens:** C `bool` is not `int` in Dart FFI. The generated code will use `ffi.Bool` type.
**How to avoid:** Use `arena<ffi.Bool>()` for the output parameter, access with `.value` which returns Dart `bool`. Verify Dart SDK version supports `ffi.Bool`.
**Warning signs:** Type errors at compile time.

### Pitfall 4: Julia do-block Argument Order
**What goes wrong:** `transaction(db) do db ... end` doesn't work because Julia's `do` syntax passes the anonymous function as the FIRST argument.
**Why it happens:** Julia desugars `f(args) do x ... end` to `f(x -> ..., args)`. So the function signature must be `transaction(fn, db)` not `transaction(db, fn)`.
**How to avoid:** Define `function transaction(fn, db::Database)` with `fn` as the first parameter.
**Warning signs:** `MethodError: no method matching transaction(::Database, ::Function)`.

### Pitfall 5: Lua Transaction Return Value
**What goes wrong:** Transaction block's return value is lost because sol2's `protected_function_result` isn't extracted correctly.
**Why it happens:** sol2's `protected_function_result` wraps pcall results. Need to explicitly extract the return value as `sol::object`.
**How to avoid:** Check `result.return_count() > 0` and extract via `result.get<sol::object>(0)`. Return `sol::lua_nil` if no return value.
**Warning signs:** `db:transaction(function(db) return 42 end)` returns nil instead of 42.

### Pitfall 6: Dart Extension Export
**What goes wrong:** New `DatabaseTransaction` extension is not visible to consumers of the package.
**Why it happens:** Dart uses `part of` / `part` for extensions, but also needs explicit export in `quiver_db.dart`.
**How to avoid:** Add `part 'database_transaction.dart'` in `database.dart` AND add `DatabaseTransaction` to the `show` list in `quiver_db.dart`.
**Warning signs:** `The method 'beginTransaction' isn't defined for the type 'Database'` in test code.

### Pitfall 7: Lua sol2 protected_function vs function
**What goes wrong:** Using `sol::function` instead of `sol::protected_function` causes Lua errors to immediately throw as C++ exceptions, bypassing the rollback logic.
**Why it happens:** `sol::function` uses lua_call (no error handler), `sol::protected_function` uses lua_pcall (catches errors).
**How to avoid:** Accept `sol::protected_function fn` in the transaction lambda.
**Warning signs:** Rollback never executes on Lua script errors.

## Code Examples

### C API Functions to Bind (Reference)
```c
// Source: include/quiver/c/database.h lines 50-53
QUIVER_C_API quiver_error_t quiver_database_begin_transaction(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_commit(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_rollback(quiver_database_t* db);
QUIVER_C_API quiver_error_t quiver_database_in_transaction(quiver_database_t* db, bool* out_active);
```

### C++ Methods Called by Lua (Reference)
```cpp
// Source: include/quiver/database.h (public API)
void begin_transaction();
void commit();
void rollback();
bool in_transaction() const;
```

### Julia Test Pattern (Expected)
```julia
# Source: Extrapolated from bindings/julia/test/test_database_lifecycle.jl
@testset "Transaction" begin
    @testset "Begin, create, commit persists" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.create_element!(db, "Configuration"; label = "Config")

        Quiver.begin_transaction!(db)
        Quiver.create_element!(db, "Collection"; label = "Item 1")
        Quiver.commit!(db)

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        @test labels[1] == "Item 1"
        Quiver.close!(db)
    end

    @testset "Transaction block auto-commits" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.create_element!(db, "Configuration"; label = "Config")

        result = Quiver.transaction(db) do db
            Quiver.create_element!(db, "Collection"; label = "Item 1")
            42
        end
        @test result == 42

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        Quiver.close!(db)
    end
end
```

### Dart Test Pattern (Expected)
```dart
// Source: Extrapolated from bindings/dart/test/database_lifecycle_test.dart
group('Transaction', () {
  test('begin, create, commit persists', () {
    final db = Database.fromSchema(':memory:', collectionsSchema);
    try {
      db.createElement('Configuration', {'label': 'Config'});
      db.beginTransaction();
      db.createElement('Collection', {'label': 'Item 1'});
      db.commit();

      final labels = db.readScalarStrings('Collection', 'label');
      expect(labels, equals(['Item 1']));
    } finally {
      db.close();
    }
  });

  test('transaction block auto-commits', () {
    final db = Database.fromSchema(':memory:', collectionsSchema);
    try {
      db.createElement('Configuration', {'label': 'Config'});
      final result = db.transaction((db) {
        db.createElement('Collection', {'label': 'Item 1'});
        return 42;
      });
      expect(result, equals(42));
    } finally {
      db.close();
    }
  });
});
```

### Lua Test Pattern (Expected via LuaRunner)
```cpp
// Source: Extrapolated from tests/test_lua_runner.cpp
TEST_F(LuaRunnerTest, TransactionCommit) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:begin_transaction()
        db:create_element("Collection", { label = "Item 1" })
        db:commit()
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 1)
    )");
}

TEST_F(LuaRunnerTest, TransactionBlock) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    quiver::LuaRunner lua(db);

    lua.run(R"(
        local result = db:transaction(function(db)
            db:create_element("Collection", { label = "Item 1" })
            return 42
        end)
        assert(result == 42)
    )");
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual FFI declarations | Auto-generated from C headers | Already in project | Must regenerate, not hand-write |
| Dart `Pointer<Int32>` for bool | Dart `ffi.Bool` native type | Dart 3.1 / 2023 | `in_transaction` uses `Bool` not `Int32` |

**Deprecated/outdated:**
- Nothing relevant. The project's FFI toolchain is current.

## Open Questions

1. **Julia `Bool` mapping verification**
   - What we know: Generator config has `use_julia_bool = true`. C API uses `bool*` for `in_transaction`.
   - What's unclear: Whether Clang.jl maps `bool*` to `Ptr{Bool}` or `Ptr{Cuchar}` with this config.
   - Recommendation: After regeneration, verify the generated declaration. If wrong, manually fix `c_api.jl` for this one function. LOW risk -- the config setting specifically addresses this.

2. **Lua sol2 protected_function return value extraction**
   - What we know: `sol::protected_function_result` supports `get<T>()` and `return_count()`. User requires single return value passthrough.
   - What's unclear: Exact sol2 API for extracting a generic `sol::object` from `protected_function_result` when the Lua function returns a value.
   - Recommendation: Test with a simple Lua function that returns 42. Verify extraction works. If `get<sol::object>(0)` doesn't work, try iterating the result stack. MEDIUM confidence from training data.

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- C API transaction declarations (lines 49-53), verified in codebase
- `src/c/database_transaction.cpp` -- C API implementation, verified in codebase
- `bindings/julia/src/c_api.jl` -- Current Julia FFI bindings (no transaction functions), verified
- `bindings/dart/lib/src/ffi/bindings.dart` -- Current Dart FFI bindings (no transaction functions), verified
- `src/lua_runner.cpp` -- Lua binding implementation via sol2, verified
- `bindings/julia/generator/generator.toml` -- Julia generator config with `use_julia_bool = true`, verified
- `bindings/dart/ffigen.yaml` -- Dart generator config with header list, verified

### Secondary (MEDIUM confidence)
- Julia `do` block syntax desugaring rules -- from Julia language documentation (function as first arg)
- sol2 `protected_function` vs `function` behavior -- from sol2 documentation (pcall vs lua_call)
- Dart `ffi.Bool` type availability -- from Dart FFI documentation (Dart 3.1+)

### Tertiary (LOW confidence)
- Exact sol2 API for extracting return values from `protected_function_result` as `sol::object` -- based on training data, needs validation during implementation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all tools already in project, just running generators
- Architecture: HIGH -- patterns directly observed from existing binding code
- Pitfalls: HIGH -- most identified from direct codebase inspection (missing FFI declarations, type mapping)

**Research date:** 2026-02-21
**Valid until:** 2026-03-21 (stable codebase, no external dependency changes expected)
