import { dirname, join } from "jsr:@std/path";
import { QuiverError } from "./errors.ts";

// Deno FFI type shorthand constants
const P = "pointer" as const;
const BUF = "buffer" as const;
const I32 = "i32" as const;
const I64 = "i64" as const;
const USIZE = "usize" as const;
const F64 = "f64" as const;
const V = "void" as const;

// Platform constants
const EXT = Deno.build.os === "windows" ? "dll"
  : Deno.build.os === "darwin" ? "dylib" : "so";
const CORE_LIB = `libquiver.${EXT}`;
const C_API_LIB = `libquiver_c.${EXT}`;

// Opaque pointer type for native handles
export type NativePointer = Deno.PointerValue;

// ---------------------------------------------------------------------------
// Symbol definitions grouped by C API domain
// ---------------------------------------------------------------------------

const lifecycleSymbols = {
  quiver_version: { parameters: [], result: P },
  quiver_get_last_error: { parameters: [], result: P },
  quiver_clear_last_error: { parameters: [], result: V },
  quiver_database_options_default: { parameters: [], result: { struct: [I32, I32] } },
  quiver_database_from_schema: { parameters: [BUF, BUF, BUF, P], result: I32 },
  quiver_database_from_migrations: { parameters: [BUF, BUF, BUF, P], result: I32 },
  quiver_database_close: { parameters: [P], result: I32 },
  quiver_database_is_healthy: { parameters: [P, P], result: I32 },
  quiver_database_path: { parameters: [P, P], result: I32 },
  quiver_database_current_version: { parameters: [P, P], result: I32 },
  quiver_database_describe: { parameters: [P], result: I32 },
} as const;

const elementSymbols = {
  quiver_element_create: { parameters: [P], result: I32 },
  quiver_element_destroy: { parameters: [P], result: I32 },
  quiver_element_set_integer: { parameters: [P, BUF, I64], result: I32 },
  quiver_element_set_float: { parameters: [P, BUF, F64], result: I32 },
  quiver_element_set_string: { parameters: [P, BUF, BUF], result: I32 },
  quiver_element_set_null: { parameters: [P, BUF], result: I32 },
  quiver_element_set_array_integer: { parameters: [P, BUF, P, I32], result: I32 },
  quiver_element_set_array_float: { parameters: [P, BUF, P, I32], result: I32 },
  quiver_element_set_array_string: { parameters: [P, BUF, P, I32], result: I32 },
} as const;

const crudSymbols = {
  quiver_database_create_element: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_update_element: { parameters: [P, BUF, I64, P], result: I32 },
  quiver_database_delete_element: { parameters: [P, BUF, I64], result: I32 },
} as const;

const readSymbols = {
  quiver_database_read_scalar_integers: { parameters: [P, BUF, BUF, P, P], result: I32 },
  quiver_database_read_scalar_floats: { parameters: [P, BUF, BUF, P, P], result: I32 },
  quiver_database_read_scalar_strings: { parameters: [P, BUF, BUF, P, P], result: I32 },
  quiver_database_read_scalar_integer_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_scalar_float_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_scalar_string_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_vector_integers: { parameters: [P, BUF, BUF, P, P, P], result: I32 },
  quiver_database_read_vector_floats: { parameters: [P, BUF, BUF, P, P, P], result: I32 },
  quiver_database_read_vector_strings: { parameters: [P, BUF, BUF, P, P, P], result: I32 },
  quiver_database_read_set_integers: { parameters: [P, BUF, BUF, P, P, P], result: I32 },
  quiver_database_read_set_floats: { parameters: [P, BUF, BUF, P, P, P], result: I32 },
  quiver_database_read_set_strings: { parameters: [P, BUF, BUF, P, P, P], result: I32 },
  quiver_database_read_vector_integers_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_vector_floats_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_vector_strings_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_set_integers_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_set_floats_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_set_strings_by_id: { parameters: [P, BUF, BUF, I64, P, P], result: I32 },
  quiver_database_read_element_ids: { parameters: [P, BUF, P, P], result: I32 },
} as const;

const querySymbols = {
  quiver_database_query_string: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_query_integer: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_query_float: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_query_string_params: { parameters: [P, BUF, P, P, USIZE, P, P], result: I32 },
  quiver_database_query_integer_params: { parameters: [P, BUF, P, P, USIZE, P, P], result: I32 },
  quiver_database_query_float_params: { parameters: [P, BUF, P, P, USIZE, P, P], result: I32 },
} as const;

const transactionSymbols = {
  quiver_database_begin_transaction: { parameters: [P], result: I32 },
  quiver_database_commit: { parameters: [P], result: I32 },
  quiver_database_rollback: { parameters: [P], result: I32 },
  quiver_database_in_transaction: { parameters: [P, P], result: I32 },
} as const;

const metadataSymbols = {
  quiver_database_get_scalar_metadata: { parameters: [P, BUF, BUF, P], result: I32 },
  quiver_database_get_vector_metadata: { parameters: [P, BUF, BUF, P], result: I32 },
  quiver_database_get_set_metadata: { parameters: [P, BUF, BUF, P], result: I32 },
  quiver_database_get_time_series_metadata: { parameters: [P, BUF, BUF, P], result: I32 },
  quiver_database_free_scalar_metadata: { parameters: [P], result: I32 },
  quiver_database_free_group_metadata: { parameters: [P], result: I32 },
  quiver_database_list_scalar_attributes: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_list_vector_groups: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_list_set_groups: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_list_time_series_groups: { parameters: [P, BUF, P, P], result: I32 },
} as const;

const timeSeriesSymbols = {
  quiver_database_read_time_series_group: { parameters: [P, BUF, BUF, I64, P, P, P, P, P], result: I32 },
  quiver_database_update_time_series_group: { parameters: [P, BUF, BUF, I64, P, P, P, USIZE, USIZE], result: I32 },
  quiver_database_free_time_series_data: { parameters: [P, P, P, USIZE, USIZE], result: I32 },
  quiver_database_has_time_series_files: { parameters: [P, BUF, P], result: I32 },
  quiver_database_list_time_series_files_columns: { parameters: [P, BUF, P, P], result: I32 },
  quiver_database_read_time_series_files: { parameters: [P, BUF, P, P, P], result: I32 },
  quiver_database_update_time_series_files: { parameters: [P, BUF, P, P, USIZE], result: I32 },
  quiver_database_free_time_series_files: { parameters: [P, P, USIZE], result: I32 },
} as const;

const csvSymbols = {
  quiver_database_export_csv: { parameters: [P, BUF, BUF, BUF, P], result: I32 },
  quiver_database_import_csv: { parameters: [P, BUF, BUF, BUF, P], result: I32 },
} as const;

const freeSymbols = {
  quiver_database_free_integer_array: { parameters: [P], result: I32 },
  quiver_database_free_float_array: { parameters: [P], result: I32 },
  quiver_database_free_string_array: { parameters: [P, USIZE], result: I32 },
  quiver_database_free_string: { parameters: [P], result: I32 },
  quiver_database_free_integer_vectors: { parameters: [P, P, USIZE], result: I32 },
  quiver_database_free_float_vectors: { parameters: [P, P, USIZE], result: I32 },
  quiver_database_free_string_vectors: { parameters: [P, P, USIZE], result: I32 },
  quiver_database_free_scalar_metadata_array: { parameters: [P, USIZE], result: I32 },
  quiver_database_free_group_metadata_array: { parameters: [P, USIZE], result: I32 },
} as const;

const luaSymbols = {
  quiver_lua_runner_new: { parameters: [P, P], result: I32 },
  quiver_lua_runner_free: { parameters: [P], result: I32 },
  quiver_lua_runner_run: { parameters: [P, BUF], result: I32 },
  quiver_lua_runner_get_error: { parameters: [P, P], result: I32 },
} as const;

// Combined symbol map for Deno.dlopen
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

function fileExists(path: string): boolean {
  try {
    Deno.statSync(path);
    return true;
  } catch {
    return false;
  }
}

const __dirname = import.meta.dirname!;

function getBundledLibDir(): string | null {
  const platformKey = `${Deno.build.os}-${Deno.build.arch}`;
  const libDir = join(__dirname, "..", "libs", platformKey);
  if (fileExists(join(libDir, C_API_LIB))) {
    return libDir;
  }
  return null;
}

function getSearchPaths(): string[] {
  const paths: string[] = [];
  let dir = __dirname;
  for (let i = 0; i < 5; i++) {
    const candidate = join(dir, "build", "bin");
    if (fileExists(join(candidate, C_API_LIB))) {
      paths.push(candidate);
    }
    dir = dirname(dir);
  }
  return paths;
}

type DenoLib = Deno.DynamicLibrary<typeof allSymbols>;
let _lib: DenoLib | null = null;
let _coreLib: Deno.DynamicLibrary<Record<string, never>> | null = null;

function openLibrary(dir: string): DenoLib {
  const corePath = join(dir, CORE_LIB);
  const cApiPath = join(dir, C_API_LIB);

  if (Deno.build.os === "windows" && fileExists(corePath)) {
    _coreLib = Deno.dlopen(corePath, {});
  }

  return Deno.dlopen(cApiPath, allSymbols);
}

export function loadLibrary(): DenoLib {
  if (_lib) return _lib;

  // Tier 1: Bundled libs/{os}-{arch}/
  const bundledDir = getBundledLibDir();
  if (bundledDir) {
    try {
      _lib = openLibrary(bundledDir);
      return _lib;
    } catch {
      // Bundled libs found but failed to load -- fall through to dev paths
    }
  }

  // Tier 2: Dev mode -- walk up directories looking for build/bin/
  const searchPaths = getSearchPaths();
  for (const dir of searchPaths) {
    try {
      _lib = openLibrary(dir);
      return _lib;
    } catch {
      // Try next path
    }
  }

  // Tier 3: System PATH fallback
  try {
    if (Deno.build.os === "windows") {
      try {
        _coreLib = Deno.dlopen(CORE_LIB, {});
      } catch {
        // Core lib may already be loaded
      }
    }
    _lib = Deno.dlopen(C_API_LIB, allSymbols);
    return _lib;
  } catch {
    // Fall through to error
  }

  const bundledPath = join(__dirname, "..", "libs", `${Deno.build.os}-${Deno.build.arch}`);
  const allPaths = [bundledPath, ...searchPaths, "system PATH"].join(", ");
  throw new QuiverError(`Cannot load native library '${C_API_LIB}'. Searched: ${allPaths}`);
}

export function getSymbols() {
  return loadLibrary().symbols;
}

export type Symbols = ReturnType<typeof getSymbols>;
