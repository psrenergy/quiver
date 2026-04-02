# Concerns

## Technical Debt

### QUIVER_REQUIRE Macro (Low Impact)
`src/c/internal.h` — Manual arity dispatch via `QUIVER_REQUIRE_1` through `QUIVER_REQUIRE_9` macros. Works but is verbose (~90 lines of boilerplate). Could be replaced with a variadic template or fold expression, but functional as-is.

### BinaryMetadata::from_element TOML Round-Trip (Medium Impact)
`src/binary/binary_metadata.cpp` — `from_element()` serializes to TOML then deserializes back, adding unnecessary overhead. Should construct `BinaryMetadata` directly from the element data.

### Time Dimension Builder Limitation (Low Impact)
`BinaryMetadata::add_time_dimension()` cannot express multi-frequency hierarchies (e.g., monthly within yearly). Builder API only supports simple single-parent relationships.

### CSV Import Code Duplication (Medium Impact)
`src/database_csv_import.cpp` — Three near-identical code paths for vector/set/time-series import. Could be unified into a single templated or parameterized flow.

## Known Bugs / Incomplete Features

### TimeFrequency Weekly/Yearly Inner Dimensions
`src/binary/time_properties.cpp` — `TimeFrequency::Weekly` and `TimeFrequency::Yearly` throw `std::logic_error` when used as inner time dimensions. Incomplete implementation — only `Monthly`, `Daily`, `Hourly` are fully supported for nested hierarchies.

### Issue 70 Regression Coverage
Issue 70 regression is only covered in Dart tests, not in C++ test suite. Risk of re-regression if C++ layer changes.

## Security

### CSV Import FK Constraint Disable
`src/database_csv_import.cpp` — Uses `PRAGMA foreign_keys = OFF` during import for performance. If an exception occurs between disable and re-enable, FK constraints remain disabled for the connection lifetime. Should use RAII guard to ensure re-enable on all exit paths.

## Performance

### Binary File Hot Path (Documented in CLAUDE.md)
Profiled with 480x500x31 dimensions (~7.3M read/write calls):

1. **`unordered_map<string, int64_t>` dims parameter (~40%):** Every read/write constructs a hash map. Fix: add `vector<int64_t>` overloads (dimension-order values, dot-product positioning).

2. **`validate_dimension_values` (~19%):** Called unconditionally on every read/write. Fix: make validation opt-in or skippable for trusted callers (e.g., `next_dimensions()` iteration).

3. **`vector<double>` allocation per read (~3%):** Each `read()` returns a new vector. Fix: add `read_into()` overload writing to caller-provided buffer.

4. **`next_dimensions` allocation:** Returns a new vector per iteration instead of mutating in-place.

## Fragile Areas

### Write Registry (Thread Safety)
`src/binary/binary_file.cpp` — Process-global `static std::unordered_set<std::string>` tracks files open for writing. No mutex protection — not thread-safe. Also not multi-process safe. Safe only for single-threaded use.

### Binary File Position Cache
`go_to_position` uses a single cached position for both read and write heads on an `fstream`. Could lead to incorrect positioning if read/write interleave (currently prevented by separate read/write file modes).

## Test Coverage Gaps

- **Python:** No binary subsystem tests (BinaryFile, BinaryMetadata, CSVConverter)
- **Thread safety:** No tests for write registry concurrent access
- **Issue 70:** Not covered in C++ test suite (only Dart)
- **Multi-process:** No concurrency tests for file-level locking

## Dependencies

All dependencies are pinned to specific versions via FetchContent. No known CVEs in current versions. SQLite is the only PUBLIC dependency — all others are PRIVATE to the build.
