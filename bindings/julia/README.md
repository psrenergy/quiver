# Quiver.jl

[![CI](https://github.com/psrenergy/Quiver.jl/actions/workflows/CI.yml/badge.svg)](https://github.com/psrenergy/Quiver.jl/actions/workflows/CI.yml)
[![codecov](https://codecov.io/gh/psrenergy/Quiver.jl/graph/badge.svg?token=RIZWL1OPRT)](https://codecov.io/gh/psrenergy/Quiver.jl)

Choose a log file with the `log_path` keyword on `Quiver.open`, `Quiver.from_schema`, or
`Quiver.from_migrations`, including their `do` forms:

```julia
using Quiver

Quiver.open("study.db"; log_path = "logs/study.log") do db
    # Use the database.
end
```

Relative log paths start in the database directory (`pwd()` for `:memory:`). An explicit path
also enables file logging for read-only and in-memory databases. Parent directories are created,
the log is truncated on each open, and a destination that cannot be opened raises a
`DatabaseException`. The database file and its SQLite journal files cannot be log destinations.
Omitting `log_path`, or passing `nothing` or `""`, keeps the default `quiver_database.log`
for writable databases and console-only logging for read-only/in-memory databases. Lua `print()`
uses the selected destination; `console_level` controls only console output.
