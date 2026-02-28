# Deferred Items - Phase 02

## Pre-existing Dart Generator Issue

**Discovered during:** Plan 02, Task 2 (test suite validation)
**Severity:** Dart tests fail to compile
**Root cause:** `quiver_database_options_default()` and `quiver_csv_options_default()` are missing from the generated Dart FFI bindings (`bindings/dart/lib/src/ffi/bindings.dart`). The Dart code generator did not pick up these functions when regenerated during Plan 01.
**Impact:** All 15 Dart tests fail to load (compilation error, not runtime error)
**Unrelated to:** The `quiver_element_free_string` -> `quiver_database_free_string` rename
**Fix:** Re-run Dart generator or manually add the missing function declarations to bindings.dart
