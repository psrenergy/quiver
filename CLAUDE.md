# Project: PSR Database

SQLite wrapper library with C++ core and language bindings.

## Principles

- This is a WIP project
- No backwards compatibility required
- Breaking changes are acceptable
- Clean code over defensive code
- Simple solutions over complex abstractions
- Delete unused code, do not deprecate
- No compatibility shims or legacy support
- The intelligence should be in the C++ layer, not in the bindings
- The bindings should be thin wrappers over the C++ API

## Build

```bash
cmake --build build --config Debug
```

## Tests

C++ tests:
```bash
./build/bin/psr_database_tests.exe
./build/bin/psr_database_c_tests.exe
```

Julia tests (individual file):
```bash
julia --project=bindings/julia -e "include(\"bindings/julia/test/test_read/test_read.jl\")"
```

## Instructions

- To create the `bindings/julia/src/c_api.jl` run the command `bindings/julia/generator/generator.bat`
- To create the `bindings/dart/lib/src/ffi/bindings.dart` run the command `bindings/dart/generator/generator.bat`
- If Julia manifest has version conflicts, delete `bindings/julia/Manifest.toml` and run `julia --project=bindings/julia -e "using Pkg; Pkg.instantiate()"`


## Patterns

- Pimpl for ABI stability
- Factory methods: `from_migrations()`, `from_schema()`
- RAII for resource management
- Move semantics, no copying for Database class
