# Quiver DB

[![CI](https://github.com/psrenergy/quiver/actions/workflows/ci.yml/badge.svg)](https://github.com/psrenergy/quiver/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/psrenergy/quiver/graph/badge.svg?token=TwhY6YNDNN)](https://codecov.io/gh/psrenergy/quiver)

Quiver DB is a schema-first SQLite wrapper for decision support models. Define your schema in plain SQL, and Quiver gives you typed collections with scalars, vectors, sets, and time series — plus parameterized queries, CSV import/export, transaction control, and built-in migration support. Implemented in C++ with bindings for **Julia**, **Dart**, **Python**, and embedded **Lua** scripting.

## Features

- **Schema-first** — define tables in `.sql` files, Quiver introspects them at runtime
- **Typed collections** — integer, float, and string scalars with automatic type marshaling
- **Vectors and sets** — first-class array and set attributes per element
- **Time series** — multi-column time series with dimension ordering
- **Parameterized queries** — type-safe positional `?` parameters
- **CSV import/export** — round-trip data with enum resolution and date formatting
- **Transaction control** — explicit begin/commit/rollback with nest-aware RAII
- **Lua scripting** — run Lua scripts with full database access
- **Multi-language** — consistent API across C++, Julia, Dart, Python, and Lua

## Quick Start

### C++

```cpp
#include <quiver/database.h>
#include <quiver/element.h>

auto db = quiver::Database::from_schema("my.db", "schema.sql");

quiver::Element el;
el.set("label", std::string("Item 1")).set("value", int64_t{42});
db.create_element("Collection", el);

auto values = db.read_scalar_integers("Collection", "value");
// values == {42}
```

### Julia

```julia
using Quiver

db = Quiver.from_schema("my.db", "schema.sql")

Quiver.create_element!(db, "Collection"; label="Item 1", value=42)

values = Quiver.read_scalar_integers(db, "Collection", "value")
# values == [42]

Quiver.close!(db)
```

### Dart

```dart
import 'package:quiverdb/quiverdb.dart';

final db = Database.fromSchema('my.db', 'schema.sql');

db.createElement('Collection', {'label': 'Item 1', 'value': 42});

final values = db.readScalarIntegers('Collection', 'value');
// values == [42]

db.close();
```

### Python

```python
from quiverdb import Database

db = Database.from_schema("my.db", "schema.sql")

db.create_element("Collection", label="Item 1", value=42)

values = db.read_scalar_integers("Collection", "value")
# values == [42]

db.close()
```

### Lua (embedded)

```lua
db:create_element("Collection", { label = "Item 1", value = 42 })

local values = db:read_scalar_integers("Collection", "value")
-- values == {42}
```

## Build

All dependencies (SQLite, spdlog, sol2, GoogleTest, etc.) are fetched automatically by CMake — no manual installs required.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

To build everything and run all tests:

```bash
scripts/build-all.bat            # Debug build + all tests
scripts/build-all.bat --release  # Release build + all tests
```

## Documentation

See the [`docs/`](docs/) folder:

- [Introduction](docs/introduction.md) — SQLite basics and tooling
- [Attributes](docs/attributes.md) — scalars, vectors, sets
- [Time Series](docs/time_series.md) — time series tables and operations
- [Migrations](docs/migrations.md) — schema versioning
- [Rules](docs/rules.md) — schema conventions and validation

## License

[MIT](LICENSE)
