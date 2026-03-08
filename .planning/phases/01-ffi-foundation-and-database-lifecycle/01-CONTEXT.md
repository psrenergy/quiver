# Phase 1: FFI Foundation and Database Lifecycle - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Load native libraries via bun:ffi, establish error handling and memory helpers, and provide Database.fromSchema() / Database.fromMigrations() / db.close() to JS/TS users. Covers requirements FFI-01 through FFI-04 and LIFE-01 through LIFE-03.

</domain>

<decisions>
## Implementation Decisions

### Library loading strategy
- Auto-detect DLLs from package location first (relative to node_modules), then fall back to system PATH
- Lazy loading: DLLs loaded on first Database.fromSchema() or fromMigrations() call, not on import
- On failure, throw QuiverError with a message listing all searched paths and the missing library name
- No standalone health-check function (users discover DLL issues on first database call)

### Binding file organization
- Multi-file by topic: loader.ts, errors.ts, database.ts, ffi-helpers.ts (Phase 2-4 add create.ts, read.ts, query.ts, etc.)
- Located at bindings/js/ in the repo (follows bindings/julia/, bindings/dart/, bindings/python/ pattern)
- npm package named `quiverdb` (matches Python package name)
- Single Database class with methods added directly in later phases (no mixins or multiple inheritance)

### Database close semantics
- Track `_closed` boolean in the Database instance
- Throw QuiverError("Database is closed") on any method call after close()
- close() itself is idempotent — second call is a silent no-op
- Symbol.dispose deferred to future milestone (EXT-07)
- Native pointer stays private — no public getter

### Claude's Discretion
- Error handling pattern: check() helper design (matches established Julia/Dart/Python pattern)
- Out-parameter helper implementation details (FFI-03)
- Type conversion helpers for BigInt, C strings, typed arrays (FFI-04)
- Exact auto-detect search paths for DLL discovery
- Internal module structure within topic files

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. The binding should follow established patterns from existing Julia/Dart/Python wrappers.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- Python `_loader.py`: Bundled + PATH fallback pattern with Windows `os.add_dll_directory()` — model for JS loader
- Dart `library_loader.dart`: Cached lazy loading with `_cachedLibrary` pattern — model for lazy init
- Dart `exceptions.dart` / Python `_helpers.py`: `check(err)` helper pattern for error handling
- All bindings: `_isClosed` / `_closed` guard pattern for post-close protection

### Established Patterns
- Every binding uses a `check(err)` function: if err != QUIVER_OK, read quiver_get_last_error(), throw typed exception
- Factory methods (fromSchema, fromMigrations) allocate out-parameter, call C API, wrap returned pointer
- Windows requires pre-loading libquiver.dll before libquiver_c.dll (dependency chain)
- Cross-layer naming: C snake_case → JS/TS camelCase (documented in CLAUDE.md)

### Integration Points
- C API headers in include/quiver/c/ define all function signatures to bind
- include/quiver/c/options.h defines quiver_database_options_t and defaults
- include/quiver/c/database.h defines lifecycle functions (from_schema, from_migrations, close)
- build/bin/ contains compiled DLLs (libquiver.dll, libquiver_c.dll)

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-ffi-foundation-and-database-lifecycle*
*Context gathered: 2026-03-08*
