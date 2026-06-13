# JavaScript Binding (quiverdb on npm)

Runs on **Bun** (`bun:ffi`), not Deno/Node. Cross-layer naming follows the Dart camelCase rule
(`readTimeSeriesRow`, ...) with `Csv` cased as `exportCsv`/`importCsv`; JS deliberately keeps a
string-based datetime surface — no DateTime wrappers (root design decision). Publish flow lives
in `.github/CLAUDE.md`.

## Layout

```
mod.ts            # Package entry point (re-exports src/index.ts)
src/              # Module per C API category: database.ts, create.ts, read.ts, metadata.ts,
                  # query.ts, time-series.ts, transaction.ts, csv.ts, introspection.ts,
                  # composites.ts, lua-runner.ts (index.ts re-exports the public surface)
src/loader.ts     # HAND-WRITTEN FFI symbol table + 3-tier library loader
src/types.ts      # Central DATA_TYPE_* / LOG_LEVEL_* constants and DatabaseOptions type
src/ffi-helpers.ts # Alloc helpers, makeDefaultOptions()
src/errors.ts     # QuiverError (always thrown; message from quiver_get_last_error)
test/             # bun:test suite (*.test.ts per area) + test.bat
package.json      # Version must match CMakeLists.txt; scripts: test/lint/format (biome)
biome.json        # Lint/format config
```

## Rules and gotchas

- **No generator** — when the C API changes, add the symbol to `src/loader.ts` by hand as
  `{ name: { args, returns } }`. This is the drift-prone spot: check it whenever a new C function
  exists in other bindings but not here.
- **Library loader**: lazy `getSymbols()` (init on first use — eager init would hit a
  `QuiverError` TDZ during the loader↔errors import cycle). Three tiers: bundled
  `libs/{os}-{arch}/` (shipped in the npm package) → dev `build/bin` walk-up → system PATH. On
  Windows, `ensureCoreOnPath` prepends the lib dir to `process.env.PATH` so the OS loader finds
  the sibling `libquiver.dll` (Bun's `dlopen` cannot preload the core lib — it rejects an empty
  symbol map).
- **Bun FFI gotchas (load-bearing — do not "fix"):**
  - `FFIType.buffer` is rejected as an argument ABI type → buffer/string params are declared
    `"pointer"` and call sites pass the `Uint8Array` (`alloc.buf`) directly; Bun pins the
    TypedArray for the call.
  - For out-params, **pass the TypedArray to the FFI call**, never a precomputed `ptr()` number,
    and create/read the `DataView` *after* the call. Touching `.buffer` between `ptr(buf)` and
    the call materializes/relocates a small JSC typed array, leaving the stored pointer stale
    (deterministic silent corruption — C writes land in abandoned memory). This is the single
    house marshaling style across all of `src/`.
  - No struct-by-value FFI return (oven-sh/bun#6139) → `quiver_database_options_default` is
    omitted from the symbol table; `ffi-helpers.makeDefaultOptions()` builds the options struct
    in JS.
- **int64 handling**: input params accept `number | bigint` — `allocNativeInt64` writes each
  element with `DataView.setBigInt64`, so `bigint` inputs (scalar or array) are preserved
  exactly, never coerced through `Number`. Read paths return `number` (converted via `Number()`
  after the FFI call) — the deliberately simple surface.
- **`updateTimeSeriesGroup` validates before marshalling** (`src/time-series.ts`): rejects jagged
  columns and named-but-empty columns (`rowCount === 0`) with a `QuiverError`. This is load-bearing
  — an empty column would otherwise marshal a `null` data pointer that the C API dereferences
  against the first column's `row_count`. Pass `{}` (no columns) to clear the group.
- **Time-series NULL cells** (`TimeSeriesData = Record<string, (number | string | null)[]>`): a
  `null` value marshals to a per-column `uint8_t` mask (0 = NULL) with a placeholder in the data
  array; an all-`null` column is tagged FLOAT with a zeroed placeholder (the C API ignores the tag
  for masked cells). Reads decode the mask and null-out cells; string columns use the null-guarded
  pointer loop (never `decodeStringArray`, which constructs a `CString` from a NULL pointer). Masks
  are built by direct `Uint8Array` indexing — never a `DataView` — per the TypedArray house rule.
- **Test/lint/format**: `bun test test`, `bun run lint`, `bun run format` (biome, project-pinned
  version). No permission flags needed (Bun has none — don't carry over Deno habits). There is
  pre-existing lint debt in untouched files — fix only what your change orphans, don't drive-by
  reformat.

## Packaging (package-local parts)

`package.json` `files` allowlist ships `libs/**`; a `.npmignore` with no ignore patterns (just a
comment) stops `npm pack` falling back to the root `.gitignore` (which excludes `*.dll`/`*.so`). `publishConfig.provenance: true` emits a
signed provenance attestation. The npm OIDC trusted-publishing workflow is described in
`.github/CLAUDE.md`.
