# Phase 5: Package and Distribution - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Make the JS/TS binding a well-structured npm package with TypeScript types, complete test suite, development tooling, and proper package metadata. Covers requirements PKG-01 (npm installable), PKG-02 (TypeScript types), PKG-03 (test suite with bun:test), PKG-04 (test runner with PATH/DLL setup).

</domain>

<decisions>
## Implementation Decisions

### Distribution format
- Ship raw .ts source files (no compile step) — Bun runs .ts natively
- Consumers must use Bun (already a requirement for this binding)
- Use `files` field in package.json to whitelist: src/, package.json, tsconfig.json, README.md
- DLL discovery via PATH only — consumers must have libquiver.dll and libquiver_c.dll in PATH. Document in README.
- Package entry points: keep `main`, add `exports` map (`".": "./src/index.ts"`) and `types` field (`"./src/index.ts"`)

### Package metadata
- License: MIT
- Add `engines` field: `{ "bun": ">=1.0.0" }` to enforce Bun-only
- Full metadata: description, repository, homepage, bugs, keywords
- Description: technical/direct style (e.g., "Bun-native SQLite wrapper binding for Quiver via bun:ffi")
- Keywords: sqlite, database, ffi, bun, quiver

### Test suite
- Existing 54 tests across 5 files are sufficient — no new tests needed
- Both `test.bat` and `bun test` should work
- Use `bunfig.toml` with `[test] preload = ["./test/setup.ts"]` for standalone `bun test` support
- setup.ts handles DLL PATH setup so tests work without the .bat wrapper
- test.bat remains as the official runner (handles PATH explicitly)

### Dev tooling
- Usage-focused README.md: installation, DLL setup, quick start example, API methods list
- Add `"typecheck": "tsc --noEmit"` script to package.json
- Add Biome for lint + format with strict/recommended rules
- Fix any existing violations as part of this phase
- Add `"lint": "biome check"` and `"format": "biome format --write"` scripts

### Claude's Discretion
- Biome configuration details (specific rule overrides if needed)
- README structure and exact wording
- bunfig.toml additional settings beyond test preload
- setup.ts implementation for DLL PATH resolution
- Exact keywords and repository URL format

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. Follow established patterns from existing Julia/Dart/Python binding packaging.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `package.json`: Already has name (quiverdb), version (0.5.0), type (module), main, exports, test script
- `tsconfig.json`: Already configured with strict, ESNext, bundler resolution, bun-types
- `test/test.bat`: Already handles PATH setup (`set PATH=%~dp0..\..\..\build\bin;%PATH%`)
- `index.ts`: Already exports Database, QuiverError, and all public types
- `types.ts`: Already defines ScalarValue, ArrayValue, Value, ElementData, QueryParam

### Established Patterns
- Dart binding: pubspec.yaml with full metadata, test.bat for PATH setup
- Python binding: test.bat prepends build/bin/ to PATH
- All bindings: test scripts in `bindings/{lang}/test/test.bat` with PATH setup
- bun:ffi: `import { dlopen } from "bun:ffi"` — DLLs must be in system PATH or explicit path

### Integration Points
- `build/bin/`: Contains compiled DLLs (libquiver.dll, libquiver_c.dll)
- `bindings/js/test/test.bat`: Entry point for test execution
- `scripts/build-all.bat` and `scripts/test-all.bat`: May need updating to include JS tests

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-package-and-distribution*
*Context gathered: 2026-03-09*
