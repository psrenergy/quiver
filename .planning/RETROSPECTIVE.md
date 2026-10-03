# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v0.12.9 — Quiver Lua Runner Refactor

**Shipped:** 2026-10-03 (ships in release 0.13.0)
**Phases:** 5 | **Plans:** 17 | **Sessions:** several over 2 days (2026-10-02 → 2026-10-03)

### What Was Built
- Mutation-tested behaviour pins: move survival with a `sizeof` static_assert, check orders, the key-width cap and sync guards
- `src/lua_runner.cpp` (2,539 lines) split into `src/lua_runner/` (11 files, each 446 lines or fewer), proven behaviour-neutral
- One copy of every repeated pattern (M1, M3–M16), so each later fix landed in one place
- Release type safety: Pattern 1 errors for wrong-type table arguments, plus the `SOL_ALL_SAFETIES_ON` backstop; text-only `load`/`run()`; transaction and empty-array fixes
- Direct `SandboxedPathTest`, zero planning-ID comments, and AGENTS.md, Lua reference and CHANGELOG all matching the code

### What Worked
- **Strict phase order (pin → split → dedupe → fix → docs).** Each phase protected the next. The split and dedupe were proven neutral against pins and byte-identical Release golden output.
- **Mutation-testing the pins.** A move pin that only kept the source alive passed a "run state outside Impl" mutant. It was caught in Phase 1, before the split could rely on it.
- **Adversarial self-verification at gates** instead of rubber-stamp approval. It found real defects: weak pins (Phase 1), a stale baseline (Phase 4), and a dangling "catalogue" numbering (Phase 5).
- **Code review then fix then re-verify at every phase.** It caught a critical issue: `run()` still executed bytecode after `load` was made text-only (04-REVIEW CR-01).
- **Recording perf budgets with a fallback before measuring.** `SOL_ALL_SAFETIES_ON` cost +16.4%, and the pre-agreed `SOL_SAFE_GETTER=0` / `SOL_SAFE_STACK_CHECK=0` fallback landed without re-litigation.

### What Was Inefficient
- **sol2 parse cost.** Splitting into per-domain TUs raised CPU time across the Lua files from 61 s to 223 s, though wall time fell. It was accepted, but it ruled out adding more sol2 TUs.
- **Worktree isolation repeatedly degraded.** `rs/runner` was ahead of `origin/HEAD`, so the isolation sentinel had to be re-forced to `none` before each executor dispatch.
- **Baseline counts drifted** (441 planned → 444 → 472 → 477) and had to be re-recorded at several points. Pin the count at a commit, and never as a planned number.
- **SUMMARY frontmatter was inconsistent.** Phases 2 and 5 omitted `requirements-completed`, and the milestone CLI pulled deviation lines as "accomplishments", so both had to be repaired by hand.

### Patterns Established
- A golden-output harness (Debug and Release) as the proof for "zero behaviour change" phases
- Red-then-green with a CHANGELOG line per behaviour fix; BREAKING entries say what a script author must change
- Shared helpers in `src/lua_runner/internal.h`; every type error goes through `lua_type_error`
- Test-only access to hidden symbols by compiling the one sol2-free source into the test target, with nothing newly exported
- Comments name the pinning test, never a planning ID

### Key Lessons
1. A pin must fail on the mutation it exists to catch. Prove it with a mutant before relying on it.
2. A text-only gate has more than one entry point (`load` and `run()`). Review every path a value can take into the interpreter.
3. When removing a reference ID, check whether other comments depended on its numbering, and rewrite those too.
4. Keep the milestone id distinct from release tags. Here the GSD id `v0.12.9` collided with the existing release tag, so tagging was skipped.

### Cost Observations
- Model mix: Opus throughout (orchestrator and subagents inherited the model)
- Sessions: several (one per phase or two, with `/clear` between)
- Notable: Phase 5 ran three plans sequentially in about 75 min of executor time; the review-fix-reverify loop took about 15 min.

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v0.12.9 | several | 5 | Adversarial self-verification replaced human approval prompts at GSD gates |

### Cumulative Quality

| Milestone | Tests | Coverage | Zero-Dep Additions |
|-----------|-------|----------|-------------------|
| v0.12.9 | quiver_tests 1454 (Lua* 444 → 477, +11 SandboxedPath), quiver_c_tests 543 | not measured | 0 (no new dependencies) |

### Top Lessons (Verified Across Milestones)

1. (Needs a second milestone to cross-validate.)
