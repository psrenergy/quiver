import { dirname, join } from "jsr:@std/path@^1.1.4";
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

// `import.meta.dirname` is defined only for `file:` URLs. For `jsr:` / `https:`
// imports it is `undefined`, so treat it as optional everywhere.
const __dirname: string | undefined = import.meta.dirname;
const moduleUrl = import.meta.url;
const platformKey = `${Deno.build.os}-${Deno.build.arch}`;

function getBundledLibDir(): string | null {
  if (!__dirname) return null;
  const libDir = join(__dirname, "..", "libs", platformKey);
  if (fileExists(join(libDir, C_API_LIB))) {
    return libDir;
  }
  return null;
}

function getSearchPaths(): string[] {
  if (!__dirname) return [];
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

function tryEnv(name: string): string | null {
  try {
    return Deno.env.get(name) ?? null;
  } catch {
    // --allow-env not granted; cache falls back to a cwd-local directory.
    return null;
  }
}

function getCacheBaseDir(): string {
  if (Deno.build.os === "windows") {
    const local = tryEnv("LOCALAPPDATA");
    if (local) return join(local, "psrenergy-quiver");
    const profile = tryEnv("USERPROFILE");
    if (profile) return join(profile, "AppData", "Local", "psrenergy-quiver");
    return join(Deno.cwd(), ".psrenergy-quiver-cache");
  }
  const xdg = tryEnv("XDG_CACHE_HOME");
  if (xdg) return join(xdg, "psrenergy-quiver");
  const home = tryEnv("HOME");
  if (home) return join(home, ".cache", "psrenergy-quiver");
  return join(Deno.cwd(), ".psrenergy-quiver-cache");
}

// Version-scoped cache directory derived from the module URL so different
// versions or origins do not collide.
function getRemoteCacheDir(): string {
  const url = new URL(moduleUrl);
  // Drop ".../src/loader.ts" to get the package root pathname.
  const pkgRootPath = url.pathname.replace(/\/[^/]*\/[^/]*$/, "");
  const safe = (url.host + pkgRootPath).replace(/[^a-zA-Z0-9._@/-]+/g, "_");
  return join(getCacheBaseDir(), safe, "libs", platformKey);
}

async function fetchRemoteLib(libName: string, destPath: string): Promise<boolean> {
  try {
    const remote = new URL(`../libs/${platformKey}/${libName}`, moduleUrl);
    const res = await fetch(remote);
    if (!res.ok) return false;
    const buf = new Uint8Array(await res.arrayBuffer());
    await Deno.writeFile(destPath, buf, { mode: 0o755 });
    return true;
  } catch {
    return false;
  }
}

async function resolveRemoteLibDir(): Promise<string | null> {
  if (moduleUrl.startsWith("file:")) return null;
  const cacheDir = getRemoteCacheDir();
  const cApiCachePath = join(cacheDir, C_API_LIB);
  const coreCachePath = join(cacheDir, CORE_LIB);

  try {
    await Deno.mkdir(cacheDir, { recursive: true });
  } catch {
    return null;
  }

  if (!fileExists(cApiCachePath)) {
    const ok = await fetchRemoteLib(C_API_LIB, cApiCachePath);
    if (!ok) return null;
  }
  // Core lib is optional on Linux (resolved via RPATH=$ORIGIN from the C API
  // lib in the same dir); on Windows it must be preloaded from the same dir.
  if (!fileExists(coreCachePath)) {
    await fetchRemoteLib(CORE_LIB, coreCachePath);
  }
  return cacheDir;
}

type DenoLib = Deno.DynamicLibrary<typeof allSymbols>;
let _coreLib: Deno.DynamicLibrary<Record<string, never>> | null = null;

function openLibrary(dir: string): DenoLib {
  const corePath = join(dir, CORE_LIB);
  const cApiPath = join(dir, C_API_LIB);

  if (Deno.build.os === "windows" && fileExists(corePath)) {
    _coreLib = Deno.dlopen(corePath, {});
  }

  return Deno.dlopen(cApiPath, allSymbols);
}

async function initLibrary(): Promise<DenoLib> {
  // Tier 1: Bundled libs/{os}-{arch}/ next to the loader (file: URL / dev install).
  const bundledDir = getBundledLibDir();
  if (bundledDir) {
    try {
      return openLibrary(bundledDir);
    } catch {
      // Bundled libs found but failed to load -- fall through.
    }
  }

  // Tier 2: Dev mode -- walk up directories looking for build/bin/.
  for (const dir of getSearchPaths()) {
    try {
      return openLibrary(dir);
    } catch {
      // Try next path.
    }
  }

  // Tier 3: Remote module (jsr:/https:) -- download bundled libs to local cache.
  const remoteDir = await resolveRemoteLibDir();
  if (remoteDir) {
    try {
      return openLibrary(remoteDir);
    } catch {
      // Fall through.
    }
  }

  // Tier 4: System PATH fallback.
  try {
    if (Deno.build.os === "windows") {
      try {
        _coreLib = Deno.dlopen(CORE_LIB, {});
      } catch {
        // Core lib may already be loaded.
      }
    }
    return Deno.dlopen(C_API_LIB, allSymbols);
  } catch {
    // Fall through to error.
  }

  const searched = [
    __dirname ? join(__dirname, "..", "libs", platformKey) : null,
    ...getSearchPaths(),
    moduleUrl.startsWith("file:") ? null : getRemoteCacheDir(),
    "system PATH",
  ].filter((p): p is string => p !== null).join(", ");
  throw new QuiverError(`Cannot load native library '${C_API_LIB}'. Searched: ${searched}`);
}

// Top-level await: resolve the native library once, at module-instantiation
// time, so downstream callers can use `getSymbols()` synchronously.
const _lib: DenoLib = await initLibrary();

export function loadLibrary(): DenoLib {
  return _lib;
}

export function getSymbols() {
  return _lib.symbols;
}

export type Symbols = ReturnType<typeof getSymbols>;
