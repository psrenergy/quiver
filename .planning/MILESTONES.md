# Milestones

## v0.5 Python API Alignment (Shipped: 2026-02-26)

**Delivered:** Python binding now uses kwargs for create/update, matching Julia and Dart patterns.

**Stats:**
- Phases: 1 | Plans: 2 | Tasks: 4
- Files modified: 17 (source)
- Lines changed: 361 insertions, 493 deletions (-132 net)
- Timeline: 19 min (2026-02-26)
- Git range: `d0b643f` → `36972e1`

**Key accomplishments:**
1. `create_element` and `update_element` accept `**kwargs` instead of Element objects
2. Element class removed from Python public API (internal-only for FFI)
3. All 12 Python test files (198 tests) converted to kwargs syntax
4. CLAUDE.md updated with Python kwargs documentation across 4 sections
5. Full test suite green with zero Element usage in public API

**Requirements:** 5/5 complete (PYAPI-01 through PYAPI-05)

**Archive:** `.planning/milestones/v0.5-ROADMAP.md`, `.planning/milestones/v0.5-REQUIREMENTS.md`

---

