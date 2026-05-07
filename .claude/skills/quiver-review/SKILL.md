---
name: quiver-review
description: Critically review the Quiver codebase against its stated principles (humans-first readability, no defensive code, simple over abstract, delete unused, breaking changes OK) and architecture (C++ holds all logic, bindings are thin, tests exist in every layer), then apply the fixes. Use whenever the user asks to "review", "audit", "critique", "clean up", or "check" code in this repo, or asks "are there issues", "what should I improve", "is this code good", or wants a pre-PR sanity pass. The skill is deliberately critical and outputs "no issues found" when that is true — it refuses to invent suggestions to appear thorough.
---

# Quiver Code Review

A review-and-fix skill for the Quiver codebase. The output is either a small set of justified, actionable findings (each tied to a specific principle the project has stated) or an honest "nothing to flag." Padding a review with weak suggestions is worse than saying nothing.

This skill assumes the project context loaded from `CLAUDE.md` — the architecture (C++ core, C API, bindings, multi-layer tests), naming conventions, error patterns, and code style. Don't re-explain those; rely on them.

## The principles you are reviewing against

In priority order:

1. **Code is written for humans to read, not machines.**
2. **Clean code over defensive code** — assume internal callers obey contracts; don't double-check what the caller already validated.
3. **Simple solutions over complex abstractions.**
4. **Delete unused code — do not deprecate.**
5. **Breaking changes are acceptable.** WIP project, no backwards-compatibility burden.

A finding only earns its place in the review if you can name which principle it violates. "It would look nicer" is not a principle.

## The architecture you are checking compliance with

- **C++ core** (`src/`, `include/quiver/`) holds every business decision, every error message, every transformation.
- **C API** (`src/c/`, `include/quiver/c/`) is a marshaling layer: opaque-pointer unwrap, `QUIVER_REQUIRE` for null checks at the FFI boundary, exception → error-code translation, that's it. No business logic.
- **Bindings** (`bindings/{c,dart,julia,js,python}/`) are thin language wrappers over the C API. They convert types, call the C function, check the error code, convert results back. They never construct error messages, never branch on data, never reformat values.
- **Tests** must exist in all six layers (C++ core, C API, Dart, Julia, JS, Python) for each public method. Coverage via `lua_runner` tests does not count — that's transitive coverage of the C++ core, not of the binding's own marshaling.

## Workflow

### Step 1 — Establish scope

Always ask the user what to review. Don't assume. Offer these options (single-select):

- **Recent changes** — `git diff master...HEAD` plus untracked files. The pre-PR pass.
- **A specific subsystem** — e.g. `src/expression/`, `src/binary/`, `src/c/`, one binding directory.
- **Specific files** — they name them.
- **Whole codebase** — slow, comprehensive; only on request.

When the user picks, also ask whether they want fixes applied or just findings reported. The skill works in both modes.

### Step 2 — Read systematically

Don't rely on Grep snippets to draw conclusions. Read the actual files in scope. For findings about layer leakage or test parity, read across layers:

- For each public C++ method touched by the diff, read `src/database_*.cpp` (or wherever it lives), the matching `src/c/database_*.cpp`, and at least one binding's wrapper. The C API should be a few lines of marshaling. If the binding wrapper is doing more than the C API, you've found leakage.
- For test-parity, list each layer's test directory (`tests/`, `bindings/<lang>/test/`) and grep the symbol. Note which layers have a *dedicated* test (a file that calls the symbol explicitly) and which rely on transitive coverage.

### Step 3 — Apply principle checks

Each finding cites one of P1–P5.

| Principle | Flag | Don't flag |
|---|---|---|
| **P1 Readability** | cryptic names; misleading or redundant comments; magic numbers/strings without context; one function mixing levels of abstraction | naming preferences when the existing name is fine; missing comments where the code is self-explanatory (the project default is no comments — see CLAUDE.md) |
| **P2 Clean over defensive** | null checks inside internal call chains; try/catch that just rethrows; double-validation across layers; default values that mask bugs; "just in case" branches | input validation at API boundaries (`QUIVER_REQUIRE` at the C API surface, `validate_*` at the C++ public surface) — that's correct |
| **P3 Simple over abstract** | Pimpl on a class with no private deps; virtual hierarchies for plain value types; single-method classes; wrappers around wrappers; templates where free functions would do | complexity that's pulling its weight (Pimpl on `Database` hides sqlite3, Pimpl on `BinaryFile` hides iostream — both justified) |
| **P4 Delete unused** | orphan files; unused functions, parameters, members, headers; commented-out code; `// deprecated` markers; backwards-compat shims; helpers used only by tests that are themselves dead | code consumed by external bindings or tests — verify with grep across all layers before flagging |
| **P5 Breaking changes OK** | version-check shims; "for now" comments; deprecated paths kept around for callers that no longer exist | nothing — this principle generates findings, not non-findings |

### Step 4 — Apply architecture checks

- **Logic in C++**: anything in a binding beyond marshaling, error translation, and convenience composition (the documented binding-only convenience methods like `read_scalars_by_id` that compose multiple C API calls are fine — they're explicitly sanctioned in CLAUDE.md). Format conversions, conditional branches based on data, error-message construction in a binding — all leakage. Move the logic to C++ and surface it through C API.
- **Bindings stay thin**: a binding wrapper materially larger than its C API counterpart for the same operation is a smell. Cite both line counts in the finding.
- **Test parity**: every public C++ method should have a *dedicated* test in C++ core, C API, and each binding (Dart, Julia, JS, Python). A single `lua_runner` end-to-end test does not satisfy this — that test exercises the C++ core through Lua, not the binding's own marshaling. When you find a gap, list which layers have dedicated tests and which don't.

### Step 5 — Present findings

Use this exact structure. Predictable structure helps the user scan.

```
# Quiver Review: <scope>

## Verdict
<one sentence: "no issues found" OR "N findings across M files">

## Findings

### Finding 1: <short name>
- File: path/to/file.cpp:42-58
- Principle: P2 (clean over defensive)
- Issue: <2–3 sentences, concrete>
- Fix: <concrete change, with before/after if useful>
- Size: trivial | medium | large

### Finding 2: ...

## What I checked but didn't flag
<2–4 bullets — patterns you actively examined and decided were fine. Proves the review was real.>

## Out of scope
<what wasn't reviewed, and why>
```

If the verdict is "no issues found", **omit the Findings section entirely.** Do not pad. The "What I checked but didn't flag" section becomes the main body, demonstrating that the review actually happened.

### Step 6 — Apply fixes (only if the user asked for fixes)

Categorize each finding by size and act accordingly:

- **Trivial** — delete dead code, simplify a 2-line check, rename one symbol, remove a redundant `nullptr` check. Apply immediately.
- **Medium** — refactor a function, move 10–50 lines of logic from a binding into C++, replace a Pimpl with a value type when the class is small. Apply, but tell the user which files moved so they can `git diff` it cleanly.
- **Large** — write missing tests across 4 bindings, restructure a subsystem, convert Pimpl→value-type when many call sites are affected. **Do not apply inline.** Present the change as a planned phase and recommend the user run `/gsd-execute-phase` or a dedicated PR. A review session is not the right place for a multi-file refactor; doing it inline buries the diff and conflates concerns.

After applying changes:

1. Run the relevant tests from `CLAUDE.md`:
   - C++ core or C API touched → `scripts/build-all.bat` (or just rebuild and run `quiver_tests.exe` / `quiver_c_tests.exe`).
   - One binding touched → `bindings/<lang>/test/test.bat`.
   - Many layers touched → `scripts/test-all.bat`.
2. **Do not commit.** Leave changes staged or unstaged for the user to inspect with `git diff` and commit themselves. The project rule (CLAUDE.md) is to commit only when explicitly asked.
3. Report what was applied, what was deferred to a later phase, and any test failures with the actual error output.

## Don't pad the review

This is the part of the skill that does the most work. Read it before you write the verdict.

When you're tempted to write any of the following, skip it:

- *"Could be more flexible."* — Flexibility is a cost: more code to read, more shapes to learn, more places for bugs. Principle 3 prefers the simple form. Skip.
- *"Consider adding X for future use."* — Principle 5 says breaking changes are fine, so future-proofing isn't a virtue here. Skip.
- *"This works but might be cleaner if…"* — Only flag if a *stated* principle is actually violated. "Cleaner" by personal taste is not a principle.
- *"Has no comments."* — Only flag if the *why* is non-obvious. The project's comment policy (CLAUDE.md) is "default to no comments." A function without comments is the baseline, not a defect.
- *"This function is long."* — Length alone isn't a principle violation. If the function does one thing and is readable end-to-end, leave it.
- *"Tests could be more thorough."* — Only flag if a public method has no test in some layer. "More edge cases" is a feature request, not a review finding.
- *"This duplicates X."* — Only flag if the duplication is causing actual divergence or maintenance pain. Three similar lines is not an abstraction-worthy duplication (this is in the top-level Claude Code instructions, too).

A 3-finding review with real bite beats a 10-finding review of nitpicks. The user trusts you more if you say "no issues found" twice and then catch a real one the third time.

## Tone

Be direct. The user wants to know what's wrong, not be coddled. If the code is good, say so plainly. If something is bad, say what's bad and why, in one sentence. Avoid hedging language ("you might consider", "it could be argued"). The principles already do the hedging — every flag is justified by one.

## What this skill is not for

- Bug hunting — use `/gsd-debug` or a debugger.
- Performance profiling — different methodology, different tooling.
- Security audit — different methodology.
- Feature recommendations — out of scope.
- Style nits below the principles — see "Don't pad the review."

## A short example of what good output looks like

> # Quiver Review: src/expression/
>
> ## Verdict
> 2 findings across 2 files.
>
> ## Findings
>
> ### Finding 1: Redundant null check in `BinaryOpNode::compute_row`
> - File: `src/expression/nodes.cpp:142-146`
> - Principle: P2 (clean over defensive)
> - Issue: The function checks `lhs_ == nullptr` after the constructor has already validated both operands and stored them as `shared_ptr<Node>`. The check can never fire under any code path.
> - Fix: Delete lines 142–146.
> - Size: trivial
>
> ### Finding 2: `Expression::path()` accessor never called
> - File: `include/quiver/expression/expression.h:34`, `src/expression/expression.cpp:78-80`
> - Principle: P4 (delete unused)
> - Issue: No call sites in `src/`, `tests/`, or any binding. The constructor stores the path on `FileNode`, where it's already accessible.
> - Fix: Delete the declaration and definition.
> - Size: trivial
>
> ## What I checked but didn't flag
> - `BinaryOpNode`'s broadcast metadata pre-computation (`build_broadcast_metadata`) — looks like over-engineering at first, but the index translation tables (`lhs_to_out_`, `rhs_to_out_`) make `compute_row` a tight loop. Justified.
> - `FileNode::set_file`/`clear_file` injection during `save()` — looked at this for layer-leakage concerns, but the indirection is what enables single-handle sharing across duplicated paths in the AST. Right call.
> - Test parity for `Expression`: covered in `tests/test_expression.cpp` and indirectly via `tests/test_expression_save.cpp`. No bindings yet (per CLAUDE.md, expression bindings haven't been added), so binding-test gap is out of scope.
>
> ## Out of scope
> - `BinaryOpNode` performance — review skill, not perf review.
> - The plan to add Julia bindings for `Expression` — feature work, not review.
