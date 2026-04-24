<p align="center">
  <img src="assets/logo.svg" alt="Quiver logo" width="200">
</p>

# Quiver DB

[![CI](https://github.com/psrenergy/quiver/actions/workflows/ci.yml/badge.svg)](https://github.com/psrenergy/quiver/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/psrenergy/quiver/graph/badge.svg?token=TwhY6YNDNN)](https://codecov.io/gh/psrenergy/quiver)

Schema-first SQLite wrapper for decision support models. Define tables in plain SQL and get typed scalars, vectors, sets, and time series — with parameterized queries, CSV import/export, and transactions.

## Install

| Language   | Package                                                                |
| ---------- | ---------------------------------------------------------------------- |
| Julia      | [Quiver.jl](https://github.com/psrenergy/Quiver.jl)                    |
| Python     | [`quiverdb`](https://pypi.org/project/quiverdb/) on PyPI               |
| JavaScript | [`@psrenergy/quiver`](https://jsr.io/@psrenergy/quiver) on JSR         |
| Dart       | `quiverdb` (pub)                                                       |
| C++ / Lua  | build from source                                                      |

## Example

```python
from quiverdb import Database

db = Database.from_schema("my.db", "schema.sql")
db.create_element("Collection", label="Item 1", value=42)

values = db.read_scalar_integers("Collection", "value")
# values == [42]

db.close()
```

The same API — `from_schema`, `create_element`, `read_scalar_integers`, etc. — is available across every binding.
