# 87 — `.pre-commit-config.yaml`: drop `cmake-format`, and keep `.bat` files CRLF

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** tooling config, root CLAUDE.md
**Depends on** none · **Overlaps with** none

## Why

`.pre-commit-config.yaml` is maintained (#290 added the `tests/fixtures/` excludes), but two hooks
conflict with the repo's own conventions.

1. **`cmake-format`**:
   ```yaml
     - repo: https://github.com/cheshirekow/cmake-format-precommit
       rev: v0.6.13
       hooks:
         - id: cmake-format
           files: (CMakeLists\.txt|\.cmake)$
   ```
   It has never been applied. With its default config it would rewrite all six 4-space CMake files
   (`CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `cmake/*.cmake`), and there is
   no `.cmake-format` config file in the repo to pin a style.
2. **`mixed-line-ending` with `--fix=lf`**:
   ```yaml
         - id: mixed-line-ending
           args: [--fix=lf]
           exclude: ^tests/fixtures/
   ```
   Working-tree `.bat` files are CRLF on purpose (root CLAUDE.md: "working-tree `.bat` files are
   CRLF — unix tools ... can break them"). This hook would convert every staged `.bat` to LF.

## Constraints and decisions

- **Maintainer decision (binding):** keep the file. Exclude `\.bat$` (plus `tests/fixtures/`) from
  `mixed-line-ending`. Remove `cmake-format` from the root CLAUDE.md "Code Style Tooling" bullet.
- The narrower `exclude` is chosen over dropping `--fix=lf` altogether, because it keeps LF
  enforcement for every other text file.

## Changes

1. `.pre-commit-config.yaml`:
   - Delete the whole `cheshirekow/cmake-format-precommit` repo block (the last five lines).
   - `mixed-line-ending`: change `exclude: ^tests/fixtures/` to `exclude: (^tests/fixtures/|\.bat$)`.
2. Root `CLAUDE.md`, the `.pre-commit-config.yaml` bullet in "Code Style Tooling" (~L433):
   "... large files (>1 MB), LF line endings, clang-format, cppcheck, cmake-format." becomes
   "... large files (>1 MB), LF line endings (except `.bat`, which stay CRLF), clang-format,
   cppcheck."

## Tests

If `pre-commit` is installed, run `pre-commit run --all-files`. It must not modify any `.bat` or
CMake file. It may flag pre-existing whitespace issues elsewhere; do not fix those here.

## Verification

1. `uv run --with pyyaml python -c "import yaml; yaml.safe_load(open('.pre-commit-config.yaml'))"`
2. Optional: `uv run --with pre-commit pre-commit run mixed-line-ending --all-files`, then
   `git status` shows no `.bat` changes.

## Acceptance criteria

- [ ] No `cmake-format` hook, and `.bat` is excluded from `mixed-line-ending`.
- [ ] The root CLAUDE.md bullet matches.

## Pitfalls

- YAML regex in `exclude` is a Python regex. Keep it single-line and unquoted, as the existing
  entries are.

## Out of scope

- Running pre-commit in CI.
