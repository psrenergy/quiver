---
phase: 2
slug: config-path-locale-and-struct-size-safety
status: gaps_found
verified: 2026-09-19
method: 7-lens empirical verification workflow + adversarial challenge (partial — session limit)
score: 3/5 success criteria MET, 2 PARTIAL; 1 external blocker
---

# Phase 2 Verification — Config Path, Locale and Struct-Size Safety

**Status: `gaps_found`.** The implementation is sound and all six suites are green
(C++ 1175, C API 567, JS 231, Python 318, Julia 1465, Dart 434; code review clean, 0 critical /
0 warning). The gaps are in **proof coverage** and one **external blocker discovered during
verification**, not in behaviour — every behavioural question below was settled by running
something, none was punted to human judgement.

## Method and its limits

Seven verification lenses ran empirically (each required to prove by running commands, not by
reading SUMMARY claims), each then attacked by three adversarial skeptics from distinct angles
(vacuous-test hunt / skipped-layer hunt / try-to-break-it), plus a completeness critic.

**The run was truncated by a session limit: 11 of 20 agents completed, 9 failed.** Specifically,
all three challenges to SC1 and SC5, two of three to REQ-coverage, and the completeness critic
never ran. So SC1 and SC5's MET verdicts are **unchallenged**, not confirmed-under-attack. That is
a known limit of this verification, not a claim of completeness.

## Verdicts

| Criterion | Verdict | Challenged? |
|---|---|---|
| SC1 — locale crosses the FFI in all five hosts | **MET** | ✗ not challenged (session limit) |
| SC2 — `has_ui_config()` answers in every layer | **PARTIAL** | — |
| SC3 — loud load-time failure on struct skew | **PARTIAL** (claimed MET; 2 of 3 skeptics refuted) | ✓ |
| SC4 — JS named offsets + lifetime | **PARTIAL** | — |
| SC5 — per-binding layout tests | **MET** | ✗ not challenged (session limit) |
| Requirement coverage (OPT-01…06, SAFE-01…03) | **PARTIAL** (SAFE-01 incomplete) | ✓ (1 of 3) |
| Release scope | **PARTIAL** — external blocker | — |

## BLOCKER — the repository is working from a stale clone, and both phases built on a false premise

Verified directly by the orchestrator:

```
git rev-parse master        -> e598fb4   (v0.10.6)
git rev-parse origin/master -> e598fb4   (stale local ref)
git cat-file -e 7bd1f16     -> fatal: Not a valid object name
git ls-remote --tags origin -> 7bd1f16  refs/tags/v0.10.7
```

**`v0.10.7` is tagged on the remote and is not in this clone.** The verification agent additionally
reports it was merged to master via PR #290 and published to npm and PyPI at 2026-09-17T18:45Z —
roughly one hour *before* this milestone's first commit.

Every Phase 1 and Phase 2 artifact repeats the premise "the never-tagged 0.10.7" / "no `v0.10.7`
tag exists and none should" (e.g. `02-07-PLAN.md:20`, `02-07-SUMMARY.md:11,90,131`). That premise
is false against the remote.

Consequences:
1. `CHANGELOG.md` has **no section for the released 0.10.7** (the Lua CSV feature from #290).
2. The compare-link chain orphans a shipped version: `[0.11.0]: v0.10.6...v0.11.0` jumps straight
   to `[0.10.6]: v0.10.3...v0.10.6`.
3. As written, the 0.11.0 compare range will silently absorb #290's commit into the Phase 1+2 entry.

**Not at risk:** the target version number. `scripts/assert_version.py:43` computes a minor bump as
`{major}.{minor+1}.0`, so one `part=minor` dispatch yields 0.11.0 from either 0.10.6 or 0.10.7.

**Fix (after rebasing onto `origin/master`):** change the 0.11.0 compare base to `v0.10.7`, and add
a `## [0.10.7] — 2026-09-17` section plus its `v0.10.6...v0.10.7` link for PR #290.

This is out of scope for an autonomous agent to resolve — it requires a fetch/rebase decision.

## SC3 — PARTIAL: two independent refutations

**(a) Every in-repo struct-size test passes vacuously.** A skeptic deleted the gate itself and the
suites stayed green: Julia with `_assert_struct_sizes()` removed from `__init__` still reported
`Struct Sizes | 23 pass`; JS with `assertNativeStructSizes`'s body replaced by `void lib;` still
reported `9 pass`; Python with `_assert_struct_sizes` no-op'd still reported `5 passed`. The tests
drive the pure helper, not the wired gate. (`bindings/julia/test/test_struct_sizes.jl:18` even
contains a literal `@test true`.)

**(b) JS has no gate for the missing-accessor case, and that is the only skew that can occur
today.** A native library predating Phase 2 has the full Phase 1 surface but none of the three
`*_sizeof` exports — and Phase 2 is what introduces them, so no already-published native has them.
Python, Dart and Julia each turn that into a version-skew diagnosis. JS cannot: Bun's `dlopen`
resolves every declared symbol eagerly and throws when one is absent, and `openLibrary()` is only
called from inside `initLibrary()`'s three `try/catch` tiers, so the load dies *before*
`assertNativeStructSizes` is reached and surfaces as `Cannot load native library 'libquiver_c.dll'.
Searched: …` — no struct name, no byte count, and it points the user at "install the native
library" rather than "your native library is stale". Proven end-to-end against a byte-copy of the
live DLL with the three exports removed.

**Counter-evidence worth keeping:** the third skeptic could *not* refute the size-mismatch half. It
drifted each binding's real layout source of truth in an isolated scratch copy and drove the public
entry point against the real DLL — all 12 cases (4 bindings × 3 structs) produced a named, loud
error carrying both numbers, none masked. So the *mismatch* path is genuinely solid; the *absent
accessor* path in JS and the *vacuous tests* are the gaps.

## SC4 — PARTIAL: a real but low-exposure JS lifetime defect

The keepalive pattern exists and every call site binds it, but the verifier proved a genuine defect
on a byte-for-byte replica of `makeDefaultOptions`'s body with `Bun.gc(true)` placed inside the
window, with the keepalive read/unread as the single isolated variable. It could **not** force the
failure through the unmodified public API: 3000 iterations of `Database.fromSchema` with
`uiConfigDir` set plus per-iteration garbage allocation produced 0 failures, and the same under
`bun --smol` produced 0 failures.

So: defect proven, current exposure proven low, **not** provably unreachable on other Bun versions,
other platforms, or after any future edit that adds an allocation between `makeDefaultOptions` and
the FFI call.

Noted and not counted against the verdict: `OPTIONS_SIZE` is a pinned literal `24` validated against
the accessor at library load rather than the allocation reading the accessor directly. `getSymbols()`
(which runs the assertion) provably precedes `makeDefaultOptions` at all three call sites, so no
options buffer can be allocated before the accessor has confirmed 24.

## SC2 — PARTIAL: behaviour correct everywhere, polarity coverage thin

All behaviour was verified correct by probe in every layer. The missing piece is tests:

1. **Malformed-config polarity is tested only in C++.** The C API, Julia, Dart, Python, JS and Lua
   suites each test true + absent-directory, and nothing malformed.
2. **The `:memory:` distinction** (explicit dir loads, convention does not — D-01) is asserted only
   in C++ (`DatabaseUiOptions.ExplicitConfigDirLoadsEvenForMemoryDatabase`). No binding, C API or
   Lua test asserts it.
3. **Lua** has no test asserting `db:has_ui_config() == false` for a malformed directory, nor for
   `:memory:`.

## Requirement coverage — SAFE-01 is PARTIAL

SAFE-01 reads "size accessors … for **each** FFI struct a binding allocates a buffer for". There are
**four** such structs, not three. `quiver_csv_options_t` is allocated as a raw
`new Uint8Array(56)` by `bindings/js/src/csv.ts:24` with seven bare-literal offsets — the exact
inline-literal style OPT-05 exists to abolish — and its layout is separately hand-maintained in
Python's cdef, Julia's `c_api.jl` and Dart's `bindings.dart`. No `quiver_csv_options_sizeof()`
exists, no `static_assert` pins it, no test asserts its size, and all four load-time gates check
exactly three structs.

This was a **deliberate, documented deferral** (`bindings/js/CLAUDE.md:94-97` states it outright),
so it is not an accident — but the milestone's stated failure class remains live for the *larger* of
the two JS hand-allocated structs.

## SC1 and SC5 — MET, unchallenged

SC1's evidence is strong: a runtime negative control through `quiver_cli` showing
`enum model {2: Tendencia Lineal Local, …, 5: Ingenuo Estacional, …}` under `--ui-locale es` versus
`{2: Local Linear Trend, …, 5: Seasonal Naïve, …}` without it, with the bare-string labels
byte-identical across both runs — proving the locale changes *only* enum resolution. All five hosts
assert both the Spanish strings present and `Seasonal Naïve` absent.

Two coverage gaps it self-reported: committed binding tests exercise only `from_schema` with the new
parameters (`open` and `from_migrations` proven by ad-hoc runtime probe, not by a committed test),
and no committed test asserts a locale-specific label through whole-DB `describe()` (only
`describe_collection()`).

## Recommended follow-ups

| # | Item | Severity |
|---|---|---|
| 1 | Fetch/rebase onto `origin/master`; add the `0.10.7` CHANGELOG section and fix the compare base | **blocker — needs a human decision** |
| 2 | Make the struct-size tests exercise the wired gate, not the pure helper (all four bindings) | high |
| 3 | Give JS a missing-accessor diagnosis (probe the three symbols before `dlopen` declares them, or catch and re-throw with a version-skew message) | high |
| 4 | Bind the JS keepalive so it cannot be optimized away; or read `OPTIONS_SIZE` from the accessor at allocation | medium |
| 5 | Add malformed + `:memory:` polarity tests outside C++ | medium |
| 6 | `quiver_csv_options_sizeof()` + a fourth gate, or re-affirm the deferral explicitly in REQUIREMENTS | medium |
| 7 | Commit tests for `open`/`from_migrations` with the new parameters, and one through `describe()` | low |
