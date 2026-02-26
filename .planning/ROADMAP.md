# Roadmap: Quiver v0.5

## Overview

Python binding currently uses the `Element` builder pattern for create/update operations, while Julia and Dart use kwargs/dict patterns that feel native to their languages. This milestone replaces the Python `Element` API with kwargs-based `create_element` and `update_element`, removes the dead code, updates tests, and documents the change.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: Python kwargs API** - Replace Element builder with kwargs create/update, remove Element class, update tests and docs

## Phase Details

### Phase 1: Python kwargs API
**Goal**: Python users create and update elements using native kwargs syntax, identical in feel to Julia/Dart bindings
**Depends on**: Nothing (first phase)
**Requirements**: PYAPI-01, PYAPI-02, PYAPI-03, PYAPI-04, PYAPI-05
**Success Criteria** (what must be TRUE):
  1. User can call `db.create_element("Collection", label="x", value=42)` and the element is persisted correctly
  2. User can call `db.update_element("Collection", id, label="y")` and the element is updated correctly
  3. `Element` class is not importable from the quiver Python package (`from quiver import Element` fails)
  4. All existing Python tests pass after being rewritten to use the kwargs API
  5. CLAUDE.md cross-layer naming table accurately reflects the new Python pattern
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md -- Core API change (kwargs create/update, remove Element from exports) + convert create/update/delete test files
- [ ] 01-02-PLAN.md -- Convert remaining 9 test files + update CLAUDE.md documentation

## Progress

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Python kwargs API | 1/2 | In Progress | - |
