# Phase 9: Julia AbstractExpression and Docs - Research

**Researched:** 2026-10-04
**Domain:** Julia multiple dispatch over an FFI binding (Quiver.jl), plus AGENTS.md / CHANGELOG completion for milestone lua-2
**Confidence:** HIGH. The Julia design was built and run in a scratch copy of the package against the real
`build/bin` libraries: 0 method ambiguities, every operation on a raw file works, and the four existing
binary/expression test files pass unchanged.

## Summary

The Julia half is small and fully de-risked. Declare `abstract type AbstractExpression end` in `Quiver.jl`
before `include("binary/Binary.jl")`, add it to `Binary.jl`'s `using ..Quiver:` list, make
`mutable struct File <: AbstractExpression` and `mutable struct Expression <: AbstractExpression`, and retype
every operation in `expression.jl` from `Expression` to `AbstractExpression`. Each operation converts its operands
with a private `_expression(x)`: identity for an `Expression`, the existing `Expression(file)` constructor
(`quiver_expression_from_file`) for a file. The 54 `Binary.File` lines (97 forwarding methods) disappear. Only
the conversion constructor still names `Binary.File`. No C API, `c_api.jl` or C++ change.

The planning flag is settled. `get_metadata` must be **one** generic function. Keep its owner in `Binary` (the
existing `get_metadata(file::File)` at `src/binary/file.jl:101-105`), and add `import .Binary: get_metadata` to
`Quiver.jl` between the Binary include and the expression include. `get_metadata(e::Expression)` in
`expression.jl` then becomes a second method of the same function. `Quiver.get_metadata === Quiver.Binary.get_metadata`
holds (verified), both existing spellings keep working, and a file returns its handle's metadata, as C++ and Lua
do. `save` and the other methods need no special handling: they go through the generic `AbstractExpression`
path.

The docs half is mostly done already. The CHANGELOG `[0.13.0]` section already holds all four required BREAKING
lines (lines 63-79) and the Lua "file is an expression" Added line (83-87). It contains no planning IDs, and the
version is 0.13.0 in all five manifests. Missing: a Julia Added entry, a root `AbstractExpression` Design Decision,
the binary cross-layer rows, the stale root sandbox list and expression paragraph (`expr:save`,
`quiver.expression(file)`), the `ExpressionFile` line in `src/AGENTS.md:1069`, a `src/c/AGENTS.md` bridge note,
and a `bindings/julia/AGENTS.md` bullet. One trap: the ROADMAP's `grep -n 'caches an open BinaryFile'` check
already returns nothing, because the real line is ``Caches an open `BinaryFile` `` (capital C, backticks). Verify
with a case-insensitive pattern instead (Pitfall 1).

**Primary recommendation:** Two sequential plans. 09-01 covers the Julia code, the tests, the Julia AGENTS.md and
the CHANGELOG Julia line. 09-02 covers the root, `src/` and `src/c/` AGENTS.md files, a CHANGELOG completeness
check, and the six-suite and sync-test gate. Use the code in "Code Examples" as written; it is the code that ran.

<user_constraints>
## User Constraints (from CONTEXT.md)

No CONTEXT.md exists for Phase 9 (no discuss-phase was run). Constraints come from ROADMAP/REQUIREMENTS/STATE and the
root AGENTS.md:

### Locked Decisions
- `abstract type AbstractExpression end` declared in `bindings/julia/src/Quiver.jl` before `include("binary/Binary.jl")`; `Binary.File <: AbstractExpression`, `Expression <: AbstractExpression` (JUL-01).
- Operations defined once on `AbstractExpression`; a `Binary.File` operand is converted through the existing `quiver_expression_from_file`; the 97 forwarders are deleted; **no C API change** (`c_api.jl` and every C API header have an empty diff) (JUL-02).
- Every expression operation works on a raw `Binary.File`, including `save` and `get_metadata`, with tests (JUL-03).
- Docs: root Design Decision for `AbstractExpression`, updated binary cross-layer rows, the `src/AGENTS.md` "caches an open BinaryFile" fix, and `src/c/AGENTS.md` (`quiver_expression_from_file` stays the bridge) plus `bindings/julia/AGENTS.md` describing the type and the `src/lua_runner/` layout (DOC-01).
- CHANGELOG `[0.13.0] — unreleased` complete, each BREAKING line says what a caller must change, no planning IDs; version stays 0.13.0; all six suites + lua-api sync test green (DOC-02).
- Root rules: no planning ID in any code or test comment; Self-Updating AGENTS.md; delete unused code rather than deprecate; error messages come from C++/C (Julia crafts none here).

### Claude's Discretion
- Where the Julia `get_metadata` generic lives (planning flag), settled below: **Binary owns it, Quiver imports it.**
- Shape of the conversion helper, test layout, plan split.

### Deferred Ideas (OUT OF SCOPE)
- A C API "file as expression" handle / file-taking `quiver_expression_*` variants.
- Lua `==`/`<` between files/expressions always true (deferred decision EQ-01).
- `save` detecting stale metadata (META-01).
- Binary/expression in Dart, Python, JS.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| JUL-01 | Abstract type declared before Binary include; both structs subtype it | Pattern 1. A submodule can subtype a parent-module abstract type through `using ..Quiver: AbstractExpression` (verified, spike) |
| JUL-02 | Ops defined once on `AbstractExpression`; File converted via `quiver_expression_from_file`; 97 forwarders deleted; no C API change | Patterns 2-3, Code Examples. Spike: `grep -c 'Binary.File' expression.jl` = 1; `detect_ambiguities(Quiver; recursive=true)` = 0; existing tests green |
| JUL-03 | Every op works on a raw `Binary.File` incl. `save`/`get_metadata`, tested | Coverage gap table + parity test pattern; spike exercised all 32 operation forms on raw files |
| DOC-01 | AGENTS.md root/src/src-c/julia describe the type + layout; fix the ExpressionFile line | Doc Delta Inventory (exact current text + line numbers) |
| DOC-02 | CHANGELOG complete, no planning IDs, version 0.13.0, six suites + sync green | CHANGELOG audit (4/4 BREAKING present, Julia Added missing), `assert_version.py` output, gate commands |
</phase_requirements>

## Project Constraints (from AGENTS.md)

No `CLAUDE.md` / `.claude/CLAUDE.md` exists (`.claude/` holds only `worktrees`). The directives below come from the
root and `bindings/julia/AGENTS.md`:
- Bindings stay thin. Julia crafts no error messages for these paths: every failure goes through `check` → `quiver_get_last_error`.
- **Always `GC.@preserve`** objects whose pointers cross a ccall (`bindings/julia/AGENTS.md`, "Rules and gotchas").
- `c_api.jl` is GENERATED; do not hand-edit it (none is needed here).
- No planning IDs (`JUL-0x`, `Phase 9`, ...) in code or test comments, or in CHANGELOG entries.
- Self-Updating: the AGENTS.md nearest each change is updated in the same phase.
- CHANGELOG: user-visible changes go under `[0.13.0] — unreleased`. **BREAKING** entries say what to do. No version bump (STATE/ROADMAP).
- Python is invoked as `uv run python ...`. Run quoted test filters directly, not through `cmd //c` (memory note).
- Julia formatting: `bindings/julia/format/format.bat` (PSR `Style` package; not enforced in CI). The spike file is already format-clean.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Expression semantics (ops, validation, save, metadata) | C++ core (`src/expression/`) | — | Logic lives in C++ (root Principles) |
| File → expression conversion | C API bridge `quiver_expression_from_file` | C++ `Expression(const AbstractExpression&)` | Copies the path only (`BinaryFile::node()`); no new C entry point |
| One abstract type + dispatch | Julia binding (`expression.jl`, `Quiver.jl`) | `binary/file.jl` (subtype declaration) | Pure Julia type hierarchy over two opaque handles |
| File metadata | C API `quiver_binary_file_get_metadata` (handle copy) | — | Mirrors the C++ virtual override and Lua `f:get_metadata` |
| Documentation of the finished surface | AGENTS.md files + CHANGELOG | `bindings/js/src/lua-api.ts` (already done in Phase 8) | Self-Updating rule |

## Current State (verified this session)

### `bindings/julia/src/Quiver.jl` (34 lines, read in full)
Module `Quiver`. It includes `c_api.jl` (`import .C`), then the database files, `helper_maps.jl`, `lua_runner.jl`,
then at lines 26-27:
```
include("binary/Binary.jl")
include("expression.jl")
```
The package has **no `export` statement** (`grep -rn "export " src` finds none), so every call site is qualified
(`Quiver.save`, `Quiver.Binary.get_metadata`). [VERIFIED: bindings/julia/src/Quiver.jl:26-27]

### `bindings/julia/src/binary/Binary.jl` (9 lines)
```
module Binary

using ..Quiver: C, check, Element, Optional

include("metadata.jl")
include("file.jl")
include("csv_converter.jl")

end
```
[VERIFIED: bindings/julia/src/binary/Binary.jl:1-9]

### `bindings/julia/src/binary/file.jl` (key parts)
- `mutable struct File` with `ptr::Ptr{C.quiver_binary_file}`; its finalizer calls `C.quiver_binary_file_close(x.ptr)` (lines 1-9).
- `File(path::String)` calls `C.quiver_binary_file_create` and returns an **unopened** handle (lines 27-31).
- `close!(file::File)` closes and nulls `ptr` (lines 39-45). This is `Binary.close!`, a different generic from `Quiver.close!`.
- `function get_metadata(file::File)` calls `C.quiver_binary_file_get_metadata(file.ptr, out_md)` and returns `Metadata(out_md[])` (lines 101-105).
[VERIFIED: bindings/julia/src/binary/file.jl:1-9,27-31,39-45,101-105]

### `bindings/julia/src/expression.jl` (266 lines, read in full)
- `mutable struct Expression` with `ptr::Ptr{C.quiver_expression}`. The inner constructor attaches the finalizer
  `x -> x.ptr != C_NULL && C.quiver_expression_close(x.ptr)` (lines 1-9). Each `Expression` owns its C handle, and
  `close!(e::Expression)` (lines 17-23) is idempotent.
- Conversion: `function Expression(file::Binary.File)` calls `check(C.quiver_expression_from_file(file.ptr, out))` (lines 11-15). Nothing preserves `file` across the call.
- Kernels `_binop(operation, ::Expression, ::Expression|::Real)`, `_binop(operation, ::Real, ::Expression)`,
  `_unop` (lines 25-47). They pass `x.ptr` with no `GC.@preserve`.
- Forwarders, all wrapping with `Expression(file)`: arithmetic 71-93 (20), comparison `@eval` 107-111 ×6 (30),
  operator sugar 120-124 ×4 (20), logical 137-141 ×2 (10), `!` 146 (1), unary 148-152 (5), `ifelse` 163-176 (7),
  `aggregate`/`aggregate_agents` 224-239 (2), `select_agents`/`rename_agents` 265-266 (2). **Total 97.**
  `grep -c 'Binary.File' src/expression.jl` → **54**. [VERIFIED: grep this session; breakdown matches DESIGN-STUDY.md:38]
- `save(e::Expression, path::String)` (178-181) and `get_metadata(e::Expression)` (183-187) exist only for
  `Expression`. **`Quiver.save(file, p)` and `Quiver.get_metadata(file)` are a `MethodError` today.**
- Keep the explanatory comments at lines 95-99 (why `eq`/`neq` are named-only) and 128-131 (why `&`/`|`/`!`) when
  rewriting. The spike dropped them, and that was a mistake.

### C++ / C semantics the Julia code relies on
- `class QUIVER_API AbstractExpression` has one pure virtual `node()` and a virtual `get_metadata()`. Its comment
  reads "BinaryFile overrides it to return the handle's in-memory metadata"; `save`/`aggregate*`/`*_agents` are
  non-virtual. [VERIFIED: include/quiver/expression/abstract_expression.h:22-54]
- `BinaryFile::node()` is `return std::make_shared<ExpressionFile>(get_file_path());`, and `ExpressionFile` reads the
  TOML in its constructor and owns a **private** `BinaryFile file_(path)`.
  [VERIFIED: src/expression/expression_file.cpp:11-13,30-32]
- `quiver_expression_from_file` runs `*out = new quiver_expression(quiver::Expression(file->binary_file));`.
  [VERIFIED: src/c/expression/expression.cpp:99-112] **Consequence:** once the call returns, the C expression holds
  no reference to the file handle. The Julia `File` only has to stay alive *during* that ccall.
- `quiver_expression_get_metadata` copies `expression->expression.get_metadata()` into a new metadata handle
  [VERIFIED: src/c/expression/expression.cpp:241-254]. A file converted to an `Expression` therefore reports
  **the TOML re-read at conversion**, not the handle's copy.
- A path-only `BinaryFile` (Julia `Binary.File(path)`) holds an empty metadata: `impl_(std::make_unique<Impl>(nullptr, file_path, BinaryMetadata{}))`. [VERIFIED: src/binary/binary_file.cpp:45-46]
- Lua: `get_metadata` binds `self.get_metadata()` (virtual) on both usertypes:
  `auto get_metadata = [](const AbstractExpression& self) -> BinaryMetadata { return self.get_metadata(); };` and
  `binary_file_type["get_metadata"] = get_metadata;`. So Lua `f:get_metadata()` returns the handle's metadata.
  [VERIFIED: src/lua_runner/expression.cpp:139,195]

## Planning Flag: where the Julia `get_metadata` generic lives

**Recommendation: `Binary` keeps owning the generic. `Quiver.jl` adds `import .Binary: get_metadata` between
`include("binary/Binary.jl")` and `include("expression.jl")`. `get_metadata(e::Expression)` in `expression.jl`
then extends that one function.** Confidence: HIGH (spike-verified).

Evidence:
| Fact | Source |
|------|--------|
| Two distinct generics today: `Quiver.get_metadata` (Expression only) and `Quiver.Binary.get_metadata` (File only) | expression.jl:183-187, file.jl:101-105; DESIGN-STUDY.md:40 |
| Callers: `Quiver.Binary.get_metadata(file)` in test_binary_file.jl:188 and the rename_agents file test (~test_expression.jl:1972); `Quiver.get_metadata(e)` at test_expression.jl:491, 545, 561, 2130 | grep this session |
| No `export` anywhere, so neither name leaks into user scope; ambiguity between exported names cannot occur | grep this session |
| C++ and Lua semantics: a file's `get_metadata` is the handle's in-memory copy (virtual override) | abstract_expression.h:30-32, lua_runner/expression.cpp:139,195 |
| After the change: `Quiver.get_metadata === Quiver.Binary.get_metadata` is `true`; 0 ambiguities; `Quiver.get_metadata(file)`, `Binary.get_metadata(expr)` both work | spike run this session |

Options rejected:
- **A separate `Quiver.get_metadata(a::AbstractExpression) = get_metadata(_expression(a))`.** This leaves two
  functions named `get_metadata` that answer differently for the same file: `Binary.get_metadata(f)` gives the
  handle copy, `Quiver.get_metadata(f)` the TOML re-read. They diverge on an unopened `Binary.File(path)` (empty
  vs TOML, binary_file.cpp:45-46) and on a file rewritten after opening. It also departs from C++/Lua. Reject.
- **Quiver declares `function get_metadata end`, Binary does `import ..Quiver: get_metadata`.** This is
  equivalent at runtime, but needs a new `import` line in `Binary.jl`, because a name brought in with `using`
  cannot be extended. It also moves the File method's owner away from where `File`/`Metadata` live. Acceptable
  but more edits. Not chosen.
- Leaving `get_metadata` out of the abstract path is correct either way. A file must **not** take the generic
  `_expression` route for metadata, since that would re-read the TOML. Dispatch picks `get_metadata(::File)`
  (handle copy) or `get_metadata(::Expression)`. This is the one per-type operation, and it mirrors the C++
  virtual.

Ordering constraint: the `import` must come **before** `include("expression.jl")`. If `expression.jl` defines
`get_metadata` first, the later `import` errors at load ("conflicts with an existing identifier"), which is
loud, not silent. [ASSUMED: exact Julia 1.12 error wording; the failure being a load error is standard Julia
binding semantics]

## Standard Stack

No new dependency. Julia 1.12.5 (the version `test.bat` pins via `julia +1.12.5`; Project.toml compat `julia = "1.11"`),
`Test` stdlib (`Test.detect_ambiguities`), existing CEnum/Libdl/Artifacts/Dates deps. [VERIFIED: bindings/julia/test/test.bat, Project.toml]

## Package Legitimacy Audit

Not applicable. The phase installs no external package.

## Architecture Patterns

### Data flow (Julia operation on a raw file)
```
user: a::Binary.File + 2.0
   │  dispatch: Base.:+(::AbstractExpression, ::Real)
   ▼
_binop(ADD, a, 2.0)
   │  _expression(a) ── File ──► Expression(file) ── ccall quiver_expression_from_file(file.ptr)
   │                                                   (C++ copies the PATH; reads .toml; no handle ref)
   │                  └─ Expression ──► itself
   ▼
GC.@preserve lhs  ccall quiver_expression_apply_scalar_right(lhs.ptr, 2.0, out)
   ▼
new Expression(out[]) (owns C handle, finalizer) ──► returned
   (temporary file-Expression: unreachable, freed by its finalizer)
```

### Pattern 1: Parent-module abstract type, submodule subtype
**What:** declare the type in `Quiver`, pull it into `Binary` with `using ..Quiver: AbstractExpression`, subtype in `file.jl`.
**Verified:** the spike loaded and `Quiver.Binary.File <: Quiver.AbstractExpression` is `true`. Subtyping needs no
`import`; only *extending a function* needs `import`.

### Pattern 2: One private conversion, ops typed on the abstract type
```julia
_expression(e::Expression) = e
_expression(a::AbstractExpression) = Expression(a)   # -> Expression(file::Binary.File)
```
Use `_expression` rather than a public `Expression(e::Expression) = e`. An identity constructor on a mutable,
finalizer-owning type is a trap: `close!(Expression(e))` would close `e`, while C++/Lua `Expression(e)` returns a
new value. With the helper, `Binary.File` stays on exactly one line (the existing constructor). The spike was
re-run with this exact helper: 0 ambiguities, all operations OK, and Expression testset 186/186.

### Pattern 3: Signature shapes (exactly three per binary op)
For each of `+ - * / & |`, the six named comparisons `gt lt gte lte eq neq`, and the four sugar operators `> < >= <=`:
`(a::AbstractExpression, b::AbstractExpression)`, `(a::AbstractExpression, b::Real)`, `(a::Real, b::AbstractExpression)`.
Unary: `- abs sqrt log exp !` on `(a::AbstractExpression)`. `Base.ifelse(::AbstractExpression ×3)` only, with no
`Real` branches; Phase 7 decided "no ifelse double overload in any layer" (STATE). Methods `save`, `aggregate`,
`aggregate_agents`, `select_agents`, `rename_agents` take `a::AbstractExpression` as their first argument.
**Ambiguity:** `Test.detect_ambiguities(Quiver; recursive=true)` returns 0 before (live code) and 0 after (spike).
`detect_ambiguities(Quiver, Base)` restricted to Quiver methods also returns 0. `AbstractExpression` is not a
`Number`/`Function`/`Missing`, so it is disjoint from every Base method that could collide (`!(::Function)`,
`ifelse(::Bool, x, y)`, `+(::Number, ::Missing)`, Bool arithmetic). [VERIFIED: spike]

### Pattern 4: `GC.@preserve` every handle whose `.ptr` crosses a ccall
The converted operands are now locals inside the kernels. Preserve them, and the file in the conversion:
`GC.@preserve file check(C.quiver_expression_from_file(file.ptr, out))`, `GC.@preserve lhs rhs check(...)`, etc.
Julia can collect an object after its last use, and loading `.ptr` counts as that last use. A finalizer
(`quiver_binary_file_close` deletes the handle) running mid-call would be a use-after-free. The window is tiny and
the old forwarders had the same exposure without failures. This hardening follows the binding's own "Always
`GC.@preserve`" rule. [ASSUMED: practical likelihood; the rule itself is VERIFIED from bindings/julia/AGENTS.md]

### Anti-Patterns to Avoid
- **Re-adding per-type methods** (`+(::Binary.File, ...)`) for "speed": the conversion cost is the same either way.
- **`Union{Expression, Binary.File}` alias instead of the abstract type:** rejected by JUL-01.
- **Routing `get_metadata(::File)` through `_expression`:** it changes semantics, giving the TOML re-read instead of the handle copy.
- **Touching `c_api.jl`** or regenerating it: criterion 2 requires an empty diff.
- **A public `Expression(::Expression)` identity:** see Pattern 2.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| File→expression in Julia | a new C entry point or borrowed handle | `quiver_expression_from_file` (exists) | Out of scope; violates the ownership rule |
| Ambiguity check | manual method audit | `Test.detect_ambiguities(Quiver; recursive = true)` | One line, exhaustive |
| Parity tests | per-op expected-value arithmetic | compare `save(op(file...))` cells with `save(op(Expression(file)...))` | The Expression path is already tested; equality proves parity |

## Runtime State Inventory

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None. No persisted data names the Julia types | none |
| Live service config | Published mirror `psrenergy/Quiver.jl` is generated from this directory at release (`.github/AGENTS.md`) | none (mirror regenerates) |
| OS-registered state | None | none |
| Secrets/env vars | `QUIVER_LIB_DIR` (optional override) unaffected | none |
| Build artifacts | Julia precompile cache `~/.julia/compiled/v1.12/Quiver*` is invalidated automatically by the source change; native libs unchanged (no C change) | none |

## Common Pitfalls

### Pitfall 1: The ROADMAP doc check is vacuous as written
**What goes wrong:** `grep -n 'caches an open BinaryFile' src/AGENTS.md` **already** returns nothing. The live
line is `src/AGENTS.md:1069`: ``- `ExpressionFile`: lazy reads from a `.qvr`. Caches an open `BinaryFile` and a reusable `unordered_map` across calls (mutable members; not thread-safe per instance).``
**How to avoid:** the plan must edit line 1069. Verify with `grep -niE 'caches an open .?BinaryFile' src/AGENTS.md`
(must be empty) **and** a positive grep for the new wording. Accurate replacement: the leaf is built from a path,
reads the `.toml` once at construction, owns a private **unopened** `BinaryFile` that `save()` opens with `'r'` and
closes on exit (`expression_file.cpp:11-13`, `expression.cpp:45-66`), never the caller's handle, and caches a
reusable `unordered_map` across `compute_row` calls.

### Pitfall 2: `import` after `include("expression.jl")`
Load-time error. Put `import .Binary: get_metadata` directly after `include("binary/Binary.jl")`.

### Pitfall 3: Windows file locks in new tests
Raw file handles opened in a test keep the `.qvr` locked. Close them (`Quiver.Binary.close!`) in `finally`, then
`cleanup(...)`; the helper already runs `GC.gc()` first (test_expression.jl:10-21). The spike that skipped the
closes left `a.qvr` EBUSY at temp cleanup.

### Pitfall 4: `get_metadata` on an unopened file
`Quiver.get_metadata(Binary.File("x"))` returns **empty** metadata (handle copy, binary_file.cpp:45-46), while
`get_metadata(Expression(file))` reads the TOML. This matches C++ (virtual override) and is unchanged from today's
`Binary.get_metadata`. Do not "fix" it in this phase. Test `get_metadata` on an opened file.

### Pitfall 5: Losing the explanatory comments
The eq/neq and `&`/`|`/`!` comments (expression.jl:95-99, 128-131) document deliberate surface choices. Keep them,
reworded from "{Expression, Real, Binary.File} combos" to the abstract type.

### Pitfall 6: Dart suite stale hook cache in `test-all.bat`
Since Phase 6, delete `bindings/dart/.dart_tool/hooks_runner/` and `bindings/dart/.dart_tool/lib/` before the
six-suite gate (ROADMAP preamble), or Dart fails for an unrelated reason.

### Pitfall 7: CHANGELOG "no planning ID" is easy to break in new text
The new Julia entry and doc text must not say "Phase 9", "JUL-01", "EQ-01", etc. Phase 8's review called the
`==`/`<` gap "EQ-01". In AGENTS.md, call it "an open decision", not by ID.

## Code Examples

### `Quiver.jl` (lines 25-28 after change)
```julia
include("lua_runner.jl")

abstract type AbstractExpression end

include("binary/Binary.jl")
import .Binary: get_metadata
include("expression.jl")
```
### `binary/Binary.jl` line 3 / `binary/file.jl` line 1
```julia
using ..Quiver: AbstractExpression, C, check, Element, Optional
mutable struct File <: AbstractExpression
```
### `expression.jl` (spike, ran green with `_expression`; restore the two explanatory comments)
```julia
mutable struct Expression <: AbstractExpression
    ptr::Ptr{C.quiver_expression}
    # inner constructor + finalizer unchanged
end

function Expression(file::Binary.File)            # the only Binary.File line
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve file check(C.quiver_expression_from_file(file.ptr, out))
    return Expression(out[])
end

_expression(e::Expression) = e
_expression(a::AbstractExpression) = Expression(a)

function _binop(operation, a::AbstractExpression, b::AbstractExpression)
    lhs, rhs = _expression(a), _expression(b)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve lhs rhs check(C.quiver_expression_apply(operation, lhs.ptr, rhs.ptr, out))
    return Expression(out[])
end
# _binop(op, ::AbstractExpression, ::Real) -> apply_scalar_right; _binop(op, ::Real, ::AbstractExpression) -> apply_scalar_left
# _unop(op, ::AbstractExpression) -> apply_unary; all with GC.@preserve on the converted local

Base.:-(a::AbstractExpression) = _unop(C.QUIVER_EXPRESSION_UNARY_OPERATION_NEGATE, a)   # + abs sqrt log exp !

for (op, cop) in ((:+, :QUIVER_EXPRESSION_OPERATION_ADD), (:-, :QUIVER_EXPRESSION_OPERATION_SUBTRACT),
    (:*, :QUIVER_EXPRESSION_OPERATION_MULTIPLY), (:/, :QUIVER_EXPRESSION_OPERATION_DIVIDE),
    (:&, :QUIVER_EXPRESSION_OPERATION_AND), (:|, :QUIVER_EXPRESSION_OPERATION_OR))
    @eval begin
        Base.$op(a::AbstractExpression, b::AbstractExpression) = _binop(C.$cop, a, b)
        Base.$op(a::AbstractExpression, b::Real) = _binop(C.$cop, a, b)
        Base.$op(a::Real, b::AbstractExpression) = _binop(C.$cop, a, b)
    end
end
# gt..neq @eval loop and the > < >= <= sugar loop: same three shapes, unchanged bodies

function Base.ifelse(condition::AbstractExpression, then_value::AbstractExpression, else_value::AbstractExpression)
    c, t, e = _expression(condition), _expression(then_value), _expression(else_value)
    out = Ref{Ptr{C.quiver_expression}}(C_NULL)
    GC.@preserve c t e check(
        C.quiver_expression_apply_ternary(C.QUIVER_EXPRESSION_TERNARY_OPERATION_IFELSE, c.ptr, t.ptr, e.ptr, out),
    )
    return Expression(out[])
end

function save(a::AbstractExpression, path::String)
    e = _expression(a)
    GC.@preserve e check(C.quiver_expression_save(e.ptr, path))
    return nothing
end

function get_metadata(e::Expression)          # extends Binary.get_metadata (imported in Quiver.jl)
    out = Ref{Ptr{C.quiver_binary_metadata}}(C_NULL)
    GC.@preserve e check(C.quiver_expression_get_metadata(e.ptr, out))
    return Binary.Metadata(out[])
end
# aggregate / aggregate_agents / select_agents / rename_agents: first arg a::AbstractExpression,
# `e = _expression(a)` first line, add `e` to the existing GC.@preserve lists; delete the four File forwarders.
```
Every C symbol above appears verbatim in the live `expression.jl` (lines 13, 27, 33, 39, 45, 49-69, 100-102, 132,
145, 157, 179, 185, 197, 214, 246, 260). [VERIFIED: bindings/julia/src/expression.jl, read in full]
Resulting size: 266 → about 170 lines. Spike formatted clean with `Style.format`.

### Parity test pattern (JUL-03): one testset, raw file vs `Expression(file)`
```julia
@testset "Binary.File is an AbstractExpression" begin
    @test Quiver.Binary.File <: Quiver.AbstractExpression
    @test Quiver.Expression <: Quiver.AbstractExpression
    @test Quiver.get_metadata === Quiver.Binary.get_metadata
    # write_fixture(path_a, ...); write_fixture(path_b, ...)  (positive values so log/sqrt are finite)
    ops = [
        (a, b) -> a + b, (a, b) -> a - b, (a, b) -> a * b, (a, b) -> a / b,
        (a, b) -> a + 2.0, (a, b) -> 2.0 - a, (a, b) -> 3.0 * a, (a, b) -> a / 4.0, (a, b) -> 60.0 / a,
        (a, b) -> a > b, (a, b) -> a < 12.0, (a, b) -> 12.0 >= a, (a, b) -> a <= b,
        (a, b) -> Quiver.gt(a, b), (a, b) -> Quiver.lt(a, b), (a, b) -> Quiver.gte(a, b),
        (a, b) -> Quiver.lte(a, b), (a, b) -> Quiver.eq(a, b), (a, b) -> Quiver.neq(1.0, a),
        (a, b) -> a & b, (a, b) -> a | b, (a, b) -> !a,
        (a, b) -> -a, (a, b) -> abs(a), (a, b) -> sqrt(a), (a, b) -> log(a), (a, b) -> exp(a),
        (a, b) -> ifelse(a > 12.0, a, b),
        (a, b) -> Quiver.aggregate(a, "row", Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM),
        (a, b) -> Quiver.aggregate_agents(a, Quiver.C.QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN),
        (a, b) -> Quiver.select_agents(a, ["val1"]),
        (a, b) -> Quiver.rename_agents(a, Dict("val1" => "alpha")),
    ]
    # for each op: save(op(fa, fb), out1) and save(op(Expression(fa), Expression(fb)), out2),
    # compare cells with isequal (NaN-safe) and labels via Binary.get_labels(get_metadata(...))
    # plus: Quiver.save(fa, out) copies the file; Quiver.get_metadata(fa) labels/unit;
    # Quiver.save(fa, path_a) throws Quiver.DatabaseException (self-collision);
    # save(writer_file, out) throws (write registry) and get_metadata(writer_file) still works
end
```
Mixed file/expression operands (`a + e`, `ifelse(e > x, a, e)`) are already covered by "Mixed Binary.File and
Expression" (test_expression.jl:925). Add one mixed `ifelse` case to the loop.

## Test Coverage Gap (raw `Binary.File` today, test_expression.jl)

| Operation | Covered on raw file? | Where |
|-----------|---------------------|-------|
| `+` file+file, `*` file*Real / Real*file, file+Expression | yes | :846, :867, :897, :925 |
| `-` `/` between files, Real−file, Real/file, file+Real | **no** | — |
| `> < >= <=`, `gt lt gte lte eq neq` on files | **no** (only via `with_expr`) | :1982 uses Expressions |
| `& | !` on files | **no** | :2086 uses Expressions |
| `-file`, `sqrt(file)` | yes | :1607, :1589 |
| `abs log exp` on file | **no** | — |
| `ifelse` all files | yes | :1763; mixed **no** |
| `aggregate`, `aggregate_agents`, `select_agents`, `rename_agents` | yes | :1390, :1409, :1878, :1958 |
| `save(file, path)` | **no** (MethodError today) | — |
| `Quiver.get_metadata(file)` | **no** (MethodError today) | — |
| subtype assertions | **no** | — |

Baselines (this session, Julia 1.12.5, Debug libs in `build/bin`): Expression testset **186** pass; full Julia suite
**1574** pass in about 2 min; `detect_ambiguities` **0**; lua-api sync **6 pass**.

## Doc Delta Inventory (DOC-01)

| File | Current (stale) | Required change |
|------|-----------------|-----------------|
| `AGENTS.md` Design Decisions (after the "Binary + expression subsystems" bullet, line 72) | no AbstractExpression decision | New bullet. One C++ base with one pure virtual `node()`; `Expression final` + `BinaryFile` derive; every op/method takes `const AbstractExpression&`. A file's `node()` is a path-based leaf, never the caller's handle (why: `save()` opens and closes inputs). `get_metadata` is the one virtual accessor (a file returns its handle's copy). Ctor is `explicit`. Lua: compile-time sol2 traits in `src/lua_runner/internal.h` + typed operands + one variadic fallback keeping the Pattern 1 texts and the too-many-arguments error (the runtime `bases` tag measured +2.3% on `f:read`). Julia: `abstract type AbstractExpression`, conversion through `quiver_expression_from_file`, one `get_metadata` generic. C API gets no abstract handle (ownership rule); `quiver_expression_from_file` is the bridge. Rejected: abstract `Expression` + renamed concrete type. Open: Lua `==`/`<` between expressions/files is always true. |
| `AGENTS.md:80` sandbox list | `` ..., `db:write_csv`, `expr:save`) `` | `` `save` on a file or an expression `` (`f:save` is sandboxed, lua_runner/expression.cpp:136-138) |
| `AGENTS.md:733-737` "Binary & Expression Subsystems" | describes `Expression` DAGs only | add: `BinaryFile` and `Expression` share `AbstractExpression`, so a file takes every expression op/method |
| `AGENTS.md:806-820` binary cross-layer table | `Get metadata` row is file-only | `Get metadata` row: C++ `x.get_metadata()` (file or expression), C `quiver_binary_file_get_metadata()` / `quiver_expression_get_metadata()`, Julia `get_metadata(x)`, Lua `x:get_metadata()`. New rows: **Is an expression** (`BinaryFile : AbstractExpression` / `quiver_expression_from_file()` bridge / `Binary.File <: AbstractExpression` / file usertype has every op), **Save** (`x.save(path)` / `quiver_expression_save()` / `save(x, path)` / `x:save(path)`), **Aggregate / agents** (`x.aggregate(...)`, `aggregate_agents`, `select_agents`, `rename_agents` / `quiver_expression_aggregate*`, `_select_agents`, `_rename_agents` / `aggregate(x, dim, op[, p])` ... / `x:aggregate(dim, op[, p])` ...) |
| `AGENTS.md:822-834` Lua expression paragraph | "build from a file with `quiver.expression(file)` (or operate on files directly)"; methods listed as `expr:...`; "`expr:save` paths are sandboxed" | a file handle takes the six methods directly (`f:aggregate`, ..., `f:save`, `f:get_metadata`); `quiver.expression(f)` still converts; `save` on a file or expression is sandboxed. (Phase 8 review IN-03 deferred this here.) |
| `src/AGENTS.md:1069` | `Caches an open BinaryFile ...` | Pitfall 1 wording |
| `src/AGENTS.md:1046` and `:976` | `` `expr:save` paths are sandboxed `` | `` `save` on a file or an expression `` |
| `src/AGENTS.md:330` | "`Expression` is a plain value type wrapping `shared_ptr<ExpressionNode>`" | add: `final`, derives from the stateless polymorphic `AbstractExpression` (protected copy/move, no slicing); `BinaryFile` (Pimpl) derives from it too. Optional, for consistency |
| `src/c/AGENTS.md:46` | `` src/c/expression/ # Expression node constructors, save, free `` | note: `quiver_expression_from_file` is the only file → expression bridge; it copies the path (the handle may be closed right after); no abstract/borrowed handle by design (ownership rule) |
| `src/c/AGENTS.md:38` | `` lua_runner.cpp # LuaRunner C API ... `` | optional: "over the core `LuaRunner`, whose sol2 binders live in `src/lua_runner/` (one file per core file it binds)" (layout clause of DOC-01) |
| `bindings/julia/AGENTS.md` "Julia-only surfaces" bullet (~line 144) | binary/expression mentioned only as Julia-only | new bullet: `AbstractExpression` in `Quiver.jl` before the Binary include (`using ..Quiver: AbstractExpression` in `Binary.jl`); ops defined once, `_expression` conversion, `GC.@preserve` of the converted handle and the file; a file's expression copies its path, so the file may close right after; one `get_metadata` generic owned by `Binary`, imported into `Quiver` before `include("expression.jl")` (file → handle metadata); do not re-add per-type forwarders. Its existing `src/lua_runner/lua_runner.cpp` citation (line 160) is already correct. |

Already done by Phases 6-8 (do not redo): the `src/lua_runner/` layout in root (LuaRunner Class paragraph,
Design Decisions file paths) and `src/AGENTS.md:47`; `src/AGENTS.md:31` (abstract_expression.h map), `:1055-1058`
(AbstractExpression bullets), `:865-875` (sol2 traits); `bindings/js/src/lua-api.ts` reference.

## CHANGELOG Audit (DOC-02)

`[0.13.0] — unreleased` (CHANGELOG.md:8-109), read in full:
| Required entry | Present? | Location |
|----------------|----------|----------|
| BREAKING C++ copy-init from a file (`Expression e = file;` → `Expression e(file);`) | yes | :63-69 |
| BREAKING C++ `Expression::metadata()` → `get_metadata()` | yes | :70-71 |
| BREAKING Lua `e:metadata()` → `e:get_metadata()` | yes | :72-73 |
| BREAKING Lua strict arity of `quiver.*` expression functions | yes | :74-79 |
| Added: Lua file gains expression methods | yes | :83-87 |
| Added: **Julia** `Binary.File`/`Expression` subtype `AbstractExpression`; `Quiver.save(file, path)`; `Quiver.get_metadata`/`Quiver.Binary.get_metadata` one function | **missing** | add under `### Added` |
Planning-ID scan of lines 8-110 (`[A-Z]{2,}-[0-9]+|Phase|phase`): **0 hits**. Compare link `[0.13.0]` present at
:1400. The Julia change is not BREAKING. Every previously accepted call still dispatches; the old file
`rename_agents(f, ::AbstractDict)` looser signature only moved where a non-string dict raises `MethodError`.
Suggested entry: "**Julia: a binary file is an expression.** `Quiver.Binary.File` and `Quiver.Expression` are
subtypes of `Quiver.AbstractExpression`, and every expression operation is defined on it, so a file also takes
`Quiver.save(file, path)`. `Quiver.get_metadata` and `Quiver.Binary.get_metadata` are now one function: either name
works on a file (its handle's metadata) or an expression."

`uv run python scripts/assert_version.py` output (this session): `All project files at 0.13.0` (CMakeLists, pyproject,
package.json, pubspec, Project.toml), exit 0.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Julia (juliaup channel 1.12.5) | Julia suite, formatter | ✓ | 1.12.5 (default `julia` is 1.11.9) | — |
| Built libs `build/bin/libquiver{,_c}.dll` | Julia in-tree loader | ✓ | Debug, current | rebuild `cmake --build build --config Debug` |
| bun | lua-api sync test | ✓ | 1.3.14 | — |
| uv | assert_version.py | ✓ | — | — |
| Dart/Python/JS toolchains | six-suite gate | ✓ (green at end of Phase 8) | — | — |

## Validation Architecture

Skipped (`workflow.nyquist_validation: false` in `.planning/config.json`). Commands the plans need:
- Julia, one file: `bindings/julia/test/test.bat test_expression.jl` (runtests.jl includes `ARGS[1]`); faster ad hoc:
  `julia +1.12.5 --project=bindings/julia -e "using Test, Quiver; include(\"bindings/julia/test/test_expression.jl\")"`
- Julia full: `bindings/julia/test/test.bat` (1574 baseline).
- Ambiguity gate: `julia +1.12.5 --project=bindings/julia -e "using Test, Quiver; @assert isempty(Test.detect_ambiguities(Quiver; recursive=true))"`
- Grep gates: `grep -c 'Binary.File' bindings/julia/src/expression.jl` → 1; `git diff --stat -- bindings/julia/src/c_api.jl include/quiver/c` → empty; `grep -niE 'caches an open .?BinaryFile' src/AGENTS.md` → empty.
- Six suites: delete the Dart hook cache, then `scripts/test-all.bat`. Sync: `cd bindings/js && bun test test/lua-api-sync.test.ts` (6 pass).

## Security Domain

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2/V3/V4 | no | — |
| V5 Input Validation | yes (unchanged) | C++ core validates operands/paths; Julia only marshals |
| V6 Cryptography | no | — |

| Threat | STRIDE | Mitigation |
|--------|--------|------------|
| Use-after-free: finalizer frees a C handle while its `.ptr` is in a ccall | Tampering / DoS | `GC.@preserve` on the converted locals and the file (Pattern 4) |
| `Quiver.save(file, path)` writes anywhere | — (by design) | Julia is a trusted host; the sandbox is LuaRunner policy only (root decision: "Julia's standalone `open_file` is unaffected") |
| Overwriting an input via save | Tampering | C++ `save` path-collision check (`Quiver.save(f, path_of_f)` throws), tested |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Exact Julia error wording when `import` follows a local definition | Planning Flag | None: either way it fails loudly at load |
| A2 | The GC race without `GC.@preserve` is real but rare | Pattern 4 | None: the preserve is cheap and rule-mandated |

## Open Questions (RESOLVED)

None blocking. One judgment call: whether `src/AGENTS.md:330` / `:335` ("plain value type" wording for `Expression`)
should mention the base. Recommend a one-clause edit for accuracy.

## Sources

### Primary (HIGH)
- Live code read this session: `bindings/julia/src/{Quiver.jl, expression.jl, binary/Binary.jl, binary/file.jl}`,
  `include/quiver/expression/abstract_expression.h`, `src/expression/expression_file.cpp`, `src/c/expression/expression.cpp`,
  `src/binary/binary_file.cpp:40-50,306-313`, `src/lua_runner/expression.cpp`, `CHANGELOG.md:1-112`, all four AGENTS.md files.
- Spike: scratch copy of `bindings/julia` with the recommended edits, run with `QUIVER_LIB_DIR=build/bin` on Julia 1.12.5.
  Results: subtypes true, `get_metadata` identity true, 0 ambiguities, 32 operation forms on raw files return
  `Expression`, `save(file)` round-trips, guards throw `DatabaseException`, and test_expression (186), test_binary_file (83),
  test_binary_metadata (161), test_csv_converter (70) all pass. `Style.format` leaves it unchanged.
- `.planning/research/{SUMMARY.md, DESIGN-STUDY.md}`, Phase 7/8 VERIFICATION/REVIEW/SUMMARY, STATE.md.

## Metadata

**Confidence breakdown:** Julia design HIGH (executed); planning flag HIGH (executed + semantic evidence);
doc deltas HIGH (line-cited); CHANGELOG HIGH (read in full).
**Research date:** 2026-10-04. **Valid until:** the next change to `bindings/julia/src/` or the four AGENTS.md files.
