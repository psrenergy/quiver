# 87 — `.pre-commit-config.yaml`: drop `cmake-format`, and keep `.bat` files CRLF

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** tooling config, root AGENTS.md
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
   Working-tree `.bat` files are CRLF on purpose (root AGENTS.md: "working-tree `.bat` files are
   CRLF — unix tools ... can break them"). This hook would convert every staged `.bat` to LF.

## Constraints and decisions

- **Maintainer decision (binding):** keep the file. Exclude `\.bat$` (plus `tests/fixtures/`) from
  `mixed-line-ending`. Remove `cmake-format` from the root AGENTS.md "Code Style Tooling" bullet.
- The narrower `exclude` is chosen over dropping `--fix=lf` altogether, because it keeps LF
  enforcement for every other text file.

## Changes

1. `.pre-commit-config.yaml`:
   - Delete the whole `cheshirekow/cmake-format-precommit` repo block (the last five lines).
   - `mixed-line-ending`: change `exclude: ^tests/fixtures/` to `exclude: (^tests/fixtures/|\.bat$)`.
2. Root `AGENTS.md`, the `.pre-commit-config.yaml` bullet in "Code Style Tooling" (~L433):
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

- [x] No `cmake-format` hook, and `.bat` is excluded from `mixed-line-ending`.
- [x] The root AGENTS.md bullet matches.

## Pitfalls

- YAML regex in `exclude` is a Python regex. Keep it single-line and unquoted, as the existing
  entries are.

## Out of scope

- Running pre-commit in CI.

## Implementation notes

Branch `rs/plan87`, after a fast-forward merge of `origin/master` at `a0a5432` (plans 79 and 81
had landed; neither touches these lines).

**Done:** exactly the two Changes. In `.pre-commit-config.yaml`, `mixed-line-ending` now has
`exclude: (^tests/fixtures/|\.bat$)`, and the `cheshirekow/cmake-format-precommit` block is gone,
along with the blank line before it. The file now ends at `cppcheck`'s `files:` line. The root
AGENTS.md bullet says "LF line endings (except `.bat`, which stay CRLF), clang-format, cppcheck."

**Drift fixed:** the AGENTS.md bullet was at L496–497, not ~L433. Its text matched.

**Repro, before and after.** I ran both in a throwaway `git worktree` so the real tree was never
rewritten. `uv run --with pre-commit pre-commit run mixed-line-ending --files scripts/build-all.bat`:
- Before: `Failed`, "scripts/build-all.bat: fixed mixed line endings", and `git ls-files --eol`
  went from `w/crlf` to `w/lf`.
- After: `(no files to check) Skipped`. With `--all-files`, all 20 `.bat` stayed `w/crlf` (plus the
  one `w/none`, `bindings/dart/format.bat`).

`pre-commit validate-config` exited 0. The pyyaml `safe_load` passed. `scripts/format.bat` exited
0. As in plans 68 and 78, Biome rewrote the 43 JS files from CRLF to LF with no content diff
(`git diff --ignore-cr-at-eol` is empty), and I restored them with `git checkout -- bindings/js`.

**No CHANGELOG entry:** this is contributor tooling, not a user-visible change, and the plan asks
for none.

**For the maintainer (observation only, not acted on):** `--fix=lf` is not only "LF enforcement".
Under `* text=auto` with `core.autocrlf=true`, every text file that is not `eol=lf` (md, yaml, json,
ts, CMake, ...) is CRLF in a Windows working tree. The `--all-files` run "fixed" 249 such files,
the six CMake files included. Git sees no content diff, because the index is LF either way, so
this is working-tree churn plus a failed first commit attempt, not damage. `.bat` was the only
type where LF is harmful. The plan's Tests line ("must not modify any `.bat` or CMake file") holds
for content and for cmake-format, but not for the CMake files' working-tree EOL bytes. If that
churn matters, the fix is to drop `--fix=lf`, since the hook's default `auto` only fixes genuinely
mixed files. The plan rejected that on purpose, so I left it.
