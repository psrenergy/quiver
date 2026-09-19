import { dlopen, type FFIFunction, type Library, type Pointer, suffix } from "bun:ffi";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { QuiverError } from "./errors.ts";
import {
  CSV_OPTIONS_SIZE,
  GROUP_METADATA_SIZE,
  OPTIONS_SIZE,
  SCALAR_METADATA_SIZE,
} from "./ffi-helpers.ts";

// Bun FFI type shorthand constants. Deno's "buffer" parameter type (pass a
// TypedArray, auto-converted to a pointer) has no Bun equivalent -- Bun rejects
// `FFIType.buffer` as an argument ABI type -- so buffer args use "pointer";
// Bun accepts a TypedArray for a pointer argument and pins it for the call.
const P = "pointer" as const;
const BUF = "pointer" as const;
const I32 = "i32" as const;
const I64 = "i64" as const;
const USIZE = "usize" as const;
const F64 = "f64" as const;

// Platform constants. `suffix` from bun:ffi is the platform-correct shared
// library extension ("dll" | "dylib" | "so").
const EXT = suffix;
const C_API_LIB = `libquiver_c.${EXT}`;

// Opaque pointer type for native handles.
export type NativePointer = Pointer | null;

// ---------------------------------------------------------------------------
// Symbol definitions grouped by C API domain
// ---------------------------------------------------------------------------

const lifecycleSymbols = {
  quiver_version: { args: [], returns: P },
  quiver_get_last_error: { args: [], returns: P },
  // quiver_database_options_default is intentionally omitted: it returns a
  // struct by value, which Bun FFI does not support (oven-sh/bun#6139). It was
  // never called -- makeDefaultOptions() in ffi-helpers.ts builds the options
  // struct in JS. The three *_sizeof accessors below (this one plus the two in
  // metadataSymbols) exist precisely BECAUSE Bun cannot call the struct-by-value
  // default -- they are what assertNativeStructSizes calls instead (D-07).
  quiver_database_options_sizeof: { args: [], returns: USIZE },
  quiver_database_has_ui_config: { args: [P, P], returns: I32 },
  quiver_database_from_schema: { args: [BUF, BUF, BUF, P], returns: I32 },
  quiver_database_from_migrations: { args: [BUF, BUF, BUF, P], returns: I32 },
  quiver_database_validate_migrations: { args: [BUF], returns: I32 },
  quiver_database_open: { args: [BUF, BUF, P], returns: I32 },
  quiver_database_close: { args: [P], returns: I32 },
  quiver_database_is_healthy: { args: [P, P], returns: I32 },
  quiver_database_path: { args: [P, P], returns: I32 },
  quiver_database_current_version: { args: [P, P], returns: I32 },
  quiver_database_describe: { args: [P, P], returns: I32 },
} as const;

const elementSymbols = {
  quiver_element_create: { args: [P], returns: I32 },
  quiver_element_destroy: { args: [P], returns: I32 },
  quiver_element_set_integer: { args: [P, BUF, I64], returns: I32 },
  quiver_element_set_float: { args: [P, BUF, F64], returns: I32 },
  quiver_element_set_string: { args: [P, BUF, BUF], returns: I32 },
  quiver_element_set_null: { args: [P, BUF], returns: I32 },
  quiver_element_set_array_integer: { args: [P, BUF, P, I32, P], returns: I32 },
  quiver_element_set_array_float: { args: [P, BUF, P, I32, P], returns: I32 },
  quiver_element_set_array_string: { args: [P, BUF, P, I32, P], returns: I32 },
} as const;

const crudSymbols = {
  quiver_database_create_element: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_update_element: { args: [P, BUF, I64, P], returns: I32 },
  quiver_database_update_element_by_label: { args: [P, BUF, BUF, P], returns: I32 },
  quiver_database_delete_element: { args: [P, BUF, I64], returns: I32 },
  quiver_database_delete_element_by_label: { args: [P, BUF, BUF], returns: I32 },
  quiver_database_update_relation: { args: [P, BUF, BUF, BUF, I64, BUF], returns: I32 },
  quiver_database_update_relation_by_label: { args: [P, BUF, BUF, BUF, BUF, BUF], returns: I32 },
} as const;

const readSymbols = {
  quiver_database_read_scalar_integers: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_scalar_floats: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_scalar_strings: { args: [P, BUF, BUF, P, P], returns: I32 },
  quiver_database_read_scalar_integer_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_scalar_float_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_scalar_string_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_vector_integers: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_vector_floats: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_vector_strings: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_set_integers: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_set_floats: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_set_strings: { args: [P, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_read_vector_integers_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_vector_floats_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_vector_strings_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_set_integers_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_set_floats_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_set_strings_by_id: { args: [P, BUF, BUF, I64, P, P], returns: I32 },
  quiver_database_read_element_ids: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_number_of_elements: { args: [P, BUF, P], returns: I32 },
} as const;

const querySymbols = {
  quiver_database_query_string: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_query_integer: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_query_float: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_query_string_params: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
  quiver_database_query_integer_params: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
  quiver_database_query_float_params: { args: [P, BUF, P, P, USIZE, P, P], returns: I32 },
} as const;

const transactionSymbols = {
  quiver_database_begin_transaction: { args: [P], returns: I32 },
  quiver_database_commit: { args: [P], returns: I32 },
  quiver_database_rollback: { args: [P], returns: I32 },
  quiver_database_in_transaction: { args: [P, P], returns: I32 },
  quiver_database_begin_dry_run: { args: [P], returns: I32 },
  quiver_database_end_dry_run: { args: [P], returns: I32 },
  quiver_database_in_dry_run: { args: [P, P], returns: I32 },
} as const;

const metadataSymbols = {
  quiver_scalar_metadata_sizeof: { args: [], returns: USIZE },
  quiver_group_metadata_sizeof: { args: [], returns: USIZE },
  quiver_database_get_scalar_metadata: { args: [P, BUF, BUF, P], returns: I32 },
  quiver_database_get_vector_metadata: { args: [P, BUF, BUF, P], returns: I32 },
  quiver_database_get_set_metadata: { args: [P, BUF, BUF, P], returns: I32 },
  quiver_database_get_time_series_metadata: { args: [P, BUF, BUF, P], returns: I32 },
  quiver_database_free_scalar_metadata: { args: [P], returns: I32 },
  quiver_database_free_group_metadata: { args: [P], returns: I32 },
  quiver_database_list_scalar_attributes: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_list_vector_groups: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_list_set_groups: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_list_time_series_groups: { args: [P, BUF, P, P], returns: I32 },
} as const;

const describeSymbols = {
  quiver_database_describe_collection: { args: [P, BUF, P], returns: I32 },
  quiver_database_summarize_collection: { args: [P, BUF, P], returns: I32 },
} as const;

const timeSeriesSymbols = {
  quiver_database_read_time_series_group: {
    args: [P, BUF, BUF, I64, P, P, P, P, P, P],
    returns: I32,
  },
  quiver_database_read_time_series_row: { args: [P, BUF, BUF, BUF, BUF, P, P, P], returns: I32 },
  quiver_database_upsert_time_series_row: {
    args: [P, BUF, BUF, I64, P, P, P, USIZE],
    returns: I32,
  },
  quiver_database_upsert_time_series_row_by_label: {
    args: [P, BUF, BUF, BUF, P, P, P, USIZE],
    returns: I32,
  },
  quiver_database_update_time_series_group: {
    args: [P, BUF, BUF, I64, P, P, P, P, USIZE, USIZE],
    returns: I32,
  },
  quiver_database_update_time_series_group_by_label: {
    args: [P, BUF, BUF, BUF, P, P, P, P, USIZE, USIZE],
    returns: I32,
  },
  quiver_database_update_vector_group: {
    args: [P, BUF, BUF, I64, P, P, P, P, USIZE, USIZE],
    returns: I32,
  },
  quiver_database_update_vector_group_by_label: {
    args: [P, BUF, BUF, BUF, P, P, P, P, USIZE, USIZE],
    returns: I32,
  },
  quiver_database_update_set_group: {
    args: [P, BUF, BUF, I64, P, P, P, P, USIZE, USIZE],
    returns: I32,
  },
  quiver_database_update_set_group_by_label: {
    args: [P, BUF, BUF, BUF, P, P, P, P, USIZE, USIZE],
    returns: I32,
  },
  quiver_database_free_time_series_data: { args: [P, P, P, P, USIZE, USIZE], returns: I32 },
  quiver_database_has_time_series_files: { args: [P, BUF, P], returns: I32 },
  quiver_database_list_time_series_files_columns: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_read_time_series_files: { args: [P, BUF, P, P, P], returns: I32 },
  quiver_database_update_time_series_files: { args: [P, BUF, P, P, USIZE], returns: I32 },
  quiver_database_free_time_series_files: { args: [P, P, USIZE], returns: I32 },
} as const;

const csvSymbols = {
  // Fourth *_sizeof accessor (D-07) -- csv.ts hand-allocates a raw quiver_csv_options_t buffer,
  // the same hazard class as quiver_database_options_sizeof above; assertNativeStructSizes
  // checks it last.
  quiver_csv_options_sizeof: { args: [], returns: USIZE },
  quiver_database_export_csv: { args: [P, BUF, BUF, BUF, P], returns: I32 },
  quiver_database_import_csv: { args: [P, BUF, BUF, BUF, P], returns: I32 },
} as const;

const freeSymbols = {
  quiver_database_free_integer_array: { args: [P], returns: I32 },
  quiver_database_free_float_array: { args: [P], returns: I32 },
  quiver_database_free_mask: { args: [P], returns: I32 },
  quiver_database_free_string_array: { args: [P, USIZE], returns: I32 },
  quiver_database_free_string: { args: [P], returns: I32 },
  quiver_database_free_integer_vectors: { args: [P, P, USIZE], returns: I32 },
  quiver_database_free_float_vectors: { args: [P, P, USIZE], returns: I32 },
  quiver_database_free_string_vectors: { args: [P, P, USIZE], returns: I32 },
  quiver_database_free_scalar_metadata_array: { args: [P, USIZE], returns: I32 },
  quiver_database_free_group_metadata_array: { args: [P, USIZE], returns: I32 },
} as const;

const luaSymbols = {
  quiver_lua_runner_new: { args: [P, P], returns: I32 },
  quiver_lua_runner_free: { args: [P], returns: I32 },
  quiver_lua_runner_run: { args: [P, BUF, BUF], returns: I32 },
  quiver_lua_runner_free_string: { args: [P], returns: I32 },
} as const;

// Combined symbol map for dlopen.
const allSymbols = {
  ...lifecycleSymbols,
  ...elementSymbols,
  ...crudSymbols,
  ...readSymbols,
  ...querySymbols,
  ...transactionSymbols,
  ...metadataSymbols,
  ...describeSymbols,
  ...timeSeriesSymbols,
  ...csvSymbols,
  ...freeSymbols,
  ...luaSymbols,
} as const;

// ---------------------------------------------------------------------------
// Library search and loading
// ---------------------------------------------------------------------------

// `import.meta.dir` is the absolute directory of this module on disk.
const __dirname: string = import.meta.dir;

// Map Bun's process.platform/arch onto the `libs/{os}-{arch}/` directory names
// used by the bundled native libraries (e.g. "windows-x86_64", "macos-aarch64").
const OS_NAMES: Record<string, string> = { win32: "windows", darwin: "macos", linux: "linux" };
const ARCH_NAMES: Record<string, string> = { x64: "x86_64", arm64: "aarch64" };
const osName = OS_NAMES[process.platform] ?? process.platform;
const archName = ARCH_NAMES[process.arch] ?? process.arch;
const platformKey = `${osName}-${archName}`;
const isWindows = process.platform === "win32";

function getBundledLibDir(): string | null {
  const libDir = join(__dirname, "..", "libs", platformKey);
  if (existsSync(join(libDir, C_API_LIB))) {
    return libDir;
  }
  return null;
}

function getSearchPaths(): string[] {
  const paths: string[] = [];
  let dir = __dirname;
  for (let i = 0; i < 5; i++) {
    const candidate = join(dir, "build", "bin");
    if (existsSync(join(candidate, C_API_LIB))) {
      paths.push(candidate);
    }
    dir = dirname(dir);
  }
  return paths;
}

type QuiverLib = Library<typeof allSymbols>;

// On Windows, libquiver_c.dll depends on libquiver.dll (the C++ core). The OS
// loader resolves that sibling dependency via PATH, so put the library's own
// directory on PATH before loading. (Unlike Deno, Bun's dlopen cannot preload
// the core lib directly -- it rejects an empty symbol map.)
function ensureCoreOnPath(dir: string): void {
  if (!isWindows) return;
  const current = process.env.PATH ?? "";
  if (!current.split(";").includes(dir)) {
    process.env.PATH = `${dir};${current}`;
  }
}

function openLibrary<S extends Record<string, FFIFunction>>(dir: string, symbols: S): Library<S> {
  ensureCoreOnPath(dir);
  return dlopen(join(dir, C_API_LIB), symbols);
}

function initLibrary<S extends Record<string, FFIFunction>>(symbols: S): Library<S> {
  let lastError: unknown;

  // Tier 1: Bundled libs/{os}-{arch}/ next to the loader (npm install / dev install).
  const bundledDir = getBundledLibDir();
  if (bundledDir) {
    try {
      return openLibrary(bundledDir, symbols);
    } catch (e) {
      lastError = e; // Bundled libs found but failed to load -- fall through.
    }
  }

  // Tier 2: Dev mode -- walk up directories looking for build/bin/.
  for (const dir of getSearchPaths()) {
    try {
      return openLibrary(dir, symbols);
    } catch (e) {
      lastError = e; // Try next path.
    }
  }

  // Tier 3: System PATH fallback -- the core lib is expected to be discoverable
  // on PATH alongside the C API lib.
  try {
    return dlopen(C_API_LIB, symbols);
  } catch (e) {
    lastError = e; // Fall through to error.
  }

  const searched = [
    join(__dirname, "..", "libs", platformKey),
    ...getSearchPaths(),
    "system PATH",
  ].join(", ");
  const detail = lastError instanceof Error ? `: ${lastError.message}` : "";
  throw new QuiverError(
    `Cannot load native library '${C_API_LIB}'. Searched: ${searched}${detail}`,
  );
}

// One symbol that has existed since before this milestone -- a successful probe with only this
// symbol declared means "a Quiver native library is present and loadable here", distinguishing a
// stale native (present, missing the newer *_sizeof exports) from no native at all (T-02-12-02).
const PROBE_SYMBOLS = {
  quiver_get_last_error: lifecycleSymbols.quiver_get_last_error,
} as const;

// The four size accessors introduced by this milestone, named here once for the version-skew
// diagnosis message below.
const SIZEOF_ACCESSOR_NAMES = [
  "quiver_database_options_sizeof",
  "quiver_scalar_metadata_sizeof",
  "quiver_group_metadata_sizeof",
  "quiver_csv_options_sizeof",
] as const;

// Resolves the full symbol map through the three tiers above. On failure, re-runs the same tiered
// resolution with PROBE_SYMBOLS -- one symbol that predates this milestone. If that probe
// succeeds, a Quiver native library is present and loadable but lacks the newer *_sizeof exports,
// so the failure is a version skew, not a missing library: throw a message naming all four
// accessors instead of the generic "Cannot load native library" text. If the probe also fails, no
// native is loadable at all, so rethrow the original error unchanged -- a genuine not-found must
// still read exactly as it does today (D-13, must_haves EDGE).
//
// This is a probe, not a catch-and-annotate: initLibrary() already collapses three distinct
// failure paths into one lastError, so annotating that failure could only ever guess "maybe your
// native is stale" -- and would guess it just as loudly when no native exists at all. Running a
// second, independent resolution with a minimal symbol map is a determination, not a guess.
export function resolveLibrary<S extends Record<string, FFIFunction>>(symbols: S): Library<S> {
  try {
    return initLibrary(symbols);
  } catch (e) {
    try {
      initLibrary(PROBE_SYMBOLS);
    } catch {
      throw e; // No native loadable at all -- rethrow the original not-found unchanged.
    }
    throw new QuiverError(
      `Native library '${C_API_LIB}' was found and loaded, but it does not export ` +
        `${SIZEOF_ACCESSOR_NAMES.join(", ")}. This native library predates this release of the ` +
        `JS binding -- reinstall a matching native library.`,
    );
  }
}

// ---------------------------------------------------------------------------
// Load-time struct-size gate (SAFE-02/SAFE-03, D-08/D-09/D-10)
// ---------------------------------------------------------------------------

// Throws a QuiverError naming the struct and both numbers when they differ. This is one of the
// binding's few locally crafted messages (D-09) -- the C API cannot diagnose a disagreement
// about its own layout. Kept pure and parameterized so a test can drive the failure path
// without a second native library.
export function checkStructSize(name: string, expected: number, native: number): void {
  if (expected !== native) {
    throw new QuiverError(
      `Native struct layout mismatch for ${name}: expected ${expected} bytes, native library reports ${native} bytes. The JS binding and the native library were built from different C API headers.`,
    );
  }
}

// Names of the structs assertNativeStructSizes has actually checked, in check order, populated
// only after each check returns without throwing. Exists so a test can fail when the gate is
// unwired: driving only checkStructSize (as five of this file's tests already did) stayed green
// when assertNativeStructSizes's body was replaced with `void lib;` -- a vacuous pass this
// record makes impossible, since an unwired gate leaves the array empty.
const _checkedStructs: string[] = [];

export function checkedStructNames(): readonly string[] {
  return _checkedStructs;
}

// Checks all four structs the moment the library opens, in a fixed order (options, scalar
// metadata, group metadata, csv options), short-circuiting on the first mismatch -- a version
// skew makes all four suspect, so reporting more than one error is noise (must_haves
// EDGE/ordering).
//
// Bun returns a `bigint` for a USIZE FFI return (probe-verified in this repo's Bun:
// `dlopen("kernel32.dll", { GetACP: { args: [], returns: "usize" } })` yields `typeof ===
// "bigint"`, and `1252n === 1252` is `false`). Every other USIZE use in this file is an ARGUMENT
// position, so there is no in-repo precedent to copy for a USIZE return -- wrap each accessor
// call in Number(...) before comparing, matching readUint64Out's `Number(dv.getBigUint64(...))`.
// `bun test` does not typecheck, so a bare `24 !== 24n` would throw a bogus mismatch on every
// single load.
export function assertNativeStructSizes(lib: QuiverLib["symbols"]): void {
  _checkedStructs.length = 0;

  checkStructSize(
    "quiver_database_options_t",
    OPTIONS_SIZE,
    Number(lib.quiver_database_options_sizeof()),
  );
  _checkedStructs.push("quiver_database_options_t");

  checkStructSize(
    "quiver_scalar_metadata_t",
    SCALAR_METADATA_SIZE,
    Number(lib.quiver_scalar_metadata_sizeof()),
  );
  _checkedStructs.push("quiver_scalar_metadata_t");

  checkStructSize(
    "quiver_group_metadata_t",
    GROUP_METADATA_SIZE,
    Number(lib.quiver_group_metadata_sizeof()),
  );
  _checkedStructs.push("quiver_group_metadata_t");

  checkStructSize(
    "quiver_csv_options_t",
    CSV_OPTIONS_SIZE,
    Number(lib.quiver_csv_options_sizeof()),
  );
  _checkedStructs.push("quiver_csv_options_t");
}

// Resolve the native library lazily on first use. Initializing eagerly at
// module-evaluation time would run during the loader<->errors import cycle,
// before `errors.ts` has declared `QuiverError` -- making the not-found throw
// hit a temporal dead zone. Deferring to first call lets the module graph
// settle first; the result is cached, so callers still get it synchronously.
//
// assertNativeStructSizes runs HERE, outside every initLibrary() try/catch tier and outside
// resolveLibrary()'s own try/catch -- each tier swallows its load failure as "try the next path",
// so a size-mismatch thrown inside a tier would be masked as "Cannot load native library ..."
// (or, since resolveLibrary(), as the version-skew message), losing the struct name and both
// numbers. loadLibrary() sits outside every catch and runs the gate exactly once per process, on
// the memoized path.
let _lib: QuiverLib | null = null;

export function loadLibrary(): QuiverLib {
  if (_lib === null) {
    const lib = resolveLibrary(allSymbols);
    assertNativeStructSizes(lib.symbols);
    _lib = lib;
  }
  return _lib;
}

export function getSymbols() {
  return loadLibrary().symbols;
}
