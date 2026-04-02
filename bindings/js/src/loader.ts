import koffi from "koffi";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { QuiverError } from "./errors.js";

const EXT = process.platform === "win32" ? "dll" : process.platform === "darwin" ? "dylib" : "so";
const CORE_LIB = `libquiver.${EXT}`;
const C_API_LIB = `libquiver_c.${EXT}`;

const DatabaseOptionsType = koffi.struct("quiver_database_options_t", {
  read_only: "int",
  console_level: "int",
});

type NativePointer = unknown;
export type { NativePointer };

// All koffi FFI functions accept various types at runtime (Buffer, TypedArray, string, number).
// eslint-disable-next-line @typescript-eslint/no-explicit-any
export type FFIFunction = (...args: any[]) => any;
export type Symbols = Record<string, FFIFunction>;

// eslint-disable-next-line @typescript-eslint/no-explicit-any
function bindSymbols(lib: any): Symbols {
  const P = "void *";
  const S = "str";
  const I64 = "int64_t";

  return {
    quiver_version: lib.func("quiver_version", S, []),
    quiver_get_last_error: lib.func("quiver_get_last_error", S, []),
    quiver_clear_last_error: lib.func("quiver_clear_last_error", "void", []),
    quiver_database_options_default: lib.func(
      "quiver_database_options_default", DatabaseOptionsType, [],
    ),

    quiver_database_from_schema: lib.func("quiver_database_from_schema", "int", [S, S, P, P]),
    quiver_database_from_migrations: lib.func("quiver_database_from_migrations", "int", [S, S, P, P]),
    quiver_database_close: lib.func("quiver_database_close", "int", [P]),

    quiver_element_create: lib.func("quiver_element_create", "int", [P]),
    quiver_element_destroy: lib.func("quiver_element_destroy", "int", [P]),
    quiver_element_set_integer: lib.func("quiver_element_set_integer", "int", [P, S, I64]),
    quiver_element_set_float: lib.func("quiver_element_set_float", "int", [P, S, "double"]),
    quiver_element_set_string: lib.func("quiver_element_set_string", "int", [P, S, S]),
    quiver_element_set_null: lib.func("quiver_element_set_null", "int", [P, S]),
    quiver_element_set_array_integer: lib.func("quiver_element_set_array_integer", "int", [P, S, P, "int"]),
    quiver_element_set_array_float: lib.func("quiver_element_set_array_float", "int", [P, S, P, "int"]),
    quiver_element_set_array_string: lib.func("quiver_element_set_array_string", "int", [P, S, "const char **", "int"]),

    quiver_database_create_element: lib.func("quiver_database_create_element", "int", [P, S, P, P]),
    quiver_database_update_element: lib.func("quiver_database_update_element", "int", [P, S, I64, P]),
    quiver_database_delete_element: lib.func("quiver_database_delete_element", "int", [P, S, I64]),

    quiver_database_read_scalar_integers: lib.func("quiver_database_read_scalar_integers", "int", [P, S, S, P, P]),
    quiver_database_read_scalar_floats: lib.func("quiver_database_read_scalar_floats", "int", [P, S, S, P, P]),
    quiver_database_read_scalar_strings: lib.func("quiver_database_read_scalar_strings", "int", [P, S, S, P, P]),
    quiver_database_read_scalar_integer_by_id: lib.func("quiver_database_read_scalar_integer_by_id", "int", [P, S, S, I64, P, P]),
    quiver_database_read_scalar_float_by_id: lib.func("quiver_database_read_scalar_float_by_id", "int", [P, S, S, I64, P, P]),
    quiver_database_read_scalar_string_by_id: lib.func("quiver_database_read_scalar_string_by_id", "int", [P, S, S, I64, P, P]),

    quiver_database_read_vector_integers: lib.func("quiver_database_read_vector_integers", "int", [P, S, S, P, P, P]),
    quiver_database_read_vector_floats: lib.func("quiver_database_read_vector_floats", "int", [P, S, S, P, P, P]),
    quiver_database_read_vector_strings: lib.func("quiver_database_read_vector_strings", "int", [P, S, S, P, P, P]),
    quiver_database_read_set_integers: lib.func("quiver_database_read_set_integers", "int", [P, S, S, P, P, P]),
    quiver_database_read_set_floats: lib.func("quiver_database_read_set_floats", "int", [P, S, S, P, P, P]),
    quiver_database_read_set_strings: lib.func("quiver_database_read_set_strings", "int", [P, S, S, P, P, P]),

    quiver_database_read_vector_integers_by_id: lib.func("quiver_database_read_vector_integers_by_id", "int", [P, S, S, I64, P, P]),
    quiver_database_read_vector_floats_by_id: lib.func("quiver_database_read_vector_floats_by_id", "int", [P, S, S, I64, P, P]),
    quiver_database_read_vector_strings_by_id: lib.func("quiver_database_read_vector_strings_by_id", "int", [P, S, S, I64, P, P]),
    quiver_database_read_set_integers_by_id: lib.func("quiver_database_read_set_integers_by_id", "int", [P, S, S, I64, P, P]),
    quiver_database_read_set_floats_by_id: lib.func("quiver_database_read_set_floats_by_id", "int", [P, S, S, I64, P, P]),
    quiver_database_read_set_strings_by_id: lib.func("quiver_database_read_set_strings_by_id", "int", [P, S, S, I64, P, P]),

    quiver_database_read_element_ids: lib.func("quiver_database_read_element_ids", "int", [P, S, P, P]),

    quiver_database_free_integer_array: lib.func("quiver_database_free_integer_array", "int", [P]),
    quiver_database_free_float_array: lib.func("quiver_database_free_float_array", "int", [P]),
    quiver_database_free_string_array: lib.func("quiver_database_free_string_array", "int", [P, "uint64_t"]),
    quiver_database_free_string: lib.func("quiver_database_free_string", "int", [P]),
    quiver_database_free_integer_vectors: lib.func("quiver_database_free_integer_vectors", "int", [P, P, "uint64_t"]),
    quiver_database_free_float_vectors: lib.func("quiver_database_free_float_vectors", "int", [P, P, "uint64_t"]),
    quiver_database_free_string_vectors: lib.func("quiver_database_free_string_vectors", "int", [P, P, "uint64_t"]),

    quiver_database_query_string: lib.func("quiver_database_query_string", "int", [P, S, P, P]),
    quiver_database_query_integer: lib.func("quiver_database_query_integer", "int", [P, S, P, P]),
    quiver_database_query_float: lib.func("quiver_database_query_float", "int", [P, S, P, P]),
    quiver_database_query_string_params: lib.func("quiver_database_query_string_params", "int", [P, S, P, P, "uint64_t", P, P]),
    quiver_database_query_integer_params: lib.func("quiver_database_query_integer_params", "int", [P, S, P, P, "uint64_t", P, P]),
    quiver_database_query_float_params: lib.func("quiver_database_query_float_params", "int", [P, S, P, P, "uint64_t", P, P]),

    quiver_database_begin_transaction: lib.func("quiver_database_begin_transaction", "int", [P]),
    quiver_database_commit: lib.func("quiver_database_commit", "int", [P]),
    quiver_database_rollback: lib.func("quiver_database_rollback", "int", [P]),
    quiver_database_in_transaction: lib.func("quiver_database_in_transaction", "int", [P, P]),

    quiver_database_get_scalar_metadata: lib.func("quiver_database_get_scalar_metadata", "int", [P, S, S, P]),
    quiver_database_get_vector_metadata: lib.func("quiver_database_get_vector_metadata", "int", [P, S, S, P]),
    quiver_database_get_set_metadata: lib.func("quiver_database_get_set_metadata", "int", [P, S, S, P]),
    quiver_database_get_time_series_metadata: lib.func("quiver_database_get_time_series_metadata", "int", [P, S, S, P]),
    quiver_database_free_scalar_metadata: lib.func("quiver_database_free_scalar_metadata", "int", [P]),
    quiver_database_free_group_metadata: lib.func("quiver_database_free_group_metadata", "int", [P]),

    quiver_database_list_scalar_attributes: lib.func("quiver_database_list_scalar_attributes", "int", [P, S, P, P]),
    quiver_database_list_vector_groups: lib.func("quiver_database_list_vector_groups", "int", [P, S, P, P]),
    quiver_database_list_set_groups: lib.func("quiver_database_list_set_groups", "int", [P, S, P, P]),
    quiver_database_list_time_series_groups: lib.func("quiver_database_list_time_series_groups", "int", [P, S, P, P]),
    quiver_database_free_scalar_metadata_array: lib.func("quiver_database_free_scalar_metadata_array", "int", [P, "uint64_t"]),
    quiver_database_free_group_metadata_array: lib.func("quiver_database_free_group_metadata_array", "int", [P, "uint64_t"]),

    quiver_database_read_time_series_group: lib.func("quiver_database_read_time_series_group", "int", [P, S, S, I64, P, P, P, P, P]),
    quiver_database_update_time_series_group: lib.func("quiver_database_update_time_series_group", "int", [P, S, S, I64, P, P, P, "uint64_t", "uint64_t"]),
    quiver_database_free_time_series_data: lib.func("quiver_database_free_time_series_data", "int", [P, P, P, "uint64_t", "uint64_t"]),

    quiver_database_has_time_series_files: lib.func("quiver_database_has_time_series_files", "int", [P, S, P]),
    quiver_database_list_time_series_files_columns: lib.func("quiver_database_list_time_series_files_columns", "int", [P, S, P, P]),
    quiver_database_read_time_series_files: lib.func("quiver_database_read_time_series_files", "int", [P, S, P, P, P]),
    quiver_database_update_time_series_files: lib.func("quiver_database_update_time_series_files", "int", [P, S, P, P, "uint64_t"]),
    quiver_database_free_time_series_files: lib.func("quiver_database_free_time_series_files", "int", [P, P, "uint64_t"]),

    quiver_database_is_healthy: lib.func("quiver_database_is_healthy", "int", [P, P]),
    quiver_database_current_version: lib.func("quiver_database_current_version", "int", [P, P]),
    quiver_database_path: lib.func("quiver_database_path", "int", [P, P]),
    quiver_database_describe: lib.func("quiver_database_describe", "int", [P]),

    quiver_database_export_csv: lib.func("quiver_database_export_csv", "int", [P, S, S, S, P]),
    quiver_database_import_csv: lib.func("quiver_database_import_csv", "int", [P, S, S, S, P]),

    quiver_lua_runner_new: lib.func("quiver_lua_runner_new", "int", [P, P]),
    quiver_lua_runner_free: lib.func("quiver_lua_runner_free", "int", [P]),
    quiver_lua_runner_run: lib.func("quiver_lua_runner_run", "int", [P, S]),
    quiver_lua_runner_get_error: lib.func("quiver_lua_runner_get_error", "int", [P, P]),
  };
}

const __dirname = dirname(fileURLToPath(import.meta.url));

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

let _symbols: Symbols | null = null;
let _coreLib: unknown = null;

export function loadLibrary(): Symbols {
  if (_symbols) return _symbols;

  const searchPaths = getSearchPaths();

  for (const dir of searchPaths) {
    const corePath = join(dir, CORE_LIB);
    const cApiPath = join(dir, C_API_LIB);
    if (!existsSync(cApiPath)) continue;

    try {
      if (process.platform === "win32" && existsSync(corePath)) {
        _coreLib = koffi.load(corePath);
      }
      const lib = koffi.load(cApiPath);
      _symbols = bindSymbols(lib);
      return _symbols;
    } catch {
      // Try next path
    }
  }

  // Fallback: system PATH
  try {
    if (process.platform === "win32") {
      try {
        _coreLib = koffi.load(CORE_LIB);
      } catch {
        // Core lib may already be loaded
      }
    }
    const lib = koffi.load(C_API_LIB);
    _symbols = bindSymbols(lib);
    return _symbols;
  } catch {
    // Fall through to error
  }

  const searched =
    searchPaths.length > 0 ? `${searchPaths.join(", ")}, system PATH` : "system PATH";
  throw new QuiverError(`Cannot load native library '${C_API_LIB}'. Searched: ${searched}`);
}

export function getSymbols(): Symbols {
  return loadLibrary();
}
