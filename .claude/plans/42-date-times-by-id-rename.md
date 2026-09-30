# 42 — Julia/Python: rename `read_{vector,set}_date_time_by_id` to `read_{vector,set}_date_times_by_id`

**Batch** 4 · **Severity** low · **Breaking** yes — Julia and Python callers of the two old names must switch to the new ones · **Size** S · **Layers** Julia binding, Python binding, root AGENTS.md, CHANGELOG
**Depends on** 18 recommended first (it deletes the per-column group-reader compositions, which contain four of the call sites listed below) · **Overlaps with** 18 (same functions' callers), 41 (comments right next to these functions), 30 (Python docstrings in the same file)

## Why

Root AGENTS.md "Naming Convention": *"Singular vs plural: Type name matches return cardinality
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
AGENTS.md DateTime wrapper table (~L746, ~L748) records the inconsistency:

```
| `read_vector_date_time_by_id` | `readVectorDateTimesById` | `read_vector_date_time_by_id` | string vector read + date parsing |
| `read_set_date_time_by_id`    | `readSetDateTimesById`    | `read_set_date_time_by_id`    | string set read + date parsing    |
```

Principle: Homogeneity, meaning mechanically derivable names.

## Constraints and decisions

- **Maintainer decision (binding):** rename only. Keep `read_scalar_date_time_by_id` singular: it
  returns one optional value. Keep Julia's `read_time_series_row(...; date_time::DateTime)` keyword
  as is. BREAKING, with a CHANGELOG entry under 0.12.0 listing old and new names. Update the root
  AGENTS.md table. No manifest bump is needed (0.12.0 is already the unreleased minor).
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

- Root `AGENTS.md`, DateTime wrappers table (~L746, ~L748). New rows:
  ```
  | `read_vector_date_times_by_id` | `readVectorDateTimesById` | `read_vector_date_times_by_id` | string vector read + date parsing |
  | `read_set_date_times_by_id`    | `readSetDateTimesById`    | `read_set_date_times_by_id`    | string set read + date parsing    |
  ```
  Re-align the Markdown columns if the table uses padded columns.
- Grep for other mentions: `grep -rn "date_time_by_id" docs/ bindings/*/AGENTS.md bindings/python/README* bindings/julia/README*`.
  Update any hit except `read_scalar_date_time_by_id`.
- `CHANGELOG.md`, `## [0.12.0] — unreleased` → `### Changed`:
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
4. `grep -rn "read_vector_date_time_by_id\|read_set_date_time_by_id" bindings/ AGENTS.md docs/`.
   The only hits should be the two new "names are gone" tests.
5. `scripts/format.bat`.

## Acceptance criteria

- [x] Both bindings define only the plural names. Every internal caller and test uses them.
- [x] The "old name is gone" tests pass in both bindings.
- [x] The root AGENTS.md table and CHANGELOG BREAKING entry are updated.

## Pitfalls

- Plan 18's rewritten group readers may call these readers or may decode natively. Grep after
  rebasing.
- The Python composites `read_element_by_id` and `read_vectors_by_id` call these through `self.`.
  Rename those calls too, or the composites break at runtime rather than import time.

## Out of scope

- Renaming `read_scalar_date_time_by_id`, or changing Julia's `read_time_series_row` keyword.
- Dart and JS (Dart is already plural; JS has no datetime wrappers by design).

## Implementation notes

Implemented on `rs/plan42`. At planning time the branch sat at master `4863906`. By the time implementation started it had been fast-forwarded to master `60ed75b`, which brought in plans 33-40 (#345-#352). So `git fetch origin && git merge origin/master` was a no-op. Plan 41 had not landed. Every anchor was re-checked after the fast-forward and none had moved.

Before any edit, a three-lens read-only workflow checked the plan:
- **consumers:** a devil's-advocate case against implementing, plus a caller search;
- **overlap:** plans 33-41 in flight and 43-88;
- **completeness:** every hit, the formatter margins, and `isdefined`/`hasattr` semantics.

Its verdict was **implement**. After the edits, a two-lens adversarial review of the diff (code/tests, docs/CHANGELOG) returned no findings.

**Why implement.** The consumer lens searched:
- every repo under `C:/Development` outside `Quiver/`;
- the Julia depot, the uv cache and the psrenergy GitHub org (default branches).

It found **no external caller** of either old name. Every hit was a copy of Quiver itself: the Keynotes talk snapshot, spine's vendored `quiver/` copy, and installed Quiver.jl versions. BDCont, LightPSRIO and SDDPSQLite depend on Quiver but use none of the date-time by-id readers.

An old call fails loudly, with `UndefVarError` or `AttributeError`, never silently. Every sibling reader is already plural: Dart, the boolean `_by_id` readers, and the bulk `read_{vector,set}_date_times`. The only cost is that the names have been published since v0.10.3, so unseen callers must rename.

### Drift fixed

- **Julia composites never called these readers.** `read_vectors_by_id` / `read_sets_by_id` decode through `_read_{vector,set}_strings_by_id(..., not_null)` + `_to_date_times`, and plan 18 made the group readers native. Julia step 3 therefore had nothing to do. Only the two definitions changed, and they are now one-line `name(...) =` forms at `database_read.jl:455` / `:547` (the plan quoted `function ...(` at ~L385/~L446).
- **Python lines moved.** The definitions are at `database.py:521` / `:532`, and the composite calls at `:1959` / `:1981`. The docstrings do not name their own methods, so step 4 was a no-op.
- **Three test call sites the plan did not list** were added by the NULL-cell work: `test_database_read_set.jl:336`, `test_database_read_vector.py:333` (`TestVectorNullCells`) and `test_database_read_set.py:354` (`TestSetNullCells`). All three are renamed.
- **Python test classes renamed too.** `TestReadVectorDateTimeById` / `TestReadSetDateTimeById` → `...DateTimesById`, matching `TestReadVectorDateTimesBulk` and the other `...ById` neighbours. The "gone" test lives in `TestReadVectorDateTimesById`.
- **CHANGELOG section.** `## [0.12.0] — unreleased` does not exist (0.12.0 was released). The entry is the plan's text verbatim, as the last `### Changed` bullet of `## [0.12.6] — unreleased`, after plan 39's Dart `Element.set` bullet. There is no manifest bump. As with plans 21/22/24/27/31/39, this still contradicts root AGENTS.md's "breaking ⇒ 0.x minor bump" rule.
- **`bindings/julia/type_stability_followup.md:16`** read `read_{vector,set}_date_times` / `read_{vector,set}_date_time_by_id`. Neither of the plan's greps matches that brace form. It now reads `read_{vector,set}_date_times[_by_id]`, the same form as the booleans on the same line. Line 43 (the scalar reader) stays.
- **Root AGENTS.md table.** It is at L823-831, not ~L746. The new names are wider than the old padding, and the three "bulk read" rows already overflowed the Wraps column, so the whole table was re-aligned.
- **Small addition beyond the plan (coverage gap).** Nothing exercised Python `read_vectors_by_id`'s DATE_TIME branch, so leaving that rename out would have passed the suite and broken at runtime (this plan's own Pitfall). One assert now covers it, in `TestVectorNullCells.test_string_and_date_time_readers_keep_null_cells`: `db.read_vectors_by_id("Items", item)["date_event"] == [jan_first, None]`. The set branch has no equivalent, because no schema has a DATE_TIME set column, so the grep is its only guard.

### Results

- **Red** (tests renamed plus the "gone" tests added, sources untouched):
  - Python `test.bat -k date_time`: **5 failed, 28 passed**. `AttributeError: 'Database' object has no attribute 'read_vector_date_times_by_id'. Did you mean: 'read_vector_date_time_by_id'?`, and the "gone" test failed on `hasattr`.
  - Julia `test.bat test_database_read_vector.jl`: `UndefVarError: read_vector_date_times_by_id not defined in Quiver` at L180 (fail-fast). `isdefined(Quiver, :read_{vector,set}_date_time_by_id)` printed `true true`.
- **Green.**
  - Julia `test_database_read_vector.jl`: Read Vector **80/80**. `test_database_read_set.jl`: Read Set **62/62**. Full Julia suite: **1575/1575**.
  - Full Python suite: **350 passed**, the previous 349 plus the new "gone" test.
- **Grep.** `git grep -n "read_vector_date_time_by_id\|read_set_date_time_by_id" -- bindings AGENTS.md docs` prints only the four lines of the two "gone" tests.
- **`cmake --build build --config Debug`.** This checkout had no `build/`, so it was configured first with the root AGENTS.md configure line. Both steps succeeded.
- **`scripts\format.bat`** exits 0. clang-format, JuliaFormatter, dart format (44 files, 0 changed) and ruff (35 unchanged) changed nothing. Biome rewrote 43 JS files CRLF→LF with an empty `git diff --ignore-cr-at-eol`, and `git checkout -- bindings/js` reverted them (this plan has no JS edits). No `.bat` file was touched.
  - **Fresh-checkout pitfall.** The first run exited 1. `bindings/dart` had never had `dart pub get`, so `analysis_options.yaml`'s `package:lints` include did not resolve, `dart format` fell back to 80 columns, and it rewrote 28 Dart files. `bindings/js` had no `bun install` (`bun: command not found: biome`).
  - Fix: `git checkout -- bindings/dart`, then `dart pub get` and `bun install`, then re-run. Any new checkout needs both before `format.bat`.
- **Not run:** `scripts/test-all.bat`. The change touches only the Julia and Python bindings, whose suites pass above. The README asks for test-all at the end of each batch.

### For later plans

- **Plan 41** (stale alignment comments, all bindings) had not landed. It edits comments on the bulk readers: `database_read.jl` ~L119/181/215/276 and `database.py` ~L826/901/1122. None of those is a line this plan renamed (Julia L455/547, Python L521/532/1959/1981), and 41 names neither by-id reader, so no conflict is expected.
- **Batch-4 CHANGELOG.** `## [0.12.6] — unreleased` `### Changed` now ends with plan 39's Dart `Element.set` BREAKING bullet, then this one.
- The old names remain only in historical text: `.claude/plans/README.md:92`, `master-plan.md:304`, and this plan file. README L9's "0.12.0 is unreleased" note is stale (the current section is 0.12.6). All are left for the maintainer.
