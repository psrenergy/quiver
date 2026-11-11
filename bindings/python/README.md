# quiverdb

Choose a log file with the `log_path` keyword on `Database.open`, `Database.from_schema`, or
`Database.from_migrations`:

```python
from quiverdb import Database

with Database.open("study.db", log_path="logs/study.log") as db:
    print(db.current_version())
```

Relative log paths start in the database directory (the current directory for `:memory:`).
An explicit path also enables file logging for read-only and in-memory databases. Parent
directories are created, the log is truncated on each open, and a destination that cannot be
opened raises `QuiverError`. The database file and its SQLite journal files cannot be log
destinations. Omitting `log_path`, or passing `None` or `""`, keeps the default
`quiver_database.log` for writable databases and console-only logging for read-only/in-memory
databases. Lua `print()` uses the selected destination; `console_level` controls only console
output.
