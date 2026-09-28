# 29 — Python: delete Makefile, dotenv dev-dep, unused fixtures, Element.clear/_ensure_valid

**Batch** 4 · **Severity** low · **Breaking** no. `Element` is internal (not exported from `quiverdb`), so no public surface changes. The only behaviour change is the message a use of an already-destroyed internal `Element` raises: `Element has been destroyed` becomes the C API's `Null argument: element`. No caller can reach this. · **Size** S · **Layers** Python binding only (`src/quiverdb/element.py`, `tests/`, `pyproject.toml`, `format.bat`, `Makefile`), plus `bindings/python/AGENTS.md`

**Depends on** none · **Overlaps with** 07, 21, 24, 25, 26, 28, 30, 67, 87 (what each shares is listed below)

- **24, 25, 28** all edit `Element.set` / `Element._set_array` in `bindings/python/src/quiverdb/element.py` before this plan runs: 24 types numeric columns, 25 adds a datetime branch, 28 drops the bool branches. This plan deletes only the `self._ensure_valid()` line at the top of `set`, the `_ensure_valid` and `clear` methods, and possibly the `QuiverError` import. Every edit below is anchored on text those plans do not touch. Expect different line numbers.
- **26** records that neither plan changes `Element.__init__`'s ordering (`self._destroyed = False` set after `check(...)`). This plan keeps that line as it is.
- **07** adds `TestListGroups.test_list_groups_unknown_collection(self, collections_db)` to `tests/test_database_metadata.py`. Once this plan deletes the module-local fixture, that test resolves to conftest's file-backed `collections_db`. It asserts `QuiverError` only, so it behaves the same.
- **30 / 67** delete the Python lifecycle describe test (`test_database_lifecycle.py:91-92`). Plan 30 cites `test_database_metadata.py`'s `TestDescribe.test_returns_string` as the surviving copy. This plan keeps `TestDescribe`, `TestDescribeCollection` and `TestSummarizeCollection` and changes only the fixture they resolve to.
- **30** owns the `_c_api.py` header comment. **21** owns the `check()` docstring's mention of `quiver_clear_last_error`. Neither is touched here.
- **87** owns `.pre-commit-config.yaml` and the "keep .bat files CRLF" policy. This plan rewrites `bindings/python/format.bat` and keeps it all-CRLF.

---

## Why

Six pieces of dead or misleading code and config in the Python binding. Everything was re-verified at HEAD 58dfe7a.

1. **`bindings/python/Makefile`: never invoked, and misleading.**
   ```make
   .PHONY: lint test docker_run
   lint:
   	uv sync
   	uv run ruff check . --fix
   	uv run ruff format .
   test:
   	uv run pytest -v -s
   publish:
   	uv sync
   	uv build
   	uv publish
   ```
   - `git grep -n -E "Makefile|make (lint|test|publish)|docker_run"` finds only the file itself and the unrelated `"Unix Makefiles"` generator in `CMakePresets.json:60`.
   - `docker_run` is declared phony, but no such rule exists.
   - `publish` runs `uv build` / `uv publish`, which bypasses the real publish path (`.github/workflows/publish-python.yml:36`, `pypa/cibuildwheel@v4.2.0`).
   - `test` duplicates `tests/test.bat` without the `build/bin` PATH prepend, so on Windows it cannot find the DLLs.
   - The layout in `bindings/python/AGENTS.md` does not list the Makefile.

   **Correction to the finding:** `format.bat` does *not* cover `lint`. `bindings/python/format.bat` is only:
   ```bat
   uv sync
   uv run ruff format .
   ```
   So `ruff check . --fix` (the isort rule, `ruff.toml` `[lint] select = ["I"]`) runs only from the Makefile. Nothing in `.github`, `scripts` or `.pre-commit-config.yaml` runs ruff lint. Proof that the isort rule is currently unenforced (read-only run at HEAD):
   ```
   $ bindings/python/.venv/Scripts/ruff.exe check . --no-cache     (from bindings/python)
   I001 [*] Import block is un-sorted or un-formatted
    --> tests\test_database_query.py:3:1
   Found 1 error.
   ```
   Deleting the Makefile without moving that step would leave the isort config with nothing that runs it. The maintainer's decision is to move it into `format.bat` first.

2. **`dotenv>=0.9.9` dev dependency: unused.** `bindings/python/pyproject.toml:19`:
   ```toml
   dev = ["dotenv>=0.9.9", "pytest>=8.4.1", "ruff>=0.12.2"]
   ```
   `git grep -n dotenv` finds only this line. There is no `import dotenv` / `load_dotenv` anywhere. `uv.lock` is gitignored (`bindings/python/.gitignore`, last line `/uv.lock`), so no committed lockfile needs updating.

3. **Three pytest fixtures with zero users** in `bindings/python/tests/conftest.py`:
   - `tests_path` (currently ~L11-14)
   - `csv_db_export` (~L85-88)
   - `csv_db_import` (~L91-94)

   `git grep -n -w -E "tests_path|csv_db_export|csv_db_import" -- '*.py'` hits only the three definitions. `pytest tests --fixtures-per-test -q | grep -cE "^(tests_path|csv_db_export|csv_db_import) "` prints `0`. No `getfixturevalue` or `usefixtures` exists anywhere in `tests/`. `schemas_path` builds its path from `Path(__file__)` itself, so nothing depends on `tests_path`. `csv_db` (their parent) stays, because it is used.

4. **A module-local `collections_db` that silently shadows conftest's.** `bindings/python/tests/test_database_metadata.py` (currently ~L206-211):
   ```python
   @pytest.fixture
   def collections_db(collections_schema_path: Path) -> Generator[Database, None, None]:
       """An in-memory database opened from the collections schema."""
       database = Database.from_schema(":memory:", str(collections_schema_path))
       yield database
       database.close()
   ```
   It sits under the "Schema inspection" banner and reads as describe-only. Pytest resolves fixtures per module, though, so it overrides `conftest.py`'s `collections_db` for every test in the file. Verified at HEAD (read-only run):
   ```
   $ pytest tests/test_database_metadata.py --fixtures-per-test -q -k "test_list_vector_groups or test_group_metadata_frozen"
   ---- fixtures used by test_list_vector_groups ----
   collections_db -- tests\test_database_metadata.py:207
       An in-memory database opened from the collections schema.
   ---- fixtures used by test_group_metadata_frozen ----
   collections_db -- tests\test_database_metadata.py:207
   ```
   So `TestGetGroupMetadata`, `TestListGroups` and `TestMetadataFrozen.test_group_metadata_frozen` do not use the fixture a reader would assume. The result is harmless, but it misleads the reader and duplicates conftest.

5. **`Element.clear`: no production caller.** `bindings/python/src/quiverdb/element.py` (currently ~L107-111):
   ```python
   def clear(self) -> None:
       """Clear all set attributes from this element."""
       self._ensure_valid()
       lib = get_lib()
       check(lib.quiver_element_clear(self._ptr))
   ```
   Its only caller is `tests/test_element.py::test_element_clear` (~L121-129). `Element` is internal (`bindings/python/AGENTS.md`: "element.py # Element builder - INTERNAL ONLY"). Its three production users are `Database.create_element`, `update_element` and `update_element_by_label` (`database.py` ~L206-269). Each one builds an `Element`, calls `set` in a loop, makes one C call and destroys it in a `finally`. None of them ever clears it.

6. **`Element._ensure_valid`: redundant guard with a binding-crafted message.** `element.py` (currently ~L94-96):
   ```python
   def _ensure_valid(self) -> None:
       if self._destroyed:
           raise QuiverError("Element has been destroyed")
   ```
   It is called from `set` (first statement, ~L24) and from `clear`. It can never fire in production (see 5). Even when it could, it is redundant. `destroy()` sets `self._ptr = ffi.NULL`, and every C setter starts with `QUIVER_REQUIRE(element, ...)` (`src/c/element.cpp`, e.g. `quiver_element_set_integer`). That macro (`src/c/internal.h`, `QUIVER_REQUIRE_1`) sets `"Null argument: element"` and returns `QUIVER_ERROR`, which `check()` raises as `QuiverError`. `ElementCApi.NullElementErrors` pins that C behaviour; it passes in the current `build/bin/quiver_c_tests.exe`. The root AGENTS.md "Error Messages" principle is explicit: "All error messages are defined in the C++/C API layer. Bindings retrieve and surface them — they never craft their own." Once `_ensure_valid` goes, `from quiverdb.exceptions import QuiverError` (~L5) has no other use in `element.py`. Nothing would flag the stale import, because `ruff.toml` selects only `I`.

   Reproduction of the message change:
   ```python
   e = Element(); e.destroy(); e.set("label", "x")
   # today : QuiverError("Element has been destroyed")   <- crafted in the binding
   # after : QuiverError("Null argument: element")       <- from the C API
   ```

Principles this change follows (root `AGENTS.md`): "Delete unused code, do not deprecate." "Clean code over defensive code." The Error Messages rule quoted above.

---

## Constraints and decisions

- **Maintainer decisions (binding):**
  1. "Move `ruff check --fix` into format.bat before deleting the Makefile."
  2. "Keep the `quiver_element_clear` cdef (mirror)." `_c_api.py` mirrors the whole C header, including other element functions Python never calls: `quiver_element_has_scalars`, `has_arrays`, `scalar_count` and `array_count` (~L87-90). That mirror is what keeps it diffable against `generator/generator.bat` output (`bindings/python/AGENTS.md`: "`_c_api.py` declarations must match the C headers exactly … diff its output against `_c_api.py`").
  3. "Delete the shadowing `collections_db` fixture in test_database_metadata.py."
- **Root AGENTS.md, "Python's `Element` is internal; users pass `**kwargs` to create/update"** (Design Decisions). Deleting `Element.clear` therefore changes no public API. It also creates no cross-binding asymmetry: in Julia (`clear!`) and Dart (`Element.clear`) `Element` is public, and there it stays bound. The C API function `quiver_element_clear` stays too.
- **Keep `self._destroyed`.** `destroy()` (idempotency), `__repr__` (`"Element(destroyed)"`, pinned by `test_element_repr_destroyed`) and `__del__` still read it.
- **Keep `Element.__init__` as is** (plan 26's recorded decision).
- **`.bat` files are CRLF in the working tree** (root AGENTS.md, "Code Style Tooling" caution). `git ls-files --eol bindings/python/format.bat` shows `i/lf w/crlf attr/text=auto`, with `core.autocrlf=true`. The index stores LF and the working tree has CRLF. The rewritten `format.bat` must be CRLF on every line.
- **No CHANGELOG entry.** Root AGENTS.md: "user-visible changes get an entry". Nothing here is user-visible. `Element` is not exported. The Makefile, `format.bat` and the dev dependency group are contributor tooling. `uv.lock` is not committed.

Alternatives considered and rejected:
- *Also delete the `quiver_element_clear` cdef* (original proposal). Rejected by the maintainer: it would break the header mirror and add noise to every future generator diff.
- *Delete the Makefile and rely on `format.bat` as is* (original proposal's claim). Rejected: `format.bat` never ran `ruff check`, so isort would stop running entirely.
- *Keep `_ensure_valid` but switch it to the C message.* Rejected: it would duplicate a check the C API already makes, and the message would still be crafted in the binding.
- *Run `uv lock` as a step.* Unnecessary: `uv.lock` is gitignored, and the next `uv sync` / `uv run` re-locks locally on its own.
- *Add a CI ruff-lint job or widen `select`.* Out of scope for a dead-code item; the maintainer asked only to move the existing step.

---

## Changes

All paths are relative to the repo root `C:\Development\Quiver\quiver1`. Python files are LF (`.gitattributes`: `*.py text eol=lf`). Edit them with the Edit tool.

### 1. `bindings/python/format.bat`: add the isort fix step (do this BEFORE step 2)

Current file (2 lines, CRLF, no trailing newline after the last line):
```bat
uv sync
uv run ruff format .
```

New file (CRLF on every line, including a final CRLF):
```bat
uv sync
uv run ruff check . --fix
uv run ruff format .
```

Order matches the deleted Makefile's `lint` target: isort fixes first, then the formatter. With `select = ["I"]` every violation is auto-fixable, so `ruff check --fix` exits 0 once it has fixed things. Write it from PowerShell at the repo root so the CRLFs are exact:
```powershell
[IO.File]::WriteAllText("$PWD\bindings\python\format.bat", "uv sync`r`nuv run ruff check . --fix`r`nuv run ruff format .`r`n")
```
Check it with Git Bash: `cat -A bindings/python/format.bat`. Every line must end in `^M$`, and there must be exactly three lines.

### 2. `bindings/python/Makefile`: delete

```bash
git rm bindings/python/Makefile
```
Nothing references it (see Why §1). Its `test` target is covered by `bindings/python/tests/test.bat`, `lint` by `format.bat` after step 1, and `publish` by `.github/workflows/publish-python.yml`.

### 3. `bindings/python/pyproject.toml`: drop `dotenv` from the dev group

Current (`[dependency-groups]`, ~L18-19):
```toml
[dependency-groups]
dev = ["dotenv>=0.9.9", "pytest>=8.4.1", "ruff>=0.12.2"]
```
New:
```toml
[dependency-groups]
dev = ["pytest>=8.4.1", "ruff>=0.12.2"]
```
The file is `text=auto` (CRLF in the working tree). The Edit tool keeps the file's existing line endings, so a one-line edit is safe. Do not run `uv lock` as a separate step. `uv.lock` is gitignored, and the next `uv sync` (step 1's `format.bat`, or `tests/test.bat`'s `uv run`) re-locks it and uninstalls `dotenv` / `python-dotenv` from `.venv`.

### 4. `bindings/python/tests/conftest.py`: delete three unused fixtures

Delete this block (currently ~L11-15, directly after the imports, together with the two blank lines that follow it):
```python
@pytest.fixture
def tests_path() -> Path:
    """Return the tests/ directory path."""
    return Path(__file__).resolve().parent


```
After the deletion the file starts:
```python
from __future__ import annotations

from collections.abc import Generator
from pathlib import Path

import pytest

from quiverdb import Database


@pytest.fixture
def schemas_path() -> Path:
    """Return the shared test schemas directory."""
    return Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas"
```

Delete this block (currently ~L85-96, between `csv_db` and `all_types_schema_path`):
```python
@pytest.fixture
def csv_db_export(csv_db: Database) -> Database:
    """Return csv_db typed as DatabaseCSVExport (Database inherits it)."""
    return csv_db


@pytest.fixture
def csv_db_import(csv_db: Database) -> Database:
    """Return csv_db typed as DatabaseCSVImport (Database inherits it)."""
    return csv_db


```
so that `csv_db` is followed directly by:
```python
@pytest.fixture
def csv_db(csv_export_schema_path: Path, tmp_path: Path) -> Generator[Database, None, None]:
    """Create a test database with the CSV export schema."""
    database = Database.from_schema(str(tmp_path / "csv.db"), str(csv_export_schema_path))
    yield database
    database.close()


@pytest.fixture
def all_types_schema_path(schemas_path: Path) -> Path:
```
Every import in `conftest.py` stays used: `Generator`, `Path`, `pytest`, `Database`. Keep `csv_db`, because the CSV test files use it.

### 5. `bindings/python/tests/test_database_metadata.py`: delete the shadowing fixture and its imports

Delete the fixture (currently ~L206-212) and the two blank lines after it:
```python
@pytest.fixture
def collections_db(collections_schema_path: Path) -> Generator[Database, None, None]:
    """An in-memory database opened from the collections schema."""
    database = Database.from_schema(":memory:", str(collections_schema_path))
    yield database
    database.close()


```
Keep the banner comment above it. It still correctly heads the describe tests. The result must read:
```python
# -- Schema inspection (describe / describe_collection / summarize_collection).
#    The binding only verifies each method returns a string; the report content
#    is covered by the C++ core tests (tests/test_database_describe.cpp).
# -----------------------------------------------------------------------------


class TestDescribe:
    def test_returns_string(self, collections_db: Database) -> None:
        assert isinstance(collections_db.describe(), str)
```

Delete the now-unused imports at the top (currently ~L7-8). Their only use was the fixture's signature (`grep -n "Generator\|Path" tests/test_database_metadata.py` shows only L7, L8 and L207 at HEAD):
```python
from collections.abc import Generator
from pathlib import Path
```
The import block becomes:
```python
from __future__ import annotations

import dataclasses

import pytest

from quiverdb import Database, DataType, GroupMetadata, QuiverError, ScalarMetadata
```
After this change every test in the module uses conftest's `collections_db` (file-backed, under `tmp_path`). No assertion in the module depends on `:memory:`. They check metadata values, list contents, frozen dataclasses, `str` returns and `QuiverError` raises.

### 6. `bindings/python/src/quiverdb/element.py`: delete `_ensure_valid`, `clear` and the stale import

Plans 24, 25 and 28 have already edited `set` and `_set_array` by the time this runs. Find each excerpt by text, not by line number.

**6a.** In `Element.set`, delete the first statement after the docstring:
```python
        self._ensure_valid()
```
So `set` goes straight from its docstring into the `if value is None:` dispatch (or whatever first branch plans 24, 25 and 28 left). Do not change anything else in `set`.

**6b.** Delete the method (currently ~L94-97, including the blank line after it):
```python
    def _ensure_valid(self) -> None:
        if self._destroyed:
            raise QuiverError("Element has been destroyed")

```

**6c.** Delete the method (currently ~L107-112, including the blank line after it):
```python
    def clear(self) -> None:
        """Clear all set attributes from this element."""
        self._ensure_valid()
        lib = get_lib()
        check(lib.quiver_element_clear(self._ptr))

```

**6d.** In `destroy`, annotate the NULL assignment. The deleted guard relied on this, so the next reader needs to know why no binding check exists:
Current:
```python
        lib.quiver_element_destroy(self._ptr)
        self._ptr = ffi.NULL
        self._destroyed = True
```
New:
```python
        lib.quiver_element_destroy(self._ptr)
        self._ptr = ffi.NULL  # a later set() fails in the C API: "Null argument: element"
        self._destroyed = True
```

**6e.** Run `grep -n "QuiverError" bindings/python/src/quiverdb/element.py`. If the only remaining hit is the import line
```python
from quiverdb.exceptions import QuiverError
```
delete that line. At HEAD it is the only other use; plans 24, 25 and 28 raise `TypeError` / `ValueError`, not `QuiverError`. If an earlier plan did add a real `QuiverError` use, keep the import.

After 6a–6e, the tail of the class reads, from `_set_array_string` to the end:
```python
    def _set_array_string(self, name: str, values: list[str]) -> None:
        ...  # unchanged (possibly edited by plan 24)

    def destroy(self) -> None:
        """Free the underlying C element. Idempotent."""
        if self._destroyed:
            return
        lib = get_lib()
        lib.quiver_element_destroy(self._ptr)
        self._ptr = ffi.NULL  # a later set() fails in the C API: "Null argument: element"
        self._destroyed = True

    def __repr__(self) -> str:
        if self._destroyed:
            return "Element(destroyed)"
        lib = get_lib()
        out = ffi.new("char**")
        check(lib.quiver_element_to_string(self._ptr, out))
        result = decode_string(out[0])
        lib.quiver_database_free_string(out[0])
        return result

    def __del__(self) -> None:
        if not self._destroyed:
            self.destroy()
```
The imports keep `ffi, get_lib` and `check, decode_string`. `check` is still used by `__init__`, the setters and `__repr__`; `decode_string` by `__repr__`. Plus anything plan 25 added, such as a datetime formatter from `_helpers`.

### 7. `bindings/python/src/quiverdb/_c_api.py`: NO change

Keep `quiver_error_t quiver_element_clear(quiver_element_t* element);` (currently ~L64), per the maintainer's decision (mirror of `include/quiver/c/element.h`).

### 8. Other layers: NO change

- The C++ core, the C API (`quiver_element_clear` stays, still pinned by `ElementCApi.ClearNull` etc. in `tests/test_c_api_element.cpp`) and Lua are unchanged.
- Julia (`clear!`), Dart (`Element.clear`) and JS are unchanged; their `Element` surfaces are independent of Python's internal class.
- There are no FFI regenerations, because no C header changes.

### 9. Expected side effect of step 1: `bindings/python/tests/test_database_query.py`

The first run of the new `format.bat` applies the isort fix that no tool has applied so far. It removes one of the two blank lines between `from quiverdb import Database, QuiverError` and `# -- Simple queries (QUERY-01) ---…` (currently ~L10-11). This was simulated on a copy with the repo's `ruff.toml` and ruff 0.16.9: `ruff check --fix` removes the line, `ruff format` leaves it removed, and a re-check prints `All checks passed!`. Commit this one-line change with the plan. If an earlier plan (24–28) left another unsorted import block, `ruff check --fix` fixes that too. Accept import-order-only diffs in `bindings/python/**/*.py`, and nothing else.

---

## Tests

### Python: `bindings/python/tests/test_element.py`

**Delete** `test_element_clear` (currently ~L121-129). It exercises the deleted method:
```python
def test_element_clear() -> None:
    e = Element()
    try:
        e.set("label", "test").set("value", 42)
        e.clear()
        # After clear, can set new values
        e.set("label", "new_test").set("value", 99)
    finally:
        e.destroy()
```

**Add** in its place, right after `test_element_repr_destroyed` and before `test_element_not_in_public_api`:
```python
def test_element_set_after_destroy_raises() -> None:
    """A destroyed Element holds a NULL pointer; the C API, not the binding, rejects it."""
    e = Element()
    e.destroy()
    with pytest.raises(QuiverError, match="Null argument: element"):
        e.set("label", "x")
```
and extend the imports (first-party section, sorted):
```python
from __future__ import annotations

import pytest

from quiverdb import QuiverError
from quiverdb.element import Element
```

This new test **fails before the fix**. At HEAD, `_ensure_valid` raises `QuiverError("Element has been destroyed")`, so `match="Null argument: element"` does not match. It **passes after** it, because `set` → `_set_string` → `quiver_element_set_string(NULL, …)` → `QUIVER_REQUIRE(element, name, value)` → `"Null argument: element"` → `check()` raises `QuiverError`. It pins the claim this deletion rests on: the C API reports use-after-destroy.

### Python: `bindings/python/tests/conftest.py`, `tests/test_database_metadata.py`

No test code or assertion changes; only fixtures are deleted (Changes §4, §5). `TestGetGroupMetadata` (4 tests), `TestListGroups` (3 tests, plus plan 07's `test_list_groups_unknown_collection` if it landed), `TestMetadataFrozen.test_group_metadata_frozen`, `TestDescribe`, `TestDescribeCollection` and `TestSummarizeCollection` now resolve `collections_db` to `conftest.py`. They must still pass unchanged.

### Other layers

No behaviour change is visible outside the Python binding, so no C++, C API, Lua, Julia, Dart or JS test changes. The existing C API pins (`ElementCApi.ClearNull`, `ElementCApi.NullElementErrors`) keep covering both `quiver_element_clear` and the NULL-element rejection.

---

## Docs and changelog

### `bindings/python/AGENTS.md`, `## Layout` code block

Old:
```
tests/            # Test suite (test_*.py per area) + test.bat
pyproject.toml    # Version must match CMakeLists.txt; requires-python >=3.13; deps: cffi>=2.0
ruff.toml         # Lint/format config (format.bat runs ruff)
```
New:
```
tests/            # Test suite (test_*.py per area) + test.bat
format.bat        # uv sync, ruff check --fix (isort), ruff format; run from bindings/python
pyproject.toml    # Version must match CMakeLists.txt; requires-python >=3.13; deps: cffi>=2.0;
                  # dev group: pytest, ruff
ruff.toml         # Lint/format config; lint is isort only (select = ["I"])
```

### `bindings/python/AGENTS.md`, `## Rules and gotchas`, the "**API shape**" bullet

Old sentence:
```
  `db.create_element("Collection", **my_dict)`); the `Element` class is internal. Properties are
```
New:
```
  `db.create_element("Collection", **my_dict)`); the `Element` class is internal — built, set and
  destroyed inside one `try/finally` by each writer, with no `clear` and no binding-side
  use-after-destroy guard (a destroyed `Element` holds `ffi.NULL`, which the C API rejects as
  `Null argument: element`). Properties are
```
The rest of the bullet ("regular methods, not `@property` (design decision). `LogLevel` is …") stays. Re-wrap to ~100 columns like the surrounding text.

### Root `AGENTS.md`

No change. "Code Style Tooling" says `scripts/format.bat` runs "each binding's own `format.bat` (JuliaFormatter, dart format, ruff, biome)", which stays accurate. No root passage mentions the Makefile, `dotenv`, the fixtures or Python's `Element.clear`. Verified: `git grep -n -E "conftest|csv_db|collections_db|Element\.clear|Makefile" -- '*.md'` finds nothing relevant.

### `CHANGELOG.md`

No entry (see Constraints: nothing user-visible).

### Other docs

None. No doc under `docs/`, no README and no `bindings/js/src/lua-api.ts` text mentions any of the deleted items.

---

## Verification

Run from the repo root `C:\Development\Quiver\quiver1` unless stated otherwise. The PowerShell forms are given; Git Bash equivalents work too.

1. Static greps, all expected to print nothing unless noted:
   ```bash
   git grep -n -E "_ensure_valid|def clear|Element has been destroyed" -- bindings/python/src
   git grep -n -w -E "tests_path|csv_db_export|csv_db_import|dotenv" -- bindings/python
   git grep -n quiver_element_clear -- bindings/python      # expected: exactly one hit, src/quiverdb/_c_api.py
   git ls-files bindings/python/Makefile                    # expected: nothing (deleted)
   ```
   `cat -A bindings/python/format.bat` must show three lines, each ending `^M$`.
2. Build, so `build/bin` holds current DLLs for the Python tests. There is no C++ change here, but earlier plans change C++:
   ```bash
   cmake --build build --config Debug
   ```
3. Run the new Python lint/format step (from `bindings\python`, because `format.bat` has no `pushd`):
   ```powershell
   Push-Location bindings\python; cmd /c format.bat; uv run ruff check .; Pop-Location
   ```
   Expected: `uv sync` removes `dotenv` / `python-dotenv` (and may rebuild the editable `quiverdb`, see Pitfalls). `ruff check . --fix` reports `Found N errors (N fixed, 0 remaining).` on the first run (N = 1 at HEAD 58dfe7a; more only if an earlier plan left an unsorted import block), and the trailing `uv run ruff check .` prints `All checks passed!`. `git diff --stat` then additionally lists `bindings/python/tests/test_database_query.py | 1 -` (Changes §9).
4. Confirm the fixture resolution changed (from `bindings\python`):
   ```powershell
   Push-Location bindings\python; uv run pytest tests/test_database_metadata.py --fixtures-per-test -q -k "TestListGroups or TestMetadataFrozen or TestDescribe"; Pop-Location
   ```
   Expected: every `collections_db -- …` line points at `tests\conftest.py` (the `def collections_db` line, ~L51 after step 4), with docstring "Create a test database with the collections schema." No line points at `test_database_metadata.py`.
5. Python suite:
   ```powershell
   bindings\python\tests\test.bat
   ```
   Expected: all pass, including `tests/test_element.py::test_element_set_after_destroy_raises`. `test_element_clear` no longer exists. To run only the element file: `Push-Location bindings\python; $env:PATH = "$PWD\..\..\build\bin;$env:PATH"; uv run pytest tests/test_element.py -v; Pop-Location`.
6. Repo formatting, then the full sweep:
   ```powershell
   scripts\format.bat
   scripts\test-all.bat
   ```
   Expected: green. `git status --short` lists only these, and `uv.lock` does not appear because it is gitignored:
   - `bindings/python/Makefile` (deleted)
   - `bindings/python/format.bat`
   - `bindings/python/pyproject.toml`
   - `bindings/python/src/quiverdb/element.py`
   - `bindings/python/tests/conftest.py`
   - `bindings/python/tests/test_database_metadata.py`
   - `bindings/python/tests/test_database_query.py`
   - `bindings/python/tests/test_element.py`
   - `bindings/python/AGENTS.md`

   If `git diff` shows `format.bat` or any other `.bat` file with changed line endings, restore CRLF (Pitfalls).

---

## Acceptance criteria

- [ ] `bindings/python/format.bat` is exactly `uv sync` / `uv run ruff check . --fix` / `uv run ruff format .`, all-CRLF with a final CRLF.
- [ ] `bindings/python/Makefile` is deleted (`git rm`).
- [ ] `pyproject.toml` dev group is `["pytest>=8.4.1", "ruff>=0.12.2"]`; no `dotenv` anywhere in tracked files.
- [ ] `conftest.py` no longer defines `tests_path`, `csv_db_export`, `csv_db_import`; `csv_db` is kept.
- [ ] `test_database_metadata.py` has no local `collections_db` fixture and no `Generator` / `Path` imports; `--fixtures-per-test` shows conftest's fixture for its tests.
- [ ] `element.py` has no `_ensure_valid`, no `clear`, no `self._ensure_valid()` call and (unless an earlier plan added a real use) no `QuiverError` import; `_destroyed`, `destroy`, `__repr__`, `__del__` and `__init__` are unchanged apart from the one-line comment in `destroy`.
- [ ] `_c_api.py` still declares `quiver_element_clear`.
- [ ] `test_element_clear` is deleted; `test_element_set_after_destroy_raises` exists and passes.
- [ ] `uv run ruff check .` (from `bindings/python`) prints `All checks passed!`; `tests/test_database_query.py` carries the one-line isort fix.
- [ ] `bindings/python/AGENTS.md` Layout and API-shape bullet updated as above.
- [ ] `bindings\python\tests\test.bat` and `scripts\test-all.bat` pass.

---

## Pitfalls

- **CRLF in `format.bat`.** Do not write it with `echo`, `sed` or a bash heredoc. Those produce LF, and `cmd` will still run it, but `git diff` will show the whole file changed and the repo's CRLF convention breaks. Use the PowerShell `WriteAllText` line in Changes §1, then check it with `cat -A`. The pre-commit `mixed-line-ending --fix=lf` hook only touches *mixed* files, so an all-CRLF file is safe.
- **Order within the change.** Do step 1 (format.bat) before or together with step 2 (Makefile). A commit that deletes the Makefile first leaves the isort rule with nothing that runs it.
- **The first `uv sync` / `uv run` after editing `pyproject.toml` may take minutes.** `.venv` holds an *editable* scikit-build-core install of `quiverdb` (`.venv/Lib/site-packages/_editable_skbc_quiverdb.pth`), and uv re-installs a local project when its `pyproject.toml` changes. That re-runs a CMake Release build of the whole C++ tree. This is expected, not a hang.
- **Line numbers have moved.** Plans 24, 25 and 28 rewrote parts of `Element.set` / `_set_array` before this plan. Anchor on the quoted text (`self._ensure_valid()`, `def _ensure_valid`, `def clear`, `self._ptr = ffi.NULL`), not on `~L` numbers.
- **Do not delete `_destroyed`.** `destroy()`, `__repr__` (pinned by `test_element_repr_destroyed` → `"Element(destroyed)"`) and `__del__` need it.
- **Do not delete the `quiver_element_clear` cdef**, nor the other unused element cdefs (`has_scalars`, `has_arrays`, `scalar_count`, `array_count`). This is the maintainer's decision: `_c_api.py` is a header mirror.
- **`tests/test.bat` passes `%*` after `tests/`.** Giving it a node id still collects the whole `tests/` directory. To run one file, use the `uv run pytest tests/test_element.py` form in Verification §5, with `build\bin` on PATH.
- **The isort side effect is real.** The `tests/test_database_query.py` one-line diff is the proof that the moved step now runs. Commit it; do not revert it.
- **Pytest fixture override is by name, per module.** After deleting the local `collections_db`, do not add another module-level fixture with that name in `test_database_metadata.py`, or the shadowing comes back.

---

## Out of scope

- The `_c_api.py` header comment ("Phase 1 CFFI declarations …"): **plan 30**.
- The `check()` docstring in `_helpers.py` mentioning `quiver_clear_last_error`: **plan 21**.
- `Element.__init__`'s `_destroyed`-after-`check` ordering: left as is by the recorded decision in **plan 26**.
- Deleting the Python lifecycle describe test and the other describe-test cleanups: **plans 30 / 67**.
- `Element.set` / `_set_array` typing, datetime and bool changes: **plans 24 / 25 / 28**.
- `.pre-commit-config.yaml` changes: **plan 87**.
- `Database._ensure_open`'s binding-crafted `"Database has been closed"`: not part of this finding; not touched.
- `ruff.toml`'s stale `known-first-party = ["app"]`, widening `select` beyond `I`, and adding a ruff step to CI: not in this item's findings.
- The empty `bindings/python/README.md`: not in this item's findings.
