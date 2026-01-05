# CLAUDE.md

This file provides guidance for Claude Code when working with this repository.

## Project Overview

**psr_database** is a cross-platform C++17 SQLite wrapper library with migration support and a C API for FFI integration.

## Quick Reference

```bash
# Build (release with C API)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPSR_BUILD_C_API=ON
cmake --build build --config Release

# Test
ctest --test-dir build -C Release --output-on-failure

# Format code
find include src tests -name '*.cpp' -o -name '*.h' | xargs clang-format -i

# Install
cmake --install build --prefix /usr/local
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `PSR_BUILD_SHARED` | ON | Build shared library (.dll/.so) |
| `PSR_BUILD_TESTS` | ON | Build GoogleTest suite |
| `PSR_BUILD_C_API` | OFF | Build C API wrapper |

## Dependencies

Managed via CMake FetchContent in `cmake/Dependencies.cmake`:

- **SQLite** (v3.47.2) - Database engine
- **toml++** (v3.4.0) - TOML parsing
- **spdlog** (v1.15.0) - Logging
- **GoogleTest** (v1.15.2) - Testing

## Project Structure

```
database/
├── include/psr/        # Public headers
│   ├── database.h      # C++ Database class
│   ├── result.h        # C++ Result/Row/Value
│   ├── export.h        # DLL export macros
│   └── c/              # C API headers
│       ├── database.h
│       └── result.h
├── src/                # Implementation
│   ├── database.cpp
│   ├── result.cpp
│   └── c_api.cpp       # C API wrapper
├── tests/
├── cmake/
│   ├── Dependencies.cmake
│   ├── CompilerOptions.cmake
│   └── Platform.cmake
└── .github/workflows/ci.yml
```

## Architecture

```
┌─────────────────────────────────────────┐
│          C API (src/c_api.cpp)          │
│        include/psr/c/ - extern "C"      │
├─────────────────────────────────────────┤
│        Core C++ Library (src/)          │
│   psr::Database, psr::Result, psr::Row  │
├─────────────────────────────────────────┤
│         Dependencies                    │
│   SQLite | spdlog | toml++              │
└─────────────────────────────────────────┘
```

## Code Conventions

- **C++ Standard**: C++17
- **Namespace**: `psr`
- **Naming**: snake_case for functions/variables, PascalCase for classes
- **Formatting**: clang-format (see .clang-format)

## Key Classes

### C++ API

- `psr::Database` - Connection wrapper with migrations
  - `Database(path)` - Open database
  - `Database::from_schema(db_path, schema_path)` - Open with migrations
  - `execute(sql)` / `execute(sql, params)` - Run queries
  - `current_version()` / `set_version(v)` - Schema version
  - `migrate_up()` - Apply pending migrations
  - `begin_transaction()` / `commit()` / `rollback()`
- `psr::Result` - Query result container (iterable)
- `psr::Row` - Single row (get_int, get_string, get_blob, etc.)
- `psr::Value` - Variant: `nullptr_t | int64_t | double | string | vector<uint8_t>`

### C API

- Handles: `psr_database_t*`, `psr_result_t*`
- Error codes: `PSR_OK`, `PSR_ERROR_*`, `PSR_ERROR_MIGRATION`
- Functions: `psr_database_*`, `psr_result_*`
- Migration: `psr_database_from_schema()`, `psr_database_current_version()`, `psr_database_migrate_up()`

## Migration System

Schema migrations use numbered folders with `up.sql` files:

```
schema/
├── 1/up.sql
├── 2/up.sql
└── 3/up.sql
```

- Version tracked via `PRAGMA user_version`
- Each migration runs in a transaction
- Rollback on failure

## Adding Features

1. Add to `include/psr/` and `src/`
2. Expose via C API in `include/psr/c/` and `src/c_api.cpp`
3. Add tests in `tests/`
4. Update documentation

## Platform Notes

- **Windows**: DLLs in `build/bin/`
- **Linux/macOS**: Shared libs in `build/lib/`
