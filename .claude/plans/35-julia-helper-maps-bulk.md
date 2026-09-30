# 35 — Julia `helper_maps`: two bulk reads and a `Dict` instead of N queries and O(N·M) scans

**Batch** 4 · **Severity** medium · **Breaking** no (same results, same errors) · **Size** S · **Layers** Julia binding only
**Depends on** none · **Overlaps with** 42 (renames Julia date-time readers in `database_read.jl`; no overlap with the functions used here), 41 (edits comments in `database_read.jl` only)

## Why

`bindings/julia/src/helper_maps.jl` is a documented Julia-only convenience (root AGENTS.md
"Relation map helpers (Julia only)"). Both helpers issue **one FFI round-trip and one SQL query per
element** of `collection_from`, and resolve each related id with a linear `findfirst` over all ids
of `collection_to`:

```julia
function scalar_relation_map(db::Database, collection_from::String, collection_to::String, relation_type::String)
    attribute_on_collection_from = lowercase(collection_to) * "_" * relation_type
    collection_from_ids = read_scalar_integers(db, collection_from, "id")
    collection_to_ids = read_scalar_integers(db, collection_to, "id")
    map_of_indexes = Vector{Int}(undef, length(collection_from_ids))

    for (index_from, id_from) in enumerate(collection_from_ids)
        related_id = read_scalar_integer_by_id(db, collection_from, attribute_on_collection_from, id_from)
        if related_id !== nothing
            # It has to find some match every time
            index_to = findfirst(isequal(related_id), collection_to_ids)
            map_of_indexes[index_from] = index_to
        else
            map_of_indexes[index_from] = -1
        end
    end

    return map_of_indexes
end
```

`set_relation_map` has the same shape with `read_set_integers_by_id`, and names a local variable
`set_relation_map` (currently ~L65), shadowing the function it is inside.

For 10,000 children that is 10,000 `SELECT ... WHERE id = ?` statements plus ~10^8 comparisons. The
core already guarantees what a bulk read needs (root AGENTS.md "Bulk reads of one collection are
positionally aligned"): `read_scalar_integers` returns one entry per element in `rowid` order with
NULL kept as `nothing`, and `read_set_integers` returns one inner vector per element in the same
`rowid` order (`src/database_read.cpp`: `... FROM <collection> c LEFT JOIN <set_table> g ON g.id =
c.id ORDER BY c.rowid, g.rowid`), with the same inner order as `read_set_integers_by_id`
(`WHERE id = ? ORDER BY rowid`). `read_scalar_integers(db, c, "id")` is also `rowid`-ordered, so
position *i* of every bulk read is element *i* of `collection_from`.

Principles: readability and simplicity (three FFI calls and one `Dict` say what the function means),
and the thin-binding rule is respected (still composes public reads).

## Constraints and decisions

- **Maintainer notes (binding):** keep the docstrings and the results; the existing
  `bindings/julia/test/test_helper_maps.jl` pins behaviour. Keep the "missing relation" sentinel
  exactly: `-1` for a scalar with no relation, `Int[]` for an element with no set rows.
- The helpers stay in Julia (root AGENTS.md documents them as a deliberate exception to the
  thin-bindings rule, "kept in the binding because only Julia consumers use it"). Do not move them
  to C++.
- Julia has **no** `read_element_ids` wrapper (`grep -rn "function read_element_ids"
  bindings/julia/src` finds nothing; only `c_api.jl` has the raw ccall), so keep using
  `read_scalar_integers(db, collection_to, "id")` for the id list, as the current code does. It
  returns a concrete `Vector{Int64}` (`id` is reported `not_null`, root AGENTS.md scalar-NULL
  decision).
- Error behaviour on an id that cannot be resolved (a dangling FK — impossible with `ON DELETE SET
  NULL/CASCADE`, but reachable via raw SQL with foreign keys off): today `findfirst` returns
  `nothing` and the assignment into `Vector{Int}` throws a `MethodError`/`InexactError`-style
  conversion error; after the change `position[id]` throws `KeyError`. Both are errors; the new one
  names the missing id. Accept this.
- `read_scalar_integers` on the FK column returns `Vector{Optional{Int64}}` (nullable column) or
  `Vector{Int64}` (a `NOT NULL` FK). Handle both with `isnothing`.

## Changes

### `bindings/julia/src/helper_maps.jl` — rewrite both function bodies

Keep both docstrings byte-for-byte. Replace the bodies.

`scalar_relation_map`, new body:
```julia
function scalar_relation_map(
    db::Database,
    collection_from::String,
    collection_to::String,
    relation_type::String,
)
    attribute_on_collection_from = lowercase(collection_to) * "_" * relation_type
    position = Dict(id => index for (index, id) in enumerate(read_scalar_integers(db, collection_to, "id")))
    related_ids = read_scalar_integers(db, collection_from, attribute_on_collection_from)
    return Int[isnothing(related_id) ? -1 : position[related_id] for related_id in related_ids]
end
```

`set_relation_map`, new body:
```julia
function set_relation_map(
    db::Database,
    collection_from::String,
    collection_to::String,
    relation_type::String,
)
    attribute_on_collection_from = lowercase(collection_to) * "_" * relation_type
    position = Dict(id => index for (index, id) in enumerate(read_scalar_integers(db, collection_to, "id")))
    related_ids = read_set_integers(db, collection_from, attribute_on_collection_from)
    return Vector{Int}[Int[position[related_id] for related_id in ids] for ids in related_ids]
end
```

Why this is equivalent:
- Outer order: `read_scalar_integers(db, collection_from, attr)` and `read_set_integers(db,
  collection_from, attr)` both return one entry per `collection_from` element in `rowid` order —
  the same order as the old `read_scalar_integers(db, collection_from, "id")` loop.
- Inner order (sets): `ORDER BY c.rowid, g.rowid` in the bulk reader equals the by-id reader's
  `ORDER BY rowid`.
- An element with no set rows is an empty inner vector in the bulk reader (LEFT JOIN), matching
  the old `Int[]`.
- Return types: `Vector{Int}` and `Vector{Vector{Int}}`, as before.
- The shadowing local `set_relation_map` is gone.
- The `# It has to find some match every time` comments are gone; `position[...]` throws a
  `KeyError` if that assumption is ever broken.

`read_set_integers` resolves the table with `find_set_table(collection, attribute)` exactly as
`read_set_integers_by_id` does, so the `parent_ref` column name shared by `Child_vector_refs` and
`Child_set_parents` in `tests/schemas/valid/relations.sql` resolves to the same set table as today.

## Tests

No new behaviour, so the existing `bindings/julia/test/test_helper_maps.jl` is the regression net.
It must pass **unchanged**. It covers:
- `scalar_relation_map`: basic mapping `[1, 2, -1]`, after deleting parents (SET NULL → `-1`),
  after deleting children, after deleting both.
- `set_relation_map`: basic mapping `[Int64[1, 2], Int64[2, 3], Int64[]]`, with set relations (4
  children, one with no refs → `Int[]`), after deleting parents (CASCADE), after deleting children.

Add one test that pins the property the rewrite relies on — the result does not depend on id
gaps or on `collection_to` order — at the end of `@testset "scalar_relation_map"`:

```julia
        @testset "Positions Follow Collection Order Not Ids" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")
            for i in 1:5
                Quiver.create_element!(db, "Parent"; label = "Parent $i")  # ids 1..5
            end
            Quiver.delete_element!(db, "Parent", Int64(1))
            Quiver.delete_element!(db, "Parent", Int64(3))                 # remaining ids [2, 4, 5]

            Quiver.create_element!(db, "Child"; label = "Child A", parent_id = 5)
            Quiver.create_element!(db, "Child"; label = "Child B", parent_id = 2)
            Quiver.create_element!(db, "Child"; label = "Child C")

            @test Quiver.scalar_relation_map(db, "Child", "Parent", "id") == [3, 1, -1]

            Quiver.close!(db)
        end
```

Before writing it, check that `relations.sql`'s `Child.parent_id` references `Parent(id)` with a
nullable column and that `create_element!(...; parent_id = 5)` takes an integer id there (the
existing "Basic Mapping" test does exactly that, so it does).

## Docs and changelog

- No AGENTS.md text describes the helpers' implementation (root AGENTS.md "Relation map helpers" and
  `bindings/julia/AGENTS.md:106` describe what they return), so no doc edit is needed. Re-read both
  passages after the change to confirm.
- `CHANGELOG.md`, `## [0.12.0] — unreleased` → `### Fixed` (user-visible performance only):
  ```markdown
  - **Julia: `scalar_relation_map` / `set_relation_map` read in bulk.** They issued one query per
    element and a linear search per relation; they now make three reads and a dictionary lookup, so
    they scale linearly. Results are unchanged.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `bindings/julia/test/test.bat test_helper_maps.jl` — all existing testsets plus the new one pass.
3. `bindings/julia/test/test.bat` — full suite.
4. `scripts/format.bat`.

## Acceptance criteria

- [ ] Neither helper calls a `_by_id` reader or `findfirst`.
- [ ] Docstrings unchanged; return types `Vector{Int}` / `Vector{Vector{Int}}`.
- [ ] `test_helper_maps.jl` passes unchanged, plus the new ordering test.

## Pitfalls

- Do not call a non-existent Julia `read_element_ids`; use `read_scalar_integers(db, c, "id")`.
- `read_scalar_integers` on a nullable FK returns `Vector{Union{Nothing, Int64}}`; the comprehension
  must test `isnothing` before the `Dict` lookup.
- The formatter may reflow the long `Dict(...)` line; that is fine.

## Out of scope

- Moving the helpers into C++ or other bindings.
- The array fan-out behaviour the root AGENTS.md mentions for `create_element!` in these tests.

## Implementation notes

Implemented on `rs/plan35` in `C:\Development\Quiver\quiver3`. The branch already sat at
`origin/master` `a859229`, which includes plans 32 and 34 (#344, #345). `git fetch origin && git merge
origin/master` reported "Already up to date". Neither plan touched `helper_maps.jl` or its test. Plan 34
added a bullet to `bindings/julia/AGENTS.md` below the anchor edited here, and an `### Added` block to
`CHANGELOG.md`; both are untouched. **Depends on** none, so nothing blocked.

### Drift fixed

- **`set_relation_map` had already learned to skip NULL set cells.** The plan's excerpt predates that.
  The code called the kernel `_read_set_integers_by_id(..., false)`, not `read_set_integers_by_id`. The
  docstring says "A null cell in the set group … is skipped", and the `"NULL Cell Is Not A Target"`
  testset pins it (`parent_ref = [1, nothing]` → `[Int64[1]]`). The plan's body
  `Int[position[related_id] for related_id in ids]` would raise `KeyError(nothing)` there. The inner
  comprehension filters instead, keeping the existing comment:
  `Int[position[id] for id in ids if !isnothing(id)]`.
- **Julia now has `read_element_ids`** (`database_read.jl`, exercised by the delete, read-scalar and
  csv-import suites). The plan's constraint and its first pitfall assume it does not exist. The id list
  comes from `read_element_ids(db, collection_to)` instead of `read_scalar_integers(db, collection_to,
  "id")`. It is the same `SELECT id ... ORDER BY rowid` with the same concrete `Vector{Int64}`, matches
  the docstring's "list of ids of the related collection", and skips the `get_scalar_metadata` round-trip.
- **`bindings/julia/AGENTS.md` did describe the implementation.** The plan says no AGENTS.md text does.
  The nullability bullet said "`set_relation_map` passes `false` (its values end up untyped), so no
  composite pays a `list_*_groups` round-trip per column or per element". That clause is removed; the
  sentence now covers only `read_{vectors,sets}_by_id` ("per column"). Root AGENTS.md "Relation map
  helpers" and the Julia "Julia-only surfaces" bullet describe results only and were left as they are.
  `tests/AGENTS.md`'s "`set_relation_map`'s skip of a NULL cell" is still true.
- **CHANGELOG section.** The current unreleased section is `## [0.12.6] — unreleased`, not 0.12.0.
  The plan's entry also said "three reads"; the rewrite makes two bulk reads, and the entry says so.
  It is filed under `### Fixed`, as the plan says.

### Deviations

- The id list uses `read_element_ids` and the set comprehension filters `nothing`; both are drift fixes
  above. Everything else is the plan's text: the ordering test verbatim, and both docstrings
  byte-for-byte.
- **No red-green run.** This is a refactor with unchanged results, so the new testset passes on the
  old code too, and no test could fail before the change. Instead, a throwaway script in the session
  scratchpad (not committed) ran the old bodies and the new ones side by side. It used
  `relations.sql` in memory with 2,000 parents (4 deleted), and 5,000 children (every 7th with no
  relation, the rest with one `parent_id` and two `parent_ref`s). It asserted that old and new return
  equal results for both helpers, then took the best of 5 runs:

  | helper | old | new |
  |---|---|---|
  | `scalar_relation_map` | 255.8 ms | 27.6 ms |
  | `set_relation_map` | 484.0 ms | 94.5 ms |

- Behaviour differences, both only reachable outside the schema contract:
  - A dangling FK (possible only via raw SQL with foreign keys off) raises `KeyError` instead of a
    `MethodError` on `convert(Int, nothing)`. The plan accepts this.
  - The set helper now reads through the public bulk `read_set_integers`, which asks
    `list_set_groups` once per call for the column's `not_null`. For a `NOT NULL` set column holding a
    masked cell (a non-STRICT table), that reader raises where the old `false`-kernel path decoded the
    cell as `nothing` and skipped it. The raise is the public readers' documented behaviour.

### Verification results

- `cmake --build build --config Debug`: no work to do.
- `bindings/julia/test/test.bat test_helper_maps.jl`:
  - before the edit: `Helper Maps | 24 24`
  - after: `Helper Maps | 25 25`, which is every existing testset, byte-unchanged (`git diff` shows only
    additions to the test file), plus "Positions Follow Collection Order Not Ids"
- `bindings/julia/test/test.bat` (full suite): `test set | 1567 1567 1m26.6s`, "Testing Quiver tests
  passed". The `[error] Failed to apply schema` lines in the log come from the invalid-schema tests,
  which expect them.
- `scripts/format.bat`: exit 0. JuliaFormatter left every Julia file unchanged, Ruff reported "35 files
  left unchanged", and clang-format/dart had nothing to change.
  - Biome again "fixed" 43 JS files by rewriting CRLF→LF only (`git diff --stat` counted no content
    change there), the quirk plan 30's notes describe. Restored with `git checkout -- bindings/js`.
  - The final diff is exactly `CHANGELOG.md`, `bindings/julia/AGENTS.md`, `helper_maps.jl`,
    `test_helper_maps.jl` and this file. No `.bat` file was touched.

### Acceptance criteria

- [x] Neither helper calls a `_by_id` reader or `findfirst`. `grep -nE "_by_id|findfirst"
  bindings/julia/src/helper_maps.jl` finds nothing.
- [x] Docstrings unchanged; return types `Vector{Int}` (`Int[...]`) / `Vector{Vector{Int}}`
  (`Vector{Int}[...]`).
- [x] `test_helper_maps.jl` passes unchanged, plus the new ordering test (25/25).

### For later plans

- Plans 41 and 42 edit `database_read.jl`, and 42 renames `read_{vector,set}_date_time_by_id`. Neither
  touches these helpers, which now call only `read_element_ids`, `read_scalar_integers` and
  `read_set_integers`.
- No caller passes a literal `false` to `_read_set_integers_by_id` any more. The kernels' `not_null`
  parameter still takes `false` from `read_{vectors,sets}_by_id` (via `_group_value_not_null`), so it
  stays.
