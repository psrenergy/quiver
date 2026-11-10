<p align="center">
  <img src="assets/logo.svg" alt="Quiver logo" width="200">
</p>

# Quiver DB

[![CI](https://github.com/psrenergy/quiver/actions/workflows/ci.yml/badge.svg)](https://github.com/psrenergy/quiver/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/psrenergy/quiver/graph/badge.svg?token=TwhY6YNDNN)](https://codecov.io/gh/psrenergy/quiver)

Schema-first SQLite wrapper for decision support models. Define tables in plain SQL and get typed scalars, vectors, sets, and time series — with parameterized queries, CSV import/export, and transactions.

| Language   | Package                                                                |
| ---------- | ---------------------------------------------------------------------- |
| Julia      | [Quiver.jl](https://github.com/psrenergy/Quiver.jl)              |
| Python     | [`quiverdb`](https://pypi.org/project/quiverdb/)                 |
| JavaScript | [`quiverdb`](https://www.npmjs.com/package/quiverdb)             |

## Example

```python
from quiverdb import Database

db = Database.from_schema("my.db", "schema.sql")
db.create_element("Collection", label="Item 1", value=42)

values = db.read_scalar_integers("Collection", "value")
# values == [42]

db.close()
```

Pass `log_path="logs/run.log"` to `Database.open`, `from_schema`, or `from_migrations` to
choose the log file (`logPath` in Dart/JavaScript). Relative paths start at the database's
directory; `:memory:` uses the current directory. An explicit path enables file logging even
in read-only/in-memory mode, creates missing parent directories, and raises if the log cannot
be opened. The file is truncated on each open. Without a path, writable databases use
`quiver_database.log` beside the database, while read-only/in-memory databases use only the
console. The CLI accepts the same setting as `--log-path logs/run.log`.
