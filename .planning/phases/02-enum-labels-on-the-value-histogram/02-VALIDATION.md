---
phase: 2
slug: enum-labels-on-the-value-histogram
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
status: validated
nyquist_compliant: true
wave_0_complete: false
created: 2026-09-20
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest v1.17.0 (already a build dependency) |
| **Config file** | `tests/CMakeLists.txt` (target `quiver_tests`) — no new file, no new registration |
| **Quick run command** | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*:*UiMetadata*` |
| **Full suite command** | `build/bin/quiver_tests.exe` |
| **Estimated runtime** | ~35 seconds full; ~2 seconds filtered |

Wave 0 is **not** required: `tests/test_database_ui_metadata.cpp` and its `UiTempTreeFixture`
already exist from Phase 1 and are registered. This phase extends that file.

---

## Sampling Rate

- **After every task commit:** `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*:*UiMetadata*`
- **After the wave:** `build/bin/quiver_tests.exe` **and** `build/bin/quiver_c_tests.exe`
- **Before `/gsd-verify-work`:** full `quiver_tests` green (baseline 1270 passing)
- **Max feedback latency:** 35 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------------|-----------|-------------------|-------------|--------|
| 2-01-01 | 01 | 1 | RENDER-02 | Labelled entry renders `code SP "Label": count` (D2-01/D2-02), escaped via D-02/D-03 (D2-03) | integration | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Histogram*` | ✅ file exists | ✅ green |
| 2-01-02 | 01 | 1 | RENDER-02 (SC-2) | An observed code absent from the vocabulary stays bare and byte-identical to today (D2-05) | integration | same filter — asserted in the same string | ✅ | ✅ green |
| 2-01-03 | 01 | 1 | D2-07 | A vocabulary code with zero observed rows is not printed | integration | same test, `EXPECT_FALSE` | ✅ | ✅ green |
| 2-01-04 | 01 | 1 | SAFE-02 (regression) | `expect_reports_match` stays green and **unmodified** | regression | `build/bin/quiver_tests.exe --gtest_filter=*DatabaseUiMetadata*Malformed*:*NoUiDir*` | ✅ | ✅ green |
| 2-01-05 | 01 | 1 | SC-3 (vacuous) | The four `test_database_lifecycle.cpp` describe assertions pass **unmodified** | regression | `build/bin/quiver_tests.exe --gtest_filter=*DescribeVectorsHeaderPrintedOnce*:*DescribeSetsHeaderPrintedOnce*:*DescribeTimeSeriesWithDimensionColumn*:*DescribeNoCategoryHeaderWhenEmpty*` | ✅ | ✅ green |
| 2-01-06 | 01 | 1 | D2-14 | `CHANGELOG.md` no longer claims summarize does not render this; root `CLAUDE.md` bullet updated | doc assertion | `grep -c "does not yet render this metadata" CHANGELOG.md` returns 0 | ✅ | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

**Note on 2-01-05:** this row is a regression anchor, not evidence. Per D2-09 those four assertions
are `from_schema(":memory:")` + `db.describe()` and cannot be reached by a summarize-only change —
they will pass, but they prove nothing about this phase. Do not cite them as injection safety.

---

## Edge Coverage (spec-less probe fallback)

No phase SPEC exists, so the probe ran over RENDER-02 and surfaced 4 applicable edges, all
`unresolved` pending planner resolution. Resolutions expected from CONTEXT.md:

| Category | Probe | Disposition |
|----------|-------|-------------|
| `empty` | Result for empty / single-element / null input | **split, deliberately** — D2-05 (no vocabulary, empty vocabulary or null `UiAttribute*` → bare, byte-identical) and the existing zero-distinct-codes guard are **explicit**, asserted by the new test. D2-06 (label normalizing to empty → annotation dropped, entry kept) is **backstop**: CONTEXT.md `<specifics>` caps this phase's test at one `EXPECT_TRUE` plus one `EXPECT_FALSE`, so nothing asserts it and the guarantee rests on the `if (!text.empty())` guard. Narrowed from a blanket "explicit" during planning rather than overclaiming coverage. |
| `encoding` | Bytes vs code points vs normalized form | **explicit** — D2-03 reuses `normalize_ui_text` (C0 + 0x7F → space, UTF-8 bytes untranscoded) and `quote_ui_text` verbatim; `squash()` is NOT used here (D2-08) |
| `ordering` | Is output order specified and stable when elements compare equal | **explicit** — D2-04: the existing `ORDER BY <col>` is ascending code; codes are unique per entry, so no tie exists |
| `adjacency` | Exactly-equal or touching things — merge, collide, or separate | **backstop** — a label whose text is the code's own digits (`1 "1": 5`) renders, per D2-08; quoting keeps it unambiguous |

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. No Wave 0.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Rendering against a real PSR study corpus | RENDER-02 | Corpora live outside this repo and cannot be committed as fixtures | Open a real study with `from_migrations`, call `summarize_collection`, confirm histogram entries carry their labels (noting the recorded `HasCommitment` inversion renders verbatim by design) |

All other phase behaviors have automated verification.

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or an existing-infrastructure reference
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] No watch-mode flags
- [x] Feedback latency < 35s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** validated 2026-09-20 — all 6 map rows green; one manual-only item carried forward (real PSR study corpus).

---

## Validation Audit 2026-09-20

| Metric | Count |
|--------|-------|
| Gaps found | 1 |
| Resolved | 1 |
| Escalated | 0 |

The single gap was the `empty` edge's **backstop** disposition: D2-06 (a label that normalizes to
empty drops the annotation but keeps the histogram entry) was implemented but unasserted, so the
guarantee rested on the `if (!text.empty())` guard alone. It was found independently by the code
review as WR-01 and closed by `1fa2f23`, which added
`DatabaseUiMetadataTest.SummarizeHistogramKeepsEntryWhenLabelNormalizesToEmpty` — the reviewer then
re-traced the test through `parse_vocabularies` to confirm it reaches the D-09 branch rather than
the look-alike "vocabulary entry absent" branch. The `empty` edge is therefore **explicit**, not
backstop, as of this audit; no `gsd-nyquist-auditor` run was needed.

All 6 Per-Task Verification Map rows re-run green at `1fa2f23`:
`quiver_tests.exe` 1272/1272, `quiver_c_tests.exe` 557/557.

The Manual-Only row (rendering against a real PSR study corpus) is unchanged and remains
manual-only — corpora live outside this repo.
