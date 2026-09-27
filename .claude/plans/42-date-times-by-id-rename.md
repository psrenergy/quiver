# 42 — Julia/Python: rename `read_{vector,set}_date_time_by_id` to `read_{vector,set}_date_times_by_id`

**Batch** 4 · **Severity** low · **Breaking** yes — Julia and Python callers of the two old names must switch to the new ones · **Size** S · **Layers** Julia binding, Python binding, root CLAUDE.md, CHANGELOG
**Depends on** 18 recommended first (it deletes the per-column group-reader compositions, which contain four of the call sites listed below) · **Overlaps with** 18 (same functions' callers), 41 (comments right next to these functions), 30 (Python docstrings in the same file)

## Why

Root CLAUDE.md "Naming Convention": *"Singular vs plural: Type name matches return cardinality
(`read_scalar_integers` returns vector, `read_scalar_integer_by_id` returns optional)"*, and
*"given any C++ method name, you can derive the equivalent in any layer."*

These two readers return a **list**, but their Julia and Python names are singular:

- Julia `bindings/julia/src/database_read.jl`:
  `function read_vector_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)` (~L385)
  and `function read_set_date_time_by_id(...)` (~L446).
- Python `bindings/python/src/quiverdb/database.py`: `def read_vector_date_time_by_id(` (~L547) and
  `def read_set_date_time_by_id(` (~L558).

Their counterparts are already plural: Dart `readVectorDateTimesById` / `readSetDateTimesById`, and
every binding's boolean readers (`read_vector_booleans_by_id`, `read_set_booleans_by_id`). The root
CLAUDE.md DateTime wrapper table (~L746, ~L748) records the inconsistency:

```
| `read_vector_date_time_by_id` | `readVectorDateTimesById` | `read_vector_date_time_by_id` | string vector read + date parsing |
| `read_set_date_time_by_id`    | `readSetDateTimesById`    | `read_set_date_time_by_id`    | string set read + date parsing    |
```

Principle: Homogeneity, meaning mechanically derivable names.

## Constraints and decisions

- **Maintainer decision (binding):** rename only. Keep `read_scalar_date_time_by_id` singular: it
  returns one optional value. Keep Julia's `read_time_series_row(...; date_time::DateTime)` keyword
  as is. BREAKING, with a CHANGELOG entry under 0.11.0 listing old and new names. Update the root
  CLAUDE.md table. No manifest bump is needed (0.11.0 is already the unreleased minor).
- No aliases or deprecation shims. Root principle: "Delete unused code, do not deprecate."
- Julia does not `export` these names (`bindings/julia/src/Quiver.jl` has no export list for them);
  callers use `Quiver.<name>`. Python has no `__init__.py` re-export of them either.

## Changes

### Julia — `bindings/julia/src/database_read.jl`

1. Rename the definition `function read_vector_date_time_by_id(` (~L385) to
   `function read_vector_date_times_by_id(`. Keep the body.
2. Rename `function read_set_date_time_by_id(` (~L446) to `function read_set_date_times_by_id(`.
3. Update the internal callers:
   - `read_vectors_by_id` composite (~L511): `result[name] = read_vector_date_time_by_id(db, collection, name, id)`
     becomes `read_vector_date_times_by_id`.
   - `read_sets_by_id` composite (~L532): same, with `read_set_date_times_by_id`.
   - `read_vector_group_by_id` / `read_set_group_by_id` (~L562, ~L605): plan 18 deletes these
     per-column compositions. If 18 has **not** landed, rename these two call sites too. If it has,
     they are gone.

Check with `grep -n "date_time_by_id" bindings/julia/src/*.jl`. Only `read_scalar_date_time_by_id`
may remain.

### Python — `bindings/python/src/quiverdb/database.py`

1. Rename `def read_vector_date_time_by_id(` (~L547) to `def read_vector_date_times_by_id(`.
2. Rename `def read_set_date_time_by_id(` (~L558) to `def read_set_date_times_by_id(`.
3. Update the internal callers:
   - `read_vectors_by_id` (~L1948): `self.read_vector_date_time_by_id(...)` becomes
     `self.read_vector_date_times_by_id(...)`.
   - `read_sets_by_id` (~L1970): same, with the set name.
   - `read_vector_group_by_id` / `read_set_group_by_id` (~L2013, ~L2047): plan 18 replaces these.
     Rename them only if 18 has not landed.
4. If either docstring mentions its own name, update it.

Check with `grep -n "date_time_by_id" bindings/python/src/quiverdb/database.py`. Only
`read_scalar_date_time_by_id` may remain.

## Tests

Rename every test call (no behaviour change):

- `bindings/julia/test/test_database_read_vector.jl:~180`:
  `@test_throws ArgumentError Quiver.read_vector_date_time_by_id(db, "AllTypes", "label_value", 1)`
  becomes `Quiver.read_vector_date_times_by_id(...)`.
- `bindings/julia/test/test_database_read_set.jl:~187`: `Quiver.read_set_date_time_by_id(db, "AllTypes", "tag", 1)`
  becomes `Quiver.read_set_date_times_by_id(...)`.
- `bindings/python/tests/test_database_read_vector.py`:
  - ~L177: `all_types_db.read_vector_date_time_by_id("AllTypes", "label_value", 1)` becomes the new
    name.
  - ~L189-197: rename the test method `test_read_vector_date_time_by_id` to
    `test_read_vector_date_times_by_id`, its docstring
    `"""read_vector_date_time_by_id wraps ..."""` to `"""read_vector_date_times_by_id wraps ..."""`,
    and the call at ~L197.
- `bindings/python/tests/test_database_read_set.py`:
  - ~L95: the call becomes the new name.
  - ~L176-184: rename the method `test_read_set_date_time_by_id`, its docstring and the call at
    ~L184.

Add one small test per binding pinning that the old name is gone, so an alias can't creep back:
- Julia (`test_database_read_vector.jl`, inside its top testset):
  `@test !isdefined(Quiver, :read_vector_date_time_by_id)` and
  `@test !isdefined(Quiver, :read_set_date_time_by_id)`.
- Python (`test_database_read_vector.py`):
  ```python
  def test_singular_date_time_by_id_names_are_gone(self, all_types_db: Database) -> None:
      assert not hasattr(all_types_db, "read_vector_date_time_by_id")
      assert not hasattr(all_types_db, "read_set_date_time_by_id")
  ```
  Put it in the same class as `test_read_vector_date_times_by_id`. Check the class name with
  `grep -n "^class" bindings/python/tests/test_database_read_vector.py`.

## Docs and changelog

- Root `CLAUDE.md`, DateTime wrappers table (~L746, ~L748). New rows:
  ```
  | `read_vector_date_times_by_id` | `readVectorDateTimesById` | `read_vector_date_times_by_id` | string vector read + date parsing |
  | `read_set_date_times_by_id`    | `readSetDateTimesById`    | `read_set_date_times_by_id`    | string set read + date parsing    |
  ```
  Re-align the Markdown columns if the table uses padded columns.
- Grep for other mentions: `grep -rn "date_time_by_id" docs/ bindings/*/CLAUDE.md bindings/python/README* bindings/julia/README*`.
  Update any hit except `read_scalar_date_time_by_id`.
- `CHANGELOG.md`, `## [0.11.0] — unreleased` → `### Changed`:
  ```markdown
  - **BREAKING — Julia/Python: two date-time readers are renamed to the plural form.**
    `read_vector_date_time_by_id` → `read_vector_date_times_by_id` and `read_set_date_time_by_id` →
    `read_set_date_times_by_id`. They return a list, and the naming rule makes a list-returning
    reader plural (Dart already spelled them `readVectorDateTimesById` / `readSetDateTimesById`).
    `read_scalar_date_time_by_id` is unchanged. *Adapt:* rename the calls; there is no alias.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `bindings/julia/test/test.bat test_database_read_vector.jl`, then `test_database_read_set.jl`,
   then the full `bindings/julia/test/test.bat`.
3. `bindings/python/tests/test.bat`, the full Python suite.
4. `grep -rn "read_vector_date_time_by_id\|read_set_date_time_by_id" bindings/ CLAUDE.md docs/`.
   The only hits should be the two new "names are gone" tests.
5. `scripts/format.bat`.

## Acceptance criteria

- [ ] Both bindings define only the plural names. Every internal caller and test uses them.
- [ ] The "old name is gone" tests pass in both bindings.
- [ ] The root CLAUDE.md table and CHANGELOG BREAKING entry are updated.

## Pitfalls

- Plan 18's rewritten group readers may call these readers or may decode natively. Grep after
  rebasing.
- The Python composites `read_element_by_id` and `read_vectors_by_id` call these through `self.`.
  Rename those calls too, or the composites break at runtime rather than import time.

## Out of scope

- Renaming `read_scalar_date_time_by_id`, or changing Julia's `read_time_series_row` keyword.
- Dart and JS (Dart is already plural; JS has no datetime wrappers by design).
