import { dlopen, FFIType, suffix } from "bun:ffi";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { QuiverError } from "./errors";

const CORE_LIB = `libquiver.${suffix}`;
const C_API_LIB = `libquiver_c.${suffix}`;

/**
 * Pre-load a DLL on Windows using kernel32 LoadLibraryA.
 * bun:ffi dlopen() requires at least one symbol, so it cannot be used
 * for dependency-only pre-loading (libquiver.dll has no C API symbols).
 * This is the equivalent of Python's os.add_dll_directory() approach.
 */
function preloadWindows(dllPath: string): void {
  const kernel32 = dlopen("kernel32.dll", {
    LoadLibraryA: { args: [FFIType.ptr], returns: FFIType.ptr },
  });
  kernel32.symbols.LoadLibraryA(Buffer.from(`${dllPath}\0`));
}

const symbols = {
  quiver_version: {
    args: [],
    returns: FFIType.cstring,
  },
  quiver_get_last_error: {
    args: [],
    returns: FFIType.ptr,
  },
  quiver_clear_last_error: {
    args: [],
    returns: FFIType.void,
  },
  quiver_database_from_schema: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_from_migrations: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_close: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },

  // Element lifecycle
  quiver_element_create: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_element_destroy: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },

  // Element scalar setters
  quiver_element_set_integer: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.i64],
    returns: FFIType.i32,
  },
  quiver_element_set_float: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.f64],
    returns: FFIType.i32,
  },
  quiver_element_set_string: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_element_set_null: {
    args: [FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Element array setters
  quiver_element_set_array_integer: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i32],
    returns: FFIType.i32,
  },
  quiver_element_set_array_float: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i32],
    returns: FFIType.i32,
  },
  quiver_element_set_array_string: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i32],
    returns: FFIType.i32,
  },

  // Database CRUD
  quiver_database_create_element: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_update_element: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_delete_element: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.i64],
    returns: FFIType.i32,
  },

  // Read scalar arrays
  quiver_database_read_scalar_integers: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_scalar_floats: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_scalar_strings: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Read scalar by ID
  quiver_database_read_scalar_integer_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_scalar_float_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_scalar_string_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Read vector bulk
  quiver_database_read_vector_integers: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_vector_floats: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_vector_strings: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Read set bulk
  quiver_database_read_set_integers: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_set_floats: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_set_strings: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Read vector by ID
  quiver_database_read_vector_integers_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_vector_floats_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_vector_strings_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Read set by ID
  quiver_database_read_set_integers_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_set_floats_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_read_set_strings_by_id: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.i64, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Read element IDs
  quiver_database_read_element_ids: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Free functions
  quiver_database_free_integer_array: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_free_float_array: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_free_string_array: {
    args: [FFIType.ptr, FFIType.u64],
    returns: FFIType.i32,
  },
  quiver_database_free_string: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },

  // Free vector/set bulk read results
  quiver_database_free_integer_vectors: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u64],
    returns: FFIType.i32,
  },
  quiver_database_free_float_vectors: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u64],
    returns: FFIType.i32,
  },
  quiver_database_free_string_vectors: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.u64],
    returns: FFIType.i32,
  },

  // Query operations (plain)
  quiver_database_query_string: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_query_integer: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_query_float: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },

  // Query operations (parameterized)
  quiver_database_query_string_params: {
    args: [
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.u64,
      FFIType.ptr,
      FFIType.ptr,
    ],
    returns: FFIType.i32,
  },
  quiver_database_query_integer_params: {
    args: [
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.u64,
      FFIType.ptr,
      FFIType.ptr,
    ],
    returns: FFIType.i32,
  },
  quiver_database_query_float_params: {
    args: [
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.u64,
      FFIType.ptr,
      FFIType.ptr,
    ],
    returns: FFIType.i32,
  },

  // Transaction control
  quiver_database_begin_transaction: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_commit: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_rollback: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_in_transaction: {
    args: [FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
} as const;

type Library = ReturnType<typeof dlopen<typeof symbols>>;

let _library: Library | null = null;

function getSearchPaths(): string[] {
  const paths: string[] = [];

  // Walk up from import.meta.dir (bindings/js/src/) looking for build/bin/
  let dir = import.meta.dir;
  for (let i = 0; i < 5; i++) {
    const candidate = join(dir, "build", "bin");
    if (existsSync(join(candidate, C_API_LIB))) {
      paths.push(candidate);
    }
    dir = dirname(dir);
  }

  return paths;
}

export function loadLibrary(): Library {
  if (_library) return _library;

  const searchPaths = getSearchPaths();

  for (const dir of searchPaths) {
    const corePath = join(dir, CORE_LIB);
    const cApiPath = join(dir, C_API_LIB);
    if (!existsSync(cApiPath)) continue;

    try {
      // Windows: pre-load core library (dependency chain)
      if (suffix === "dll" && existsSync(corePath)) {
        preloadWindows(corePath);
      }
      _library = dlopen(cApiPath, symbols);
      return _library;
    } catch {}
  }

  // Fallback: try system PATH (bare library name)
  try {
    if (suffix === "dll") {
      try {
        preloadWindows(CORE_LIB);
      } catch {
        // Core lib may already be loaded or not needed on this platform
      }
    }
    _library = dlopen(C_API_LIB, symbols);
    return _library;
  } catch {
    // Fall through to error
  }

  const searched =
    searchPaths.length > 0 ? `${searchPaths.join(", ")}, system PATH` : "system PATH";

  throw new QuiverError(`Cannot load native library '${C_API_LIB}'. Searched: ${searched}`);
}

export function getSymbols(): Library["symbols"] {
  return loadLibrary().symbols;
}
