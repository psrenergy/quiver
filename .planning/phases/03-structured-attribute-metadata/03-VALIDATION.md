---
phase: 3
slug: structured-attribute-metadata
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-09-19
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Derived from `03-RESEARCH.md` §Validation Architecture. All six META requirements start at
> **zero** coverage: no getter exists, so nothing can test one. The one existing asset is
> `lua-api-sync.test.ts`, which enforces META-06 by itself and needs no new file.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (C++ / C API); Test.jl, Dart `test` 1.31.2+, pytest 8.4.1+, `bun:test` per binding |
| **Config file** | `tests/CMakeLists.txt` for C++/C API; each binding's existing `test.bat`. **None is new.** |
| **Quick run command** | `./build/bin/quiver_tests.exe --gtest_filter='*UiMetadata*'` |
| **Full suite command** | `scripts/test-all.bat` |
| **Estimated runtime** | quick ~5 s; full suite ~4–6 min (Dart pays a native rebuild per run since Phase 2's D-15 cache clearing) |

**CMake note (RESEARCH finding, corrects 03-CONTEXT D-43):** `src/CMakeLists.txt` uses
hand-maintained explicit source lists for both the `quiver` and `quiver_c` targets — no glob. A new
`.cpp` must be added to **both** lists by hand or it silently never compiles and every test for it
fails with a link error, not a missing-file error. Wave 0 must verify the new file actually builds.

---

## Sampling Rate

- **After every task commit:** the single relevant suite's quick filter — `quiver_tests.exe
  --gtest_filter='*UiMetadata*'`, or the one binding's own `test.bat`.
- **After every plan wave:** `scripts/test-all.bat` — all six suites plus the CLI smoke test.
- **Before `/gsd-verify-work`:** full suite green, **plus** the four struct-size gate tests updated
  to assert **five** structs, not four (`struct-sizes.test.ts`, `test_struct_sizes.jl`,
  `test_struct_sizes.py`, `struct_sizes_test.dart`). A gate test still reading `4` is the single
  highest-signal indicator that the fifth struct was added to the loader but not to its gate.
- **Max feedback latency:** 10 s (quick), ~360 s (full suite incl. the Dart rebuild).

---

## Per-Task Verification Map

Task IDs are assigned by the planner. Every row is ❌ W0 — none of this surface exists.
Threat refs point at the `<threat_model>` blocks the planner must write (security enforcement is
active, ASVS L1, block on `high`).

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | 1 | META-01 | — | Public record carries all 8 fields + vocabulary name; moved type is byte-compatible with the private one it replaces | unit (C++) | `quiver_tests.exe --gtest_filter='DatabaseUiMetadata.*'` | ❌ W0 — new `tests/test_database_ui_metadata.cpp` | ⬜ pending |
| TBD | TBD | 1 | META-02 | — | Unconfigured **real** column → default-constructed record, **not** a throw (D-36) | unit (C++) | same filter | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | META-02 | **T-03-01** | Unknown **column** → Pattern 2 throw naming collection + attribute (D-36). Both polarities asserted; a test that only checks the happy path cannot distinguish D-36 from its rejected alternative | unit (C++) | same filter | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | META-05 | — | `list_ui_vocabularies()` returns map-order (= alphabetical, `std::map`); `get_ui_vocabulary(name)` returns ordered `{code,label}` entries | unit (C++) | same filter | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | META-05 | T-03-01 | Unknown **vocabulary name** → Pattern 2 throw | unit (C++) | same filter | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 | META-01 | — | **Declared-but-empty vocabulary** returns `[]`, does **not** throw — the confirmed corpus gap below | unit (C++) | same filter | ❌ W0 — **fixture missing** | ⬜ pending |
| TBD | TBD | 2 | META-03 | **T-03-02** | `sizeof(quiver_ui_metadata_t) == 64`, hole-free, pinned by `static_assert`; **and** `quiver_scalar_metadata_t`/`quiver_group_metadata_t` byte-diff proves they are untouched (criterion 4, D-16) | unit (C API) | `quiver_c_tests.exe --gtest_filter='CApiDatabaseUiMetadata.*'` | ❌ W0 — new `tests/test_c_api_database_ui_metadata.cpp` | ⬜ pending |
| TBD | TBD | 2 | META-03 | T-03-02 | `quiver_ui_metadata_sizeof()` returns the native value; the round trip through the C getter preserves every field | unit (C API) | same filter | ❌ W0 | ⬜ pending |
| TBD | TBD | 2 | META-03 | **T-03-03** | The dedicated `quiver_database_free_ui_vocabulary` frees **both** arrays; double-free and free-of-empty are safe (D-37) | unit (C API) | same filter | ❌ W0 | ⬜ pending |
| TBD | TBD | 3 | META-04, SAFE-02 | T-03-02 | **JS**: new struct decoded from named offset constants; fifth entry in the load-time gate; gate test asserts **five** | unit (JS) | `bun test test/struct-sizes.test.ts test/ui-metadata.test.ts` | ❌ W0 | ⬜ pending |
| TBD | TBD | 3 | META-04, SAFE-02 | T-03-02 | **Julia**: regenerated `c_api.jl` carries the struct; fifth gate entry; `GC.@preserve` around the ccall | unit (Julia) | `bindings/julia/test/test.bat` | ❌ W0 | ⬜ pending |
| TBD | TBD | 3 | META-04, SAFE-02 | T-03-02 | **Dart**: hand-edited `bindings.dart` (no ffigen regen); fifth gate entry; wiring-evidence test **runs first** in its file (the Phase 2 masking bug) | unit (Dart) | `bindings/dart/test/test.bat` | ❌ W0 | ⬜ pending |
| TBD | TBD | 3 | META-04, SAFE-02 | T-03-02 | **Python**: hand-edited CFFI cdef; fifth gate entry; `ffi.sizeof` assertion | unit (Python) | `bindings/python/tests/test.bat` | ❌ W0 | ⬜ pending |
| TBD | TBD | 3 | META-04 | — | **Per binding**: unconfigured → default record; unknown column → that binding's surfaced Pattern 2 message. Both polarities, all four | unit ×4 | per-binding | ❌ W0 | ⬜ pending |
| TBD | TBD | 1 or 3 | META-04 | — | **Lua**: the three `db:` methods; empty field arrives as `nil` not `""` (D-38); vocabulary is a 1-based array of `{code,label}` tables (D-39). **Not gated by the C freeze** — LuaRunner calls C++ directly | unit (C++/Lua) | `quiver_tests.exe --gtest_filter='*LuaRunnerUiMetadata*'` | ❌ W0 | ⬜ pending |
| TBD | TBD | 4 | META-06 | — | `lua-api-sync.test.ts` green after all three `db:` tokens land | automated | `bun test test/lua-api-sync.test.ts` | ✅ **exists** | ⬜ pending |
| TBD | TBD | 4 | — | — | `describe`/`describe_collection`/`summarize_collection` output **byte-identical** — this phase adds getters, never touches rendering | regression | `quiver_tests.exe --gtest_filter='*Describe*'` | ✅ exists | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_database_ui_metadata.cpp` — META-01/02/05 at the C++ layer. **Add to
      `tests/CMakeLists.txt`.**
- [ ] `tests/test_c_api_database_ui_metadata.cpp` — META-03: the 64-byte layout proof and the
      struct-independence byte-diff. **Add to `tests/CMakeLists.txt`.**
- [ ] `tests/test_lua_runner_ui_metadata.cpp` — the Lua surface (D-38 nil semantics, D-39 array
      shape). **Add to `tests/CMakeLists.txt`.**
- [ ] One new test file or section per binding (Julia, Dart, Python, JS) — META-04.
- [ ] **A new fixture for the one confirmed corpus gap** (below).
- [ ] **Verify the new `src/*.cpp` is in BOTH `src/CMakeLists.txt` source lists** before any test
      that depends on it is written — otherwise every failure reads as a link error.

### The one confirmed corpus gap

No fixture exercises a vocabulary **declared with a name but zero entries** (`my_vocab = []` in
`enum.toml`). This was verified against the parser, not assumed: `UIConfigSet::parse_enum_content`
(`src/ui_config.cpp:149-180`) maps a zero-length TOML array to `vocabularies[name] = {}` — a
**present key holding an empty vector**, so `find_vocabulary` returns a non-null pointer to an
empty vector.

The existing `empty_enum` fixture is the **zero-byte-file** case (`tests/schemas/ui/README.md:29`)
— an absent map entirely, which throws Pattern 2 identically to an undeclared name. The two states
are structurally different and only one is covered.

Fix either way: a tiny new tracked fixture, or a scratch-directory TOML string following Phase 1's
`ScratchSidecarDir` pattern for edge cases not worth tracking. `get_ui_vocabulary("empty_but_declared")`
must return `[]`, not throw.

No other gap found — configured-vs-not, empty-string label vs absent, and bare-string vocabulary
are all already covered by Phase 1's `enum_basic`, `htd_like` and `bess_like`.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| claw prompt-budget headroom | META-06 | Lives in another repo; no Quiver CI job can see it | After the `lua-api.ts` edit, check the added character count against the ~5.6k headroom recorded in `03-CONTEXT.md` (`C:/Development/Claw/claw1/test/prompt.test.ts:38-46`). Report the number; do not edit claw. |

Everything else has automated verification.

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or a Wave 0 dependency
- [ ] Sampling continuity: no 3 consecutive tasks without an automated verify
- [ ] Wave 0 covers all ❌ references, including the declared-but-empty-vocabulary fixture
- [ ] Both polarities asserted wherever D-36 chose throwing over degrading — a happy-path-only
      test cannot tell the two designs apart
- [ ] All four struct-size gate tests assert **five** structs
- [ ] No watch-mode flags
- [ ] Feedback latency < 10 s quick / 360 s full
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
