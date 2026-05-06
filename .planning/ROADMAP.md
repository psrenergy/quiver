# Roadmap: Quiver — Lazy Expressions (v1)

## Overview

This milestone adds a new `quiver::expr` subsystem that layers lazy arithmetic on top of the existing `BinaryFile` infrastructure. The work follows Quiver's mandatory dependency chain — C++ core lands first, the C ABI follows, and the four C-API-driven bindings (Julia, Dart, Python, JS/Deno) plus the Lua binding (which lives inside the C++ core via sol2) close the milestone. After Phase 2 completes, Phases 3-6 can plan/execute in parallel because they all consume the same stable `libquiver_c` symbol surface; Phase 7 (Lua) only depends on Phase 1 because Lua binds directly to C++ classes inside `src/lua_runner.cpp` and bypasses the C API entirely. The final deliverable is identical `(a + b) * 2.0`-style ergonomics in every supported language, materialized row-by-row to a new `.qvr` file via `expression.save(path)`.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: C++ Core** - Build `quiver::expr` namespace, Node hierarchy, Expression class, operators, and row-by-row save engine, all covered by GoogleTest
- [ ] **Phase 2: C API** - Wrap the C++ core in a stable `quiver_expression_*` ABI surface (opaque handle, ops, save, free, error round-trip) with GoogleTest coverage
- [ ] **Phase 3: Julia Binding** - Expose `Expression` in Quiver.jl with native `+ - * /` operators and `save(expr, path)`, plus round-trip test
- [ ] **Phase 4: Dart Binding** - Expose `Expression` in quiverdb (Dart) with operator overloads and `save(path)` method, plus round-trip test
- [ ] **Phase 5: Python Binding** - Expose `Expression` in quiverdb (Python) with `__add__`/`__sub__`/`__mul__`/`__truediv__` (+ reflected) and `save(path)`, plus round-trip test
- [ ] **Phase 6: JS/Deno Binding** - Expose `Expression` in @psrenergy/quiver with `add`/`sub`/`mul`/`div` methods and `save(path)`, plus round-trip test
- [ ] **Phase 7: Lua Binding** - Register `expression` userdata inside `src/lua_runner.cpp` with arithmetic metamethods and `save(expr, path)`, plus round-trip test

## Phase Details

### Phase 1: C++ Core
**Goal**: Deliver a working `quiver::expr` subsystem in the C++ core — `Expression` value type, virtual `Node` AST (FileNode / ScalarNode / BinaryOpNode), four binary operators, scalar broadcast, and a row-by-row `save(path)` engine that materializes any expression tree into a new `.qvr` file.
**Depends on**: Nothing (first phase)
**Requirements**: CORE-01, CORE-02, CORE-03, CORE-04, CORE-05, CORE-06, TEST-01
**Success Criteria** (what must be TRUE):
  1. User can construct an `Expression` from an open `BinaryFile` (implicit conversion) and call `expression.save(path)` to copy the file row-by-row through the AST engine.
  2. User can write `(a + b) * 2.0` in C++ and `expression.save("out.qvr")` produces a `.qvr` file whose values match element-wise expectations against goldens.
  3. User can chain operators into trees of arbitrary depth (e.g. `((a + b) - c) / 2.0`, scalar on left or right) and the materialized output remains correct.
  4. `quiver_tests.exe` passes a new `test_expression.cpp` GoogleTest suite that covers AddTwoFilesAndScale, ScalarBroadcast, Chained, IdentityFile, and MismatchedShapes cases — verifying CORE-01..06 end-to-end.
  5. The `save()` loop reuses a single `vector<double>` row buffer across iterations (verifiable in code review) — no per-row heap allocation in the steady-state inner loop.
**Plans**: 3 plans
- [ ] 01-01-PLAN.md — Binary subsystem extensions (D-12 free-function iterators, D-13 fast read overloads + read_into, D-14 fast write overload)
- [ ] 01-02-PLAN.md — quiver::expr core: Expression value type, Node hierarchy (FileNode, ScalarNode, BinaryOpNode with full broadcast validation), 12 operator overloads, save engine with D-11 self-save check, baseline tests
- [ ] 01-03-PLAN.md — Comprehensive Expression test suite: full op matrix, 8 scalar broadcast variants, broadcast/union edge cases, large-grid smoke

### Phase 2: C API
**Goal**: Expose the C++ Expression API through a stable `extern "C"` surface — opaque `quiver_expression_t` handle, `quiver_expression_from_file`, four binary ops + four scalar ops (with both scalar-on-left and scalar-on-right), `quiver_expression_save`, and `quiver_expression_free` — all returning `quiver_error_t` with messages retrievable via `quiver_get_last_error`.
**Depends on**: Phase 1
**Requirements**: CAPI-01, CAPI-02, CAPI-03, TEST-02
**Success Criteria** (what must be TRUE):
  1. C user can call `quiver_expression_from_file`, chain `quiver_expression_add` / `_sub` / `_mul` / `_div` (and their `_scalar` variants), then `quiver_expression_save` and produce the same `.qvr` output as the C++ `(a + b) * 2.0` golden test.
  2. C user can call `quiver_expression_free` after `_save` without crashing or leaking; valgrind/MSVC debug heap reports no leaks for the showcase round-trip.
  3. `quiver_c_tests.exe` passes a new `test_c_api_expression.cpp` GoogleTest suite covering free-after-save lifetime, error retrieval via `quiver_get_last_error`, and mismatched-shape rejection (TEST-02).
  4. Every error path bubbles a C++ message in one of the three CLAUDE.md formats (`Cannot…` / `…not found:` / `Failed to…`) unchanged through the C API boundary.
**Plans**: TBD

### Phase 3: Julia Binding
**Goal**: Expose `Expression` in Quiver.jl with native Julia operator overloading (`Base.:+`, `Base.:-`, `Base.:*`, `Base.:/`) over the C API, plus a `save(expr, path)` function. Covers BIND-01 plus the Julia slice of the per-binding round-trip test (TEST-03).
**Depends on**: Phase 2
**Requirements**: BIND-01, TEST-03 (Julia slice)
**Success Criteria** (what must be TRUE):
  1. Julia user can write `e = (a + b) * 2.0` in the REPL where `a`, `b` are `BinaryFile` or `Expression` instances and the resulting `Expression` is a thin wrapper over the C API handle.
  2. Julia user can call `save(e, "out.qvr")` and the output file matches the C++ golden values from TEST-01 byte-for-byte.
  3. `bindings/julia/test/test.bat` passes a new `test_expression.jl` round-trip test that builds the showcase expression and asserts against the golden output.
  4. Garbage-collected Julia `Expression` calls `quiver_expression_free` via finalizer; running the test suite leaves no leaked C handles.
**Plans**: TBD

### Phase 4: Dart Binding
**Goal**: Expose `Expression` in quiverdb (Dart) with operator overloads (`operator +`, `operator -`, `operator *`, `operator /`) over the C API, plus a `save(path)` method. Covers BIND-02 plus the Dart slice of TEST-03.
**Depends on**: Phase 2
**Requirements**: BIND-02, TEST-03 (Dart slice)
**Success Criteria** (what must be TRUE):
  1. Dart user can write `final e = (a + b) * 2.0` where `a`, `b` are `BinaryFile` or `Expression` instances and Dart's operator dispatch routes through `quiver_expression_*` cleanly.
  2. Dart user can call `e.save('out.qvr')` and the output file matches the C++ golden values byte-for-byte.
  3. `bindings/dart/test/test.bat` passes a new `expression_test.dart` round-trip test mirroring the showcase expression.
  4. ffigen-regenerated bindings in `lib/src/ffi/bindings.dart` include all `quiver_expression_*` symbols and `Expression` disposal calls `quiver_expression_free` deterministically.
**Plans**: TBD

### Phase 5: Python Binding
**Goal**: Expose `Expression` in quiverdb (Python, CFFI ABI-mode) with arithmetic dunders (`__add__`, `__sub__`, `__mul__`, `__truediv__` plus reflected `__radd__` / `__rsub__` / `__rmul__` / `__rtruediv__`) and a `save(path)` method. Covers BIND-03 plus the Python slice of TEST-03.
**Depends on**: Phase 2
**Requirements**: BIND-03, TEST-03 (Python slice)
**Success Criteria** (what must be TRUE):
  1. Python user can write `e = (a + b) * 2.0` (or `2.0 * (a + b)`) where `a`, `b` are `BinaryFile` or `Expression` instances and scalar broadcast routes to `quiver_expression_*_scalar` correctly.
  2. Python user can call `e.save("out.qvr")` and the output file matches the C++ golden values byte-for-byte.
  3. `bindings/python/tests/test.bat` (pytest) passes a new `test_expression.py` round-trip test mirroring the showcase expression.
  4. The hand-written `_c_api.py` CFFI cdef block declares every `quiver_expression_*` symbol and `Expression.__del__` (or context manager) calls `quiver_expression_free`; pytest leaves no warnings.
**Plans**: TBD

### Phase 6: JS/Deno Binding
**Goal**: Expose `Expression` in @psrenergy/quiver (Deno FFI) with named methods `add`/`sub`/`mul`/`div` (JS has no operator overloading) and a `save(path)` method. Covers BIND-04 plus the JS slice of TEST-03.
**Depends on**: Phase 2
**Requirements**: BIND-04, TEST-03 (JS slice)
**Success Criteria** (what must be TRUE):
  1. JS/Deno user can write `const e = a.add(b).mul(2.0)` where `a`, `b` are `BinaryFile` or `Expression` instances and the chain routes through `quiver_expression_*`.
  2. JS user can call `await e.save("out.qvr")` (or sync, per existing binding convention) and the output file matches the C++ golden values byte-for-byte.
  3. `bindings/js/test/test.bat` passes a new `expression.test.ts` round-trip test mirroring the showcase expression under Deno's test runner.
  4. `src/loader.ts` registers a new `expressionSymbols` group with all `quiver_expression_*` symbols and `Expression`'s explicit `dispose()` (or finalization registry) calls `quiver_expression_free`.
**Plans**: TBD

### Phase 7: Lua Binding
**Goal**: Register `expression` userdata inside `src/lua_runner.cpp` (sol2) with arithmetic metamethods (`__add`, `__sub`, `__mul`, `__div`) and a global `save(expr, path)` function. Crucially, Lua does **not** go through the C API — it binds directly to the `quiver::expr::Expression` C++ class via sol2's usertype API, the same way the existing `db` userdata works. Covers BIND-05 plus the Lua slice of TEST-03.
**Depends on**: Phase 1 (Lua binds C++ classes directly; in principle parallelizable with Phase 2, but standard order keeps it last)
**Requirements**: BIND-05, TEST-03 (Lua slice)
**Success Criteria** (what must be TRUE):
  1. Lua script user can write `local e = (a + b) * 2.0` inside a `LuaRunner`-executed script where `a`, `b` are `expression` (or `binary_file`) userdata, with sol2 routing the metamethods to `quiver::expr::operator+/*/-/...`.
  2. Lua user can call `save(e, "out.qvr")` and the output file matches the C++ golden values byte-for-byte.
  3. `quiver_tests.exe` (or a binding-specific runner under `example/`) passes a new round-trip Lua test mirroring the showcase expression — invoked via `LuaRunner` from C++ test code, consistent with existing Lua test patterns.
  4. The new sol2 registrations live inside `Impl::bind_database()` (or a new `bind_expression()` helper) in `src/lua_runner.cpp`; no headers are added to the C API for Lua's sake.
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7. Phases 3-6 can plan/execute in parallel after Phase 2 completes (each consumes the same stable `libquiver_c` ABI). Phase 7 only requires Phase 1 and could parallelize with Phase 2; the standard order keeps it last to benefit from cross-binding C++-core validation.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. C++ Core | 0/3 | Not started | - |
| 2. C API | 0/TBD | Not started | - |
| 3. Julia Binding | 0/TBD | Not started | - |
| 4. Dart Binding | 0/TBD | Not started | - |
| 5. Python Binding | 0/TBD | Not started | - |
| 6. JS/Deno Binding | 0/TBD | Not started | - |
| 7. Lua Binding | 0/TBD | Not started | - |
