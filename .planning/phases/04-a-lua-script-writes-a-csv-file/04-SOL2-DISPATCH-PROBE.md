# Phase 4 — sol2 cell-dispatch probe (EXECUTED)

Written to settle a review claim **before** it could be "fixed". Compiled and run against this
repo's own `build/_deps/sol2-src` and `build/_deps/lua-build`, with the project's real flags
(`SOL_SAFE_NUMERICS=1`, `SOL_SAFE_FUNCTION=1`, `src/CMakeLists.txt:61-62`).

Source: `04-sol2-dispatch-probe.cpp` (this directory).

## The claim under test

A reviewer argued that the plan's cell-dispatch order — nil → boolean → `is<std::int64_t>` →
`is<double>` → `is<std::string>` — silently renumbers string cells, because Lua's string→number
coercion would make `is<int64_t>()` true for `"0012"`. Cited as support: the comment at
`src/lua_runner.cpp:977-979`, which says `get_type()` is used *"rather than lua_cell_as … This also
rules out a quoted \"2\", which Lua's own string->number coercion would otherwise let through."*

## Result — the claim is FALSE

| cell | lua type | `is<int64_t>` | `is<double>` | `is<std::string>` | plan order emits |
|---|---|---|---|---|---|
| `"0012"` | string | **0** | **0** | 1 | `0012` ✓ verbatim |
| `"1e3"` | string | **0** | **0** | 1 | `1e3` ✓ verbatim |
| `"12"` | string | **0** | **0** | 1 | `12` ✓ verbatim |
| `42` | number | 1 | 1 | 0 | `42` ✓ integer path |
| `3.5` | number | 0 | 1 | 0 | `3.5` ✓ double path |
| `true` | boolean | 0 | 0 | 0 | handled by the boolean branch |

**Key-walk half (FMT-08 row width):**

```
key type=number   is<int64>=1  -> counts as an index
key type=number   is<int64>=1  -> counts as an index
key type=string   is<int64>=0  -> correctly rejected as a non-integer key
```

## Why the comment does not say what it appeared to say

`SOL_SAFE_NUMERICS=1` is exactly the switch that stops `is<>` from honouring Lua's string→number
coercion. The comment at `:977-979` is about **`lua_cell_as` / `as<>`**, which *converts* — and
conversion does apply the coercion. `is<>` *checks*, and under this build's flags it does not.
The two are not interchangeable, which is the whole reason that comment exists.

## Consequences

1. The dispatch order in the plan is **correct as written**. A string cell reaches
   `is<std::string>` and is copied verbatim, satisfying FMT-09's "no transcoding, normalization or
   validation" and the must_have "a string cell writes verbatim".
2. Integer-before-double correctly preserves Lua 5.4's integer subtype, which is what FMT-04 and
   TEST-07 require.
3. The copied `append_json_table` key walk correctly rejects a string key, so the FMT-08 must_have
   ("a non-integer row key …") holds without extra guards.
4. **Do not add `get_type()` guards to the cell dispatch.** They would be dead code, and this
   document exists so the concern is not re-raised and "fixed" by a later reviewer.
