# 28 — Python: delete redundant bool branches (bool is an int subclass)

**Batch** 4 · **Severity** low · **Breaking** no. Behaviour is unchanged on every write path; this is a readability and deletion change · **Size** S · **Layers** Python binding (`bindings/python/src/quiverdb/database.py`, `bindings/python/src/quiverdb/element.py`), Python tests (`bindings/python/tests/test_database_boolean.py`), root `CLAUDE.md`
**Depends on** 24 (ordering only; see Overlaps) · **Overlaps with**
- **24** (Python (+Dart): type numeric columns from all cells) rewrites the numeric dispatch of `_marshal_group_columns` and `Element._set_array`. Both functions contain bool branches this finding also names: `isinstance(first, bool) or isinstance(first, int)` in `_marshal_group_columns` (currently ~L2223) and `if isinstance(first, bool):` in `_set_array` (currently ~L66). **Plan 24 owns both functions.** This plan only checks, in Step 3, that no redundant `isinstance(..., bool)` test is left in them after 24, and removes one if there is. It must not restructure 24's code.
- **25** (Python: accept datetime on every write path) adds a `datetime` branch to `_marshal_row_columns`, `Element.set` and `Element._set_array`, and changes the `_marshal_row_columns` `TypeError` text and the `Element.set` docstring. The bool branches deleted here are separate lines, but line numbers will have shifted. Anchor on the quoted excerpts, not on line numbers.
- **27** (Python: positional-only `/` on every `**kwargs` method) changes the `upsert_time_series_row[_by_label]` signatures. The test added here calls them with positional `collection, group, id|label` and keyword columns, which works with or without the `/`.
- **29** (Python dead code) later deletes `self._ensure_valid()` from `Element.set`. This plan leaves that line alone.
- **30** (Python stale docstrings) later rewrites the `upsert_time_series_row` docstring's type line (`bool -> INTEGER (0/1), int -> INTEGER, ...`) and the `_marshal_group_columns` docstring. This plan does not touch either docstring. Both stay accurate after this change.

## Why

Python's `bool` is a subclass of `int` (`isinstance(True, int)` is `True`, `isinstance(True, float)` is `False`). cffi accepts a `bool` wherever it accepts a C integer. I checked this in the binding venv (`bindings/python/.venv`, `python -B`):

```
>>> list(ffi.new("int64_t[]", [True, False]))   ->  [1, 0]
>>> ffi.new("int64_t*", True)[0]                ->  1
>>> C.abs(True)          # int parameter      ->  1
>>> isinstance(True, int), isinstance(True, float)  ->  (True, False)
```

So every `isinstance(v, int)` branch already writes `True`/`False` as INTEGER 1/0. `_marshal_params` relies on exactly that, and `test_boolean_input` exercises it (`query_boolean(..., parameters=[True])`). Four other sites still have a separate `bool` test placed before or beside the `int` one. Each does what the `int` branch does, and two carry comments describing a hazard that does not exist:

1. `bindings/python/src/quiverdb/database.py`, `_marshal_row_columns` (currently ~L2261-2273):
   ```python
           # bool is a subclass of int; test it explicitly first so True/False
           # marshal as INTEGER 1/0 rather than being rejected by the `is int`
           # check. Mirrors `_marshal_params` policy in this same file.
           if isinstance(v, bool):
               arr = ffi.new("int64_t[]", [int(v)])
               ...
           elif isinstance(v, int):
               arr = ffi.new("int64_t[]", [v])
               ...
   ```
   There is no `is int` check. The next branch is `isinstance(v, int)`, which matches a bool. The comment says the branch "mirrors `_marshal_params`", but `_marshal_params` has no bool branch. Its own comment (currently ~L2145) reads `elif isinstance(p, int):  # bool is subclass of int, handled here`. The two bodies are identical except for `[int(v)]` versus `[v]`, and cffi turns those into the same `int64_t`.
2. `bindings/python/src/quiverdb/element.py`, `Element.set` (currently ~L27-29):
   ```python
           elif isinstance(value, bool):
               # Must check bool before int (bool is subclass of int)
               self._set_integer(name, int(value))
           elif isinstance(value, int):
               self._set_integer(name, value)
   ```
   Nothing forces bool to come first. Both branches call `_set_integer`, which passes the value to `quiver_element_set_integer(..., int64_t)`.
3. `bindings/python/src/quiverdb/element.py`, `Element._set_array` (currently ~L66-67), `if isinstance(first, bool): self._set_array_integer(name, [int(v) for v in values])`, and `database.py` `_marshal_group_columns` (currently ~L2223), `elif isinstance(first, bool) or isinstance(first, int):`. Both belong to plan 24 (see Overlaps).

The test that is meant to pin this behaviour gets the reason wrong. `bindings/python/tests/test_database_boolean.py`, `test_boolean_input` (currently ~L85-90):
```python
    """A native bool on the write side.

    Python needs no special handling in most places (`bool` is an `int` subclass), but
    `Element.set` and the group/row marshallers each test `bool` explicitly and before `int`,
    so a stray reordering would send a bool down the float or the unsupported-type path.
    """
```
Reordering cannot send a bool to the float path, because `isinstance(True, float)` is `False`. The real hazard is narrowing an `isinstance(v, int)` test to `type(v) is int`, which would reject a bool. The test also has a gap: it never writes a bool through `upsert_time_series_row`, which is the only path served by `_marshal_row_columns`, the function this plan edits.

**Reproduction.** No input produces a wrong output. The defect is code and comments that send a maintainer looking for a hazard that isn't there, plus three spellings of one rule. Proof that the branches are dead weight: delete them, and `test_boolean_input` (extended below) and the rest of the suite still pass.

This violates the root CLAUDE.md principles "Human-Centric: Codebase optimized for human readability" and "Simple solutions over complex abstractions. Delete unused code". It also leaves the root CLAUDE.md boolean passage naming "`element.py`'s `isinstance(value, bool)` branch" as the Python conversion point, which becomes false once the branch is gone (Self-Updating).

## Constraints and decisions

- **Root CLAUDE.md, "Boolean wrappers ..." design decision (currently ~L173-182):** "On the write side a boolean is accepted wherever an integer is, in every layer including Lua: `create_element`/`update_element` (scalars and arrays), query parameters, the vector/set/time-series group writers, and `upsert_time_series_row` — all mapping it to INTEGER 1/0 ... Julia and Python need no explicit branch on most paths because `Bool <: Integer` and `bool` is an `int` subclass respectively — which makes the behaviour dispatch-order-dependent and worth a test rather than an assumption." The rule is about behaviour, and this change keeps it. The passage asks for a test, and this plan keeps `test_boolean_input` and extends it.
- **Maintainer decision (binding):** "Keep test_boolean_input and extend with an upsert_time_series_row bool write. Update root CLAUDE.md boolean passage that names element.py isinstance(value, bool) branch. If plan 24 lands first, the _marshal_group_columns part is already gone - coordinate." Plans run in numeric order, so 24 will have landed. Step 3 handles whatever it left.
- **Root "Error Messages":** unchanged. No message is added or reworded here. The `TypeError` texts in `Element.set` and `_marshal_row_columns` stay as they are (plan 25 may have reworded the latter).
- **Root "Changelog":** only user-visible changes get an entry, and this change is not user-visible (see Docs and changelog).
- `bindings/python/CLAUDE.md` does not describe the bool branches (grep `bool` finds only the `_integer_to_boolean` notes, ~L67-78). Nothing there goes stale, so it needs no edit.
- `tests/CLAUDE.md` (~L110-117) says boolean input "is tested in the Lua layer and in all four bindings ... the four bindings extend their own boolean files". Extending `test_database_boolean.py` matches that, so it needs no edit.

Alternatives considered and rejected:
- *Keep the bool branches and only fix the comments.* Rejected: the branches are behaviourally identical to the `int` branch below them. Keeping a second spelling of one rule is the complexity the finding is about, and "delete unused code" applies.
- *Add a `bool` branch to `_marshal_params` for symmetry.* Rejected, for the same reason in reverse.
- *Pin the rule with a new test file or a per-marshaller unit test.* Rejected: `test_boolean_input` already exercises every write path except the row upsert. Adding the row upsert to it is the whole gap.
- *Put the upsert assertions in a separate test function.* Rejected: the maintainer asked for `test_boolean_input` to be extended. pytest takes a second fixture parameter without trouble.
- *Normalise `_marshal_params`'s existing `# bool is subclass of int, handled here` comment to the new wording.* Skipped: it is already accurate, and rewording it would be churn.

## Changes

Before starting, confirm the starting state:
```
git grep -n "isinstance([a-z_]*, bool)" -- bindings/python/src
```
At HEAD 58dfe7a this prints four lines (`database.py` ~2223 and ~2264, `element.py` ~27 and ~66). If plan 24 has landed as expected, the `database.py` ~2223 and `element.py` ~66 lines may already be gone.

### 1. `bindings/python/src/quiverdb/database.py`, `_marshal_row_columns` (currently ~L2240-2289)

Current (inside the `for i, (name, v) in enumerate(kwargs.items()):` loop, right after `c_col_names[i] = name_buf`):
```python
        # bool is a subclass of int; test it explicitly first so True/False
        # marshal as INTEGER 1/0 rather than being rejected by the `is int`
        # check. Mirrors `_marshal_params` policy in this same file.
        if isinstance(v, bool):
            arr = ffi.new("int64_t[]", [int(v)])
            keepalive.append(arr)
            c_col_types[i] = DataType.INTEGER
            c_col_data[i] = ffi.cast("void*", arr)
        elif isinstance(v, int):
            arr = ffi.new("int64_t[]", [v])
            keepalive.append(arr)
            c_col_types[i] = DataType.INTEGER
            c_col_data[i] = ffi.cast("void*", arr)
        elif isinstance(v, float):
```

New (the three-line comment and the whole `bool` branch are deleted; the `int` branch's `elif` becomes `if`):
```python
        if isinstance(v, int):  # bool is an int subclass: True/False marshal as 1/0
            arr = ffi.new("int64_t[]", [v])
            keepalive.append(arr)
            c_col_types[i] = DataType.INTEGER
            c_col_data[i] = ffi.cast("void*", arr)
        elif isinstance(v, float):
```
Leave everything from `elif isinstance(v, float):` down unchanged, including any `datetime` branch plan 25 added and the final `else: raise TypeError(...)`.

Why: the deleted branch has the same effect as the `int` branch, and its comment describes a check (`is int`) that does not exist.

### 2. `bindings/python/src/quiverdb/element.py`, `Element.set` (currently ~L18-39)

Current:
```python
        if value is None:
            self._set_null(name)
        elif isinstance(value, bool):
            # Must check bool before int (bool is subclass of int)
            self._set_integer(name, int(value))
        elif isinstance(value, int):
            self._set_integer(name, value)
        elif isinstance(value, float):
```

New:
```python
        if value is None:
            self._set_null(name)
        elif isinstance(value, int):  # bool is an int subclass: True/False marshal as 1/0
            self._set_integer(name, value)
        elif isinstance(value, float):
```
Leave the rest of the method alone: the `self._ensure_valid()` line (plan 29 owns it), any `datetime` branch from plan 25, the `str`/`list` branches and the final `TypeError`. The docstring line "Supported types: int, float, str, None, bool (stored as int), ..." stays. It describes behaviour, and that behaviour is unchanged.

Why: `_set_integer(name, True)` passes `True` to cffi for an `int64_t` parameter, which becomes 1, the same as `_set_integer(name, int(True))`.

### 3. Plan-24-owned sites: `Element._set_array` and `_marshal_group_columns` (check, then act only if needed)

Run:
```
git grep -n "isinstance([a-z_]*, bool)" -- bindings/python/src
```
After Steps 1-2 it must print nothing. If a line is still listed, it is in code that plan 24 did not rewrite, or rewrote while keeping a bool test. Handle it this way:

- **`Element._set_array`, if it still reads** (HEAD form, currently ~L65-69):
  ```python
          first = values[0]
          if isinstance(first, bool):
              self._set_array_integer(name, [int(v) for v in values])
          elif isinstance(first, int):
              self._set_array_integer(name, values)
          elif isinstance(first, float):
  ```
  replace it with:
  ```python
          first = values[0]
          if isinstance(first, int):  # bool is an int subclass: True/False marshal as 1/0
              self._set_array_integer(name, values)
          elif isinstance(first, float):
  ```
- **`_marshal_group_columns`, if it still reads** (HEAD form, currently ~L2223):
  ```python
          elif isinstance(first, bool) or isinstance(first, int):
  ```
  change only the condition and keep the body (the `int(v)` truncation is plan 24's to fix):
  ```python
          elif isinstance(first, int):  # bool is an int subclass: True/False marshal as 1/0
  ```
- **Anything else plan 24 introduced** (for example in a cell-classification helper in `_helpers.py`): delete an `isinstance(x, bool)` only when it is OR-ed with, or sits directly before, an `isinstance(x, int)` test whose branch does the same thing. That test already covers bool. Leave `type(x) is ...` checks and any bool test with a distinct purpose unchanged. If you are unsure, leave it and say so in the commit message.

If `git grep` printed nothing, this step is a no-op.

### 4. `bindings/python/tests/test_database_boolean.py`, `test_boolean_input`

See Tests.

### 5. Root `CLAUDE.md`, boolean write-side passage

See Docs and changelog.

No C++, C API, FFI declaration (`_c_api.py`, Julia `c_api.jl`, Dart `bindings.dart`, JS `loader.ts`), Julia, Dart, JS or Lua change. The behaviour is a Python marshalling detail with no FFI signature involved.

## Tests

### Python: `bindings/python/tests/test_database_boolean.py`, `test_boolean_input` (currently ~L84-123)

Fixtures: `all_types_db` (existing, `tests/schemas/valid/all_types.sql`) plus `mixed_time_series_db` (existing in `bindings/python/tests/conftest.py` ~L145-150, `tests/schemas/valid/mixed_time_series.sql`: `Sensor_time_series_readings` with `date_time TEXT`, `temperature REAL`, `humidity INTEGER`, `status TEXT`, all `NOT NULL`). `AllTypes` has no time-series group, which is why the second fixture is needed. Both fixtures write under the same `tmp_path` but to different files (`all_types.db`, `mixed_ts.db`), so they do not collide. No new schema file.

Change the signature and the docstring, keep the whole existing body, and append the row-upsert block at the end.

Old signature and docstring:
```python
def test_boolean_input(all_types_db: Database) -> None:
    """A native bool on the write side.

    Python needs no special handling in most places (`bool` is an `int` subclass), but
    `Element.set` and the group/row marshallers each test `bool` explicitly and before `int`,
    so a stray reordering would send a bool down the float or the unsupported-type path.
    """
```

New signature and docstring:
```python
def test_boolean_input(all_types_db: Database, mixed_time_series_db: Database) -> None:
    """A native bool is INTEGER 1/0 on every write path.

    No write path has a `bool` branch: `bool` is an `int` subclass, so each path's integer test
    (`isinstance(v, int)`) takes it. That covers `Element.set` and its arrays, query parameters,
    the group writers, and the time-series row upsert. This test pins it: narrowing any of those
    tests to `type(v) is int` would reject a bool.
    """
```

Existing body: unchanged (the `create_element` / `update_element` / `update_element_by_label` / `query_boolean(..., parameters=[True])` / `update_vector_group` / `update_set_group` assertions, ending with `assert sorted(all_types_db.read_set_booleans_by_id("AllTypes", "code", element_id)) == [False, True]`).

Append after that last assertion:
```python

    # Row upsert (`_marshal_row_columns`), by id and by label. A bool written to the REAL column
    # (`temperature`) reaches it as INTEGER 1 through the core's int-for-REAL rule.
    sensor_id = mixed_time_series_db.create_element("Sensor", label="S1")
    mixed_time_series_db.upsert_time_series_row(
        "Sensor",
        "readings",
        sensor_id,
        date_time="2024-01-01T00:00:00",
        temperature=21.5,
        humidity=True,
        status="ok",
    )
    mixed_time_series_db.upsert_time_series_row_by_label(
        "Sensor",
        "readings",
        "S1",
        date_time="2024-01-02T00:00:00",
        temperature=True,
        humidity=False,
        status="ok",
    )
    readings = mixed_time_series_db.read_time_series_group("Sensor", "readings", sensor_id)
    assert readings["humidity"] == [1, 0]
    assert readings["temperature"] == [21.5, 1.0]
```
Keep the trailing commas. ruff (`skip-magic-trailing-comma = false`, line length 120) then keeps the calls exploded. `read_time_series_group` returns column lists ordered by the dimension column, so row 2024-01-01 comes first. The reader also turns `date_time` into aware datetimes, which is why the test does not compare the whole dict.

**Which test fails before the fix:** none. This change keeps behaviour, so the extended test passes both before and after Steps 1-3. The new block is a regression pin on the one function this plan edits that no test fed a bool before. To prove it is live, run the mutation check in Verification step 4. It turns the int test into `type(v) is int` and watches `test_boolean_input` fail.

Existing tests: none change. `bindings/python/tests/test_element.py::test_element_set_bool_as_integer` (currently ~L40) and `bindings/python/tests/test_database_query.py::test_query_with_bool_param` (currently ~L87) keep passing unchanged. They call `Element.set` and query parameters with a bool. No test asserts on the deleted comments or branches.

Other layers: no test. The C++ core, C API, Julia, Dart, JS and Lua are untouched, and their boolean-input tests (`tests/CLAUDE.md` ~L110-117) are unaffected.

## Docs and changelog

### Root `CLAUDE.md`, "Boolean wrappers are Julia/Dart/Python/JS only" design decision (currently ~L177-182)

Old:
```
  is no boolean setter in the C API and none is needed: each binding converts before the FFI call
  (`Element.set` in Dart, `element.py`'s `isinstance(value, bool)` branch, `setElementField` /
  `setElementArray` / `marshalParams` / `updateGroupColumns` / `upsertRowColumns` in JS, the five
  sol2 converters in `src/lua_runner.cpp`). Julia and Python need no explicit branch on most paths
  because `Bool <: Integer` and `bool` is an `int` subclass respectively — which makes the
  behaviour dispatch-order-dependent and worth a test rather than an assumption.
```

New:
```
  is no boolean setter in the C API and none is needed: each binding converts before the FFI call
  (`Element.set` in Dart, `setElementField` / `setElementArray` / `marshalParams` /
  `updateGroupColumns` / `upsertRowColumns` in JS, the five sol2 converters in
  `src/lua_runner.cpp`). Julia and Python need no explicit branch because `Bool <: Integer` and
  `bool` is an `int` subclass respectively, so a boolean takes each writer's integer branch. That
  is worth a test rather than an assumption: in Julia it is dispatch-order-dependent (`Bool <: Real`
  too, so an `isa Integer` test must precede an `isa Real` one), and in Python it holds only while
  every write path tests `isinstance(v, int)` — `type(v) is int` would reject a bool (pinned by
  `test_boolean_input` in `bindings/python/tests/test_database_boolean.py`).
```
The next line, "`db:update_relation` is the one deliberate refusal: only `nil` may clear a relation.", stays as it is. Keep the two-space indentation of the list item.

What changed and why:
- The `element.py` clause is dropped because that branch no longer exists.
- "on most paths" becomes unqualified. I grepped `bindings/julia/src` and `bindings/python/src`, and after this change neither has a `Bool`/`bool` branch on any write path.
- The Julia ordering claim was checked against `bindings/julia/src/database_update.jl`. At ~L261/266, `v isa Integer` comes before `v isa Real`. At ~L148/155, `T <: Integer` comes before `T <: Real`. `Bool <: Real` holds in Julia.

### Other docs

- `bindings/python/CLAUDE.md`: no edit. It does not mention the bool branches.
- `tests/CLAUDE.md`: no edit (see Constraints).
- `bindings/python/README.md`, `docs/*.md`, `bindings/js/src/lua-api.ts`: no mention of this. No edit.
- Docstrings `upsert_time_series_row` (`bool -> INTEGER (0/1), ...`) and `Element.set` (`bool (stored as int)`): still accurate. Plan 30 and plan 25 own their rewording.

### CHANGELOG.md

No entry. No behaviour changes on any public path: `create_element`, `update_element[_by_label]`, query parameters, the group writers and `upsert_time_series_row[_by_label]` all still write `True`/`False` as INTEGER 1/0. The root Changelog rule covers user-visible changes only.

One edge case applies only if plan 24 did not rewrite `Element._set_array` and Step 3's first bullet was applied. Then `create_element(..., score=[True, 1.5])` changes from silently storing `[1, 1]` to raising cffi's `TypeError: an integer is required`. That is the mixed-array behaviour plan 24 fixes and records in its own CHANGELOG entry, so do not add a separate one here.

## Verification

Run from the repo root (`C:\Development\Quiver\quiver3`) in PowerShell.

1. `cmake --build build --config Debug`. Nothing in this plan changes C++, but the Python tests load `build\bin\libquiver_c.dll`, which must match any C API changes from earlier plans.
2. `git grep -n "isinstance([a-z_]*, bool)" -- bindings/python/src` should print nothing.
3. `bindings\python\tests\test.bat -k bool`. Expected: `test_boolean_convenience_methods`, `test_boolean_conversion_rejects_non_binary_integer`, `test_boolean_input`, `test_element_set_bool_as_integer` and `test_query_with_bool_param` all pass.
4. Mutation check. Do it, then revert it.
   - In `_marshal_row_columns`, change `if isinstance(v, int):` to `if type(v) is int:`. Run `bindings\python\tests\test.bat -k test_boolean_input`. It must FAIL at the first `upsert_time_series_row` call with a `TypeError` naming column `humidity` and type `bool`. Revert the change.
   - In `Element.set`, change `elif isinstance(value, int):` to `elif type(value) is int:`. Run the same command. It must FAIL at the first `create_element` with `TypeError: Unsupported type bool for Element.set('some_integer')`. Revert the change.
   - `git diff --stat` must now show only the intended edits.
5. `bindings\python\tests\test.bat` runs the full Python suite, and all tests must pass.
6. `scripts\format.bat`, then `git status`. The only modified files should be `bindings/python/src/quiverdb/database.py`, `bindings/python/src/quiverdb/element.py`, `bindings/python/tests/test_database_boolean.py` and `CLAUDE.md`. If the formatter touched anything else, it was already unformatted before this plan. Revert those files and leave them alone (root "Do Not Fix": no drive-by lint fixes).
7. `scripts\test-all.bat` runs the six suites and the CLI smoke test, and all must pass. A CLI smoke failure that predates this plan is plan 65's to fix; note it and move on.

## Acceptance criteria

- [ ] `_marshal_row_columns` has no `bool` branch and no "`is int` check" comment. Its first branch is `if isinstance(v, int):  # bool is an int subclass: True/False marshal as 1/0`.
- [ ] `Element.set` has no `bool` branch and no "Must check bool before int" comment. Its integer branch carries the same one-line comment.
- [ ] `git grep -n "isinstance([a-z_]*, bool)" -- bindings/python/src` prints nothing.
- [ ] Plan 24's code in `_marshal_group_columns` / `Element._set_array` is unchanged apart from removing a redundant bool test, if one was left.
- [ ] `test_boolean_input` takes `mixed_time_series_db`, has the corrected docstring, and asserts `humidity == [1, 0]` and `temperature == [21.5, 1.0]` after one `upsert_time_series_row` and one `upsert_time_series_row_by_label`.
- [ ] The mutation check (Verification 4) failed as described for both sites and was reverted.
- [ ] Root `CLAUDE.md` no longer mentions "`element.py`'s `isinstance(value, bool)` branch" and has the new Julia/Python sentence.
- [ ] No CHANGELOG entry and no manifest change.
- [ ] Full Python suite and `scripts\test-all.bat` pass.

## Pitfalls

- **`elif` → `if`.** In `_marshal_row_columns`, the deleted bool branch is the chain's leading `if`. The `int` branch below it must become `if`, or the file is a syntax error. In `Element.set` the bool branch sits between `if value is None:` and the `int` branch, so its `int` branch stays `elif`. Likewise in `_set_array` (Step 3), the bool branch is the leading `if`, so the `int` branch becomes `if`.
- **Anchors moved.** Plans 24 and 25 edit these same functions before this one runs. Find each edit by its quoted excerpt, and use `git grep "isinstance([a-z_]*, bool)"` to locate what is left.
- **Do not "tidy" plan 24's numeric dispatch.** Only a bool test that is plainly redundant beside an `isinstance(..., int)` test goes. If plan 24 uses `type(v)` or has a bool check with a real purpose, leave it.
- **Do not drop `int(v)` elsewhere.** `_marshal_group_columns`'s `int(v) if v is not None else 0` (if it still exists) is plan 24's to change. This plan changes only its condition.
- **numpy scalars.** `numpy.bool_` and `numpy.int64` are not `bool`/`int` subclasses. They were not accepted before and are not now. This change does not touch them.
- **pytest fixtures.** Two DB fixtures in one test share `tmp_path`. They write different file names, so nothing collides. Do not replace `mixed_time_series_db` with an in-memory DB built by hand, because conftest's fixture is the convention.
- **Line endings.** The three `.py` files are LF in both the index and the working tree (`.gitattributes`: `*.py text eol=lf`). Root `CLAUDE.md` is `text=auto`: LF in the index but **CRLF in the working tree** (`git ls-files --eol CLAUDE.md` → `i/lf w/crlf`). Edit it with the Edit tool, which keeps the file's existing line endings. Do not rewrite it with `sed` or a heredoc. Confirm with `git diff --stat CLAUDE.md`: it must show only the ~9 changed lines, not the whole file. No `.bat` file is edited. If you open `bindings\python\tests\test.bat` or `format.bat`, do not save them through a Unix tool, which would convert their CRLF endings.
- **The mutation-check `TypeError` text.** Plan 25 may have extended `_marshal_row_columns`'s message (for example, adding `datetime` to the expected list). Only require that it names `humidity` and `bool`.

## Out of scope

- Whole-column numeric typing and removing `int()`/`float()` coercion in `_marshal_group_columns` and `Element._set_array`: plan 24.
- `datetime` acceptance on `Element.set`, `_set_array` and `_marshal_row_columns`, and the `_marshal_row_columns` `TypeError` wording: plan 25.
- Positional-only `/` on `create_element` / `update_element` / `upsert_time_series_row[_by_label]`: plan 27.
- Deleting `Element._ensure_valid` / `Element.clear`: plan 29.
- The `upsert_time_series_row` docstring type line and the D-03 reference, and the `_marshal_group_columns` docstring wording: plan 30.
- JS boolean handling in its marshallers (`setElementArray`, `updateGroupColumns`): plan 31.
- The `_marshal_params` comment `# bool is subclass of int, handled here`: accurate, left as is.
