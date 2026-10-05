# Phase 9: Julia AbstractExpression and Docs - Pattern Map

**Mapped:** 2026-10-04
**Files analyzed:** 10
**Analogs found:** 10 / 10 (all modifications of existing files; the file itself is the analog)

RESEARCH.md "Code Examples" holds the spike-verified Julia code. Use it verbatim; this map only adds
the house-style excerpts that code and docs must match.

## File Classification

| File | Role | Data Flow | Closest Analog | Match |
|------|------|-----------|----------------|-------|
| `bindings/julia/src/Quiver.jl` | config (module root) | — | itself, lines 26-27 | exact |
| `bindings/julia/src/binary/Binary.jl` | config (submodule) | — | itself, line 3 | exact |
| `bindings/julia/src/binary/file.jl` | model (FFI handle) | request-response | itself, lines 1-9, 101-105 | exact |
| `bindings/julia/src/expression.jl` | service (FFI wrappers) | transform | itself (266 lines) | exact |
| `bindings/julia/test/test_expression.jl` | test | file-I/O | "Mixed Binary.File and Expression" testset, lines 925-950 | exact |
| `AGENTS.md` (root) | docs | — | "Binary + expression subsystems" Design Decision bullet; binary cross-layer table (~806-820) | exact |
| `src/AGENTS.md` | docs | — | line 1069 (`ExpressionFile`), 976, 1046, 330 | exact |
| `src/c/AGENTS.md` | docs | — | line 46 tree entry | exact |
| `bindings/julia/AGENTS.md` | docs | — | "Julia-only surfaces" bullet, ~line 147 | exact |
| `CHANGELOG.md` | docs | — | `[0.13.0]` BREAKING lines 63-79, Added 83-87 | exact |

## Pattern Assignments

### `bindings/julia/src/Quiver.jl`
Current lines 26-27: `include("binary/Binary.jl")` / `include("expression.jl")`. Insert
`abstract type AbstractExpression end` before, and `import .Binary: get_metadata` between them
(must precede `include("expression.jl")`, else load error). No `export` anywhere in the package — keep it so.

### `bindings/julia/src/binary/Binary.jl` line 3
`using ..Quiver: C, check, Element, Optional` → add `AbstractExpression` (alphabetical first).

### `bindings/julia/src/binary/file.jl` line 1
`mutable struct File` → `mutable struct File <: AbstractExpression`. Leave `get_metadata(file::File)`
(101-105) untouched: it stays the owner of the generic and returns the handle's copy.

### `bindings/julia/src/expression.jl`
Rewrite per RESEARCH "Code Examples". Rules:
- `Binary.File` appears on exactly one line (the `Expression(file::Binary.File)` constructor).
- Private `_expression` helper, not a public identity `Expression(::Expression)`.
- `GC.@preserve` every converted local and the file in the conversion ccall.
- Keep comments at current lines 95-99 (eq/neq named-only) and 128-131 (`&`/`|`/`!`), reworded to the abstract type.
- `get_metadata(e::Expression)` stays as a method extending `Binary.get_metadata`.
- `ifelse`: only the all-`AbstractExpression` signature (no Real overloads).

### `bindings/julia/test/test_expression.jl` (add one testset)
**Analog:** lines 925-950. Fixture/cleanup idiom to copy:
```julia
@testset "Mixed Binary.File and Expression" begin
    path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
    try
        write_fixture(path_a, (r, c, k) -> r + c + k)
        write_fixture(path_b, (r, c, k) -> r * 10 + c + k)
        a = Quiver.Binary.open_file(path_a; mode = 'r')
        b = Quiver.Binary.open_file(path_b; mode = 'r')
        try
            ...
            Quiver.save(c, path_out)
            Quiver.close!(c)
        finally
            Quiver.Binary.close!(a)
            Quiver.Binary.close!(b)
        end
        @test read_all_cells(path_out) == ...
    finally
        cleanup(path_a, path_b, path_out)
    end
end
```
Helpers available at top of file: `make_path(name)` (lines 6-8, tempdir), `cleanup(paths...)`
(10-21, runs `GC.gc()` then removes `.qvr`/`.toml`), `make_simple_metadata()` (23+), `write_fixture`,
`read_all_cells`. Close raw handles in `finally` before `cleanup` (Windows EBUSY). Parity loop
skeleton: RESEARCH "Parity test pattern". Use positive fixture values (log/sqrt) and `isequal` (NaN-safe).
Errors asserted as `@test_throws Quiver.DatabaseException`. No planning IDs in comments.

### `CHANGELOG.md` — add under `### Added` (after line 87)
House style (lines 83-87): bold lead sentence with layer prefix, then plain prose:
```markdown
- **Lua: a binary file is an expression.** A file from `db:open_file` takes `aggregate`,
  `aggregate_agents`, `select_agents`, `rename_agents` and `save` directly, `get_metadata` works on
  files and expressions alike, ...
```
BREAKING style (lines 70-71), for reference only — the Julia entry is NOT breaking:
```markdown
- **BREAKING** **C++: `Expression::metadata()` is renamed `get_metadata()`**, the name `BinaryFile`
  and the C API already use. Call `get_metadata()`.
```
Wrap at ~100 cols, two-space continuation indent. Suggested text in RESEARCH "CHANGELOG Audit".

### Root `AGENTS.md`
- **Design Decision bullet** (insert after "Binary + expression subsystems are exposed in Julia and Lua only."):
  style = `- **Bold one-sentence decision.** Rationale prose, file paths in backticks, rejected alternatives
  named ("was implemented first and rejected: ..."), consequences stated; wrapped ~100 cols, two-space
  continuation. Content list: RESEARCH "Doc Delta Inventory" row 1. Call the Lua `==`/`<` gap "an open decision", no ID.
- **Binary cross-layer table**: columns `| Category | C++ | C API | Julia | Lua |`; existing row to mimic:
  `| Get metadata | binary_file.get_metadata() | quiver_binary_file_get_metadata() | get_metadata(file) | file:get_metadata() |`.
  Update that row and add Is-an-expression / Save / Aggregate+agents rows (inventory row 4).
- Line 80 sandbox list `expr:save` → `save` on a file or an expression; lines 733-737 and 822-834 paragraphs per inventory.

### `src/AGENTS.md`
Line 1069 current:
```
  - `ExpressionFile`: lazy reads from a `.qvr`. Caches an open `BinaryFile` and a reusable `unordered_map` across calls (mutable members; not thread-safe per instance).
```
Replace with: built from a path, reads `.toml` once at construction, owns a private unopened
`BinaryFile` that `save()` opens `'r'` and closes on exit (never the caller's handle), caches a reusable
`unordered_map` across `compute_row` calls. Verify with `grep -niE 'caches an open .?BinaryFile'` (empty).
Lines 976, 1046: `expr:save` → `save` on a file or an expression.

### `src/c/AGENTS.md` line 46
```
src/c/expression/           # Expression node constructors, save, free
```
Extend comment / add a note below the tree: `quiver_expression_from_file` is the only file → expression
bridge, copies the path (handle may close right after); no abstract/borrowed handle by design.

### `bindings/julia/AGENTS.md`
New bullet next to "Julia-only surfaces" (~line 147), same `- **Bold label**: prose` style; content per
inventory last row. "Do not re-add per-type forwarders."

## Shared Patterns

### FFI call + error
**Source:** `bindings/julia/src/expression.jl` (every op): `out = Ref{Ptr{C.quiver_expression}}(C_NULL)`;
`check(C.quiver_expression_...(…, out))`; `return Expression(out[])`. Julia crafts no messages; `check`
surfaces `quiver_get_last_error`.

### GC safety
**Source:** `bindings/julia/AGENTS.md` "Always `GC.@preserve`" — wrap each ccall touching `.ptr`.

### Docs hygiene
No planning IDs (`Phase 9`, `JUL-0x`, `EQ-01`) in code, tests, AGENTS.md or CHANGELOG.

## No Analog Found
None.

## Metadata
**Search scope:** `bindings/julia/{src,test}`, `CHANGELOG.md`, four AGENTS.md files.
**Pattern extraction date:** 2026-10-04
