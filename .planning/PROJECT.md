---
project_name: Quiver
project_code: QVR
created: 2026-05-05
last_updated: 2026-05-05
---

# Quiver

## What This Is

Quiver is a structured-data library: a C++20 core wrapping SQLite with a strict schema model
(collections, scalars, vectors, sets, time series), plus a stable C ABI exposed to bindings in
Julia, Dart, Python, JS/Deno, and Lua. The product is a single homogeneous API for
domain-specific data — currently used for energy-modeling workflows — that lets every
language call into the same C++ logic without re-implementing it.

It also ships a standalone **Binary subsystem** for N-dimensional `.qvr` binary files with
TOML metadata sidecars, used for time-aware tensor-shaped numerical data.

## Core Value

**One C++ codebase, identical behavior across every language binding.** Logic, error
messages, and validation live in C++; bindings are thin marshaling shims. If a feature
works in C++, it works the same way in Julia, Dart, Python, JS, and Lua — with no
language-specific business logic to drift.

## Requirements

### Validated

<!-- Existing capabilities inferred from codebase at v0.7.9. These shipped and are in production use. -->

- ✓ **Database core (SQLite-backed)** — `from_schema`/`from_migrations` factories, RAII lifecycle, explicit + nest-aware transactions, schema validation, foreign-key enforcement
- ✓ **Element model** — collections with scalar attributes, vector groups, set groups, time-series groups, time-series files; full CRUD via `Element` builder and `create/read/update/delete_element`
- ✓ **Reads** — typed scalar/vector/set readers (integer/float/string/datetime), `_by_id` variants, multi-column time series read with columnar typed arrays
- ✓ **Queries** — `query_string/integer/float/datetime` with positional `?` parameter binding
- ✓ **Metadata** — `get_*_metadata` and `list_*_groups` for scalars/vectors/sets/time series; unified `GroupMetadata` with optional dimension column
- ✓ **CSV** — `export_csv`/`import_csv` with `CSVOptions` for enum labels, datetime formatting, custom delimiters
- ✓ **Lua scripting** — `LuaRunner` exposing the full Database API as Lua usertype via sol2
- ✓ **Binary subsystem** — `BinaryFile` (Pimpl) for `.qvr` N-dim arrays, `BinaryMetadata` (TOML sidecars) with time dimensions, `CSVConverter` for round-trip
- ✓ **C API** — full `extern "C"` surface (`quiver_database_*`, `quiver_binary_*`) with `quiver_error_t` codes, thread-local last-error, opaque handles, alloc/free co-location
- ✓ **Bindings** — Julia (Quiver.jl, `@ccall`), Dart (`dart:ffi`, native assets), Python (CFFI ABI-mode, wheels for cp313), JS/Deno (`Deno.dlopen`, JSR), Lua (in-process via sol2)
- ✓ **CLI** — `quiver_cli` for running Lua scripts against a database

### Active

<!-- Current scope: v0.8.0 — tensor milestone. -->

- [ ] Lazy-evaluating tensor framework (Expression Templates with CRTP) usable from C++ alongside Quiver
- [ ] Element-wise binary ops, unary ops, scalar broadcasts
- [ ] NumPy-style shape broadcasting between non-matching shapes
- [ ] Reductions (sum, mean, max) with optional axis

### Out of Scope

<!-- v0.8.0 boundaries — deferred to later milestones. -->

- **Element/Database bridge for tensors** — `Database::read_tensor`, `Element::set(Tensor)` — deferred to v0.9.0; lazy core must stabilize first
- **Tensor C API** (`quiver_tensor_*`) — Expression Templates can't cross FFI; immediate-mode wrappers come with the bridge milestone
- **Tensor language bindings** (Julia/Dart/Python/JS/Lua) — depends on the C API surface above
- **Autograd / autodiff** — would require a tape mechanism on top of the lazy graph; not in roadmap
- **GPU / device dispatch** — CPU-only for v0.8.0 (and likely well beyond)
- **Sparse tensors** — dense-only for v0.8.0; sparse would require a separate Expression hierarchy
- **Convolutions, einsum, linear algebra** (matmul, decompositions) — out of scope; this is a *lazy element-wise* framework, not a numerical-algebra library
- **Promotion of `Binary*` to `Tensor*`** — the existing binary subsystem keeps its current naming; tensor is a *new* parallel namespace, not a rename

## Context

- **Maturity:** Active codebase at v0.7.9 across 7+ months of development. CI green on Windows/Linux/macOS for C++, Julia, Dart, Python, JS test matrices.
- **Codebase analysis:** Full codebase mapping under `.planning/codebase/` (STRUCTURE, STACK, ARCHITECTURE, CONVENTIONS, TESTING, INTEGRATIONS, CONCERNS) — read this before planning any phase.
- **First GSD milestone:** Quiver had no formal `PROJECT.md`/`ROADMAP.md` before v0.8.0; existing capabilities are inferred from the codebase analysis and treated as Validated.
- **Domain:** Energy-modeling time-series workflows (PSR Energy). Heavy users of the time-series and binary subsystems; tensor framework targets numerical pipelines that consume these arrays.
- **Performance baseline:** Binary subsystem already has documented hot-path bottlenecks (`unordered_map` dimension lookups, validation cost, per-read allocation) — separately tracked, not addressed in v0.8.0.

## Constraints

- **Tech stack:** C++20, SQLite 3.50.2, spdlog, sol2/Lua 5.4, tomlplusplus, rapidcsv, GoogleTest. CMake ≥ 3.26, FetchContent-only for deps. No system packages.
- **Compatibility:** Default-hidden visibility, explicit `QUIVER_API`/`QUIVER_C_API` exports. `libquiver_c` depends on `libquiver` at runtime. Linux SONAME `libquiver.so.0`.
- **ABI:** C API is the stable boundary; C++ headers are private to C++ users. **Pimpl only** when hiding private dependencies; plain value types use Rule of Zero.
- **Style:** `.clang-format` (LLVM, 4-space, 120 cols, C++20), `.clang-tidy`, pre-commit (clang-format, cppcheck, cmake-format, trailing whitespace).
- **Error patterns:** Three forms only — `Cannot {operation}: {reason}`, `{Entity} not found: {identifier}`, `Failed to {operation}: {reason}`. Surfaced unchanged through C API and bindings.
- **Tensor framework specifically:** header-only (no compiled translation unit unless needed for non-template helpers); no SQLite/Element coupling in the core; deterministic compile-time dispatch.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Pimpl for classes hiding private deps; Rule of Zero for value types | ABI stability without paying the indirection cost on plain data | ✓ Good |
| C API uses binary `quiver_error_t` + thread-local `quiver_get_last_error` | Predictable FFI marshaling, exceptions can't cross C ABI | ✓ Good |
| Logic in C++, bindings are thin marshalers | One source of truth for behavior across 5+ languages | ✓ Good |
| Per-concern file split (`database_create.cpp`, `database_read.cpp`, ...) | Mirrors the test suite, keeps PRs reviewable, alloc/free co-located | ✓ Good |
| Schema-first with mandatory `Configuration` table + naming convention for groups | Predictable structure enables generic readers/writers and CSV/TOML round-trip | ✓ Good |
| Binary subsystem as a standalone parallel surface (not stored in SQLite) | `.qvr` files are too large to live inside SQLite blobs; TOML sidecar keeps metadata human-editable | ✓ Good |
| **v0.8.0**: Expression Templates with CRTP for lazy tensors | Industry standard for high-performance C++ (Eigen, xtensor, Blaze); zero-overhead op fusion | — Pending |
| **v0.8.0**: Element/Database bridge deferred | Lazy core needs to stabilize before designing the integration surface; avoids re-work | — Pending |

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
*Last updated: 2026-05-05 — bootstrapped at start of milestone v0.8.0 (tensor)*
