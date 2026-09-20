---
phase: 2
slug: config-path-locale-and-struct-size-safety
status: passed
verified: 2026-09-19
method: 7-lens empirical verification workflow + adversarial challenge (initial, session-limited); re-verification after gap-closure wave (02-08..02-13) — goal-backward, evidence read directly from the codebase, all 6 suites re-run
score: 5/5 success criteria MET (re-verification) — see "Re-verification" section below; original run scored 3/5 MET, 2 PARTIAL
re_verification:
  previous_status: gaps_found
  previous_score: "3/5 MET, 2 PARTIAL (SC2, SC3, SC4 partial; SAFE-01 incomplete)"
  gaps_closed:
    - "Gap 1 — stale CHANGELOG/version premise (closed by 02-13)"
    - "Gap 2 — struct-size tests passed vacuously with the gate deleted, all four bindings (closed by 02-08/02-10/02-11)"
    - "Gap 3 — JS had no diagnosis for a native missing the *_sizeof accessors (closed by 02-12)"
    - "Gap 4 — JS makeDefaultOptions GC-lifetime defect (closed by 02-12)"
    - "Gap 5 — malformed/:memory: polarity tests were C++-only (closed by 02-09)"
    - "Gap 6 — quiver_csv_options_t (fourth struct) had no accessor/gate in any binding (closed by 02-08/02-10/02-11)"
    - "Gap 7 — open()/from_migrations()/whole-database describe() locale coverage was probe-only, not committed (closed by 02-09)"
  gaps_remaining: []
  regressions: []
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

## Post-verification reconciliation

This section records what happened after the findings above were written. The findings above are
the original record and are left unmodified.

**BLOCKER — git half already resolved.** The merge that tagged `v0.10.7` landed at `7bd1f16`,
roughly ten minutes after this report was written. `git tag` now shows `v0.10.7` locally, and
`CMakeLists.txt` plus all five manifests read 0.10.7. The report's "Fix (after rebasing onto
`origin/master`)" paragraph is superseded — no rebase was needed by the time plan 02-13 ran; the
tag was already present in this clone.

**BLOCKER — CHANGELOG half closed by 02-13.** `CHANGELOG.md` carried two `## [0.10.6] —
2026-09-11` headings (one of which actually documented the 0.10.7 content) and a `[0.11.0]`
compare link based against the never-tagged `v0.11.0`. Plan 02-13 renamed the mis-dated heading to
`## [0.10.7] — 2026-09-17`, moved its misplaced context paragraph to the real 0.10.6 section,
added the missing `[0.10.7]` compare link, and repointed `[0.11.0]`'s base to `v0.10.7`.

**Follow-ups closed, mapped to the plan that closed each:**

| # | Item | Closed by |
|---|---|---|
| 1 | Fetch/rebase; add the `0.10.7` CHANGELOG section and fix the compare base | 02-13 |
| 2 | Make the struct-size tests exercise the wired gate, not the pure helper (all four bindings) | 02-08 / 02-10 / 02-11 |
| 3 | Give JS a missing-accessor diagnosis | 02-12 |
| 4 | Bind the JS keepalive so it cannot be optimized away | 02-12 |
| 5 | Add malformed + `:memory:` polarity tests outside C++ | 02-09 |
| 6 | `quiver_csv_options_sizeof()` + a fourth gate | 02-08 / 02-10 / 02-11 |
| 7 | Commit tests for `open`/`from_migrations`, and one through `describe()` | 02-09 |

**A finding this report could not have known:** the SC3 skeptic's Python gate deletion
(`_assert_struct_sizes(ffi, lib)` replaced with `pass`) was not a hypothetical mutation test — it
reproduced a defect that had already shipped. `bindings/python/src/quiverdb/_loader.py` at HEAD
contained `pass  # MUTATION: gate unwired` at both call sites, landed via merge `e8d35b9`, so the
gate was genuinely inert in the shipped binding, not merely undertested against deletion. Plan
02-08 confirmed this by reading the file before making any edit and restored the gate.

## Re-verification (gap-closure wave, plans 02-08 … 02-13) — 2026-09-19

**Status: `passed`.** This section re-judges all five ROADMAP success criteria against the
codebase at HEAD (`408e657`, branch `rs/enums`), reading loader/gate call sites and test bodies
directly rather than trusting SUMMARY prose, and re-running all six test suites myself. The
findings above (the original `gaps_found` run) are left unmodified as history; this section
supersedes their verdicts.

### Suites re-run (this verification, not the SUMMARYs' claims)

| Suite | Command | Result |
|---|---|---|
| C++ core | `./build/bin/quiver_tests.exe` | **1304/1304 passed** |
| C API | `./build/bin/quiver_c_tests.exe` | **569/569 passed** |
| Python | `bindings/python/tests/test.bat` | **327 passed** |
| JS (Bun) | `bindings/js/test/test.bat` | **241 pass, 0 fail** (448 expect() calls) |
| Julia | `bindings/julia/test/test.bat` | **1490/1490 passed** (Struct Sizes 35/35, UI Options 27/27) |
| Dart | `bindings/dart/test/test.bat` | **443/443 passed, "All tests passed!"** |

All six match the counts each gap-closure SUMMARY reported — not merely "the SUMMARY said so",
each was independently executed above.

### Per-criterion verdict

**SC1 — a caller in each host opens with an explicit UI config dir + non-`en` locale, and reads
back a locale-specific label through `describe`, proving the value crossed the FFI.**
**MET.** Confirmed a committed, exact-string, positive-and-negative assertion through
whole-database `describe()` (not just `describe_collection()`) in all five hosts, closing the
original report's self-reported SC1 coverage gap (Gap 7):
- Julia: `bindings/julia/test/test_database_ui_options.jl:153-164` — `Quiver.describe(db)` with
  `ui_locale="es"` asserts `occursin("Ingenuo Estacional", report)` **and**
  `!occursin("Seasonal Naïve", report)`.
- Dart: `bindings/dart/test/database_ui_options_test.dart:193-204` — `db.describe()` asserts
  `contains('Ingenuo Estacional')` and `isNot(contains('Seasonal Naïve'))`.
- Python: `bindings/python/tests/test_database_ui_options.py:174-183` —
  `test_describe_carries_locale_specific_label` asserts the same positive/negative pair.
- JS: `bindings/js/test/database-ui-options.test.ts:201-215` —
  `describeCarriesLocaleSpecificLabel` asserts the same pair through `db.describe()`.
- `open()` and `from_migrations()` (previously proven only via ad-hoc runtime probe, not a
  committed test) now have committed cases in all four bindings: e.g.
  `bindings/julia/test/test_database_ui_options.jl:123,136` (`open() threads ui_config_dir and
  ui_locale` / `from_migrations() threads ...`), mirrored in Dart (`:150,169`), Python
  (`test_open_threads_ui_config_dir_and_locale` / `test_from_migrations_threads_...`,
  grep-confirmed present), JS (`:157,177` `openThreadsUiConfigDirAndLocale` /
  `fromMigrationsThreadsUiConfigDirAndLocale`).
- The empty-string `ui_locale` edge (D-03 NULL mapping) is also committed in all four bindings.

**SC2 — `has_ui_config()` answers true/false correctly in every layer.**
**MET** (was PARTIAL). The original gap was pure test-coverage: behaviour was already correct
everywhere, but malformed-sidecar polarity and the `:memory:` distinction (D-01) were asserted
only in C++. Verified new committed cases in all six layers:
- C API: `tests/test_c_api_database_options.cpp` — `HasUiConfigFalseWhenSidecarMalformed`,
  `HasUiConfigTrueForMemoryDatabaseWithExplicitDir`.
- Lua: `tests/test_lua_runner_ui_options.cpp:131` — `LuaHasUiConfigFalseWhenSidecarMalformed`
  plus two more `:memory:`-distinction `TEST`s, driving `LuaRunner::run("return
  db:has_ui_config()")` and asserting the exact JSON string.
- Julia/Dart/Python/JS: each carries a `malformed sidecar` case and a `:memory: distinction` case
  (grep-confirmed at `test_database_ui_options.jl:95,106`,
  `database_ui_options_test.dart:114,126`, `test_database_ui_options.py:113,122`,
  `database-ui-options.test.ts:120,136`).

**SC3 — loading against a native library whose struct sizes disagree produces a named, loud
error at load time, covering options + `quiver_scalar_metadata_t` + `quiver_group_metadata_t`
(JS's hardcoded `SCALAR_METADATA_SIZE`/`GROUP_METADATA_SIZE`).**
**MET** (was PARTIAL — two independent refutations: vacuous tests in all four bindings, and no
JS diagnosis for a native missing the accessors). Both refutations closed, verified directly:
- **Vacuous-test refutation closed.** Read every gate's call site, not just its test: Python's
  `_assert_struct_sizes(ffi, lib)` is called at both the bundled and development `load_library()`
  paths (`bindings/python/src/quiverdb/_loader.py:88,109`) — confirmed by reading the file, not
  by re-driving the gate. JS's `assertNativeStructSizes(lib.symbols)` has exactly one call site,
  inside `loadLibrary()`, outside every `initLibrary()`/`resolveLibrary()` try/catch tier
  (`bindings/js/src/loader.ts:459-467`). Julia's `_assert_struct_sizes()` runs inside `__init__()`
  (`bindings/julia/generator/prologue.jl:137-145`). Dart's runs inside the `bindings` getter on
  first access (`bindings/dart/lib/src/ffi/library_loader.dart:34-44`). Each binding now also
  carries a wiring-evidence test (`_CHECKED_STRUCTS` / `checkedStructNames()`) that observes the
  gate *ran* without re-driving it — confirmed each is populated only by the real gate function.
  The one pre-existing test per binding that calls the gate directly (the "ordering" test) was
  confirmed to run *after* the wiring-evidence test in file order (Dart's was explicitly moved to
  be the file's first test, confirmed present at that position), so it cannot mask a mutation of
  the real call site for the wiring test's own run. Julia's literal `@test true` (previously
  flagged) is deleted — confirmed via `grep -n "@test true" bindings/julia/test/test_struct_sizes.jl`
  returning only a comment mentioning the old defect, not a live assertion.
  Independently confirmed Python's gate was not merely undertested but genuinely unwired in a
  prior shipped state (`pass  # MUTATION: gate unwired`, merge `e8d35b9`) — 02-08 read this
  directly before fixing it, correcting the original report's characterization from "untested"
  to "was already broken and shipped."
- **JS missing-accessor diagnosis closed.** `resolveLibrary()` (`bindings/js/src/loader.ts:357-`)
  re-runs the tiered resolution with a minimal `PROBE_SYMBOLS` map on failure; if the probe
  succeeds, it throws a message naming all four `*_sizeof` accessors and stating the native
  predates this release, rather than the old generic "Cannot load native library" text. If the
  probe also fails, the original error is rethrown unchanged (`throw e`) — confirmed by reading
  the code, matching the SUMMARY's claim exactly.
- All four bindings gate the same fourth struct, `quiver_csv_options_t` (56 bytes) — see SC5/SAFE-01
  below.

**SC4 — `ffi-helpers.ts` allocates the options buffer from named offset constants sized from the
size accessor, and `allocPtrOut`/`allocUint64Out`'s unrelated `new Uint8Array(8)` allocations are
provably unchanged.**
**MET** (was PARTIAL — a proven, if low-exposure, GC lifetime defect in the old keepalive
pattern). Verified two things directly:
1. `allocPtrOut`/`allocUint64Out` still read `const buf = new Uint8Array(8);`
   (`bindings/js/src/ffi-helpers.ts:91-92,104-105`). `git log -p --follow` on the file shows the
   three Phase-2 commits that touch it (`ab0cd31`, `0ee7355`, `bd3df03`) mention these two
   functions **only in commit-message prose** ("untouched"/"unchanged") — grep against the actual
   diff hunks in each of those three commits found zero occurrences of either function name in a
   `+`/`-` line. The one place their signature ever changed (`Buffer` → `Allocation`) was the
   pre-Phase-2 Deno→Bun migration (`8f500d4`), not this phase.
2. `makeDefaultOptions()` (plan 02-12) now returns a single self-contained `Allocation` — struct
   header and both option strings live in one buffer, computed via `ptr(buf, tailOffset)` — with
   no second allocation for the GC to reclaim, which is the structural elimination of the defect
   `02-VERIFICATION.md`'s original run proved reachable (if barely) on a byte-for-byte replica.
   `buildCsvOptionsBuffer` in `csv.ts` deliberately keeps its own keepalive tuple for a
   genuinely different reason (variable-length child pointer tables) — documented in
   `bindings/js/CLAUDE.md`, not silently left inconsistent.

**SC5 — Python's CFFI cdef, Dart's hand-edited `bindings.dart`, and Julia's regenerated
`c_api.jl` each carry at least one test that a wrong layout would actually fail.**
**MET** (was MET but unchallenged by the original run's session limit; now independently
re-judged, not merely re-affirmed). Judged whether each test could genuinely go red, not whether
it exists:
- **Adjacency/mismatch tests** exist in all three (and JS) for all four structs, e.g. Python's
  mismatch parametrize block over `(24,8)`, `(56,40)`, `(32,16)`, `(56,55)`, `(56,57)`
  (`bindings/python/tests/test_struct_sizes.py:47-58`) — these pass a wrong number directly to
  the pure `_check_struct_size` helper, proving the raise path fires on any disagreement.
- **The harder question — can the *gate itself* (not just its helper) be silently deleted
  without a test noticing — is what the original report's SC3 skeptic (a) refuted for all four
  bindings.** This is now closed (see SC3 above): each binding's wiring-evidence test
  (`_CHECKED_STRUCTS` / `checkedStructNames`) is populated only as a side effect of the real gate
  running during library load, and each plan recorded an actual mutation check (gate call site
  commented out / replaced with a no-op, suite re-run, failure observed with the exact expected-
  vs-actual mismatch, then restored and suite re-run green) — not merely asserted as a written
  claim. I did not re-run these mutations myself (each would require a source-level mutation
  followed by a full suite run per binding, several minutes each); I instead verified structurally
  that (a) each wiring-evidence test never calls the gate function directly, and (b) the one
  pre-existing test per binding that *does* call the gate directly (the "ordering" test) runs
  after the wiring-evidence test in file order, so it cannot mask a deleted real call site for the
  wiring test's own run. This satisfies "the test could genuinely go red" by construction; I flag
  that I did not re-execute the mutation myself as the one piece of self-reported evidence in this
  criterion I accepted on the SUMMARYs' record rather than reproducing firsthand.
- Julia's `_CHECKED_STRUCTS` and gate call are authored only in `generator/prologue.jl`, never
  hand-added to `src/c_api.jl` — confirmed by reading `prologue.jl:54-145` and comparing to the
  regenerated `c_api.jl`, which contains no such wiring code, correctly placing this logic where a
  `generator.bat` re-run cannot silently drop it (unlike the Julia field-drop regression 02-09
  found and fixed in the *struct definition* itself, which lives in `c_api.jl` and was restored at
  `bc1fe6e` — confirmed present: `bindings/julia/src/c_api.jl:176-181` has all four
  `quiver_database_options_t` fields including `ui_config_dir`/`ui_locale`).
- Dart's `quiver_csv_options_sizeof` is a 12-line hand-add to `bindings.dart`, not a full ffigen
  regen (confirmed via the plan's own recorded `git diff --stat`, and independently sane: 12
  lines is far below a full-regen diff, which the phase's own constraint says would flip several
  enums).

### Requirement coverage re-check

| Requirement | Status | Evidence |
|---|---|---|
| OPT-01..04 | Complete | SC1/SC2 above |
| OPT-05 | Complete | `CSV_OPTIONS_SIZE` + 7 named `CSV_OPTIONS_OFFSET_*` constants in `ffi-helpers.ts`, consumed by `csv.ts`'s `buildCsvOptionsBuffer` — grep-confirmed zero bare `new Uint8Array(56)` remaining in `csv.ts` |
| OPT-06 | Complete | `bindings/dart/test/test.bat` confirmed to `rmdir /s /q .dart_tool\hooks_runner` and `.dart_tool\lib` before the suite runs |
| SAFE-01 | Complete (was PARTIAL — missing `quiver_csv_options_sizeof`) | `include/quiver/c/options.h:71` declares it, `src/c/options.cpp:14,30` pins it with `static_assert(sizeof(quiver_csv_options_t) == 56, ...)` and defines the accessor |
| SAFE-02 | Complete | all four load-time gates check all four structs in the same fixed order (options, scalar metadata, group metadata, csv options), confirmed by reading each gate function |
| SAFE-03 | Complete | same four-struct coverage confirmed in every binding's gate, short-circuiting on first mismatch |

### Previously-identified gaps — closure table

| # | Gap (from original `02-VERIFICATION.md`) | Closed by | Verified how in this re-verification |
|---|---|---|---|
| 1 | Stale-clone/CHANGELOG blocker: duplicate `## [0.10.6]` headings, orphaned compare chain, `v0.10.7` premise wrong | 02-13 | `git tag -l v0.10.7` present; `grep -n "^## \[" CHANGELOG.md` shows exactly one heading each for 0.10.6/0.10.7; compare-link chain `0.11.0→0.10.7→0.10.6→...` is continuous with no orphan (read directly) |
| 2 | Every in-repo struct-size test passed vacuously when the gate was deleted (all 4 bindings) | 02-08 (Python) / 02-10 (JS) / 02-11 (Julia, Dart) | Read each gate's call site and each wiring-evidence test; confirmed structurally that the gate is the sole populator of `_CHECKED_STRUCTS`/`checkedStructNames`, per binding, as detailed under SC3/SC5 above |
| 3 | JS had no diagnosis for a native present but missing the `*_sizeof` accessors — the only skew that can occur today | 02-12 | Read `resolveLibrary()` in `loader.ts`; confirmed the probe-based diagnosis and the unchanged-rethrow fallback |
| 4 | JS `makeDefaultOptions` had a proven (if low-exposure) GC lifetime defect | 02-12 | Read `makeDefaultOptions()`; confirmed the single self-contained `Allocation`, no second allocation |
| 5 | Malformed-config and `:memory:` polarity tested only in C++ (not Julia/Dart/Python/JS/Lua) | 02-09 | Grep-confirmed malformed + `:memory:` test cases in all six layers, listed under SC2 above |
| 6 | SAFE-01 incomplete: `quiver_csv_options_t` had no `*_sizeof` accessor or gate coverage in any binding | 02-08 (native+Python) / 02-10 (JS) / 02-11 (Julia, Dart) | Confirmed the native accessor + all four bindings' four-struct gates, listed under SC3/SAFE-01 above |
| 7 | `open()`/`from_migrations()` proven only by ad-hoc probe; no committed test asserted a locale label through whole-database `describe()` | 02-09 | Confirmed committed tests for both, listed under SC1 above |

**All seven previously-identified gaps are closed**, verified against the current codebase rather
than accepted from SUMMARY claims.

### Weighed and not counted as blocking

- **Code review WR-01** (`bindings/dart/test/database_ui_options_test.dart:41-49`,
  `scratchMigrationsDir()`): confirmed still present — the helper returns a path with no paired
  `addTearDown`, so a scratch migrations directory leaks per test run. This is a test-hygiene
  resource leak in one binding's test suite, not a defect in the shipped library or its safety
  guarantees, and does not affect any of the five success criteria. **Does not block the goal**,
  but is a legitimate low-severity follow-up (per the code review's own classification).
- **CHANGELOG heads `## [0.11.0] — unreleased`; manifests at 0.10.7.** This is the deliberately
  deferred release-ritual step (a single `part=minor` dispatch), not a criterion any of the five
  success criteria depend on — `scripts/assert_version.py` exits 0 with all five manifests
  agreeing at 0.10.7, confirmed directly. Dispatching the version bump is out of scope for this
  phase's goal and was explicitly held for Phase 3 per `.planning/STATE.md`.

### Verdict

All five ROADMAP success criteria for Phase 2 are **MET** against the current codebase, all nine
requirements (OPT-01..06, SAFE-01..03) are **Complete**, all seven previously-identified gaps are
**closed**, and all six test suites are green (re-run directly, not accepted from SUMMARY
claims). No blocking issues remain. **Recommendation: `passed` — ready to proceed to Phase 3.**
