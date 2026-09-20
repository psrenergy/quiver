---
phase: 02-enum-labels-on-the-value-histogram
reviewed: 2026-09-20T00:00:00Z
depth: standard
files_reviewed: 2
files_reviewed_list:
  - src/database_describe.cpp
  - tests/test_database_ui_metadata.cpp
findings:
  critical: 0
  warning: 0
  info: 2
  total: 2
status: clean
---

# Phase 02: Code Review Report

**Reviewed:** 2026-09-20T00:00:00Z
**Depth:** standard
**Files Reviewed:** 2
**Status:** clean

## Summary

Iteration 2. Since the prior review, commit `1fa2f23` added
`DatabaseUiMetadataTest.SummarizeHistogramKeepsEntryWhenLabelNormalizesToEmpty` to close WR-01
("D-09's empty-label-keeps-entry behavior has no test"). No non-test file changed in this
iteration — `git diff 6863f64..HEAD -- src/database_describe.cpp tests/test_database_ui_metadata.cpp`
shows `src/database_describe.cpp` unchanged since iteration 1's reviewed state; only the new test
was added to `tests/test_database_ui_metadata.cpp`.

**WR-01 verified closed, not vacuously.** Traced the new test's actual code path rather than
trusting the test's own comment:

- `write_ui_file("enum.toml", ...)` sets `label.en = "   "` for code 0 of the `initial_volume_type`
  vocabulary. `parse_vocabularies` (`src/ui_metadata.cpp`) stores whatever `read_localized` returns
  verbatim — there is no whitespace/emptiness filter at load time (only "no id" / "no readable
  label" entries are dropped). So `meta->enum_labels` genuinely contains `{0: "   "}` at render
  time; the lookup in `database_describe.cpp:284` (`meta->enum_labels.find(rows[i][0])`) hits
  `label_it != end()`, and it is `normalize_ui_text("   ")` collapsing to `""` (confirmed by
  hand-tracing `normalize_ui_text`: every byte is already `' '`, so `last_was_space` gates every
  push and the trim loop finds nothing left to trim) that then skips the `out << " " <<
  quote_ui_text(...)` annotation while `out << ": " << rows[i][1]` still runs unconditionally.
  This is exactly the D-09 branch — "label found, but normalizes to empty" — not the adjacent
  "label absent from vocabulary" branch, which would produce the same bare-code output for the
  wrong reason and make the test vacuous. It is not vacuous: the two branches are genuinely
  distinguished here because the vocabulary entry demonstrably exists in the loaded map.
- The assertion (`EXPECT_NE(report.find(R"(values {0: 1})"), std::string::npos)`) matches the
  code's actual emitted text for this scenario (one element, code 0, no annotation).

No new problems introduced: the change is additive, test-only, compiles as ordinary
`TEST_F` syntax consistent with the file's existing style, and does not touch
`src/database_describe.cpp` further.

**IN-01 and IN-02 carried forward from iteration 1, unescalated (both remain Info per this pass's
scope: re-assess but do not upgrade a deliberately-left Info):**

## Info

### IN-01: New test's negative assertion uses a different idiom than the rest of the file

**File:** `tests/test_database_ui_metadata.cpp:1087`
**Issue:** `EXPECT_FALSE(report.find("\"Volume\"") != std::string::npos)` is functionally
equivalent to `EXPECT_EQ(report.find("\"Volume\""), std::string::npos)`, which is the idiom every
other negative-presence assertion in this file uses. Still present, unchanged since iteration 1.
Purely cosmetic.
**Fix:** `EXPECT_EQ(report.find("\"Volume\""), std::string::npos) << report;`

### IN-02: Histogram annotation path has no dedicated quote/backslash-escaping test

**File:** `tests/test_database_ui_metadata.cpp` (histogram test block, ~line 1049)
**Issue:** `RenderEscapesQuotesAndBackslashes` (line 288) proves `quote_ui_text` escapes correctly
for the `; label` clause, and the histogram annotation calls the same function the same way, so
the risk is low. No test in this file exercises a vocabulary label containing `"` or `\` through
`summarize_collection`'s histogram specifically. Still not covered by the WR-01 fix (that fix
targeted D-09's empty-label branch, a different concern). Low priority — same reasoning as
iteration 1.
**Fix:** Optional — add a variant of `SummarizeHistogramAnnotatesCodesWithEnumLabels` whose
vocabulary label is `Say "Hi" \ here` and assert the escaped form appears in the `; values {}`
clause.

---

_Reviewed: 2026-09-20T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
