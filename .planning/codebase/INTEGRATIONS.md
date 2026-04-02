# Integrations

## Database Engine

**SQLite 3.50.2** — Embedded, zero-config relational database.

- Linked as PUBLIC dependency (consumers see sqlite3 headers)
- Used via raw `sqlite3_*` C API in `src/database_impl.h`
- STRICT mode enforced on all tables
- Foreign key constraints enabled at runtime
- WAL mode available via `DatabaseOptions`
- Schema introspected at open via `Schema::from_database(db)`

## File I/O

### Binary File System (`.qvr` files)
- Custom binary format for multi-dimensional numeric data
- TOML metadata sidecars (`.toml`) via toml++ library
- `BinaryFile` class handles file positioning, read/write of `double` arrays
- In-process write registry prevents concurrent write access (not thread-safe)

### CSV Import/Export
- rapidcsv library for CSV parsing/writing
- Round-trip support: export then import preserves data
- Configurable delimiters, enum label resolution, date formatting via `CSVOptions`

## Scripting Engine

**Lua 5.4.8 via sol2 3.5.0** — Embedded scripting for database operations.

- `LuaRunner` class exposes database methods to Lua scripts
- All database read/write operations available as `db:method_name()` in Lua
- sol2 provides type-safe C++/Lua bridging with safety options (`SOL_SAFE_NUMERICS`, `SOL_SAFE_FUNCTION`)

## External APIs

None. Quiver is a fully self-contained library with no network dependencies, no authentication providers, no webhooks, and no cloud service integrations. All data is local (SQLite files, binary files, CSV files).

## FFI Boundary

The C API (`libquiver_c`) serves as the universal FFI surface:

```
Julia ─────┐
Dart ──────┤
Python ────┼──► C API (libquiver_c.dll) ──► C++ Core (libquiver.dll)
Lua ───────┘ (embedded, uses sol2 directly)
```

- Thread-local error storage (`quiver_set_last_error`)
- Binary return codes (`QUIVER_OK`/`QUIVER_ERROR`)
- Manual memory management with matching `_free_*` functions
- Opaque handle pattern for `Database`, `Element`, `BinaryFile`, `BinaryMetadata`
