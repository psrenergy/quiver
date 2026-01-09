# Project: PSR Database

SQLite wrapper library with C++ core and language bindings.

## Principles

- This is a WIP project. No backwards compatibility required. Breaking changes are acceptable.
- Clean code over defensive code
- Simple solutions over complex abstractions
- Delete unused code, don't deprecate
- No compatibility shims or legacy support

## Architecture

```
include/psr/       C++ public headers
src/               C++ implementation
bindings/          Language bindings (Lua, Julia)
```

## Build

```bash
cmake --build build --config Debug
```

## Patterns

- Pimpl for ABI stability
- Factory methods: `from_migrations()`, `from_schema()`
- RAII for resource management
- Move semantics, no copying for Database class
