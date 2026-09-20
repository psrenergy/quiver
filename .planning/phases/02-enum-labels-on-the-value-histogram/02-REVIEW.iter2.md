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
  warning: 1
  info: 2
  total: 3
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-09-20T00:00:00Z
**Depth:** standard
**Files Reviewed:** 2
**Status:** issues_found

## Summary

This phase adds one focused change: `summarize_collection()`'s integer value-distribution
histogram now annotates each observed code with its enum label, reusing Phase 1's
`normalize_ui_text` / `quote_ui_text` helpers and the `impl_->ui_metadata` map. I traced the diff
against `6863f64..HEAD` rather than reviewing the whole file cold, since the task calls out two
specific concerns: the escaping/injection path and the lookup's placement relative to
`kMaxDistributionCardinality`.

Both concerns check out. The `meta` lookup sits inside the already-cardinality-bounded branch
(after `distinct[0][0] <= kMaxDistributionCardinality` passes), so it can never run more than
`kMaxDistributionCardinality` (64) times per scalar, and never at all for a scalar whose
distribution clause is suppressed — matching the "D2-12" comment's claim. The escaping path reuses
`quote_ui_text`, unmodified by this diff: only `"` and `\` are escaped, which is sufficient to keep
label text from producing a spurious unescaped closing quote (D-02's existing guarantee) — the
diff does not add a new escaping surface, it composes an already-reviewed one. `rows[i][0]` (from
`query_int_rows`, `std::vector<std::vector<int64_t>>`) and `UiAttribute::enum_labels`
(`std::map<int64_t, std::string>`) agree in type, so the `.find(rows[i][0])` lookup has no
signature mismatch. The one new test (`SummarizeHistogramAnnotatesCodesWithEnumLabels`) matches
the code's actual output byte-for-byte for the scenario it covers (covered code with a label,
uncovered code left bare, unobserved vocabulary code absent).

The one gap worth flagging is coverage, not logic: D-09 — this phase's own headline decision, that
a label normalizing to empty (e.g., all-whitespace) drops only the annotation and keeps the
histogram entry — is implemented correctly by reading the code, but has zero test exercising it.
Nothing in the two reviewed files (or, per a scoped grep, anywhere else in `tests/`) would fail if
a future edit regressed that branch back to dropping the whole entry (the `enum {}` clause's D-06
behavior). No critical issues found.

## Warnings

### WR-01: D-09's empty-label-keeps-entry behavior has no test

**File:** `src/database_describe.cpp:283-291`
**Issue:** The phase's stated headline decision (SUMMARY.md key-decisions: "a label that
normalizes to empty drops the annotation only and keeps the entry — deliberate divergence from
D-06") is implemented (`if (!normalized_label.empty()) { out << " " << quote_ui_text(...); }` sits
inside the `if (label_it != end())` block, so a whitespace-only or empty label falls through to
`out << ": " << rows[i][1];` with the entry intact) but is not covered by any test. The single new
test (`tests/test_database_ui_metadata.cpp:1054-1088`) only exercises "code with a real label,"
"code with no vocabulary entry," and "vocabulary entry never observed" — not "vocabulary entry
present but its label normalizes to empty." A regression that accidentally routed this path through
the same "drop the whole entry" logic as the `enum {}` clause (D-06) — e.g. a future refactor that
tries to share code between the two clauses — would ship silently.
**Fix:**
```cpp
// Add to tests/test_database_ui_metadata.cpp, alongside SummarizeHistogramAnnotatesCodesWithEnumLabels:
TEST_F(DatabaseUiMetadataTest, SummarizeHistogramKeepsEntryWhenLabelNormalizesToEmpty) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[initial_volume_type]]
id = 0
label.en = "   "
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "initial_volume_type"
enum = "initial_volume_type"
)");

    auto db = open_tree();
    db.create_element(
        "HydroPlant",
        quiver::Element().set("label", std::string("a")).set("initial_volume_type", static_cast<int64_t>(0)));

    auto report = db.summarize_collection("HydroPlant");

    // The entry survives with a bare code -- only the annotation is dropped.
    EXPECT_NE(report.find(R"(values {0: 1})"), std::string::npos) << report;
}
```

## Info

### IN-01: New test's negative assertion uses a different idiom than the rest of the file

**File:** `tests/test_database_ui_metadata.cpp:1087`
**Issue:** `EXPECT_FALSE(report.find("\"Volume\"") != std::string::npos)` is functionally
equivalent to `EXPECT_EQ(report.find("\"Volume\""), std::string::npos)`, which is the idiom every
other negative-presence assertion in this file uses (e.g. lines 210-212, 243, 263, 320, 611, 928).
Purely cosmetic — flagging only because the file otherwise keeps this idiom consistent.
**Fix:** `EXPECT_EQ(report.find("\"Volume\""), std::string::npos) << report;`

### IN-02: Histogram annotation path has no dedicated quote/backslash-escaping test

**File:** `tests/test_database_ui_metadata.cpp` (new test block, ~line 1049)
**Issue:** `RenderEscapesQuotesAndBackslashes` (line 288) proves `quote_ui_text` escapes correctly
for the `; label` clause, and the histogram annotation calls the same function the same way, so
the risk is low. Still, the task explicitly calls out the escaping/injection path as an area of
concern for this feature, and no test in this file exercises a vocabulary label containing `"` or
`\` through `summarize_collection`'s histogram specifically — a regression that swapped
`quote_ui_text` for a differently-escaped call in this one call site would not be caught here.
**Fix:** Optional — add a variant of `SummarizeHistogramAnnotatesCodesWithEnumLabels` whose
vocabulary label is `Say "Hi" \ here` and assert the escaped form appears in the `; values {}`
clause.

---

_Reviewed: 2026-09-20T00:00:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
