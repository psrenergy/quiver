# Milestones

## v1.0 — baseline (pre-GSD)

**Shipped:** before 2026-05-19

Quiver's pre-GSD baseline. Captures the codebase as it existed when GSD was bootstrapped on 2026-05-19. Full history lives in git; see `git log`.

**Capabilities:**
- Database CRUD, transactions, scalar/vector/set/time-series reads + updates, parameterized queries, CSV import/export, Lua scripting
- Binary subsystem (`.qvr` files + `.toml` metadata, iteration helpers, CSV converter)
- Expression subsystem (lazy DAG over `.qvr`, binary/unary/ternary/aggregate/label-axis nodes)
- C API mirroring the C++ surface with binary error codes + `quiver_get_last_error`
- Bindings for Julia, Dart, Python (CFFI), Lua

Locked validated requirements: see `.planning/PROJECT.md` Validated section.

## v1.1 — add_time_series_row (active)

**Started:** 2026-05-19

See `.planning/PROJECT.md` Current Milestone section and `.planning/REQUIREMENTS.md`.
