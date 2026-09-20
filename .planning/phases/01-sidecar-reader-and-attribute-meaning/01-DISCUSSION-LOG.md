# Phase 1: Sidecar Reader and Attribute Meaning - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-20
**Phase:** 1-sidecar-reader-and-attribute-meaning
**Areas discussed:** Render format, Tooltip scope, Failure granularity, CHANGELOG reconciliation

Questions were asked as prose rather than via multi-choice prompts, per a standing user preference.
All four gray areas were presented in one pass with a recommended default each, and the user
answered all four in a single reply.

---

## Render format

| Option | Description | Selected |
|--------|-------------|----------|
| Claude's stated default | One line, ASCII-only separators, ` - ` between fields, tooltip after label | |
| Delegated to Claude, optimized for LLM readability | User asked for whatever is easiest for an agent to read | ✓ |

**User's choice:** "render that way that make more easier for an agent like you to read"

**Notes:** Because the user delegated the design rather than picking a spelling, a 4-proposal /
3-judge panel was run instead of guessing. Four designers each worked a distinct angle; three
judges scored all four on LLM readability, constraint compliance, robustness under hostile data,
and token cost.

| Proposal | Angle | Judge 1 | Judge 2 | Judge 3 |
|----------|-------|---------|---------|---------|
| 1. quote-brace-hash suffix `"label" {0=Name} # tooltip` | token economy | 30 | 31 | 30 |
| 2. keyed pipe fields `\| label: … \| enum {} \| tooltip:` | maximal parseability | 31 | 34 | 31 |
| **3. semicolon clause suffix `; label "…"; enum {…}; tooltip "…"`** | **consistency with existing grammar** | **35** | **36** | **35** |
| 4. tagged continuation lines | multi-line layout | 25 | 31 | 22 |

Unanimous winner: **Proposal 3**. Judge lenses were an LLM consumer, a hostile-data adversary, and
the codebase maintainer.

Proposal 4 took the only hard-constraint violations, both from the maintainer judge: it emitted its
text *after* the header's trailing newline (contradicting the settled "append before the newline"
placement decision, which its own spec acknowledged re-slotting), and by its own arithmetic grew
GNoMo's `describe()` by +270 lines / ~10 KB and `describe_collection()` by +433 lines / ~20 KB —
the largest growth of the four, on the exact axis the tooltip decision below exists to contain.

Grafts pulled from losing proposals onto the winner (each named by at least two judges):

- Suppress a restating **tooltip**, not just a restating label (from 1 and 4) — the winner had
  specified suppression for labels only, and tooltips average 49 chars across 507 attributes
- Do whitespace normalization **inside the renderer** rather than trusting the loader (from 2)
- Assert the `describe()`-is-a-prefix-of-`describe_collection()` property in a **test**, since all
  proposals claimed it by construction and none checked it (from 1)
- Spell `squash()` as an explicit ASCII test, never `std::tolower(char)` — UB on a negative `char`,
  and 344 corpus strings are non-ASCII (from 3's maintainer review)
- Phase 2's histogram should carry the enum as its own clause reusing this grammar, rather than
  either interleaved spelling the designers proposed (from 3's maintainer review)
- Anchor the existing under-specified negative assertions in `test_database_lifecycle.cpp` (from 4)
  — **noted but deliberately not acted on**, see Deferred Ideas

---

## Tooltip scope

| Option | Description | Selected |
|--------|-------------|----------|
| Tooltip everywhere | Render it in both `describe()` and `describe_collection()`; simpler, no extra parameter | |
| Tooltip only in `describe_collection()` | Bounds whole-DB growth; costs a bool parameter on the shared helper | ✓ |

**User's choice:** "tooltip only in describe collection"

**Notes:** This is the lever PROJECT.md named for the ~15–20 KB whole-DB growth on a 236-attribute
model but did not pull. Claude's stated default had been the opposite (render it everywhere, on
the grounds that 15 KB is acceptable for an LLM reader and the parameter is a complication for a
speculative problem); the user overrode it. Consequence recorded in CONTEXT.md D-08:
`write_collection_section` takes two new parameters rather than one, which the ROADMAP's "one
injection site" framing did not anticipate.

---

## Failure granularity

| Option | Description | Selected |
|--------|-------------|----------|
| One outer try/catch only | Literal reading of the roadmap; one malformed file drops all 67 collections' metadata | |
| Per-file try/catch inside one outer try/catch | Same cost, strictly better degradation | ✓ |

**User's choice:** "per-file try/catch inside one outer try/catch" (quoting Claude's recommended
default back verbatim)

**Notes:** The roadmap's "the whole load is one try/catch → `logger->warn` → empty map" fixes the
*posture* (never throw, never fail the open) but is silent on *granularity*. Both options honour
the posture. The corpus has 0 parse failures across all 117 files, so this is insurance rather
than a response to an observed failure.

---

## CHANGELOG reconciliation

| Option | Description | Selected |
|--------|-------------|----------|
| Rename the 0.10.7 heading to 0.10.8 | The ROADMAP's literal wording | |
| Date the 0.10.7 section and open a new 0.10.8 section | Claude's default; the section's content is already released | ✓ |

**User's choice:** "your default"

**Notes:** Claude flagged that the ROADMAP's instruction conflicts with git before proposing the
alternative. A verifier agent then confirmed every part independently: tag `v0.10.7` exists at
`7bd1f16`, which is the commit that wrote the entries under the heading; `git log v0.10.7..HEAD --
CHANGELOG.md` is empty, ruling out a mixed section; `CMakeLists.txt:4` is already at `0.10.8`. The
file is missing a section, not misnumbered. The verifier also pinned the file's own formatting
conventions (em dash U+2014 with surrounding spaces, `compare/v<prev>...v<ver>` for released links)
and established that `v0.10.7` is a lightweight tag, so its date is the commit date, 2026-09-17.

---

## Claude's Discretion

Taken as Claude's call, stated up front and not contested:

- New file naming and placement — resolved to `src/ui_config.h`/`.cpp` after finding
  `src/csv_read.h`/`.cpp` as an exact in-repo precedent for an internal component with no public
  header and its dependency confined to the `.cpp`
- Test file placement — resolved to a new `tests/test_database_ui_metadata.cpp` rather than
  extending `tests/test_database_describe.cpp`, whose `open()` helper is `from_schema`-only and
  cannot build the temp-dir `migrations/` + `ui/` pair every test here needs
- The `UiConfig` type and field spelling, the internal map shape, and the signature of the single
  accessor that reads a localizable value

## Deferred Ideas

- **Phase 2 histogram spelling** — decided now (as a deferred item, not built) so the two grammars
  cannot diverge: the enum becomes its own clause on the summarize line, reusing the Phase 1
  grammar verbatim and leaving the existing histogram untouched.
- **Latent defect in four existing negative assertions** in `tests/test_database_lifecycle.cpp` —
  they match bare `Vectors:` / `Sets:` / `Time Series:` anywhere in the output instead of anchoring
  on `"\n  Vectors:\n"`. Flagged by all three judges. Deliberately not fixed: those tests use
  `from_schema`, so no sidecar text can reach them in this phase, and ROADMAP success criterion 6
  requires them to pass *unmodified*. Recorded for whichever future phase first feeds sidecar prose
  through them.
- **Escape hatch for whole-DB size** — dropping the label clause from `describe()` too, one more
  bool on the same function. Not built.

No scope creep arose during the discussion; every area stayed inside the phase boundary.
