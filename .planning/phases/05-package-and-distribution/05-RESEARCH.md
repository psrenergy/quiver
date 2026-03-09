# Phase 5: Package and Distribution - Research

**Researched:** 2026-03-09
**Domain:** npm packaging, TypeScript types, Bun test configuration, Biome linting/formatting
**Confidence:** HIGH

## Summary

Phase 5 transforms the existing JS/TS binding (already functionally complete with 61 tests across 5 files) into a well-structured npm package. The codebase already has a working `package.json`, `tsconfig.json`, `index.ts` with proper exports, and typed APIs -- the work is primarily additive: completing package metadata, adding `bunfig.toml` for standalone test support, creating a test preload script for DLL PATH setup, adding Biome for lint/format, and writing a usage-focused README.

The existing code ships raw `.ts` source files (no compile step) since Bun runs TypeScript natively. All public types (`ScalarValue`, `ArrayValue`, `Value`, `ElementData`, `QueryParam`) are already exported from `index.ts`. The `declare module` augmentation pattern used for `Database` methods means all methods are already fully typed.

This is a packaging and tooling phase, not a code logic phase. Risk is low. The main technical task is ensuring the test preload script (`setup.ts`) correctly prepends `build/bin/` to `process.env.PATH` on Windows so DLLs load without the `.bat` wrapper.

**Primary recommendation:** Execute as a single plan since all tasks are small, independent file additions/modifications with no complex dependencies.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Ship raw .ts source files (no compile step) -- Bun runs .ts natively
- Consumers must use Bun (already a requirement for this binding)
- Use `files` field in package.json to whitelist: src/, package.json, tsconfig.json, README.md
- DLL discovery via PATH only -- consumers must have libquiver.dll and libquiver_c.dll in PATH. Document in README.
- Package entry points: keep `main`, add `exports` map (`".": "./src/index.ts"`) and `types` field (`"./src/index.ts"`)
- License: MIT
- Add `engines` field: `{ "bun": ">=1.0.0" }` to enforce Bun-only
- Full metadata: description, repository, homepage, bugs, keywords
- Description: technical/direct style (e.g., "Bun-native SQLite wrapper binding for Quiver via bun:ffi")
- Keywords: sqlite, database, ffi, bun, quiver
- Existing 54 tests across 5 files are sufficient -- no new tests needed (NOTE: actual count is 61 tests)
- Both `test.bat` and `bun test` should work
- Use `bunfig.toml` with `[test] preload = ["./test/setup.ts"]` for standalone `bun test` support
- setup.ts handles DLL PATH setup so tests work without the .bat wrapper
- test.bat remains as the official runner (handles PATH explicitly)
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

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| PKG-01 | Binding is an npm package installable via `bun add` | package.json metadata, `files` whitelist, `exports`/`types` fields, `engines` field |
| PKG-02 | Package includes TypeScript type definitions | Already complete -- raw .ts source with strict tsconfig, `types` field in package.json pointing to `./src/index.ts` |
| PKG-03 | Test suite covers all bound operations using `bun:test` | Already complete -- 61 tests across 5 files; add `bunfig.toml` for standalone `bun test` support |
| PKG-04 | Test runner script handles PATH/DLL setup for development | test.bat already works; add `test/setup.ts` preload for standalone `bun test`; update test-all.bat |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| @biomejs/biome | ^2.4.6 | Lint + format (combined toolchain) | User decision; fast, single-tool replacement for ESLint+Prettier |
| bun-types | (already in tsconfig) | TypeScript type definitions for Bun APIs | Required for `bun:ffi`, `bun:test` type checking |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| typescript | (system/global) | Type checking via `tsc --noEmit` | `typecheck` script; no compilation needed since Bun runs .ts natively |

### Alternatives Considered
None -- all tools were locked decisions from CONTEXT.md.

**Installation:**
```bash
cd bindings/js
bun add -d @biomejs/biome
```

Note: `bun-types` is already configured in tsconfig.json `types` field. TypeScript is used only for type-checking (`tsc --noEmit`), not compilation.

## Architecture Patterns

### Current Project Structure (what exists)
```
bindings/js/
  package.json         # Exists - needs metadata additions
  tsconfig.json        # Exists - complete, strict mode
  src/
    index.ts           # Exists - exports Database, QuiverError, types
    database.ts         # Exists - Database class
    loader.ts           # Exists - FFI symbol loading, DLL pre-load
    errors.ts           # Exists - QuiverError, check()
    ffi-helpers.ts      # Exists - makeDefaultOptions, allocPointerOut, etc.
    create.ts           # Exists - createElement, deleteElement
    read.ts             # Exists - all read operations
    query.ts            # Exists - queryString/Integer/Float
    transaction.ts      # Exists - beginTransaction/commit/rollback/inTransaction
    types.ts            # Exists - ScalarValue, ArrayValue, etc.
  test/
    test.bat            # Exists - PATH setup + bun test
    database.test.ts    # Exists - 13 tests
    create.test.ts      # Exists - 18 tests
    read.test.ts        # Exists - 13 tests
    query.test.ts       # Exists - 10 tests
    transaction.test.ts # Exists - 7 tests
```

### Files to Create/Modify
```
bindings/js/
  package.json         # MODIFY - add metadata, scripts, files, engines, license
  biome.json           # CREATE - Biome configuration
  bunfig.toml          # CREATE - test preload configuration
  README.md            # CREATE - usage documentation
  test/
    setup.ts           # CREATE - DLL PATH preload for standalone bun test
scripts/
  test-all.bat         # MODIFY - add JS test step
```

### Pattern: declare module Augmentation (already established)
The binding uses TypeScript's `declare module` to augment the `Database` class across files. Each feature module (create.ts, read.ts, query.ts, transaction.ts) adds methods to the `Database` interface. The `index.ts` imports each module to trigger augmentation. This pattern is already fully typed -- no additional type definitions needed.

```typescript
// Example from read.ts - already provides full type signatures
declare module "./database" {
  interface Database {
    readScalarIntegers(collection: string, attribute: string): number[];
    readScalarFloats(collection: string, attribute: string): number[];
    // ... all methods fully typed
  }
}
```

### Pattern: Test Preload for DLL PATH Setup
The `test/setup.ts` preload script runs before any test file. It modifies `process.env.PATH` to include the build output directory so bun:ffi can find the DLLs.

```typescript
// test/setup.ts
import { join, resolve } from "node:path";

// Resolve path: test/setup.ts -> bindings/js/ -> ../../build/bin/
const buildBinDir = resolve(join(import.meta.dir, "..", "..", "..", "build", "bin"));
const currentPath = process.env.PATH ?? "";

if (!currentPath.includes(buildBinDir)) {
  process.env.PATH = `${buildBinDir};${currentPath}`;
}
```

Key considerations:
- Use `import.meta.dir` (Bun-specific, gives directory of current file)
- Resolve relative to `test/setup.ts` up to project root `build/bin/`
- Use `;` as PATH separator (Windows)
- Must run before any import that triggers `dlopen` -- that is the preload guarantee
- The `bunfig.toml` `[test] preload` runs the script before test files are loaded

### Anti-Patterns to Avoid
- **Compiling TypeScript to JS:** Bun runs .ts natively. No build step, no dist/ folder, no .d.ts generation needed.
- **Using `@property` or getters for Bun-specific type exports:** Keep `types` field pointing to the .ts source file.
- **Complex PATH resolution:** The build directory path is deterministic relative to the binding location. Do not try to auto-discover DLLs beyond the known `build/bin/` path.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Linting + formatting | Custom ESLint + Prettier config | Biome (`biome check`, `biome format`) | Single config file, faster, fewer dependencies |
| Test preload/setup | Complex test runner scripts | `bunfig.toml` `[test] preload` | Built-in Bun feature, runs before test files load |
| Type checking | Manual type review | `tsc --noEmit` via typecheck script | Compiler-verified type correctness |

## Common Pitfalls

### Pitfall 1: setup.ts Running After DLL Load
**What goes wrong:** If `setup.ts` modifies `process.env.PATH` but test files import from `src/` at module level, the loader may execute before PATH is set.
**Why it happens:** Module imports are evaluated eagerly. If a test file does `import { Database } from "../src/index"` and index.ts imports loader.ts, the loader runs at import time.
**How to avoid:** `bunfig.toml` `preload` runs the specified script *before* test files are loaded. The preload script modifies PATH, then test files import the binding, and `dlopen` finds the DLLs in PATH. This is the designed behavior -- preload scripts are guaranteed to run first.
**Warning signs:** "Cannot load native library" errors when running `bun test` without the `.bat` wrapper.

### Pitfall 2: Biome Conflicts with Existing Code Style
**What goes wrong:** Biome's strict/recommended rules flag existing code as violating formatting or lint rules.
**Why it happens:** Code was written without a formatter; Biome has opinions about indentation, trailing commas, quote style, semicolons.
**How to avoid:** Run `biome check --write .` after initial configuration to auto-fix all fixable violations. Review unsafe fixes manually. The CONTEXT.md explicitly says "Fix any existing violations as part of this phase."
**Warning signs:** Large number of lint errors on first run.

### Pitfall 3: `files` Field Omitting Required Files
**What goes wrong:** `bun add` installs the package but `import` fails because source files are not included.
**Why it happens:** The `files` field is a whitelist -- only listed patterns are included in the package.
**How to avoid:** Include `src/`, `package.json`, `tsconfig.json`, `README.md` in the `files` array. Test with `npm pack --dry-run` or `bun pm pack` to verify included files.
**Warning signs:** Package installs but importing throws "module not found."

### Pitfall 4: PATH Separator on Windows
**What goes wrong:** `setup.ts` uses `:` as PATH separator on Windows, so DLLs are not found.
**Why it happens:** Unix uses `:`, Windows uses `;` for PATH separators.
**How to avoid:** Since this binding is Bun-only and Bun runs on Windows, use `;` (Windows separator). The project is Windows-first based on all `.bat` scripts and `suffix === "dll"` checks.
**Warning signs:** DLLs not found on Windows despite PATH modification.

### Pitfall 5: Biome v2 Configuration Schema
**What goes wrong:** Using Biome v1 configuration format with v2.x installed causes config errors.
**Why it happens:** Biome v2 changed configuration schema. The `$schema` URL must match the installed version.
**How to avoid:** Use `bunx --bun @biomejs/biome init` to generate the correct schema, or manually use `https://biomejs.dev/schemas/2.0.0/schema.json` or newer.
**Warning signs:** "Invalid configuration" errors from Biome CLI.

## Code Examples

### Complete package.json (target state)
```json
{
  "name": "quiverdb",
  "version": "0.5.0",
  "description": "Bun-native SQLite wrapper binding for Quiver via bun:ffi",
  "type": "module",
  "main": "src/index.ts",
  "types": "src/index.ts",
  "exports": {
    ".": "./src/index.ts"
  },
  "files": [
    "src/",
    "package.json",
    "tsconfig.json",
    "README.md"
  ],
  "engines": {
    "bun": ">=1.0.0"
  },
  "license": "MIT",
  "keywords": ["sqlite", "database", "ffi", "bun", "quiver"],
  "scripts": {
    "test": "bun test",
    "typecheck": "tsc --noEmit",
    "lint": "biome check",
    "format": "biome format --write"
  },
  "devDependencies": {
    "@biomejs/biome": "^2.4.6"
  }
}
```

### bunfig.toml
```toml
[test]
preload = ["./test/setup.ts"]
```

### test/setup.ts (DLL PATH preload)
```typescript
import { join, resolve } from "node:path";

const buildBinDir = resolve(join(import.meta.dir, "..", "..", "..", "build", "bin"));
const currentPath = process.env.PATH ?? "";

if (!currentPath.includes(buildBinDir)) {
  process.env.PATH = `${buildBinDir};${currentPath}`;
}
```

### biome.json (recommended starting point)
```json
{
  "$schema": "https://biomejs.dev/schemas/2.0.0/schema.json",
  "formatter": {
    "enabled": true,
    "indentStyle": "space",
    "indentWidth": 2,
    "lineWidth": 100
  },
  "linter": {
    "enabled": true,
    "rules": {
      "recommended": true
    }
  },
  "javascript": {
    "formatter": {
      "quoteStyle": "double",
      "semicolons": "always"
    }
  },
  "files": {
    "ignore": ["node_modules/"]
  }
}
```

### test-all.bat Addition
Add a JS test step after Dart tests (step 5, renumber Python to 6):
```batch
REM ============================================================
REM Step 5: Run JavaScript Tests
REM ============================================================
echo [5/6] Running JavaScript tests...

call "%ROOT_DIR%\bindings\js\test\test.bat"
if errorlevel 1 (
    SET JS_RESULT=FAIL
    SET FAILED=1
) else (
    SET JS_RESULT=PASS
)
echo.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| ESLint + Prettier (2 tools, complex config) | Biome (single tool, single config) | 2023-2024 | Simpler config, faster execution, fewer dependencies |
| Compile .ts to .js + .d.ts for npm | Ship raw .ts (Bun-native) | Bun 1.0+ | No build step, consumers must use Bun |
| jest / mocha test runners | bun:test (built-in) | Bun 1.0+ | Zero dependency test runner |

**Deprecated/outdated:**
- ESLint + Prettier combination: Biome replaces both with a single tool
- `@types/bun` package: Replaced by `bun-types` (already in tsconfig)

## Open Questions

1. **Biome rule overrides needed?**
   - What we know: Recommended rules are comprehensive; the existing code was written cleanly
   - What is unclear: Whether any specific rules conflict with the `declare module` augmentation pattern or bun:ffi usage
   - Recommendation: Start with recommended rules, run `biome check`, and add specific overrides only if needed. This is within Claude's discretion per CONTEXT.md.

2. **Repository URL format**
   - What we know: Package needs `repository`, `homepage`, `bugs` fields
   - What is unclear: Exact GitHub URL for this project (or if it uses a different host)
   - Recommendation: Use placeholder or omit if not on a public host. Can be filled in later. This is within Claude's discretion.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in, Bun 1.3.10) |
| Config file | bunfig.toml (to be created in this phase) |
| Quick run command | `bun test` (from bindings/js/) |
| Full suite command | `bindings/js/test/test.bat` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| PKG-01 | Package installable via `bun add` | manual | Verify `files` field includes required files, check `bun pm pack` output | N/A - manual verification |
| PKG-02 | TypeScript type definitions present | unit | `cd bindings/js && bun run typecheck` | tsconfig.json exists, script to be added |
| PKG-03 | Test suite passes | unit | `cd bindings/js && bun test` | 5 test files with 61 tests exist |
| PKG-04 | Test runner handles PATH/DLL setup | integration | `bindings/js/test/test.bat` | test.bat exists, setup.ts to be created |

### Sampling Rate
- **Per task commit:** `cd bindings/js && bun test` (quick validation)
- **Per wave merge:** `bindings/js/test/test.bat` (full with PATH setup) + `cd bindings/js && bun run typecheck` + `cd bindings/js && bun run lint`
- **Phase gate:** All tests pass via both `bun test` (standalone) and `test.bat` (wrapper), typecheck passes, lint passes

### Wave 0 Gaps
- [ ] `bindings/js/bunfig.toml` -- test preload configuration for standalone `bun test`
- [ ] `bindings/js/test/setup.ts` -- DLL PATH setup preload script
- [ ] `bindings/js/biome.json` -- Biome configuration
- [ ] `@biomejs/biome` dev dependency installation

## Sources

### Primary (HIGH confidence)
- Existing codebase: `bindings/js/package.json`, `tsconfig.json`, `src/index.ts`, `src/types.ts` -- inspected directly
- Existing codebase: `bindings/js/test/*.test.ts` -- 61 tests counted via grep
- Existing codebase: `bindings/js/test/test.bat` -- PATH setup pattern verified
- Existing codebase: `scripts/test-all.bat` -- current test runner structure inspected

### Secondary (MEDIUM confidence)
- [Biome configuration reference](https://biomejs.dev/reference/configuration/) -- schema structure, defaults, sections
- [Biome getting started](https://biomejs.dev/guides/getting-started/) -- install and init commands
- [Bun bunfig.toml docs](https://bun.com/docs/runtime/bunfig) -- `[test] preload` field behavior
- [@biomejs/biome npm](https://www.npmjs.com/package/@biomejs/biome) -- latest version 2.4.6

### Tertiary (LOW confidence)
- None -- all findings verified with official docs or codebase inspection

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- Biome version confirmed on npm, Bun version confirmed locally (1.3.10)
- Architecture: HIGH -- existing codebase inspected, patterns well understood, all files accounted for
- Pitfalls: HIGH -- PATH/DLL patterns established by existing bindings (Python, Dart), preload behavior documented by Bun

**Research date:** 2026-03-09
**Valid until:** 2026-04-09 (stable domain, low churn)
