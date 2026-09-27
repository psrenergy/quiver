# 79 — `tidy.bat`: filters that don't depend on the checkout's directory name; find the runner on PATH

**Batch** 7 · **Severity** medium (`scripts/tidy.bat` silently lints zero files) · **Breaking** no · **Size** S · **Layers** tooling (`scripts/tidy.bat`, `.clang-tidy`, root `CMakeLists.txt`, root CLAUDE.md)
**Depends on** none · **Overlaps with** 86 (edits other lines of the root `CMakeLists.txt`)

## Why

`scripts/tidy.bat` (last lines) filters by a path that contains the literal checkout name:
```bat
uv run python "%RUN_CLANG_TIDY%" -p "%BUILD%" -header-filter="(include/quiver/|quiver/src/)" -quiet "quiver[\\/]src[\\/](?!binary)"
```
The file regex `quiver[\\/]src[\\/]` only matches if the repo directory is named `quiver`. In
`C:\Development\Quiver\quiver1` it matches nothing, so the script reports success after linting
**0 files**. The header filter has the same `quiver/src/` bug. The runner path is hardcoded:
```bat
SET RUN_CLANG_TIDY=C:\Program Files\LLVM\bin\run-clang-tidy
```
That path does not exist on this machine. `run-clang-tidy` is on PATH via the Visual Studio LLVM
toolset. Also, `.clang-tidy` (~L22) has `HeaderFilterRegex: '(include/quiver/|src/)'`, which
`_deps/lua-src/src/...` also matches. The script's `-header-filter` overrides it, which hides that
the config is too broad.

The root `CMakeLists.txt` (~L118-126) defines a `tidy` custom target that runs the `.bat` (so it
fails on non-Windows hosts) and nothing uses it.

## Constraints and decisions

- The `.bat` is CRLF. Keep it CRLF (root CLAUDE.md caution).
- Put the header policy in `.clang-tidy`, in one place, so clangd and ad-hoc `clang-tidy` runs follow
  the same rule. clang-tidy 19+ supports `ExcludeHeaderFilterRegex`; the installed version is 22.x
  (`clang-tidy --version`).
- Windows paths reach clang-tidy with mixed separators (`...\include\quiver/x.h`), so the regexes
  accept both `\` and `/`.
- Delete the unused CMake `tidy` target. The script is the documented entry point.

## Changes

1. `.clang-tidy` (~L22): replace
   `HeaderFilterRegex: '(include/quiver/|src/)'` with
   ```yaml
   HeaderFilterRegex: '(include[\\/]quiver[\\/]|[\\/]src[\\/])'
   ExcludeHeaderFilterRegex: '_deps'
   ```
   Single-quoted YAML keeps the backslashes literal.
2. `scripts/tidy.bat`:
   - Replace `SET RUN_CLANG_TIDY=C:\Program Files\LLVM\bin\run-clang-tidy` and its `if not exist`
     block with a PATH lookup:
     ```bat
     SET RUN_CLANG_TIDY=
     for /f "delims=" %%i in ('where run-clang-tidy 2^>nul') do if not defined RUN_CLANG_TIDY SET "RUN_CLANG_TIDY=%%i"
     if not defined RUN_CLANG_TIDY (
         echo Error: run-clang-tidy not found on PATH
         exit /b 1
     )
     ```
     `where` may return `run-clang-tidy` or `run-clang-tidy.py`. If it returns a `.exe`/`.bat`
     wrapper instead of the Python script, run it directly rather than through `uv run python`. Check
     the match on this machine with `where run-clang-tidy`, and adjust the invocation line to fit.
   - Replace the invocation's filters: drop `-header-filter=...` (the config now owns it) and use a
     name-independent file regex:
     `uv run python "%RUN_CLANG_TIDY%" -p "%BUILD%" -quiet "^(?!.*[\\/]_deps[\\/]).*[\\/]src[\\/](?!binary)"`
     run-clang-tidy matches file regexes with Python `re`, so the lookaheads work.
3. Root `CMakeLists.txt` (~L118-126): delete the `find_program(CLANG_TIDY clang-tidy)` /
   `add_custom_target(tidy ...)` block and its comment. Also change the comment near ~L92 that says
   "format and tidy targets" to "format target" (`grep -n "tidy" CMakeLists.txt`).
4. Root `CLAUDE.md`, Code Style Tooling bullet
   "`scripts/tidy.bat` — `run-clang-tidy` over `build/compile_commands.json` (strips the MinGW-only
   `-fno-keep-inline-dllexport` flag first; skips `src/binary`)": append "; finds `run-clang-tidy` on
   PATH, and the header filter lives in `.clang-tidy`".

## Tests

Run it for real. That run is the test.

## Verification

1. `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`
   (regenerates `compile_commands.json`)
2. `scripts\tidy.bat`. It must report linting about 53 files out of about 187 (the review measured
   "53 files out of 187" with the corrected regex), not 0. Warnings in the output are fine; this
   plan does not fix lint debt.
3. `git diff scripts/tidy.bat` shows only the intended lines, and `file scripts/tidy.bat` still says
   CRLF.

## Acceptance criteria

- [ ] `tidy.bat` lints the `src/` sources (except `src/binary`) in any checkout directory.
- [ ] No hardcoded LLVM path. The header filter lives only in `.clang-tidy` and excludes `_deps`.
- [ ] The CMake `tidy` target is gone, and root CLAUDE.md is updated.

## Pitfalls

- Do not run tidy's `--fix`. Fixing existing lint is out of scope.
- A bare `%` inside a `for /f` in a `.bat` must be `%%`.

## Out of scope

- Fixing the warnings tidy now reports.
- CI integration of tidy.
