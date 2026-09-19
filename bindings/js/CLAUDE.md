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
src/lua-api.ts    # LUA_DB_API_REFERENCE — agent-facing Lua `db:` API reference, as a string const
src/group-columns.ts # Shared columnar marshaller for the group writers (by id and by label)
src/loader.ts     # HAND-WRITTEN FFI symbol table + 3-tier library loader
src/types.ts      # Central DATA_TYPE_* / LOG_LEVEL_* constants and DatabaseOptions type
src/ffi-helpers.ts # Alloc helpers, makeDefaultOptions()
src/boolean.ts    # integerToBoolean — strict 0/1 conversion for the boolean convenience readers
src/errors.ts     # QuiverError (always thrown; message from quiver_get_last_error)
test/             # bun:test suite (*.test.ts per area) + test.bat
package.json      # Version must match CMakeLists.txt; scripts: test/lint/format (biome)
biome.json        # Lint/format config
```

## Rules and gotchas

- **`LUA_DB_API_REFERENCE` (`src/lua-api.ts`) is shipped prompt payload, not just docs.** The
  downstream consumer (`claw`) imports it from the package root and interpolates it verbatim into an
  LLM system prompt, which is why it stays a plain `export const`: a string constant costs no FFI,
  no file read, and no Bun loader feature, and `bun build --compile` inlines it into a consumer's
  binary. Converting it to an imported `.md` was tried and deliberately reverted (see root
  `CLAUDE.md` "Do Not Fix") — the escaped backticks are the accepted cost.
  `test/lua-api-sync.test.ts` derives the bound surface from `src/lua_runner.cpp` and fails if a
  `db:`/`quiver.*` name is undocumented, a documented name no longer exists, or the stdlib sentence
  disagrees with `open_libraries` — that check is why the doc must keep the literal-token convention
  and the canonical `Loaded standard libraries: ...` sentence. It cannot check arg order, arity,
  types, or return shapes; those still need a hand re-diff. The `## CSV file reading` section's
  worked example is exactly this uncheckable half: its Lua is real, lifted verbatim from
  `test_lua_runner_read_csv.cpp`'s regression tests and run once against `tests/fixtures/
  ma_energia_residencial.csv` / `ma_gd_data.csv` through `quiver_cli` before it shipped — but
  **nothing in CI re-runs it**, so an edit to that example has to be re-verified by hand the same
  way (a throwaway file-backed database plus the fixtures, driven through `quiver_cli`).
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
  - **`quiver_database_options_t` is 24 bytes** (`read_only`@0 int32, `console_level`@4 int32,
    `ui_config_dir`@8 `const char*`, `ui_locale`@16 `const char*`), built entirely from named
    offset constants in `ffi-helpers.ts` (`OPTIONS_OFFSET_READ_ONLY`/`_CONSOLE_LEVEL`/
    `_UI_CONFIG_DIR`/`_UI_LOCALE`, `OPTIONS_SIZE`) — never an inline literal (a search-and-replace
    over `8` in this file is itself the historical bug this guards against; `allocPtrOut` and
    `allocUint64Out` in the same file allocate 8 bytes each for unrelated reasons — a pointer-out
    and a u64-out parameter — and are pinned unchanged by `test/ffi-helpers.test.ts`).
    `makeDefaultOptions` returns `[Allocation, Allocation[]]` — the struct plus every child
    string allocation for `uiConfigDir`/`uiLocale` — copying `buildCsvOptionsBuffer`'s shape in
    `csv.ts` exactly. **Every call site MUST destructure and bind the keepalive in the same scope
    as the FFI call** (all three in `database.ts`); dropping it is a use-after-free, since the
    struct's two pointer fields point into separately allocated buffers Bun's GC can otherwise
    collect before the native call reads them. An absent or empty-string option leaves its
    pointer slot's eight zero bytes (NULL), allocating no child string — the C converter maps
    that to "not specified".
  - **`SCALAR_METADATA_SIZE` (56) and `GROUP_METADATA_SIZE` (32) live in `ffi-helpers.ts`**, not
    `metadata.ts` — `metadata.ts` imports them back. Reason: `loader.ts` needs all three
    struct-size constants (these two plus `OPTIONS_SIZE`) for its load-time assertion, and
    `metadata.ts` imports `database.ts`, so importing from there into `loader.ts` would pull the
    whole database surface into the loader and create a cycle; `ffi-helpers.ts` imports only
    `bun:ffi` and `./types.ts`, so it is the cycle-free home. Values are unchanged, only relocated.
  - **Load-time struct-size gate**: `loadLibrary()` calls `assertNativeStructSizes(lib.symbols)`
    exactly once, on the memoized path, checking `quiver_database_options_t` /
    `quiver_scalar_metadata_t` / `quiver_group_metadata_t` in that fixed order and
    short-circuiting on the first mismatch. It must NOT move inside `openLibrary()` or any of
    `initLibrary()`'s three `try`/`catch` tiers — each swallows its load failure as "try the next
    path", which would remask a real size mismatch as a generic "Cannot load native library"
    error and lose the struct name and both numbers. `checkStructSize(name, expected, native)` is
    the pure, parameterized throw — one of the binding's few locally crafted error messages,
    since the C API cannot diagnose a disagreement about its own layout. **Every `*_sizeof`
    accessor call must be wrapped in `Number(...)` before comparing**: Bun returns a `bigint` for
    a `usize` FFI return (probe-verified: `1252n === 1252` is `false`), `bun test` does not
    typecheck, and an unconverted comparison throws a bogus mismatch on every single load.
  - **Known, deliberately unfixed**: `csv.ts`'s `buildCsvOptionsBuffer` hardcodes
    `new Uint8Array(56)` for `quiver_csv_options_t` with no `sizeof` accessor and no load-time
    assertion — a fourth instance of the same hazard class as the options struct above, out of
    scope for this phase's three named structs (options, scalar metadata, group metadata).
- **int64 handling**: input params accept `number | bigint` — `allocNativeInt64` writes each
  element with `DataView.setBigInt64`, so `bigint` inputs (scalar or array) are preserved
  exactly, never coerced through `Number`. Read paths return `number` (converted via `Number()`
  after the FFI call) — the deliberately simple surface.
- **`src/group-columns.ts` is the one columnar marshaller** for `updateTimeSeriesGroup`,
  `updateVectorGroup`, `updateSetGroup` and their `ByLabel` forms. They differ only in which C
  entry point they pass to `updateGroupColumns(handle, caller, cFn, ...)` and whether `key` is a
  `number` id (a `bigint`) or a `string` label (a `Uint8Array`), so don't re-inline it per method.
  It validates before marshalling: jagged columns and named-but-empty columns (`rowCount === 0`)
  throw a `QuiverError` naming the column. Load-bearing — an empty column would otherwise marshal a
  `null` data pointer that the C API dereferences against the first column's `row_count`. Pass `{}`
  (no columns) to clear the group. The C API rejects both cases too; failing here names the column.
- **A nullable scalar string argument passes literal `null`, never `""`**
  (`updateRelation`/`updateRelationByLabel`) — Bun turns `null` into a NULL pointer for a
  `"pointer"` slot, the same way `group-columns.ts` passes `null` for the array pointers when
  clearing. The C API reads NULL as "clear the relation" and an empty string as a label to look up.
- **Scalar bulk NULLs**: `readScalarIntegers`/`readScalarFloats` read a parallel `uint8_t*` mask
  (`new Uint8Array(toArrayBuffer(...))`) and gate `mask[i] ? v : null` → `(number | null)[]` — never
  `Number()` a masked slot (would turn NULL into 0). `readScalarStrings` reads pointer-by-pointer with
  `read.ptr` + a NULL guard → `(string | null)[]` (not `decodeStringArray`). `loader.ts` carries the
  mask arg on the two numeric symbols + `quiver_database_free_mask` (hand-maintained, no generator).
- **`LuaRunner.run` owns its result**: `quiver_lua_runner_run` takes a `char** out_result` and the
  JSON string must be freed with `quiver_lua_runner_free_string` — *not* `quiver_database_free_string`
  (both are in `loader.ts`, hand-maintained). `decodeStringFromBuf` returns `""` for a NULL pointer,
  which is also what the C API leaves there on failure, and `check()` throws before the decode.
- **Time-series NULL cells** (`TimeSeriesData = Record<string, (number | string | null)[]>`): a
  `null` value marshals to a per-column `uint8_t` mask (0 = NULL) with a placeholder in the data
  array; an all-`null` column is tagged FLOAT with a zeroed placeholder (the C API ignores the tag
  for masked cells). Reads decode the mask and null-out cells; string columns use the null-guarded
  pointer loop (never `decodeStringArray`, which constructs a `CString` from a NULL pointer). Masks
  are built by direct `Uint8Array` indexing — never a `DataView` — per the TypedArray house rule.
- **`integerToBoolean` throws `RangeError`, not `QuiverError`** — the one exception to the
  "always `QuiverError`" rule above, and deliberate: that message comes from
  `quiver_get_last_error`, while this one is crafted here (the boolean readers are a binding-only
  convenience the core never sees). It names the offending `collection.attribute`; `queryBoolean`
  has no column to name. On writes a `boolean` is an INTEGER 1/0 — `setElementField`,
  `setElementArray` and `marshalParams` each carry a `typeof === "boolean"` branch, and
  `ScalarValue`/`ArrayValue`/`QueryParam`/`GroupColumns` include it. The two group/row marshallers
  instead **normalize per cell before the type dispatch** — `updateGroupColumns`
  (`group-columns.ts`) and `upsertRowColumns` (`time-series.ts`) both map `boolean → 1/0` first, so
  no boolean branch is needed at all. Both halves of that are load-bearing: *before* the dispatch,
  because `Number.isInteger(true)` is `false` and a boolean otherwise falls through to the FLOAT
  fallback and lands in the column as FLOAT 1.0 with no error; *per cell*, because a boolean branch
  chosen from the first cell would truthiness-map the rest and silently rewrite a mixed
  `[true, 5]` column to `[1, 1]`. `updateGroupColumns` skips the normalization for a string column,
  so it cannot change what a mixed `['a', true]` column already wrote. `upsertRowColumns`'s last
  branch is `typeof value === "number"`, not an untyped `else` — `Number(null)` is 0 and anything
  else is NaN, both of which used to be written with no error. **`GroupColumns` is the write type and
  `TimeSeriesData` the read type** — they are otherwise identical, but only the former admits
  `boolean`, since `readTimeSeriesGroup` never produces one and its return type should not claim
  it. The four `updateTimeSeriesGroup*`/group writers therefore take `GroupColumns`.
- **Test/lint/format**: `bun test test`, `bun run lint`, `bun run format` (biome, project-pinned
  version). No permission flags needed (Bun has none — don't carry over Deno habits). There is
  pre-existing lint debt in untouched files — fix only what your change orphans, don't drive-by
  reformat.

## Packaging (package-local parts)

`package.json` `files` allowlist ships `libs/**`; a `.npmignore` with no ignore patterns (just a
comment) stops `npm pack` falling back to the root `.gitignore` (which excludes `*.dll`/`*.so`). `publishConfig.provenance: true` emits a
signed provenance attestation. The npm OIDC trusted-publishing workflow is described in
`.github/CLAUDE.md`.
