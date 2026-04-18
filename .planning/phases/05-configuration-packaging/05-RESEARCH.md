# Phase 5: Configuration & Packaging - Research

**Researched:** 2026-04-17
**Domain:** Deno module configuration, package.json to deno.json migration
**Confidence:** HIGH

## Summary

Phase 5 replaces the Node.js-centric configuration files (`package.json`, `tsconfig.json`) with a single `deno.json` that serves as the module entry point, import map, task runner, and formatting/linting configuration. The source code in `src/` is already fully migrated to Deno APIs (Phases 1-4 completed), so this phase is purely about configuration files -- no runtime code changes.

The migration is straightforward: `deno.json` natively handles everything `package.json` + `tsconfig.json` did. The `koffi` dependency (the only runtime dep) is already unused in source code and just needs its declaration removed. Deno's built-in TypeScript support means `tsconfig.json` is unnecessary -- Deno defaults to `strict: true`, `module: "nodenext"`, and `target: "esnext"`, which match the current project settings. The `biome.json` linter config can remain as-is since Biome runs as a standalone tool, but `deno.json` can also replicate its formatting rules via built-in `deno fmt`.

**Primary recommendation:** Create `deno.json` with exports, imports, tasks, and fmt configuration. Delete `package.json`, `tsconfig.json`, `vitest.config.ts`, and `node_modules/`. Keep `biome.json` (it works independently) or migrate to `deno fmt`/`deno lint` -- both are valid.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CONF-01 | Package config migrated from package.json to deno.json with koffi dependency removed | deno.json `exports`, `imports`, `tasks` fields replace all package.json functionality; koffi not imported in any src/ file |
| CONF-03 | TypeScript config updated for Deno (no separate tsconfig needed) | Deno defaults (`strict: true`, `module: "nodenext"`, `target: "esnext"`) match current tsconfig settings; tsconfig.json can be deleted entirely |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

- **Self-Updating:** CLAUDE.md must be updated to reflect any configuration changes (build/test commands, file layout)
- **Homogeneity:** Binding interfaces must be consistent and intuitive; test.bat pattern is used by all bindings
- **Intelligence:** Logic resides in C++ layer; bindings remain thin -- configuration should be minimal
- **Build/test scripts:** `scripts/test-all.bat` calls `bindings/js/test/test.bat` -- this file must work after migration
- **Target:** Deno-only runtime (per PROJECT.md decisions)

## Standard Stack

### Core

| Component | Version/Value | Purpose | Why Standard |
|-----------|---------------|---------|--------------|
| deno.json | Deno 2.x format | Module config, import map, task runner, formatter config | Single file replaces package.json + tsconfig.json + import map [CITED: docs.deno.com/runtime/fundamentals/configuration/] |
| `jsr:@std/path` | ^1.1.4 | Path utilities (dirname, join) | Already used in loader.ts; replaces node:path [VERIFIED: deno.lock shows @std/path@1.1.4 cached] |
| Deno built-in TypeScript | 5.9.2 | TypeScript compilation | Native Deno support, no tsc needed [VERIFIED: `deno --version` shows typescript 5.9.2] |

### Supporting

| Component | Version | Purpose | When to Use |
|-----------|---------|---------|-------------|
| Biome | ^2.4.6 | Linting + formatting | Keep as dev tool if team prefers; alternatively use `deno fmt` + `deno lint` |
| `deno fmt` | built-in | Code formatting | Alternative to Biome formatter; configurable in deno.json `fmt` field |
| `deno lint` | built-in | Linting | Alternative to Biome linter; configurable in deno.json `lint` field |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Keep biome.json | deno fmt + deno lint | Built-in tools reduce deps but biome has different rule set; either works |
| deno.json fmt | biome format | Biome is already configured and familiar; no strong reason to switch |

## Architecture Patterns

### Target File Layout After Migration

```
bindings/js/
  deno.json              # NEW: module config (replaces package.json + tsconfig.json)
  deno.lock              # EXISTS: updated to remove npm deps
  biome.json             # KEEP: linter/formatter (optional -- could migrate to deno.json fmt)
  .gitignore             # UPDATE: remove Node.js artifacts, add Deno-specific ignores if needed
  README.md              # EXISTS: update content in Phase 7 (DOCS)
  src/
    index.ts             # Module entry point (unchanged)
    ...                   # All other source files (unchanged)
  test/
    test.bat             # UPDATE: change from npx vitest to deno test
    setup.ts             # DELETE: uses node: APIs, no longer needed
    *.test.ts            # Tests updated in Phase 6 (TEST), not this phase
  --- DELETED ---
  package.json           # REMOVE
  tsconfig.json          # REMOVE
  vitest.config.ts       # REMOVE
  node_modules/          # REMOVE (in .gitignore already)
```

### Pattern 1: deno.json as Single Configuration File

**What:** A `deno.json` file that combines module metadata, import maps, task definitions, and formatter configuration into one file.
**When to use:** Always in Deno-native projects.
**Example:**

```json
{
  "name": "quiverdb",
  "version": "0.6.3",
  "exports": {
    ".": "./src/index.ts"
  },
  "imports": {
    "@std/path": "jsr:@std/path@^1.1.4"
  },
  "tasks": {
    "test": "deno test --allow-ffi --allow-read --allow-env test/",
    "lint": "biome check",
    "format": "biome format --write"
  },
  "fmt": {
    "indentWidth": 2,
    "lineWidth": 100,
    "semiColons": true,
    "singleQuote": false
  },
  "lint": {
    "exclude": ["node_modules/"]
  },
  "nodeModulesDir": "none"
}
```
[CITED: docs.deno.com/runtime/fundamentals/configuration/]

**Key fields explained:**
- `exports`: Maps `"."` to `./src/index.ts` -- Deno uses TypeScript source directly, no `dist/` build step needed
- `imports`: Centralizes the `jsr:@std/path` specifier so `loader.ts` can import from `@std/path` instead of `jsr:@std/path` (though the current inline `jsr:` specifier also works)
- `tasks`: Replaces npm scripts; `deno task test` runs tests
- `nodeModulesDir`: Set to `"none"` to explicitly disable node_modules creation [CITED: docs.deno.com/runtime/fundamentals/configuration/]
- `fmt`: Replicates biome's formatting rules for `deno fmt` if desired

### Pattern 2: Import Map Centralization

**What:** Move `jsr:` specifiers from source files into deno.json `imports` field.
**When to use:** When bare specifiers are cleaner than inline `jsr:` URLs.
**Current state:** `loader.ts` uses `import { dirname, join } from "jsr:@std/path"` inline.
**Option A -- Keep inline:** No change needed. Inline `jsr:` specifiers work in Deno. Simpler.
**Option B -- Centralize:** Add `"@std/path": "jsr:@std/path@^1.1.4"` to deno.json imports, then change loader.ts to `import { dirname, join } from "@std/path"`.

**Recommendation:** Option A (keep inline). The single import in loader.ts does not warrant import map indirection. The deno.lock already pins the version.

### Anti-Patterns to Avoid

- **Keeping package.json alongside deno.json:** Deno reads both and may auto-install npm dependencies from package.json. Delete package.json entirely. [CITED: docs.deno.com/runtime/fundamentals/configuration/]
- **Adding compilerOptions to deno.json:** Deno's defaults (`strict: true`, `module: "nodenext"`, `target: "esnext"`) already match the current tsconfig.json settings. Adding explicit compilerOptions is unnecessary and creates maintenance burden. [CITED: docs.deno.com/runtime/reference/ts_config_migration/]
- **Leaving node_modules directory:** Even though it is in .gitignore, delete it to avoid Deno picking up stale npm packages.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Module resolution | Custom path resolution in deno.json | Deno's native `exports` field | Standard Deno module resolution handles `.ts` extensions natively |
| TypeScript compilation | tsc build step or custom compilation | Deno built-in TS support | Deno runs .ts directly -- no compilation step needed |
| Import mapping | Manual path aliases or custom resolver | deno.json `imports` field (if needed) | Standard Deno import map mechanism |

## Common Pitfalls

### Pitfall 1: Deno Auto-Reading package.json

**What goes wrong:** If package.json exists alongside deno.json, Deno will auto-detect it and may try to install npm dependencies (including koffi) or create node_modules.
**Why it happens:** Deno has built-in package.json support for backward compatibility with Node projects.
**How to avoid:** Delete package.json entirely before or during deno.json creation. Do not keep it "for reference."
**Warning signs:** `deno.lock` contains `npm:koffi` entries; node_modules directory reappears after `deno` commands.

### Pitfall 2: deno.lock Retaining Old npm Dependencies

**What goes wrong:** The existing `deno.lock` references `npm:koffi`, `npm:vitest`, `npm:typescript`, etc. from the package.json era. These stale entries may cause warnings or confusion.
**Why it happens:** deno.lock was generated while package.json still existed, and Deno recorded all npm dependencies from it.
**How to avoid:** After deleting package.json, delete `deno.lock` and regenerate it with `deno install` or `deno cache src/index.ts`. The new lock file will only contain the `jsr:@std/path` dependency.
**Warning signs:** Lock file contains `npm:` entries that no source file imports.

### Pitfall 3: test.bat Still Using npx vitest

**What goes wrong:** `bindings/js/test/test.bat` currently runs `npx vitest run`. After package.json removal, npx will fail.
**Why it happens:** test.bat was written for the Node.js/vitest era.
**How to avoid:** Update test.bat to use `deno test` with appropriate permissions. Note: the actual test file migration (vitest to Deno.test) is Phase 6 -- but the test.bat runner command must be updated in this phase to unblock Phase 6 execution.
**Warning signs:** `scripts/test-all.bat` fails on the JS binding step.

### Pitfall 4: test/setup.ts Using Node APIs

**What goes wrong:** `test/setup.ts` imports from `node:path`, `node:url` and uses `process.env.PATH` -- all Node.js-only APIs. Vitest used this as a setup file; Deno.test has no equivalent setup file mechanism.
**Why it happens:** This file was the vitest setupFiles entry for PATH manipulation so that the native DLLs could be found.
**How to avoid:** Delete `test/setup.ts`. The PATH setup it performs (`build/bin` on PATH) should be handled by the test.bat script instead (which already does this). Alternatively, `deno test --allow-ffi=./build/bin` with correct library search in loader.ts handles it.
**Warning signs:** Deno throws errors about `process` not being defined.

### Pitfall 5: vitest.config.ts Left Behind

**What goes wrong:** `vitest.config.ts` becomes an orphan file referencing vitest and setup.ts. Deno may try to type-check it and fail on the vitest import.
**Why it happens:** Config files referencing npm packages that are no longer installed.
**How to avoid:** Delete vitest.config.ts alongside package.json removal.
**Warning signs:** `deno check` or `deno lint` reports errors on vitest.config.ts.

## Code Examples

### deno.json -- Complete Configuration

```json
{
  "name": "quiverdb",
  "version": "0.6.3",
  "exports": {
    ".": "./src/index.ts"
  },
  "tasks": {
    "test": "deno test --allow-ffi --allow-read --allow-env test/",
    "lint": "biome check",
    "format": "biome format --write"
  },
  "fmt": {
    "indentWidth": 2,
    "lineWidth": 100,
    "semiColons": true,
    "singleQuote": false
  },
  "nodeModulesDir": "none"
}
```
[VERIFIED: field names and values confirmed against docs.deno.com/runtime/fundamentals/configuration/]

### Updated test.bat

```bat
@echo off
pushd %~dp0..
set PATH=%~dp0..\..\..\build\bin;%PATH%
deno test --allow-ffi --allow-read --allow-env test/ %*
popd
```
[ASSUMED: exact permission flags may need tuning based on what tests actually require]

### Updated .gitignore (Node artifacts to remove/replace)

Lines to remove from .gitignore:
```
node_modules/
dist
*.tsbuildinfo
.npm
```

Lines to keep or add:
```
*.sqlite
deno.lock    # Optional: some projects commit lock files
```
[ASSUMED: .gitignore cleanup scope -- may keep some lines for safety]

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| package.json + tsconfig.json + vitest.config.ts | Single deno.json | Deno 2.0 (Oct 2024) | All config in one file |
| `npm install koffi` | No external FFI dep (Deno.dlopen built-in) | Deno 1.13+ (Aug 2021) | Zero runtime dependencies |
| `tsc --build` then run JS | `deno run file.ts` directly | Deno 1.0+ (May 2020) | No build step for TypeScript |
| `nodeModulesDir: false` (boolean) | `nodeModulesDir: "none"` (string) | Deno 2.x | Boolean form deprecated [CITED: docs.deno.com/runtime/fundamentals/configuration/] |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | test.bat permission flags `--allow-ffi --allow-read --allow-env` are sufficient for all tests | Code Examples | Tests may need `--allow-write` for temp file creation; easily fixed by adding flag |
| A2 | .gitignore changes are safe to make (removing Node-specific entries) | Code Examples | Low risk -- entries like `dist` and `node_modules/` are no longer relevant |
| A3 | biome.json can coexist with deno.json without conflict | Architecture Patterns | Low risk -- Biome reads its own config independently |
| A4 | Deleting deno.lock and regenerating is safe | Pitfall 2 | Very low risk -- lock file is purely a cache; regeneration is deterministic |

## Open Questions

1. **Biome vs deno fmt/lint: which to keep?**
   - What we know: Both work. Biome is already configured. Deno has built-in equivalents.
   - What's unclear: Team preference. Biome may have rules the team relies on.
   - Recommendation: Keep biome.json for now (minimal change). Can migrate later. The `deno.json` `fmt` section is provided in case the team wants to switch.

2. **Should deno.json include `imports` for `@std/path`?**
   - What we know: loader.ts imports inline as `jsr:@std/path`. Both approaches work.
   - What's unclear: Whether the team prefers centralized dependency management.
   - Recommendation: Keep inline specifier. Only one file uses it. Less indirection.

3. **What permissions does test.bat need exactly?**
   - What we know: Tests use FFI (dlopen), file reads (schemas), file writes (temp DBs), env vars (PATH).
   - What's unclear: Complete permission list until Phase 6 migrates tests to Deno.test.
   - Recommendation: Use `--allow-ffi --allow-read --allow-write --allow-env` to be safe. Phase 6 can tighten.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Deno | Runtime | Yes | 2.7.10 | -- |
| Biome | Linting/formatting | Yes (via npm) | ^2.4.6 | deno fmt + deno lint |
| Native libs (libquiver.dll, libquiver_c.dll) | FFI at runtime | Yes | In build/bin/ | -- |

**Missing dependencies with no fallback:** None.

## Sources

### Primary (HIGH confidence)
- [docs.deno.com/runtime/fundamentals/configuration/](https://docs.deno.com/runtime/fundamentals/configuration/) -- deno.json field reference, imports, exports, tasks, nodeModulesDir
- [docs.deno.com/runtime/reference/ts_config_migration/](https://docs.deno.com/runtime/reference/ts_config_migration/) -- Deno TS defaults (strict: true, module: nodenext, target: esnext)
- [docs.deno.com/runtime/fundamentals/testing/](https://docs.deno.com/runtime/fundamentals/testing/) -- deno test configuration and CLI flags
- [docs.deno.com/runtime/fundamentals/security/](https://docs.deno.com/runtime/fundamentals/security/) -- --allow-ffi permission syntax
- [docs.deno.com/runtime/reference/cli/fmt/](https://docs.deno.com/runtime/reference/cli/fmt/) -- deno fmt configuration options

### Secondary (MEDIUM confidence)
- [jsr.io/@std/path](https://jsr.io/@std/path) -- @std/path module reference

### Tertiary (LOW confidence)
- None -- all claims verified against official docs or codebase inspection

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- deno.json is the sole standard tool; no library choices to make
- Architecture: HIGH -- file changes are mechanical (create/delete config files)
- Pitfalls: HIGH -- all pitfalls observed directly from codebase state

**Research date:** 2026-04-17
**Valid until:** 2026-05-17 (Deno config format is stable in 2.x)
