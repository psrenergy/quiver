import { dlopen, type Library, type Pointer, suffix } from "bun:ffi";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { QuiverError } from "./errors.ts";

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
const V = "void" as const;

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
  quiver_clear_last_error: { args: [], returns: V },
  // quiver_database_options_default is intentionally omitted: it returns a
  // struct by value, which Bun FFI does not support (oven-sh/bun#6139). It was
  // never called -- makeDefaultOptions() in ffi-helpers.ts builds the options
  // struct in JS.
  quiver_database_from_schema: { args: [BUF, BUF, BUF, P], returns: I32 },
  quiver_database_from_migrations: { args: [BUF, BUF, BUF, P], returns: I32 },
  quiver_database_open: { args: [BUF, BUF, P], returns: I32 },
  quiver_database_close: { args: [P], returns: I32 },
  quiver_database_is_healthy: { args: [P, P], returns: I32 },
  quiver_database_path: { args: [P, P], returns: I32 },
  quiver_database_current_version: { args: [P, P], returns: I32 },
  quiver_database_describe: { args: [P], returns: I32 },
} as const;

const elementSymbols = {
  quiver_element_create: { args: [P], returns: I32 },
  quiver_element_destroy: { args: [P], returns: I32 },
  quiver_element_set_integer: { args: [P, BUF, I64], returns: I32 },
  quiver_element_set_float: { args: [P, BUF, F64], returns: I32 },
  quiver_element_set_string: { args: [P, BUF, BUF], returns: I32 },
  quiver_element_set_null: { args: [P, BUF], returns: I32 },
  quiver_element_set_array_integer: { args: [P, BUF, P, I32], returns: I32 },
  quiver_element_set_array_float: { args: [P, BUF, P, I32], returns: I32 },
  quiver_element_set_array_string: { args: [P, BUF, P, I32], returns: I32 },
} as const;

const crudSymbols = {
  quiver_database_create_element: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_update_element: { args: [P, BUF, I64, P], returns: I32 },
  quiver_database_delete_element: { args: [P, BUF, I64], returns: I32 },
} as const;

const readSymbols = {
  quiver_database_read_scalar_integers: { args: [P, BUF, BUF, P, P], returns: I32 },
  quiver_database_read_scalar_floats: { args: [P, BUF, BUF, P, P], returns: I32 },
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
} as const;

const metadataSymbols = {
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

const timeSeriesSymbols = {
  quiver_database_read_time_series_group: { args: [P, BUF, BUF, I64, P, P, P, P, P], returns: I32 },
  quiver_database_update_time_series_group: {
    args: [P, BUF, BUF, I64, P, P, P, USIZE, USIZE],
    returns: I32,
  },
  quiver_database_free_time_series_data: { args: [P, P, P, USIZE, USIZE], returns: I32 },
  quiver_database_has_time_series_files: { args: [P, BUF, P], returns: I32 },
  quiver_database_list_time_series_files_columns: { args: [P, BUF, P, P], returns: I32 },
  quiver_database_read_time_series_files: { args: [P, BUF, P, P, P], returns: I32 },
  quiver_database_update_time_series_files: { args: [P, BUF, P, P, USIZE], returns: I32 },
  quiver_database_free_time_series_files: { args: [P, P, USIZE], returns: I32 },
} as const;

const csvSymbols = {
  quiver_database_export_csv: { args: [P, BUF, BUF, BUF, P], returns: I32 },
  quiver_database_import_csv: { args: [P, BUF, BUF, BUF, P], returns: I32 },
} as const;

const freeSymbols = {
  quiver_database_free_integer_array: { args: [P], returns: I32 },
  quiver_database_free_float_array: { args: [P], returns: I32 },
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
  quiver_lua_runner_run: { args: [P, BUF], returns: I32 },
  quiver_lua_runner_get_error: { args: [P, P], returns: I32 },
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

function openLibrary(dir: string): QuiverLib {
  ensureCoreOnPath(dir);
  return dlopen(join(dir, C_API_LIB), allSymbols);
}

function initLibrary(): QuiverLib {
  let lastError: unknown;

  // Tier 1: Bundled libs/{os}-{arch}/ next to the loader (npm install / dev install).
  const bundledDir = getBundledLibDir();
  if (bundledDir) {
    try {
      return openLibrary(bundledDir);
    } catch (e) {
      lastError = e; // Bundled libs found but failed to load -- fall through.
    }
  }

  // Tier 2: Dev mode -- walk up directories looking for build/bin/.
  for (const dir of getSearchPaths()) {
    try {
      return openLibrary(dir);
    } catch (e) {
      lastError = e; // Try next path.
    }
  }

  // Tier 3: System PATH fallback -- the core lib is expected to be discoverable
  // on PATH alongside the C API lib.
  try {
    return dlopen(C_API_LIB, allSymbols);
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

// Resolve the native library lazily on first use. Initializing eagerly at
// module-evaluation time would run during the loader<->errors import cycle,
// before `errors.ts` has declared `QuiverError` -- making the not-found throw
// hit a temporal dead zone. Deferring to first call lets the module graph
// settle first; the result is cached, so callers still get it synchronously.
let _lib: QuiverLib | null = null;

export function loadLibrary(): QuiverLib {
  if (_lib === null) {
    _lib = initLibrary();
  }
  return _lib;
}

export function getSymbols() {
  return loadLibrary().symbols;
}

export type Symbols = ReturnType<typeof getSymbols>;
