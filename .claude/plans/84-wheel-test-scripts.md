# 84 — Wheel test scripts: fail on a failed install; run the validator through `uv`

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** tooling (`scripts/test-wheel-install.bat`, `scripts/test-wheel.bat`)
**Depends on** none · **Overlaps with** none

## Why

1. **`scripts/test-wheel-install.bat` reports success after a failed install.** Step 3 (~L66-72):
   ```bat
   uv pip install --python "%VENV_PYTHON%" "!WHEEL_FILE!"
   if !errorlevel! neq 0 (
       echo FAIL: wheel install failed
       goto :cleanup
   )
   ```
   It jumps to `:cleanup` **without** setting `EXIT_CODE=1`. The cleanup block (~L110-124) then
   prints `SUCCESS: Wheel install validation passed` because `!EXIT_CODE! equ 0`, and the script
   exits 0.
2. **`scripts/test-wheel.bat` needs a dev venv that `uv build` never creates.** It sets
   `VENV_PYTHON=%PYTHON_DIR%\.venv\Scripts\python.exe` (~L12) and runs the validator with it
   (~L57): `"%VENV_PYTHON%" "%SCRIPT_DIR%validate_wheel.py" "!WHEEL_FILE!"`. In a fresh checkout
   there is no `bindings/python/.venv`. The root AGENTS.md says to run Python through `uv run`
   locally, because plain `python` is not on PATH.

## Constraints and decisions

- `.bat` files are CRLF. Keep them CRLF.
- `validate_wheel.py` only inspects a wheel file (a zip). It needs no project environment, so
  `uv run --no-project python` is right.

## Changes

1. `scripts/test-wheel-install.bat` ~L70: inside the failure block, before `goto :cleanup`, add
   `set "EXIT_CODE=1"`. Check the other failure branches in the same file set `EXIT_CODE=1` before
   jumping to `:cleanup` (`grep -n "goto :cleanup" -B3 scripts/test-wheel-install.bat`), and fix any
   that don't.
2. `scripts/test-wheel.bat`:
   - ~L57: replace `"%VENV_PYTHON%" "%SCRIPT_DIR%validate_wheel.py" "!WHEEL_FILE!"` with
     `uv run --no-project python "%SCRIPT_DIR%validate_wheel.py" "!WHEEL_FILE!"`.
   - Delete the `set "VENV_PYTHON=..."` line (~L12) if nothing else in the file uses it
     (`grep -n "VENV_PYTHON" scripts/test-wheel.bat`).
   - If `validate_wheel.py` imports a third-party module, add `--with <module>`. Check its imports
     first.

## Tests

Run both scripts. That run is the test.

## Verification

1. `scripts\test-wheel.bat` in a checkout with no `bindings\python\.venv`: it validates the wheel.
2. Force an install failure in `test-wheel-install.bat`, e.g. temporarily point `WHEEL_FILE` at a
   nonexistent path. It must print the FAIL banner and exit non-zero (`echo %errorlevel%`). Revert
   the experiment.
3. `git diff --stat` shows a few changed lines per file (so CRLF is intact).

## Acceptance criteria

- [ ] A failed wheel install makes `test-wheel-install.bat` exit non-zero, with no SUCCESS banner.
- [ ] `test-wheel.bat` works without a dev venv.

## Pitfalls

- Inside `setlocal enabledelayedexpansion` blocks, use `!EXIT_CODE!`, as the file already does.

## Out of scope

- CI wheel validation (`validate_wheel*.py` in the cibuildwheel flow).
