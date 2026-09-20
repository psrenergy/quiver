---
phase: 2
slug: enum-labels-on-the-value-histogram
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-09-20
---

# Phase 2 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

Register origin: authored at plan time in `02-01-PLAN.md`'s `<threat_model>` block. This audit
verifies the declared mitigations exist in the implementation; it does not scan for new threats.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| `ui/*.toml` sidecar → renderer | Untrusted text. The sidecar is corpus data authored outside this repo, on the filesystem beside a study's migrations. Its `label.en` strings are the only attacker-influenced bytes this phase newly copies into a report. | Free-form UTF-8 label text, arbitrary byte values |
| report string → consumer | The returned `std::string` is read by an LLM agent and printed to a terminal by `quiver_cli` and every binding's REPL. | Rendered report text |

No package-manager install occurs in this phase (no npm/pip/cargo step, no new dependency, no
CMake change), so no `T-02-SC` supply-chain row applies and no legitimacy checkpoint was required.

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-02-01 | Tampering | `normalize_ui_text` call in the new annotation, `src/database_describe.cpp` | high | mitigate | Verified present at `src/database_describe.cpp:286-288`: the emitted label goes through `normalize_ui_text` **before** `quote_ui_text`. `normalize_ui_text` (`src/database_describe.cpp:62-76`) maps every byte `< 0x20` or `== 0x7F` to a space, so ESC (`0x1B`) and `\n` cannot reach the report — no ANSI escape into a terminal, no forged report line. The existing Phase 1 helper is reused, not re-implemented (D2-03). | closed |
| T-02-02 | Spoofing | `quote_ui_text` output inside `values {}` | medium | accept | `quote_ui_text` escapes only `\` and `"`, so a corpus label spelled `Sets: 3` or `values {see manual}` renders verbatim *inside* its quotes. A **parse**-level guarantee, not substring immunity — Phase 1's D-02 posture, unchanged, and the reason the quotes exist. | closed (accepted) |
| T-02-03 | Spoofing | `  Vectors:` / `  Sets:` / `  Time Series:` headers in `summarize_collection` | medium | accept | What Phase 2 genuinely widens: `summarize_collection` emits those headers too, so a *future* naive header-count test written against summarize on a `from_migrations` database would be breakable by a label containing that text. No existing test is exposed — the four SC-3 lifecycle assertions are `from_schema(":memory:")` + `describe()` and unreachable from this diff (D2-09). Recorded ceiling per D2-10. | closed (accepted) |
| T-02-04 | Denial of Service | `kMaxDistributionCardinality`, line length | low | accept | 64 labelled entries is a ~2 KB single line. The 64-code cap (D2-13) already bounds it; no truncation is added because truncation needs its own ellipsis convention. Recorded as a known limit in `src/CLAUDE.md`. | closed (accepted) |
| T-02-05 | Information Disclosure | `impl_->ui_metadata.find` lookup | low | accept | The annotation exposes only sidecar text the same database already renders in `describe` / `describe_collection` (Phase 1). No new data source, no new file read, no new code path reaching the filesystem. | closed (accepted) |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above `workflow.security_block_on` (`high`) count toward `threats_open`*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| R-02-01 | T-02-02 | A substring blocklist on top of `quote_ui_text` is explicitly rejected by D2-03 and D2-10 — a second escaping rule can drift from D-02. Quoting gives the parse-level guarantee that is actually wanted. | Plan author (02-01-PLAN.md `<threat_model>`) | 2026-09-20 |
| R-02-02 | T-02-03 | No existing test is exposed. Remedy if it ever bites: assert on the report's structure (line prefix + indentation), not on a substring count. | Plan author (02-01-PLAN.md `<threat_model>`) | 2026-09-20 |
| R-02-03 | T-02-04 | The 64-code cap already bounds the line; truncation would need its own ellipsis convention, which is not worth introducing for a ~2 KB ceiling. | Plan author (02-01-PLAN.md `<threat_model>`) | 2026-09-20 |
| R-02-04 | T-02-05 | No new data source, file read, or filesystem code path — the text is already rendered by Phase 1's `describe` / `describe_collection`. | Plan author (02-01-PLAN.md `<threat_model>`) | 2026-09-20 |

*Accepted risks do not resurface in future audit runs.*

**Carried forward from the milestone, not this phase:** STATE.md records the accepted risk that
`enum.toml` contradicts the model's own Julia declarations for 2 of 8 mechanically-checkable
attributes (HTD `HasCommitment`). `validate_ui_config()` is out of milestone scope (VALID-01,
deferred), so rendering hands those inversions to an LLM as fact. That is a data-correctness risk in
the corpus, not a mitigable defect in this diff.

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-09-20 | 5 | 5 | 0 | gsd-secure-phase (orchestrator, L1 short-circuit — `threats_open: 0`, register authored at plan time, `asvs_level: 1`) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-09-20
