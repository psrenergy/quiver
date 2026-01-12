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
- All the *.sql tests should be in the `tests/` folder, and the bindings should refer to them from there

## Build

```bash
cmake --build build --config Debug
```

## Build and Test All

```bash
build-all.bat            # Build everything and run all tests (Debug)
build-all.bat --release  # Build in Release mode
test-all.bat             # Run all tests (assumes already built)
```

## Tests

C++ tests:
```bash
./build/bin/psr_database_tests.exe
./build/bin/psr_database_c_tests.exe
```

Julia tests:
```bash
bindings/julia/test/test.bat
```

Dart tests:
```bash
bindings/dart/test/test.bat
```

## Bindings

### Regenerating FFI Bindings

When the C API changes (new functions, changed signatures), regenerate the bindings:

```bash
# Julia
bindings/julia/generator/generator.bat

# Dart
bindings/dart/generator/generator.bat
```

### Julia

- If manifest has version conflicts, delete `bindings/julia/Manifest.toml` and run:
  ```bash
  julia --project=bindings/julia -e "using Pkg; Pkg.instantiate()"
  ```

### Dart

- DLL dependencies: `libpsr_database_c.dll` depends on `libpsr_database.dll` - both must be in PATH or same directory
- The test.bat script handles PATH setup automatically

## Patterns

- Pimpl for ABI stability
- Factory methods: `from_migrations()`, `from_schema()`
- RAII for resource management
- Move semantics, no copying for Database class
