# 27 — Python: positional-only "/" on every **kwargs method

**Batch** 4 · **Severity** low · **Breaking** yes, for Python callers only: anyone who passes `collection`, `id`, `group` or `label` **by keyword** to `create_element`, `update_element`, `upsert_time_series_row` or `upsert_time_series_row_by_label` now gets a `TypeError` · **Size** S · **Layers** Python binding only (`bindings/python/src/quiverdb/database.py`, three Python test files, `bindings/python/CLAUDE.md`, `CHANGELOG.md`)
**Depends on** none · **Overlaps with**
- **25** (Python: accept datetime on every write path) changes `Element.set` / `_marshal_row_columns`, which these five methods call, and may reword the upsert docstrings. This plan edits only the five `def` lines and the one-line `update_element` docstring, not the bodies or the upsert docstrings. After 25 lands, the `read_scalars_by_id` round trip also works on collections with a DATE_TIME attribute. The test here uses `Collection` (no DATE_TIME), so it passes before and after 25.
- **28** (delete redundant bool branches) edits `Element.set` and the marshallers. It does not touch these signatures.
- **30** (fix stale docstrings, including the upsert "types" docstring) owns the body of the `upsert_time_series_row` docstring (`"No Int->Float coercion (per D-03: Python strict typing)"` is stale). This plan does **not** touch that docstring. Plan 30 runs after this one, so it will see `/, **kwargs: object` in the `def` line above its docstring.
- **56** (one scalar typing policy; Pattern 1 messages for unknown columns) may reword the core's `Column 'collection' not found in table 'Collection'` message. The `create_element` test below matches only `'collection'`, which any rewording that names the column keeps.
- **24, 29** edit other parts of `database.py` and other bullets of `bindings/python/CLAUDE.md`. There is no shared line with this plan.

## Why

`bindings/python/CLAUDE.md` (currently ~L42-47) records the rule:

> **A parameter that shadows a column name needs a `/`.** A method that addresses a row positionally *and* takes attributes as `**kwargs` must mark the positional parameters positional-only, or the kwarg binds to the parameter and raises `TypeError: got multiple values for argument '<name>'` before the FFI call. `update_element_by_label(collection, label, /, **kwargs)` is the acute case …

Only `update_element_by_label` follows it. There are exactly five `**kwargs` methods in `bindings/python/src` (checked with `grep -rn "\*\*kwargs" bindings/python/src`). All five are in `bindings/python/src/quiverdb/database.py`:

```python
    def create_element(self, collection: str, **kwargs: object) -> int:                          # ~L206
    def update_element(self, collection: str, id: int, **kwargs: object) -> None:                # ~L229
    def update_element_by_label(self, collection: str, label: str, /, **kwargs: object) -> None: # ~L248 (already correct)
    def upsert_time_series_row(self, collection: str, group: str, id: int, **kwargs) -> None:    # ~L1785
    def upsert_time_series_row_by_label(self, collection: str, group: str, label: str, **kwargs) -> None:  # ~L1806
```

Reproduction, run against HEAD 58dfe7a with the built `build/bin` DLLs (`collections.sql`):

```python
eid = db.create_element("Collection", label="Item1", some_integer=10, some_float=2.5)
row = db.read_scalars_by_id("Collection", eid)   # {'id': 1, 'label': 'Item1', 'some_integer': 10, 'some_float': 2.5}
db.update_element("Collection", eid, **row)
# TypeError: Database.update_element() got multiple values for argument 'id'
db.upsert_time_series_row("Collection", "data", eid, id=eid, date_time="2024-01-01T00:00:00", value=1.0)
# TypeError: Database.upsert_time_series_row() got multiple values for argument 'id'
db.upsert_time_series_row_by_label("Collection", "data", "Item1", label="x", date_time="2024-01-01T00:00:00", value=1.0)
# TypeError: Database.upsert_time_series_row_by_label() got multiple values for argument 'label'
db.create_element("Collection", label="x", collection="y")
# TypeError: ... got multiple values for argument 'collection'
```

`read_scalars_by_id` always includes `id`. Its docstring says "Includes id and label", and `tests/test_database_read_scalar.py` (~L268) asserts `"id" in result`. So "read a row, change a field, write it back" fails in Python before any FFI call. It works in every other binding. Julia keeps keywords apart from positionals (`bindings/julia/src/database_update.jl` ~L6 `update_element!(db::Database, collection::String, id::Int64; kwargs...)`, ~L300 `upsert_time_series_row!(db, collection, group, id; kwargs...)`). Dart and JS take a map or object. Lua takes a table. This breaks:
- the **Homogeneity** principle in the root `CLAUDE.md`;
- the binding's own recorded rule;
- **Error Messages** ("bindings retrieve and surface [messages] — they never craft their own"). Here Python's argument binder produces the error, and the core never sees the call.

With the `/` in place I checked each method by monkeypatching a copy with `/` onto `Database` in a scratch script:
- `update_element("Collection", eid, **row)` succeeds. The core writes `UPDATE Collection SET id = ?, label = ?, … WHERE id = ?` (`src/database_update.cpp`, `Database::update_element`, ~L34-50), which stores the same `id` back.
- `upsert_time_series_row(..., id=eid, ...)` → `QuiverError: Cannot upsert_time_series_row: column 'id' not found in group 'data' for collection 'Collection'`. `time_series_schema_types` in `src/database_time_series.cpp` (~L9-17) drops `id`, and `validate_time_series_row` (~L34-37) raises this.
- `upsert_time_series_row_by_label(..., label="x", ...)` → `QuiverError: Cannot upsert_time_series_row: column 'label' not found in group 'data' for collection 'Collection'`. The `_by_label` form delegates to the id form, so the message names the id form, as root `CLAUDE.md` specifies.
- `create_element("Collection", label="x", collection="y")` → `QuiverError: Column 'collection' not found in table 'Collection'`.
- `update_element(collection="Collection", id=eid, some_integer=1)` → `TypeError: update_element() missing 2 required positional arguments: 'collection' and 'id'`. This is the intended breaking change.

## Constraints and decisions

- **Maintainer decision (binding):** "BREAKING (keyword passing of collection/id/group/label). CHANGELOG 0.11.0." The entry goes under `## [0.11.0] — unreleased` → `### Changed`, prefixed **BREAKING**, with what a caller must do. No manifest bump is needed, because 0.11.0 is already the unreleased minor.
- **Root `CLAUDE.md`, Design Decisions:** "Python's `Element` is internal; users pass `**kwargs` to create/update." The change keeps `**kwargs` and only makes the leading parameters positional-only.
- **Root `CLAUDE.md`, Cross-Layer Naming:** "C++ to Python: Same `snake_case` name … Create/update use `**kwargs`: `create_element("Collection", label="x")`." Names are unchanged. The example is still valid.
- **Root `CLAUDE.md`, Self-Updating:** the bullet in `bindings/python/CLAUDE.md` is rewritten in the same change (see Docs).
- **Scope, all five methods (including `create_element`):** the facts verifier notes that `create_element` does not "address a row", so the rule's literal wording does not cover it. Its only collision is an attribute named `collection`. The policy verifier and the item title ("every **kwargs method") both include it, and the `/` costs nothing. So the rule is restated as "every `**kwargs` method", which is simpler to follow than "every method that addresses a row".
- **Upserts gain little capability.** The core already rejects `id` and `label` as time-series columns, so there the `/` turns a Python `TypeError` into the core's `QuiverError`. The justification is uniformity and "messages come from C++", not a new feature. This is said plainly in the docs rather than overstated.
- **`**kwargs` → `**kwargs: object` on the two upsert lines.** Those two `def` lines are edited anyway. This makes all five signatures the same shape as the three that already carry the annotation. Nothing typechecks the repo (`bindings/python/CLAUDE.md`: ruff is isort-only), so this is cosmetic and has no risk.
- **The `id` attribute is an ordinary scalar in the core.** `update_element("C", 3, id=103)` changes the element's id (via `ON UPDATE CASCADE` for group rows). This is already true in Julia (`update_element!(db, "C", 3; id = 103)`), Dart and JS. It is core behaviour and not this item's to change. No test here pins it.

Rejected alternatives:
- *Rename the parameters (e.g. `_collection`, `_id`).* Keyword passing would still work, but under odd names, and a column named `_id` would still collide. `/` is the language's tool for exactly this.
- *Pop `id`/`label` out of `kwargs` in the wrapper.* That is binding-side logic that silently drops data, which breaks "Intelligence resides in C++".
- *Keyword-only `*` instead of `/`.* That is the opposite direction: it forces keywords for the addressing parameters and still collides.
- *Only the three methods the rule's wording covers.* Rejected for the reasons in "Scope, all five methods" above.

## Changes

All edits are in `bindings/python/src/quiverdb/database.py` (LF line endings, per `.gitattributes` `*.py text eol=lf`). Anchor each edit on the method name. Earlier plans (24, 25) may have shifted the line numbers.

### 1. `create_element` (currently ~L206)

Current:
```python
    def create_element(self, collection: str, **kwargs: object) -> int:
        """Create a new element. Returns the new element ID."""
```
New:
```python
    def create_element(self, collection: str, /, **kwargs: object) -> int:
        """Create a new element. Returns the new element ID."""
```
The body is unchanged.

### 2. `update_element` (currently ~L229)

Current:
```python
    def update_element(self, collection: str, id: int, **kwargs: object) -> None:
        """Update an existing element's attributes."""
```
New:
```python
    def update_element(self, collection: str, id: int, /, **kwargs: object) -> None:
        """Update an existing element's attributes.

        `collection` and `id` are positional-only so that an `id` in kwargs (e.g. the dict
        from `read_scalars_by_id`) is written as an attribute instead of colliding with this parameter.
        """
```
The body is unchanged. The docstring mirrors the one `update_element_by_label` already has (~L249-252). This is the method where the `/` makes a difference to callers.

### 3. `update_element_by_label` (currently ~L248)

No change. It already reads `def update_element_by_label(self, collection: str, label: str, /, **kwargs: object) -> None:`.

### 4. `upsert_time_series_row` (currently ~L1785)

Current:
```python
    def upsert_time_series_row(self, collection: str, group: str, id: int, **kwargs) -> None:
```
New:
```python
    def upsert_time_series_row(self, collection: str, group: str, id: int, /, **kwargs: object) -> None:
```
Do not touch the docstring below it. Plan 30 owns it.

### 5. `upsert_time_series_row_by_label` (currently ~L1806)

Current:
```python
    def upsert_time_series_row_by_label(self, collection: str, group: str, label: str, **kwargs) -> None:
```
New (116 characters, under ruff's `line-length = 120`):
```python
    def upsert_time_series_row_by_label(self, collection: str, group: str, label: str, /, **kwargs: object) -> None:
```
The docstring and body are unchanged.

### No change in other layers

- **C++ core / C API:** no change. The core already handles `id` as a scalar in `update_element` and rejects `id`/`label` in time-series rows with Pattern 1 messages.
- **FFI declarations:** no C signature changes. There is no Julia generator run and no edit to `_c_api.py`, `bindings/dart/lib/src/ffi/bindings.dart` or `bindings/js/src/loader.ts`.
- **Julia, Dart, JS, Lua:** no collision exists (Julia keyword arguments are separate from positionals; Dart/JS take a map/object; Lua takes a table), so there is nothing to change.
- **Internal callers:** `grep -rn "\.create_element(\|\.update_element(\|\.update_element_by_label(\|\.upsert_time_series_row" bindings/python/src` finds only the docstring example `db.upsert_time_series_row("Col", "grp", 1, **row_dict)`, which is positional. No other file in the repo (`scripts/`, `.github/`, `docs/`, `README.md`) calls the Python API with these parameters by keyword.

## Tests

Python only (`bindings/python/tests`). The fixture is `collections_db` from `bindings/python/tests/conftest.py`, which uses the schema `tests/schemas/valid/collections.sql`. `Collection` has `id`, `label`, `some_integer INTEGER`, `some_float REAL` and **no DATE_TIME column**, so the `read_scalars_by_id` dict has no `datetime` values. `Element.set` rejects those until plan 25. The time series group `data` has columns `date_time`, `value`. No new schema is needed.

Before the fix, each of the four new tests fails with a Python `TypeError: … got multiple values for argument '<name>'`. `pytest.raises(QuiverError)` does not catch a `TypeError`, so the three error tests fail too. They are not vacuous passes.

Existing tests: nothing pins the old behaviour. `grep -rn "multiple values" bindings/python/tests` finds nothing, and a grep for `collection=`, `group=` and `(id=` / ` id=` in `bindings/python/src` and `bindings/python/tests` finds no keyword-style call of these five methods. Every existing call is positional, including the multi-line upserts in `test_database_time_series_row.py` (~L78-115) and the `update_element` calls in `test_database_update.py`. No existing test changes.

### T1. `bindings/python/tests/test_database_update.py`, class `TestUpdateElement`

Add after `test_update_preserves_other_attributes` (the class's last method, currently ~L27-42, before `class TestUpdateElementByLabel:`):

```python
    def test_update_element_takes_an_id_attribute(self, collections_db: Database) -> None:
        """read_scalars_by_id's dict holds `id`; the positional-only `id` lets it unpack into kwargs."""
        collections_db.create_element("Configuration", label="cfg")
        elem_id = collections_db.create_element("Collection", label="Item1", some_integer=10, some_float=2.5)
        row = collections_db.read_scalars_by_id("Collection", elem_id)
        row["some_integer"] = 99
        collections_db.update_element("Collection", elem_id, **row)
        assert collections_db.read_scalars_by_id("Collection", elem_id) == {
            "id": elem_id,
            "label": "Item1",
            "some_integer": 99,
            "some_float": 2.5,
        }
```
`pytest` and `QuiverError` are already imported in this file. This test needs neither.

### T2. `bindings/python/tests/test_database_create.py`, class `TestCreateElement`

Add after `test_create_element_returns_id` (currently ~L12-16):

```python
    def test_create_element_passes_a_collection_attribute_to_the_core(self, collections_db: Database) -> None:
        """`collection` is positional-only: an attribute of that name reaches the core, not a TypeError."""
        collections_db.create_element("Configuration", label="cfg")
        with pytest.raises(QuiverError, match="'collection'"):
            collections_db.create_element("Collection", label="Item1", collection="x")
```
`pytest` and `QuiverError` are already imported (~L7, ~L9). The current core message is `Column 'collection' not found in table 'Collection'`. The match is only `'collection'` (with its quotes) so that plan 56's Pattern 1 rewording, which still quotes the column, does not break it.

### T3 and T4. `bindings/python/tests/test_database_time_series_row.py`, class `TestUpsertTimeSeriesRow`

Add after `test_upsert_time_series_row_by_label_not_found` (the class's last method, currently ~L108-117, before `class TestReadTimeSeriesRow:`). The module already imports `pytest` and `QuiverError` and defines `_create_collection_element`:

```python
    def test_upsert_passes_an_id_attribute_to_the_core(self, collections_db: Database) -> None:
        """`id` is positional-only: an `id=` kwarg reaches the core's column check instead of colliding."""
        eid = _create_collection_element(collections_db, "Item1")
        with pytest.raises(QuiverError, match="column 'id' not found in group 'data'"):
            collections_db.upsert_time_series_row("Collection", "data", eid, id=eid, date_time="2024-01-01", value=1.0)
        assert collections_db.read_time_series_group("Collection", "data", eid) == {}

    def test_upsert_by_label_passes_a_label_attribute_to_the_core(self, collections_db: Database) -> None:
        """`label` is positional-only: a `label=` kwarg reaches the core's column check instead of colliding."""
        eid = _create_collection_element(collections_db, "Item1")
        with pytest.raises(QuiverError, match="column 'label' not found in group 'data'"):
            collections_db.upsert_time_series_row_by_label(
                "Collection", "data", "Item1", label="Item1", date_time="2024-01-01", value=1.0
            )
        assert collections_db.read_time_series_group("Collection", "data", eid) == {}
```
Both messages come from `validate_time_series_row` in `src/database_time_series.cpp`. Plan 03 keeps that message (its plan text says "`validate_time_series_row` already throws `column 'id' not found in group ...`"). The trailing `== {}` matches how `test_upsert_time_series_row_by_label_not_found` asserts that nothing was written.

No tests in the other layers: the behaviour exists only in Python's argument binding.

## Docs and changelog

### `bindings/python/CLAUDE.md` (CRLF in the working tree, so use the Edit tool)

Replace the whole bullet (currently ~L42-47):

Old:
```
- **A parameter that shadows a column name needs a `/`.** A method that addresses a row
  positionally *and* takes attributes as `**kwargs` must mark the positional parameters
  positional-only, or the kwarg binds to the parameter and raises `TypeError: got multiple values
  for argument '<name>'` before the FFI call. `update_element_by_label(collection, label, /,
  **kwargs)` is the acute case — renaming via `label=` is the point of the method, and every
  collection has a `label` column by convention.
```
New:
```
- **Every `**kwargs` method marks its leading parameters positional-only (`/`).** Without it, a
  kwarg that shares a parameter's name binds to the parameter and raises `TypeError: got multiple
  values for argument '<name>'` before the FFI call. All five follow it: `create_element(collection,
  /, **kwargs)`, `update_element(collection, id, /, **kwargs)`, `update_element_by_label(collection,
  label, /, **kwargs)`, `upsert_time_series_row(collection, group, id, /, **kwargs)` and
  `upsert_time_series_row_by_label(collection, group, label, /, **kwargs)`. A new `**kwargs` method
  gets the `/` too. With it the kwarg reaches the core like any other attribute:
  `update_element(c, id, **read_scalars_by_id(c, id))` works (that dict holds `id`), `label=`
  renames through `update_element_by_label`, and `id=`/`label=` on an upsert gets the core's
  `column '<name>' not found in group` error. The cost is that these parameters cannot be passed by
  keyword.
```

No root `CLAUDE.md` edit. Its Python line (`Create/update use **kwargs: create_element("Collection", label="x")`) stays true, and the rule is Python-local. No `docs/*.md`, README or `bindings/js/src/lua-api.ts` change: `bindings/python/README.md` is empty, and root `README.md` ~L28 uses `db.create_element("Collection", label="Item 1", value=42)`, which is positional.

### `CHANGELOG.md` (CRLF, so use the Edit tool)

Under `## [0.11.0] — unreleased` → `### Changed`, append this as the **last** bullet of that list, just before the blank line that precedes `### Fixed`. Earlier plans may have added bullets; append after whatever is last:

```
- **BREAKING — Python: `collection`, `id`, `group` and `label` are positional-only on every
  `**kwargs` method.** `create_element`, `update_element`, `upsert_time_series_row` and
  `upsert_time_series_row_by_label` now mark their leading parameters positional-only, as
  `update_element_by_label` already did. An attribute with the same name as one of those parameters
  now reaches the core instead of failing with `TypeError: got multiple values for argument
  '<name>'` before the call. As a result, `db.update_element("C", eid, **db.read_scalars_by_id("C",
  eid))` works: the dict holds `id`, and it is written back as-is.

  *Adapt:* pass those parameters positionally. `update_element(collection="C", id=1, x=2)` now
  raises `TypeError`; write `update_element("C", 1, x=2)`.
```

## Verification

From the repo root `C:\Development\Quiver\quiver3` (PowerShell; `.bat` scripts run directly):

1. `cmake --build build --config Debug` makes sure `build/bin/libquiver.dll` / `libquiver_c.dll` are current, since `test.bat` puts `build/bin` on PATH. There are no C++ changes, so this should be a no-op.
2. Add the four tests **before** the source edits and run `bindings/python/tests/test.bat -k "attribute_to_the_core or takes_an_id_attribute"`. Expected: 4 failed, each with `TypeError: … got multiple values for argument 'id'` / `'label'` / `'collection'`.
3. Apply Changes 1–5, then run the same command again. Expected: `4 passed`. The four tests are:
   - `test_database_update.py::TestUpdateElement::test_update_element_takes_an_id_attribute`
   - `test_database_create.py::TestCreateElement::test_create_element_passes_a_collection_attribute_to_the_core`
   - `test_database_time_series_row.py::TestUpsertTimeSeriesRow::test_upsert_passes_an_id_attribute_to_the_core`
   - `test_database_time_series_row.py::TestUpsertTimeSeriesRow::test_upsert_by_label_passes_a_label_attribute_to_the_core`
4. `bindings/python/tests/test.bat`. Expected: the full Python suite passes, with no existing test changed.
5. `bindings/python/format.bat` (ruff format; `scripts/format.bat` runs it among all the formatters). Then `git diff --stat` should list only `bindings/python/src/quiverdb/database.py`, the three test files, `bindings/python/CLAUDE.md` and `CHANGELOG.md`. If ruff rewrapped a new test line, keep its result.
6. `git grep -n "\*\*kwargs" -- bindings/python/src`. Expected: exactly five lines, all of them `def` lines containing `/, **kwargs: object`. The `upsert_time_series_row` docstring example uses `**row_dict`, so it does not match.
7. `scripts/test-all.bat`. Expected: all suites green (no other layer changed).

## Acceptance criteria

- [ ] `create_element`, `update_element`, `upsert_time_series_row` and `upsert_time_series_row_by_label` have `/` after their leading parameters. `update_element_by_label` is unchanged. All five annotate `**kwargs: object`.
- [ ] `update_element`'s docstring says why `collection`/`id` are positional-only.
- [ ] The `upsert_time_series_row` docstring text is untouched (left to plan 30).
- [ ] The four new Python tests exist, fail on HEAD with `TypeError`, and pass after the change.
- [ ] The full Python suite passes and no existing test was edited.
- [ ] The `bindings/python/CLAUDE.md` bullet is rewritten to name all five methods and the keyword-passing cost.
- [ ] A `CHANGELOG.md` **BREAKING** entry is under `[0.11.0] — unreleased` → `### Changed`, with an *Adapt:* line.
- [ ] No C++, C API, FFI declaration, Julia, Dart, JS or Lua file is touched.
- [ ] `scripts/test-all.bat` is green.

## Pitfalls

- **CRLF files.** `bindings/python/CLAUDE.md` and `CHANGELOG.md` are CRLF in the working tree (`.gitattributes` has `* text=auto` and neither is in the LF list). Edit them with the Edit tool, not `sed`/heredocs. The `.py` files are LF (`*.py text eol=lf`). Don't touch any `.bat`.
- **The DATE_TIME trap in T1.** Do not "improve" T1 to use the `db` fixture (`basic.sql`, `Configuration` has `date_attribute`). Until plan 25 lands, `read_scalars_by_id` returns a `datetime` for that column and `Element.set` raises `TypeError: Unsupported type datetime` (`bindings/python/src/quiverdb/element.py`, `Element.set`). That is a different `TypeError`, and it would confuse the before/after check.
- **Pytest `-k` from `test.bat`.** `test.bat` forwards `%*` to `uv run pytest tests/`. Quote the `-k` expression as shown. From Git Bash use `cmd //c "bindings\python\tests\test.bat -k \"attribute_to_the_core or takes_an_id_attribute\""`, or just use PowerShell.
- **Stale `__pycache__`.** `bindings/python/src/quiverdb/__pycache__` holds compiled files for two interpreters. Pytest recompiles on mtime change, so nothing needs deleting. Don't commit them (they are untracked).
- **Plan 30 ordering.** If plan 30 has somehow already landed, the upsert docstrings will differ from HEAD. That does not matter, because this plan anchors on the `def` lines only.
- **`update_element(..., id=<other>)` renumbers the element.** This is core behaviour shared with every binding. Don't add a guard in Python (that would be binding-side logic), and don't write a test that pins it here.

## Out of scope

- Making `update_element` reject or ignore an `id` attribute that differs from the addressed id. That is a core policy question and no plan owns it; every binding already behaves this way.
- Rewording the stale `upsert_time_series_row` docstring ("No Int->Float coercion (per D-03 …)", the bool/int/float/str dispatch list): plan **30**.
- Accepting `datetime` in `Element.set` / the upsert marshaller, which would make the `read_scalars_by_id` round trip work on DATE_TIME collections: plan **25**.
- Deleting the `bool` branches in `Element.set` / the marshallers: plan **28**.
- Pattern 1 rewording of the core's `Column '<c>' not found in table '<t>'`: plan **56**.
