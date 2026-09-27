# 41 — All bindings: fix the stale "not positionally aligned" reader comments

**Batch** 4 · **Severity** low · **Breaking** no (comments only) · **Size** S · **Layers** Julia, Dart, Python, JS doc comments
**Depends on** none · **Overlaps with** 30 (Python docstrings; plan 30 deliberately leaves the alignment text to this plan), 42 (renames the Julia/Python date-time readers whose comments are edited here — if 42 lands first, the function names next to these comments differ, the comment text does not), 75 (fixes the same stale claim in `tests/AGENTS.md:140`)

## Why

The core's vector/set bulk readers now LEFT JOIN the group table onto the collection, so an element
with no group rows is an empty inner list, and entry *i* is the same element as `read_element_ids`
entry *i*. See `src/database_read.cpp`,
`"SELECT c.id, g." + attribute + " FROM " + collection + " c LEFT JOIN " + ... + " ORDER BY c.rowid, ..."`,
and the root AGENTS.md design decision "Bulk reads of one collection are positionally aligned, but
still cell-dense". Only the "NULL cells are dropped" half of these binding comments is still true.
The other half claims the opposite of what the code does, and of what each binding's own tests
assert (e.g. `bindings/julia/test/test_database_read_vector.jl` "One entry per element: the element
with no rows is an empty vector, not a gap"; `bindings/dart/test/database_read_vector_test.dart`
expects `<DateTime>[]` for the "No vector" element).

Stale sites at HEAD (`grep -rn "own rows\|positionally aligned\|alignment caveat" bindings/*/src bindings/dart/lib`):

| File | Lines (currently) | Reader |
|---|---|---|
| `bindings/julia/src/database_read.jl` | ~119-120 | `read_vector_booleans` |
| same | ~181, ~215, ~276 | `read_vector_date_times`, `read_set_booleans`, `read_set_date_times` ("Same alignment caveat as ...") |
| `bindings/dart/lib/src/database_read.dart` | ~194-196 | `readVectorBooleans` |
| same | ~305-307 | `readVectorDateTimes` |
| same | ~367-368 | `readSetBooleans` ("Same alignment caveat as [readVectorBooleans]") |
| same | ~475-476 | `readSetDateTimes` ("Same alignment caveat as [readVectorDateTimes]") |
| `bindings/python/src/quiverdb/database.py` | ~826-827 | `read_vector_booleans` |
| same | ~901-902 | `read_vector_date_times` |
| same | ~1047-1048 | `read_set_booleans` |
| same | ~1122-1123 | `read_set_date_times` |
| `bindings/js/src/read.ts` | ~370-371 | `readVectorBooleans` |
| same | ~421 | `readSetBooleans` |

Leave these alone: they are correct, and they say something different.
- `bindings/dart/lib/src/database_read.dart:~1018/~1067`, on the whole-group readers ("rows stay
  positionally aligned").
- `bindings/js/src/lua-api.ts:~389`, about two *columns* of one group not being aligned with each
  other in Lua.

Principle: human readability. A comment that says the opposite of the code is worse than no comment.

## Constraints and decisions

- **Maintainer decision (binding):** use one wording everywhere:
  *"One entry per element, aligned with read_element_ids (an element with no rows is an empty list);
  NULL cells are dropped, so each inner list is dense."* Use each binding's own name for
  `read_element_ids`: `readElementIds` in Dart and JS, `read_element_ids` in Julia and Python. Keep
  each binding's comment syntax.
- The "(unlike read_scalar_booleans)" parentheticals now point at the only remaining difference:
  NULL handling. Rewrite them as "(the scalar readers keep NULLs positionally)".
- Rewrite the set readers' "Same alignment caveat as X" one-liners as "Same shape as X". There is no
  longer an alignment caveat to point back to.
- Julia's `read_grouped_values_all` pointer can stay, because that helper now documents the
  alignment itself.
- Comments only. No code, test or CHANGELOG change.

## Changes

### Julia — `bindings/julia/src/database_read.jl`

Current (~L119-120):
```julia
# NULL cells are dropped and only ids that own rows are returned (`read_grouped_values_all`,
# `src/database_internal.h`), so the result is not positionally aligned with `read_element_ids`.
```
New:
```julia
# One entry per element, aligned with `read_element_ids` (an element with no rows is an empty list);
# NULL cells are dropped, so each inner list is dense (`read_grouped_values_all`,
# `src/database_internal.h`). The scalar readers keep NULLs positionally.
```
Current (~L181, ~L215, ~L276), three identical lines:
```julia
# Same alignment caveat as `read_vector_booleans`: NULL cells dropped, only ids that own rows.
```
New, for each:
```julia
# Same shape as `read_vector_booleans`: one entry per element, NULL cells dropped.
```

### Dart — `bindings/dart/lib/src/database_read.dart`

Current (~L194-196, on `readVectorBooleans`):
```dart
  /// NULL cells are dropped and only elements that own rows are returned, so the
  /// result is not positionally aligned with [readElementIds] (unlike
  /// [readScalarBooleans]).
```
New:
```dart
  /// One entry per element, aligned with [readElementIds] (an element with no rows is an empty
  /// list); NULL cells are dropped, so each inner list is dense (unlike [readScalarBooleans], which
  /// keeps NULLs positionally).
```
On `readVectorDateTimes` (~L305-307), make the same change with `[readScalarDateTimes]`.

Current (~L367-368, on `readSetBooleans`):
```dart
  /// Same alignment caveat as [readVectorBooleans]: NULL cells are dropped and
  /// only elements that own rows are returned.
```
New:
```dart
  /// Same shape as [readVectorBooleans]: one entry per element, NULL cells dropped.
```
On `readSetDateTimes` (~L475-476), make the same change with `[readVectorDateTimes]`.

### Python — `bindings/python/src/quiverdb/database.py`

Current (~L826-827, `read_vector_booleans` docstring):
```
        NULL cells are dropped and only elements that own rows are returned, so the result is
        not positionally aligned with read_element_ids (unlike read_scalar_booleans).
```
New:
```
        One entry per element, aligned with read_element_ids (an element with no rows is an
        empty list); NULL cells are dropped, so each inner list is dense (unlike
        read_scalar_booleans, which keeps NULLs positionally).
```
On `read_vector_date_times` (~L901-902), make the same change with `read_scalar_date_times`.

Current (~L1047-1048 and ~L1122-1123):
```
        Same alignment caveat as read_vector_booleans: NULL cells are dropped and only elements
        that own rows are returned.
```
New:
```
        Same shape as read_vector_booleans: one entry per element, NULL cells dropped.
```
The second site uses `read_vector_date_times`.

### JS — `bindings/js/src/read.ts`

Current (~L370-371):
```ts
 * NULL cells are dropped and only elements that own rows are returned, so the result is not
 * positionally aligned with `readElementIds` (unlike `readScalarBooleans`).
```
New:
```ts
 * One entry per element, aligned with `readElementIds` (an element with no rows is an empty list);
 * NULL cells are dropped, so each inner list is dense (unlike `readScalarBooleans`, which keeps
 * NULLs positionally).
```
Current (~L421):
```ts
/** Same alignment caveat as `readVectorBooleans`: NULL cells dropped, only ids that own rows. */
```
New:
```ts
/** Same shape as `readVectorBooleans`: one entry per element, NULL cells dropped. */
```

### `bindings/js/AGENTS.md` (~L95)

The "Time-series NULL cells" bullet says string columns use the null-guarded loop, "never
`decodeStringArray`, which constructs a `CString` from a NULL pointer". Check that reason against
`decodeStringArray` in `bindings/js/src/ffi-helpers.ts`. If it now maps a NULL entry to `""`, reword
the parenthetical to "(never `decodeStringArray`, which maps a NULL entry to `""` instead of
`null`)". If the current text is accurate, leave it.

## Tests

None. Comments only. Run each suite once to confirm that no docstring edit broke syntax. This
matters most for the Python triple-quoted docstrings and the Dart `///` blocks.

## Docs and changelog

No CHANGELOG entry. Plan 75 fixes the `tests/AGENTS.md` copy of this claim ("omission of elements
without group rows").

## Verification

From the repo root:
1. Run `grep -rn "own rows\|not positionally aligned\|alignment caveat" bindings/julia/src bindings/dart/lib bindings/python/src bindings/js/src`.
   It should print only the two intentional sites (the Dart whole-group readers' "rows stay
   positionally aligned" and lua-api.ts's column-alignment note).
2. Run `bindings/python/tests/test.bat`, `bindings/dart/test/test.bat`, `bindings/js/test/test.bat`
   and `bindings/julia/test/test.bat`.
3. Run `cd bindings/dart && dart analyze`.
4. Run `scripts/format.bat`.

## Acceptance criteria

- [ ] All 14 stale sites are rewritten with the agreed wording.
- [ ] The intentional "aligned" comments are untouched.
- [ ] All four binding suites pass.

## Pitfalls

- Python docstrings: keep the existing indentation (8 spaces inside a method) and do not break the
  closing `"""`.
- Do not touch the `.pyc` files the grep also matches.

## Out of scope

- The vector/set readers' NULL-dropping itself (a documented design decision).
- `tests/AGENTS.md` (plan 75).
