# Quiver

## What This Is

Quiver is a structured-data library wrapping SQLite, plus a standalone binary-file (`.qvr`) toolkit for dense numeric tensors with TOML metadata sidecars. It ships as a C++20 core, a stable C ABI, and homogeneous bindings for Julia, Dart, Python, JS/Deno, and embedded Lua — built so logic lives in C++ and bindings stay thin.

## Core Value

A single C++ implementation reaches every supported language with identical semantics: every public method on `Database`, `BinaryFile`, `BinaryMetadata`, etc. is mechanically translatable to each binding via the cross-layer naming rules in CLAUDE.md.

## Requirements

### Validated

<!-- Existing capabilities, inferred from the live codebase. These are locked. -->

- ✓ SQLite-backed `Database` class with factory methods (`from_schema`, `from_migrations`) and explicit transaction control — existing
- ✓ Element CRUD (`create_element`, `update_element`, `delete_element`) with FK-label resolution — existing
- ✓ Scalar / vector / set / time-series read + update operations + group metadata — existing
- ✓ Parameterized SQL queries (`query_string`, `query_integer`, `query_float`) — existing
- ✓ CSV export and import (`export_csv`, `import_csv`) with options — existing
- ✓ Embedded Lua scripting via `LuaRunner` (sol2 + Lua 5.4) — existing
- ✓ Binary subsystem: `BinaryFile` (read/write `.qvr`), `CSVConverter` (`bin_to_csv`, `csv_to_bin`), `BinaryMetadata` with TOML round-trip — existing
- ✓ Stable C ABI (`libquiver_c`) wrapping every public C++ method with `quiver_error_t` + out-parameters — existing
- ✓ Five language bindings (Julia, Dart, Python, JS/Deno, Lua) consuming the C API — existing
- ✓ Cross-binding homogeneity rules: identical mechanical naming transformations from C++ → each binding — existing

### Active

<!-- Current scope: lazy arithmetic on binary files. -->

- [ ] User can compose lazy arithmetic expressions on `BinaryFile` operands using natural operator syntax (`+ - * /`)
- [ ] User can broadcast a scalar against an expression (`a + 2.0`, `2.0 * a`)
- [ ] User can chain operators into trees of arbitrary depth (`(a + b) * 2.0`, `((a + b) - c) / d`)
- [ ] User can materialize an expression to a new `.qvr` file with a single `save(path)` call
- [ ] Materialization computes results row-by-row (one `vector<double>` per output row)
- [ ] Identical surface in all five bindings (Julia, Dart, Python, JS/Deno, Lua) per CLAUDE.md transformation rules
- [ ] C++ test suite verifies arithmetic correctness end-to-end (write inputs, evaluate, read output)
- [ ] C API symmetry test verifies lifetime + error paths
- [ ] Per-binding round-trip test for the showcase expression

### Out of Scope

- **CRTP / expression templates** — incompatible with FFI surface (5 bindings); optimizes a non-bottleneck (hot path is I/O, not arithmetic)
- **Reduction operators** (sum/mean across a dim) — change output shape, deferred to v2
- **Comparison / logical ops** (`<`, `where()`) — deferred to v2 once the arithmetic foundation lands
- **Mutating `Database` operations from expressions** — expressions read `BinaryFile`s, write `BinaryFile`s; no SQL involvement
- **In-place evaluation** — every `save()` writes a new file; no overwrite-source semantics
- **GPU / SIMD acceleration** — single-threaded scalar loop is sufficient for v1; revisit if profiling shows arithmetic (not I/O) is the bottleneck

## Context

- **Codebase map** lives in `.planning/codebase/` (analyzed 2026-04-18): STACK.md, ARCHITECTURE.md, STRUCTURE.md, CONVENTIONS.md, INTEGRATIONS.md, TESTING.md, CONCERNS.md.
- **Architectural design** for the lazy-expressions subsystem lives in `.planning/research/DESIGN.md` (decided 2026-05-05).
- The binary subsystem already exposes `BinaryFile::read(dims)` returning `vector<double>` (one row), `BinaryFile::write(dims, data)`, plus a row iterator (`first_dimensions` / `next_dimensions`) — the new framework consumes those and adds a thin lazy layer over them.
- The write registry in `binary.cpp` prevents reading a file currently held open for writing — `Expression::save()` must respect this; documenting in design.

## Constraints

- **Language:** C++20, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` — already set in `CMakeLists.txt`. No C++23 features.
- **ABI stability:** Every new feature must reach the C API and bindings; CRTP / template-leaking surfaces are forbidden in public headers.
- **Naming:** New subsystem follows CLAUDE.md transformation rules — `quiver::expr` (C++), `quiver_expression_*` (C), `Expression`/`expression` per binding's casing convention.
- **Errors:** Use the three CLAUDE.md error patterns (`Cannot {operation}: {reason}` / `{Entity} not found: {identifier}` / `Failed to {operation}: {reason}`); messages live in C++ core, bindings surface them unchanged.
- **Pimpl rule:** Pimpl only when hiding private dependencies. `Expression` is a value type wrapping `shared_ptr<Node>` — no Pimpl. `Node` and its subclasses use Rule of Zero where possible.
- **No new third-party deps** for v1 — the design only uses `BinaryFile`, `BinaryMetadata`, the standard library.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Subsystem name = `expressions` (`quiver::expr`, `quiver_expression_*`) | Standard math/compiler vocabulary, clean across all 5 bindings, no collision with Database verbs | — Pending |
| Architecture = AST with virtual `Node` (runtime polymorphism) | FFI requires opaque handles for 5 bindings; CRTP types are not ABI-stable. Hot path is I/O, not arithmetic — virtual dispatch is invisible against per-row file reads. Aligns with PyTorch/TensorFlow/JAX/NumPy industry pattern. | — Pending |
| Materialization = row-by-row | User-stated requirement; matches `BinaryFile::read(dims)` granularity (one `vector<double>` per row); per-row temporary buffers reused across iterations | — Pending |
| Scalar broadcast via `ScalarNode` capturing sibling metadata | Avoids requiring users to carry shape information when writing `a + 2.0`; `ScalarNode` gets metadata from the adjacent operand at construction time | — Pending |
| Leaf nodes capture file path + metadata snapshot, lazily open file on first row | Decouples `Expression` lifetime from `BinaryFile` object lifetime; works naturally across FFI (path is portable); RAII closes on destruction | — Pending |
| Test framework = GoogleTest (existing) | Already wired in `tests/CMakeLists.txt`; matches project convention | ✓ Good |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-05 after initialization*
