# Requirements: Quiver

**Defined:** 2026-02-26
**Core Value:** A single, consistent API surface across all language bindings

## v0.5 Requirements

Requirements for Python API alignment. Each maps to roadmap phases.

### Python API

- [ ] **PYAPI-01**: User can create an element with kwargs: `db.create_element("Collection", label="x", value=42)`
- [ ] **PYAPI-02**: User can update an element with kwargs: `db.update_element("Collection", id, label="y")`
- [ ] **PYAPI-03**: Element class is removed from public API and `__init__.py` exports
- [ ] **PYAPI-04**: All Python tests pass with the new kwargs API
- [ ] **PYAPI-05**: CLAUDE.md cross-layer naming table updated with new Python pattern

## Future Requirements

None — v0.5 is a focused milestone.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Dict positional arg for create/update | `**my_dict` unpacking covers this; simpler API |
| Julia/Dart binding changes | Already follow the target pattern |
| New C++ core features | v0.5 is binding alignment only |
| Python LuaRunner | Separate concern, not related to API alignment |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| PYAPI-01 | Phase 1 | Pending |
| PYAPI-02 | Phase 1 | Pending |
| PYAPI-03 | Phase 1 | Pending |
| PYAPI-04 | Phase 1 | Pending |
| PYAPI-05 | Phase 1 | Pending |

**Coverage:**
- v0.5 requirements: 5 total
- Mapped to phases: 5
- Unmapped: 0 ✓

---
*Requirements defined: 2026-02-26*
*Last updated: 2026-02-26 after initial definition*
