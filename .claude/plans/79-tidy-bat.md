# 79 — `tidy.bat`: filters that don't depend on the checkout's directory name; find the runner on PATH

**Batch** 7 · **Severity** medium (`scripts/tidy.bat` silently lints zero files) · **Breaking** no · **Size** S · **Layers** tooling (`scripts/tidy.bat`, `.clang-tidy`, root `CMakeLists.txt`, root AGENTS.md)
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

- The `.bat` is CRLF. Keep it CRLF (root AGENTS.md caution).
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
4. Root `AGENTS.md`, Code Style Tooling bullet
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

- [x] `tidy.bat` lints the `src/` sources (except `src/binary`) in any checkout directory.
- [x] No hardcoded LLVM path. The header filter lives only in `.clang-tidy` and excludes `_deps`.
- [x] The CMake `tidy` target is gone, and root AGENTS.md is updated.

## Pitfalls

- Do not run tidy's `--fix`. Fixing existing lint is out of scope.
- A bare `%` inside a `for /f` in a `.bat` must be `%%`.

## Out of scope

- Fixing the warnings tidy now reports.
- CI integration of tidy.

## Implementation notes

- **Done as planned.** All four changes landed as written: the `where` lookup, the file regex,
  `.clang-tidy`'s `HeaderFilterRegex` + `ExcludeHeaderFilterRegex: '_deps'`, the deleted CMake
  `tidy` block, the "format target" comment, and the AGENTS.md bullet. Branch integrated master at
  `3447a34` (plan 74) first, as a fast-forward.
- **Runner:** `where run-clang-tidy` returns one hit,
  `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\run-clang-tidy`.
  It is an extensionless Python script, not a `.exe` wrapper, so the `uv run python` invocation
  stays.
- **Verification (real run):** after reconfiguring, `scripts\tidy.bat` printed
  `Running clang-tidy in 14 threads for 53 files out of 187 in compilation database ...` and
  exited 0. The 53 files are src, src/c, src/c/binary, src/c/expression, src/cli, src/csv and
  src/expression; none from src/binary, tests or build/_deps. It reported warnings only, about 890
  of them (mostly identifier-naming, exception-escape and use-using), all pre-existing and out of
  scope. Not one diagnostic came from a `_deps` path. With the LLVM dir removed from PATH it
  printed `Error: run-clang-tidy not found on PATH` and exited 1. `file scripts/tidy.bat` still
  reports CRLF. `cmake --build build` succeeded. `scripts/format.bat` exited 0 with no content
  diff. Biome again rewrote 43 JS files from CRLF to LF, and `git checkout -- bindings/js`
  restored them (same as plan 70).
- **Adversarial review:** three lenses (batch semantics, regex/config, scope/docs) checked the
  following and found it fine:
  - the first `where` match wins, and a `RUN_CLANG_TIDY` preset by the caller is cleared;
  - paths with spaces and parentheses work;
  - the regex survives cmd, uv and Python argv parsing intact, and the exit code propagates;
  - `clang-tidy --dump-config` parses both keys, and llvm::Regex's `[\/]` accepts both separators;
  - nothing else references the deleted target.

  No CHANGELOG entry: this is dev tooling only.
- **Known ceiling (accepted, not fixed):** the file regex matches any path segment named `src`,
  so an ancestor directory of that name counts too. A checkout at `C:\src\quiver` or `~/src/quiver`
  selects 132 files instead of 53, adding all tests and src/binary. That fails loudly, by linting
  more. A checkout under a directory named `_deps` selects 0 files. Anchoring the regex to the
  `re.escape`d `%ROOT%` would close both, but it would make any path alias (a `subst` drive, a
  junction, an 8.3 short name, drive-letter case) silently lint 0 files, which is the bug class
  this plan fixes. Revisit only if a `~/src` checkout becomes the norm.
- **Pre-existing, untouched:** the compile db is MSVC `cl.exe`, so the MinGW
  `-fno-keep-inline-dllexport` strip is a no-op on this machine. Headers in src/binary still
  report when another src file includes them, as they did before.
- **For plan 86:** the CMake `tidy` block is gone, and the glob comment already reads
  `# Source files for format target`.
