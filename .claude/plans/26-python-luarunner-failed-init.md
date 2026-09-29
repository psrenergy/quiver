# 26 — Python LuaRunner: a failed construction must look closed to `__del__`

**Batch** 4 · **Severity** low · **Breaking** no · **Size** S · **Layers** Python binding only

**Depends on** none · **Overlaps with** none. Plans 27, 28, 29 and 30 also edit Python sources,
but none of them touches `LuaRunner.__init__` or `TestLuaRunnerLifecycle` in
`tests/test_lua_runner.py`. If one of them landed first and moved lines, anchor on the quoted
excerpts below, not on the line numbers.

## Why

Python runs `__del__` on an object even when its `__init__` raised. `LuaRunner.__init__` marks the
runner open **before** the FFI call that can fail, and assigns `_ptr` only **after** it succeeds.
If construction fails, the half-built object therefore looks open, has no `_ptr`, and its
destructor misbehaves.

`bindings/python/src/quiverdb/lua_runner.py`, `LuaRunner.__init__` (currently ~L21-27):

```python
    def __init__(self, db: Database) -> None:
        self._db = db
        self._closed = False
        lib = get_lib()
        out_runner = ffi.new("quiver_lua_runner_t**")
        check(lib.quiver_lua_runner_new(db._ptr, out_runner))
        self._ptr = out_runner[0]
```

`LuaRunner.__del__` (currently ~L67-70) and `LuaRunner.close` (currently ~L29-36):

```python
    def __del__(self) -> None:
        if not self._closed:
            warnings.warn("LuaRunner was not closed explicitly", ResourceWarning, stacklevel=2)
            self.close()
```
```python
    def close(self) -> None:
        """Free the LuaRunner handle. Idempotent."""
        if self._closed:
            return
        lib = get_lib()
        lib.quiver_lua_runner_free(self._ptr)   # <- AttributeError when __init__ failed
        self._ptr = ffi.NULL
        self._closed = True
```

**Reproduction** (verified by reading the code path end to end):

```python
db = Database.from_schema("x.db", "tests/schemas/valid/collections.sql")
db.close()            # Database.close sets db._ptr = ffi.NULL (database.py, Database.close)
LuaRunner(db)         # C API: QUIVER_REQUIRE(db, out_runner) -> "Null argument: db"
                      # check() raises QuiverError("Null argument: db")  <- correct
# when the half-built object is collected:
#   ResourceWarning: LuaRunner was not closed explicitly          <- spurious (shown under -W / dev mode)
#   Exception ignored in: <function LuaRunner.__del__ ...>
#   AttributeError: 'LuaRunner' object has no attribute '_ptr'    <- printed every time
```

The chain:

- `Database.close` (`bindings/python/src/quiverdb/database.py`, currently ~L115-122) sets
  `self._ptr = ffi.NULL`. `LuaRunner.__init__` never calls `db._ensure_open()`, so the NULL
  reaches the C API.
- `quiver_lua_runner_new` (`src/c/lua_runner.cpp`, currently ~L17-18) runs
  `QUIVER_REQUIRE(db, out_runner);`. The macro in `src/c/internal.h` (currently ~L45-51) sets
  `"Null argument: db"` and returns `QUIVER_ERROR`.
- `check` (`bindings/python/src/quiverdb/_helpers.py`) raises `QuiverError("Null argument: db")`.
  `self._ptr = out_runner[0]` never runs.
- When the object is collected, `__del__` sees `_closed == False`, warns, and calls `close()`,
  which reads the missing `self._ptr`.

A C++ constructor throw inside `quiver_lua_runner_new` fails the same way, and so does a `db`
argument without a `_ptr` attribute (the `db._ptr` read raises after `_closed` is already set).

The caller gets the right `QuiverError`, plus two false messages from a destructor running on a
half-built object. The destructor should be a no-op here, as it is for a runner that was closed.
`Database` does not have this problem: its `__init__(self, ptr)` gets a pointer that is already
valid, because its factories call the C API before they construct the object.

No existing test covers this path. `tests/test_lua_runner.py` has no closed-database case.

## Constraints and decisions

- Root `AGENTS.md` Principles: *"Clean code over defensive code … Simple solutions over complex
  abstractions."* The fix reorders one assignment and adds one line. It adds no try/except, no
  `hasattr` check in `__del__`, and no helper.
- Root `AGENTS.md` Principles, *Error Messages*: messages come from the C/C++ layer. The
  `QuiverError` text stays the C API's `"Null argument: db"`. The binding adds no message of its own.
- Root `AGENTS.md` Principles, *Changelog*: this is a user-visible fix (stderr noise on a real
  failure path). It gets a non-breaking entry under `## [0.12.0] — unreleased` → `### Fixed`.
- Root `AGENTS.md`, *Self-Updating*: the nearest AGENTS.md is `bindings/python/AGENTS.md`. It gets
  a one-bullet note so the ordering is not later "tidied" back.
- Maintainer notes for this item: none (`"notes": ""`).
- **Correction to the original finding's proposal.** Its first option was: *"Set
  `self._closed = False` after `self._ptr = out_runner[0]`"*. That is wrong on its own. Both
  verifiers are right about this: on failure `_closed` is then never assigned, and `__del__`
  raises `AttributeError: 'LuaRunner' object has no attribute '_closed'` at `if not self._closed`.
  The noise stays, only on a different attribute. This plan uses the working option: initialise
  `_closed = True` first and flip it to `False` after `_ptr` is assigned.

Alternatives considered and rejected:

- Class-level default `_closed = True`, with `self._closed = False` moved below `_ptr`. It works,
  but the invariant is then split between the class body and `__init__`. A single `__init__` that
  reads top to bottom is clearer.
- `hasattr`/`getattr(self, "_ptr", None)` guard in `__del__` or `close`. This is defensive code in
  every reader, when the real problem is one misordered assignment.
- Calling `db._ensure_open()` in `__init__` to raise `"Database has been closed"` first. It does
  not fix the bug, because a C++ constructor throw still leaves a half-built object. It is also a
  second, binding-crafted message for a case the C API already reports. Not needed.
- Fixing `Element.__init__`'s same ordering (`_destroyed` is set after `check(...)`). Only an
  allocation failure in `quiver_element_create` reaches it (policy verifier's note). This is out of
  scope: plan 29 edits `element.py`, and the case is not reachable in practice.

Other bindings checked, and no change needed:

- Julia (`bindings/julia/src/lua_runner.jl`, `function LuaRunner(db::Database)`) registers its
  `finalizer` only after `check(...)` succeeds.
- Dart (`bindings/dart/lib/src/lua_runner.dart`) and JS (`bindings/js/src/lua-runner.ts`) register
  no finalizer (no `NativeFinalizer` / `FinalizationRegistry`). A throwing constructor leaves
  nothing behind.
- C++ core, C API, FFI declarations and Lua: no change. `quiver_lua_runner_new` is already correct.
  The bug is purely in Python object lifecycle.

## Changes

### 1. `bindings/python/src/quiverdb/lua_runner.py` — `LuaRunner.__init__`

Current (currently ~L21-27):

```python
    def __init__(self, db: Database) -> None:
        self._db = db
        self._closed = False
        lib = get_lib()
        out_runner = ffi.new("quiver_lua_runner_t**")
        check(lib.quiver_lua_runner_new(db._ptr, out_runner))
        self._ptr = out_runner[0]
```

New:

```python
    def __init__(self, db: Database) -> None:
        # Closed until the native runner exists: Python still runs __del__ when __init__ raises,
        # and a failed construction must be a no-op there, not a warning plus a missing _ptr.
        self._closed = True
        self._db = db
        lib = get_lib()
        out_runner = ffi.new("quiver_lua_runner_t**")
        check(lib.quiver_lua_runner_new(db._ptr, out_runner))
        self._ptr = out_runner[0]
        self._closed = False
```

Why: `_closed = True` is the first statement, so every later failure (`get_lib()`, the `db._ptr`
read, `check`) leaves an object that `__del__` skips. `_closed = False` comes right after `_ptr`
exists, so "open" always means "has a native handle". Nothing else reads `_closed`: its only
readers are `close`, `_ensure_open` and `__del__`, all in this file. The success path is unchanged.

Leave `close`, `_ensure_open`, `run`, `__enter__`, `__exit__` and `__del__` as they are.

## Tests

### Python — `bindings/python/tests/test_lua_runner.py`

**Imports** (currently ~L1-7). Add `gc`, `sys` and `warnings` to the stdlib group, kept
alphabetical for ruff's isort rule:

Current:
```python
from __future__ import annotations

import json

import pytest

from quiverdb import Database, LuaRunner, QuiverError
```
New:
```python
from __future__ import annotations

import gc
import json
import sys
import warnings

import pytest

from quiverdb import Database, LuaRunner, QuiverError
```

**New test** in `class TestLuaRunnerLifecycle`. Put it right after `test_close_idempotent`
(currently ~L99-102), before `test_database_reference_kept`. Fixture: `collections_db` from
`tests/conftest.py` (schema `tests/schemas/valid/collections.sql`), plus pytest's built-in
`monkeypatch`.

```python
    def test_failed_construction_is_silent_when_collected(
        self, collections_db: Database, monkeypatch: pytest.MonkeyPatch
    ) -> None:
        collections_db.close()
        gc.collect()  # flush earlier tests' garbage so only this runner's __del__ is observed
        unraisable: list[sys.UnraisableHookArgs] = []
        monkeypatch.setattr(sys, "unraisablehook", unraisable.append)
        with warnings.catch_warnings(record=True) as caught:
            warnings.simplefilter("always")  # ResourceWarning is ignored by the default filters
            with pytest.raises(QuiverError, match="Null argument: db"):
                LuaRunner(collections_db)
            gc.collect()  # runs the half-built runner's __del__ even if it sits in a cycle
        assert [str(w.message) for w in caught] == []
        assert [repr(u.exc_value) for u in unraisable] == []
```

Teardown: `collections_db`'s fixture calls `database.close()` again. `Database.close` is idempotent
(`if self._closed: return`), so that is fine.

**Fails before the fix, passes after.** Before the fix, `__del__` finds `_closed == False`. Its
`warnings.warn` is recorded (the `"always"` filter overrides the default `ignore::ResourceWarning`),
so the first assert fails with `['LuaRunner was not closed explicitly'] != []`. Its `close()` then
raises `AttributeError: 'LuaRunner' object has no attribute '_ptr'`, which goes to the patched
`sys.unraisablehook`, so the second assert would fail too. After the fix, `__del__` sees
`_closed == True` and does nothing, and both lists are empty.

**Existing tests.** None pin the old ordering, and none need to change. The happy path stays
covered by `test_close_idempotent`, `test_run_after_close_raises`, `test_context_manager` and every
other test that constructs a runner.

**Other layers.** No C++, C API, Lua, Julia, Dart or JS test changes: no behaviour changes there
(see "Other bindings checked" above). No new schema file.

## Docs and changelog

### `bindings/python/AGENTS.md`

In `## Rules and gotchas`, insert a new bullet right after the bullet that begins
`- **\`LuaRunner.run\` owns its result**:` (currently ~L79-83), before the
`- **Time-series group NULLs**:` bullet:

```markdown
- **`LuaRunner.__init__` starts with `_closed = True` and sets it to `False` only after `_ptr` is
  assigned.** Python runs `__del__` even when `__init__` raised, so a runner whose construction
  failed (e.g. `LuaRunner(closed_db)`, which the C API rejects with `Null argument: db`) must
  already look closed. Otherwise `__del__` emits a spurious `ResourceWarning` and then fails on the
  missing `_ptr`. Only moving `_closed = False` below `_ptr` is not enough: `__del__` then fails on
  the missing `_closed` instead. Pinned by `test_failed_construction_is_silent_when_collected`.
```

No other AGENTS.md, `docs/*.md`, README or `bindings/js/src/lua-api.ts` text describes this
behaviour. Checked: `bindings/python/README.md` and the root `AGENTS.md` do not mention
`LuaRunner` construction failure.

### `CHANGELOG.md`

Under `## [0.12.0] — unreleased` → `### Fixed`, append this bullet after the last existing bullet
of that section (currently the `**Julia: updating \`Artifacts.toml\` now invalidates the package
precompile cache.**` entry, ~L80-82), keeping the blank line before `## [0.10.9]`:

```markdown
- **Python: a `LuaRunner` whose construction fails is silent when it is garbage-collected.**
  `LuaRunner(db)` on a closed `Database` raised `QuiverError: Null argument: db` as it should, but
  the half-built object's `__del__` then emitted a spurious `ResourceWarning: LuaRunner was not
  closed explicitly` and printed `Exception ignored in … AttributeError: 'LuaRunner' object has no
  attribute '_ptr'`. A runner now counts as closed until its native handle exists.
```

Not breaking. No manifest version bump: 0.12.0 is already the unreleased version.

## Verification

Run from the repo root (`C:\Development\Quiver\quiver1`), in PowerShell or cmd. From Git Bash,
prefix each `.bat` with `cmd //c` and use backslashes.

1. Make sure the native libraries exist (no C++ changes here, so this is a no-op on an up-to-date
   tree):
   `cmake --build build --config Debug`
2. **Write the test first, before touching `lua_runner.py`.** Run:
   `bindings\python\tests\test.bat -k failed_construction -v`
   Expected: **1 failed**, with `AssertionError` showing
   `['LuaRunner was not closed explicitly'] == []`. This confirms the test catches the bug.
   (`test.bat` does `pushd bindings\python` and `uv run pytest tests/ %*`, so `-k` passes through.)
3. Apply Change 1 and run the same command again.
   Expected: **1 passed** (`test_failed_construction_is_silent_when_collected`).
4. Run the whole Python suite: `bindings\python\tests\test.bat`. Expected: all pass, with no new
   failures.
5. Format and lint, from `bindings\python`: run `format.bat` (runs `uv sync` then
   `uv run ruff format .`), then `uv run ruff check .` (the isort rule; format.bat does not run
   it). Expected: no diff beyond this change, and `All checks passed!`.
6. Optional full sweep: `scripts\format.bat` then `scripts\test-all.bat`. Expected: green. Only
   Python changed, so the C++/C API/Julia/Dart/JS suites are unaffected.

No FFI generator run: no C API signature changes.

## Acceptance criteria

- [x] `LuaRunner.__init__` sets `self._closed = True` as its first statement and
      `self._closed = False` as its last, after `self._ptr = out_runner[0]`, with the two-line
      comment.
- [x] `close`, `_ensure_open`, `run` and `__del__` are unchanged.
- [x] `tests/test_lua_runner.py` imports `gc`, `sys` and `warnings`, and has
      `TestLuaRunnerLifecycle.test_failed_construction_is_silent_when_collected` exactly as above.
- [x] That test failed before the source change and passes after it.
- [x] The full Python suite passes. `ruff format` and `ruff check` are clean.
- [x] `bindings/python/AGENTS.md` has the new `LuaRunner.__init__` gotcha bullet.
- [x] `CHANGELOG.md` `## [0.12.0] — unreleased` → `### Fixed` has the Python `LuaRunner` entry,
      not marked BREAKING.
- [x] No files outside `bindings/python/src/quiverdb/lua_runner.py`,
      `bindings/python/tests/test_lua_runner.py`, `bindings/python/AGENTS.md` and `CHANGELOG.md`
      changed.

## Pitfalls

- **Do not only move `self._closed = False` below `_ptr`.** Without the early
  `self._closed = True`, `_closed` does not exist on failure, and `__del__` still prints
  `AttributeError`, only on `_closed`.
- **Do not bind the exception: no `with pytest.raises(...) as exc_info:`.** `exc_info` holds the
  traceback, the traceback holds `__init__`'s frame, and that frame holds `self`. The object then
  stays alive through the live test frame, `gc.collect()` cannot free it, `__del__` never runs
  inside the recording block, and the test passes vacuously before the fix.
- **Keep `warnings.simplefilter("always")`.** Python's default filters include
  `ignore::ResourceWarning`, and pytest does not override it. Without the filter, the warning half
  of the test passes vacuously.
- **Keep the `sys.unraisablehook` monkeypatch.** pytest 9.1 (the version in `uv.lock`) routes
  exceptions from `__del__` to its own hook and reports them only as a
  `PytestUnraisableExceptionWarning` in the summary. That does not fail the test, so without the
  patch the `AttributeError` half is invisible.
- **Step 2 must fail before the fix.** If it passes, one of the three traps above is in the test.
  Fix the test before touching the source.
- The `match="Null argument: db"` string comes from `QUIVER_REQUIRE(db, out_runner)` in
  `quiver_lua_runner_new` (`src/c/lua_runner.cpp`). Plan 64 edits only that function's `catch(...)`
  branches, not the `QUIVER_REQUIRE` line, so the message is stable. If a later plan rewords
  `Null argument:` messages, update this `match` too.
- `.py` files are LF (`.gitattributes`). Edit with the Edit/Write tools, not a tool that writes
  CRLF.
- `bindings/python/format.bat` has no `pushd`. Run it from inside `bindings\python`.

## Out of scope

- `Element.__init__`'s identical ordering (`self._destroyed = False` after `check(...)`): only a
  `quiver_element_create` allocation failure can reach it. Plan 29 owns other `element.py`
  cleanup, and neither plan changes this line.
- Making `LuaRunner(closed_db)` raise `"Database has been closed"` via `db._ensure_open()`: this
  would be a binding-crafted duplicate of the C API's own `Null argument: db`. Not needed.
- Runner-outlives-database safety (a `LuaRunner` borrowing a `Database&` that was closed after the
  runner was built): documented in the root `AGENTS.md` scoped-resource caveat, and not changed
  here.
- Julia, Dart and JS `LuaRunner` constructors: already correct (see "Other bindings checked").

## Implementation notes

Implemented on `rs/plan26` on top of master `3cbdcd1`, which contains plans 24 (`ac3d70e`) and 25
(`ae57b68`). `git fetch && git merge origin/master` was a no-op. Before any edit, every quoted
excerpt, symbol, path and test name was re-checked against HEAD (all matched), and a read-only
adversarial reviewer checked the test design (no vacuous pass, no spurious failure). After the
edits, a three-lens refutation workflow (test mechanics, fix completeness, docs accuracy) reviewed
the diff and returned no findings. Code, test, AGENTS.md bullet and CHANGELOG text are exactly as
this plan specifies.

### User decision

- **CHANGELOG section.** Where to file the entry was asked, since every Batch 4 session faces it.
  The answer: under `## [0.12.6] — unreleased`, not the stale `[0.12.5] — unreleased` header
  (`v0.12.5` is tagged at `7c8bf7a`; the manifests are 0.12.6).

### Drift fixed

- **CHANGELOG**:
  - `## [0.12.0] — unreleased` does not exist; 0.12.0 was released on 2026-09-27.
  - The plan's anchor, the Julia `Artifacts.toml` entry, now sits inside the released `[0.11.0]`
    section.
  - Plans 24/25 had already opened `## [0.12.6] — unreleased` with `### Fixed`. The entry went,
    word for word, at the end of that `### Fixed` list, above `## [0.12.5]`.
  - No manifest bump: 0.12.6 is already a patch bump, which fits a non-breaking fix.
  - The `[0.12.5]` header still says "unreleased". Dating it is the maintainer's release step.
- **Repo path**: the plan says `quiver1`; this checkout is `quiver3`.
- **pytest**: `uv.lock` pins 9.1.1, not 9.1. Nothing depends on the difference.
- **Line numbers**: the `bindings/python/AGENTS.md` anchor bullets moved (plans 24/25) to L99 /
  L104. The bullet was placed by content, between the same two bullets.
- **Pitfalls addition**: keep the *first* `gc.collect()` outside the recording block. pytest 9.1.1
  never runs gc between tests (only at session cleanup), so without it an earlier test's cyclic
  garbage could be finalized inside the block.

### Results

- **Red** (test written, `lua_runner.py` untouched):
  - `test.bat -k failed_construction -v` gave **1 failed**:
    `AssertionError: assert ['LuaRunner w...d explicitly'] == []`
    (`Left contains one more item: 'LuaRunner was not closed explicitly'`).
  - The first assert stops the test, so a standalone probe checked the second half. The patched
    `sys.unraisablehook` received `AttributeError("'LuaRunner' object has no attribute '_ptr'")`.
  - The reviewer also ran the half-fix this plan warns against (only moving `_closed = False`
    down). The warning list is then empty, and the second assert fails on
    `AttributeError("... no attribute '_closed'")`. Both asserts are therefore load-bearing.
- **Green**:
  - The targeted run passes 1/1, and the probe prints `warnings: []` / `unraisable: []`.
  - The full Python suite passes 345/345.
  - `scripts\test-all.bat`: C++ 1375, C API 571, Julia, JS 230 and Python 345 PASS.
  - Dart first failed 26/440 (`Failed to execute statement: expected 1 bound parameter(s) but got
    0`). The cause was this checkout's stale native-assets cache: `.dart_tool/.../libquiver_c.dll`
    was built at 13:37, before plan 22 (15:31) changed the query C API. Clearing
    `.dart_tool/hooks_runner/` and `.dart_tool/lib/`, as `bindings/dart/AGENTS.md` prescribes,
    gave 440/440. Nothing in this change touches Dart.
- **Format and lint**:
  - `bindings\python\format.bat`: ruff left 35 files unchanged.
  - `uv run ruff check` passes on both edited `.py` files. The whole-tree check reports one
    pre-existing `I001` in `tests/test_database_query.py`: an extra blank line after the imports,
    which dates from plan 22 or earlier. It was left alone as out of scope.
  - `scripts\format.bat` made no content change. Biome again rewrote all 42 JS files from CRLF to
    LF with no content diff; they were restored with `git checkout -- bindings/js`.
- **Environment notes**:
  - The first `test.bat` after the 0.12.6 manifest bump spent several minutes rebuilding the
    editable `quiverdb` (scikit-build-core compiles the core in Release). This happens only once.
  - `cmd` here does not search the current directory, so run `.\format.bat`, not `format.bat`.

### For later plans

- **27–30 (Python)**: append to `## [0.12.6] — unreleased`. This change touched only
  `LuaRunner.__init__` and `TestLuaRunnerLifecycle`. `test_lua_runner.py` now imports `gc`, `sys`
  and `warnings`.
- **29 (dead code)**: `Element.__init__` still sets `_destroyed = False` after `check(...)`, the
  same ordering fixed here. It was left alone as out of scope, as this plan says.
- **Verifying Python fixes by hand**: this machine's global uv CPython 3.13 has an editable
  `quiverdb` from `C:\Development\Quiver\quiver2`, through `_editable_skbc_quiverdb.pth`. Its
  meta-path finder beats `PYTHONPATH`, so `uv run --no-project --with pytest ...` silently imports
  quiver2's code. Use `bindings\python\tests\test.bat` or the project `.venv`.
- **Dart**: in a checkout whose Dart cache predates plan 22, clear `.dart_tool/hooks_runner/` and
  `.dart_tool/lib/` before trusting a Dart failure.
- **Changelog policy (for the maintainer, not fixed here)**: the `[0.12.6]` section holds plan
  24's BREAKING entry under a patch-level version. The root AGENTS.md rule says a `0.x` minor bump
  signals breaking changes.
