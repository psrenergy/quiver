# Phase 3: Writes and Transactions - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Python bindings for all CRUD write operations (create, update, delete elements; scalar/vector/set attribute updates; relation updates) and explicit transaction control with a context manager. All operations wrap existing C API functions. `create_element` (WRITE-01) and `update_scalar_relation` (WRITE-07) already exist from earlier phases.

</domain>

<decisions>
## Implementation Decisions

### Transaction context manager
- Implement with `@contextmanager` decorator from `contextlib`
- `with db.transaction() as db:` — yields `db` back (mirrors Julia/Dart passing db to the block)
- Auto-commits on successful exit, rolls back on exception (best-effort rollback, swallow rollback errors)
- No return value from the context manager — use variables outside the `with` block if needed

### Update method signatures
- Vector/set updates accept plain Python lists: `update_vector_integers(coll, id, attr, [1, 2, 3])`
- Empty list clears the vector/set: `update_vector_integers(coll, id, attr, [])`
- `update_scalar_string` accepts `None` to set NULL — symmetric with reads returning `None` for NULL values
- `update_element` reuses the existing `Element` class (same as `create_element`)

### Plan structure
- Plan 03-01: Write operations — CFFI declarations, update_element, delete_element, all scalar/vector/set updates, write tests
- Plan 03-02: Transactions — CFFI declarations, begin_transaction/commit/rollback/in_transaction, context manager, transaction tests

### Test organization
- One test file per functional area matching C++ structure: `test_database_create.py`, `test_database_update.py`, `test_database_delete.py`, `test_database_transaction.py`

### Claude's Discretion
- Parameter order for update methods (follow C API or adjust for Python idiom)
- Error propagation on transaction rollback (propagate original exception vs wrap)
- Internal marshaling approach for list-to-C-array conversion

</decisions>

<specifics>
## Specific Ideas

- Transaction context manager should feel like standard Python resource management (`with open(...) as f:` pattern)
- Julia and Dart both pass db to the transaction block — Python yields db for consistency across all bindings

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 03-writes-and-transactions*
*Context gathered: 2026-02-23*
