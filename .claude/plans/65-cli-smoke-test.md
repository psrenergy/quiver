# 65 — Fix the CLI smoke test: move the deleted example script under `tests/`

**Batch** 7 · **Severity** high (`test-all.bat` always reports failure) · **Breaking** no · **Size** S · **Layers** tests, scripts, docs
**Depends on** none · **Overlaps with** 82 (`build-all.bat` will call `test-all.bat`, so it inherits this fix), 75 (edits `tests/CLAUDE.md` too)

## Why

Commit 4af1397 ("refactor: move standalone CSV reader/writer into src/csv/ (#295)") deleted
`example/example1.lua` and `example/example1.bat`. `scripts/test-all.bat`'s step 7 still runs the
deleted script, currently at ~L134 and ~L141:

```bat
    "%ROOT_DIR%\build\bin\quiver_cli.exe" --schema "%ROOT_DIR%\tests\schemas\valid\collections.sql" :memory: "%ROOT_DIR%\example\example1.lua"
    ...
    "%ROOT_DIR%\build\bin\quiver_cli.exe" --schema a.sql --migrations m :memory: "%ROOT_DIR%\example\example1.lua" >nul 2>&1
```

`src/cli/main.cpp` returns 1 when the script is missing (`Script file not found: ...`). So the
positive run always fails and `test-all.bat` always prints "Some tests FAILED!". CI never runs
`test-all.bat`, which is why nobody noticed. The docs still describe the folder: root CLAUDE.md
~L24 (`example/  # example1.lua + example1.bat — quiver_cli/Lua CRUD demo`), ~L356
(`quiver_cli.exe  # CLI entry point (see example/)`), and tests/CLAUDE.md ~L198
("positive run (`--schema` + example Lua script → exit 0)").

## Constraints and decisions

- **Maintainer decision:** move the script into `tests/`, do not restore `example/`. The deletion
  was intentional.
- The old script only uses API that is still bound (`create_element`, `read_element_ids`,
  `read_element_by_id`, `update_element`, `delete_element`), and it matches today's
  `tests/schemas/valid/collections.sql` (`some_integer`, `some_float`, `value_int`).
- `.bat` files in the working tree are CRLF. Edit `test-all.bat` with an editor that keeps CRLF,
  or restore the line endings afterwards (root CLAUDE.md caution). Check with
  `file scripts/test-all.bat` before and after.

## Changes

1. Recreate the script under `tests/`:
   ```bash
   mkdir -p tests/cli
   git show 4af1397^:example/example1.lua > tests/cli/smoke.lua
   ```
   Check that it has LF line endings, like other non-.bat text files (`.gitattributes` has
   `* text=auto`).
2. `scripts/test-all.bat` ~L134 and ~L141: replace `"%ROOT_DIR%\example\example1.lua"` with
   `"%ROOT_DIR%\tests\cli\smoke.lua"` in both lines. The negative run's script argument only has to
   be parseable, since it exits 2 before reading it. Point it at the same file anyway.
3. Root `CLAUDE.md`:
   - Delete the Repo Map line `example/                  # example1.lua + example1.bat — quiver_cli/Lua CRUD demo`.
   - `./build/bin/quiver_cli.exe        # CLI entry point (see example/)` becomes
     `./build/bin/quiver_cli.exe        # CLI entry point (smoke script: tests/cli/smoke.lua)`.
4. `tests/CLAUDE.md` ~L198: "positive run (`--schema` + example Lua script → exit 0)" becomes
   "positive run (`--schema` + `tests/cli/smoke.lua` → exit 0)". Add `cli/smoke.lua` to any
   directory listing of `tests/` in that file.

## Tests

The smoke test is the test. Run it and check it now passes.

## Docs and changelog

Docs: steps 3–4. No CHANGELOG entry (developer tooling).

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `build\bin\quiver_cli.exe --schema tests\schemas\valid\collections.sql :memory: tests\cli\smoke.lua`
   should exit 0 (`echo %errorlevel%`).
3. `scripts\test-all.bat` should end with all steps PASS. If a binding suite fails for an unrelated
   reason, at least step 7 must show PASS.
4. `git diff --stat scripts/test-all.bat` should show only the two changed lines. If every line
   changed, the CRLF endings were lost; restore them.

## Acceptance criteria

- [ ] `tests/cli/smoke.lua` exists, and step 7 of `test-all.bat` passes.
- [ ] No remaining reference to `example/` in CLAUDE.md files or scripts:
      `grep -rn "example1\|example/" CLAUDE.md tests/CLAUDE.md scripts/`.

## Pitfalls

- `.bat` CRLF, as noted above.
- `bindings/julia/example2.jl` is a different, unrelated example. Leave it.

## Out of scope

- Adding the smoke test to CI.
