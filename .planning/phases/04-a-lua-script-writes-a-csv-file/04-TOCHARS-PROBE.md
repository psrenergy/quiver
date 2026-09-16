# Phase 4 — `std::to_chars` non-finite spot-check (EXECUTED)

CONTEXT.md `<specifics>` flagged this as **the single MEDIUM-confidence claim in the entire
research body** — "verified against standards text, never executed." It has now been executed.

Probe source: `scratchpad/tochars_probe.cpp`. Built and run on this machine, 2026-09-16.

## Finding 1 — `std::to_chars` SUCCEEDS on a non-finite double. It does not error.

| input | `isfinite` | `to_chars` result | text written |
|---|---|---|---|
| `0.0/0.0` | 0 | `ec == std::errc{}` (**OK**) | see Finding 2 |
| `1.0/0.0` | 0 | `ec == std::errc{}` (**OK**) | `inf` |
| `-1.0/0.0` | 0 | `ec == std::errc{}` (**OK**) | `-inf` |

**This is the load-bearing result.** `to_chars` returns success and writes a token. There is no
error code to check, no exception, nothing that would stop the value. Without FMT-05's explicit
`std::isfinite` guard, `inf` / `-inf` / `-nan(ind)` land in a CSV cell **silently**.

FMT-05's guard is not belt-and-braces — it is the only thing standing between a NaN and the file.

## Finding 2 — the platform divergence is real, and it is WORSE than documented

REQUIREMENTS FMT-05 justifies the guard with "MSVC and libstdc++ render those differently". True —
but the divergence does not need two platforms. **Two compilers on this one machine, sharing the
same MSVC STL, disagree on the sign of the identical expression:**

| expression | MSVC `cl.exe` 14.51 | `clang++` (MSVC STL) |
|---|---|---|
| `0.0/0.0` | `-nan(ind)` (9 chars) | `nan` (3 chars) |
| `-(0.0/0.0)` | `nan` (3 chars) | `-nan(ind)` (9 chars) |
| `1.0/0.0` | `inf` | `inf` |
| `-1.0/0.0` | `-inf` | `-inf` |

The sign is **inverted between the two compilers for the same source expression**. A fixture that
asserted on the emitted NaN text would pass on one compiler and fail on the other, in the same
checkout. This strengthens FMT-05 rather than weakening it: there is no correct NaN token to emit,
so refusing to emit one is the only stable answer.

## Finding 3 — TEST-07's premise confirmed empirically

```
int64  9007199254740993 -> [9007199254740993]
double 9007199254740993 -> [9007199254740992]   <-- the digit that disappears
```

The int64/double split is exactly as FMT-04 describes. TEST-07 is a real regression test, not a
theoretical one.

## Finding 4 — `append_number`'s 32-byte stack buffer is adequate

| value | emitted length |
|---|---|
| `-DBL_MAX` (`-1.7976931348623157e+308`) | 24 |
| min denormal (`5e-324`) | 6 |
| `2014.0` | 4 (`2014` — confirms D-34: no synthetic `.0`) |

Worst observed is 24 of 32. The existing buffer needs no change when the helper moves to
`src/utils/number.h` under D-38.

## Consequences for the plan

1. FMT-05's `std::isfinite` guard must run **before** `to_chars`, never as a check on its return —
   there is no failure return to check.
2. No test may assert on emitted NaN/inf *text*. Assert that the write **throws**. (Finding 2 makes
   any text assertion compiler-dependent.)
3. D-34 confirmed by execution: `2014.0` emits `2014`.
4. The 32-byte buffer moves unchanged.
