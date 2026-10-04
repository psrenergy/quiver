---
phase: 06-quiver-file-layout
reviewed: 2026-10-04T00:00:00Z
depth: standard
files_reviewed: 20
files_reviewed_list:
  - AGENTS.md
  - bindings/js/src/lua-api.ts
  - bindings/js/test/lua-api-sync.test.ts
  - src/AGENTS.md
  - src/CMakeLists.txt
  - src/lua_runner/binary.cpp
  - src/lua_runner/database.cpp
  - src/lua_runner/database_create.cpp
  - src/lua_runner/database_csv_export.cpp
  - src/lua_runner/database_csv_import.cpp
  - src/lua_runner/database_delete.cpp
  - src/lua_runner/database_describe.cpp
  - src/lua_runner/database_metadata.cpp
  - src/lua_runner/database_query.cpp
  - src/lua_runner/database_read.cpp
  - src/lua_runner/database_time_series.cpp
  - src/lua_runner/database_update.cpp
  - src/lua_runner/expression.cpp
  - src/lua_runner/internal.h
  - src/lua_runner/lua_runner.cpp
findings:
  critical: 0
  warning: 1
  info: 1
  total: 2
status: issues_found
---

# Phase 06: Code Review Report

**Reviewed:** 2026-10-04
**Depth:** standard
**Files Reviewed:** 20
**Status:** issues_found

## Narrative Findings (AI reviewer)

## Summary

The move is faithful. Checked mechanically against `21ba6f8`:

- **Nothing dropped or duplicated.** I diffed the sorted, whitespace-trimmed lines of the old
  `db_core/db_write/db_read/db_metadata/db_time_series/binary.cpp` against the new
  `database*.cpp + binary.cpp + expression.cpp`. Every code line is accounted for. The only
  differences are includes, namespace braces, renamed binder signatures, the `list_metadata_lua` /
  `get_metadata_lua` templates (moved verbatim into `internal.h`), and the dropped NOLINT pairs.
- **Registration.** The base and HEAD have the same sorted set of `bind.`/`ns.set_function("…")`
  names: 86 calls, each name registered once. So binder order cannot shadow anything.
  `lua_runner.cpp` calls all 14 binders, and `new_usertype<Database>` still appears once.
- **Linkage.** Helpers that stayed file-local are still in anonymous namespaces (`run_in_scope`,
  `lua_table_to_values`, `query_*_lua`, `string_key`, `require_dense_array`,
  `create_element_lua`, `lua_data_type_name`, `to_expression`, `binop`, `parse_aggregate_op`,
  `bind_expression_operators`). The newly shared symbols (`metadata_to_lua` overloads,
  `parse_csv_options`) have external linkage, are declared once in `internal.h`, and are defined
  once. `table_to_element` was already declared there. There is no ODR hazard. The one sol2
  trait that differs by translation unit (BinaryFile comparability) is only instantiated in
  `binary.cpp`, which still includes `expression.h`.
- **binary/expression split.** BinaryFile's arithmetic and logical metamethods are still set on
  the same usertype. `bind_binary` returns that usertype and `bind_expression` sets them first
  thing. Expression and its operators are byte-identical, and so are the 12 `quiver.*`
  expression functions.
- **NOLINT pairs.** Removing the pairs from `database.cpp`, `database_query.cpp` and `binary.cpp`
  is correct, because none of them keeps a by-value non-trivial sol2 parameter. `expression.cpp`
  keeps its pair.
- **Docs.** The layout, the binder names, the "17 plain forwarders" count and the file-to-core
  mapping are accurate. I checked each against the `Database::` definitions in
  `src/database*.cpp`. No stale `db_*` or `bind_core`/`bind_write` references remain in tracked
  files.

There is one real defect. It predates this phase, but the phase's new comment and doc paragraph
now describe it as behavior worth keeping.

## Warnings

### WR-01: Automatic `__eq`/`__lt`/`__le` on BinaryFile and Expression always return true, and the phase now documents this as behavior to preserve

**File:** `src/lua_runner/binary.cpp:8-10`, `src/AGENTS.md:666-670` (also affects `src/lua_runner/expression.cpp:111`)

**Issue:** sol2 adds comparison metamethods automatically when a usertype is created
(`comparsion_operator_wrap`, `sol/stack_core.hpp:1388`). The `Expression` comparison operators
return an `Expression`, not a `bool`. sol2 pushes that result as userdata, and Lua coerces any
`__eq`/`__lt`/`__le` result to a boolean, so the answer is always truthy. I confirmed this with
`quiver_cli` on two distinct files `a` and `b`:

```
{"expr_eq":true,"expr_lt":true,"expr_ne":false,"file_eq":true,"file_gt":true,"file_lt":true}
```

So `fa == fb` is true for two different files, `fa < fb` and `fa > fb` are both true, and
`ea ~= eb` is false. A script that writes `f < g`, expecting an element-wise comparison or an
error, silently gets `true`.

This is not a regression: the base already behaved this way. What this phase changed is the
framing. The new comment in `binary.cpp` keeps the `expression.h` include *because* it produces
these metamethods, and `src/AGENTS.md` says "removing it changes `f < g` and `f == g` in scripts
while every test stays green". Both treat broken, untested behavior as a contract. Severity is
WARNING only because the phase's goal was to change no behavior, and that goal is met. As a
product defect this is incorrect behavior.

**Fix:** Override the automatic comparisons explicitly in `bind_expression_operators`. That makes
the include in `binary.cpp` no longer load-bearing, so the comment and doc paragraph can go:

```cpp
// expression.cpp, inside bind_expression_operators<T>
type[sol::meta_function::equal_to] = [](const sol::object& a, const sol::object& b) {
    return a.pointer() == b.pointer();  // identity, like any plain userdata
};
type[sol::meta_function::less_than] = [](const sol::object&, const sol::object&) -> bool {
    throw std::runtime_error("Cannot lt: use quiver.lt/quiver.gt for element-wise comparison");
};
type[sol::meta_function::less_than_or_equal_to] = [](const sol::object&, const sol::object&) -> bool {
    throw std::runtime_error("Cannot le: use quiver.lte/quiver.gte for element-wise comparison");
};
```

Alternatively, pass `sol::automagic_enrollments` with the three comparison flags off to both
`new_usertype` calls. Either way, add one `test_lua_expression.cpp` case that pins the result for
`f == g`, `f < g` and `e == e2`, so the "every test stays green" gap closes. Then delete the
`binary.cpp:8-9` comment and the `src/AGENTS.md` sentence about the include.

## Info

### IN-01: The documented binder call order does not match the core-file order it claims to mirror

**File:** `src/AGENTS.md:656-657`, `src/lua_runner/lua_runner.cpp:113-126`

**Issue:** The doc says the binders are "called in the order of the core files they mirror".
`lua_runner.cpp` calls `bind_describe` between `bind_delete` and `bind_metadata`, but the
core-file listing at `src/AGENTS.md:37-39` puts `database_describe.cpp` after
`database_time_series.cpp`, and the order is not alphabetical either. The order also does not
matter: every name is registered exactly once (verified above).

**Fix:** Drop the ordering claim ("hands it to the fourteen binders as `bind`"), or move
`bind_describe` after `bind_time_series` so it matches the listing.

---

_Reviewed: 2026-10-04_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
