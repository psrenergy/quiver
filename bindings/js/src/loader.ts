import { dlopen, FFIType, suffix } from "bun:ffi";
import { existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { QuiverError } from "./errors";

const CORE_LIB = `libquiver.${suffix}`;
const C_API_LIB = `libquiver_c.${suffix}`;

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
        dlopen(corePath, {});
      }
      _library = dlopen(cApiPath, symbols);
      return _library;
    } catch {
      continue;
    }
  }

  // Fallback: try system PATH (bare library name)
  try {
    if (suffix === "dll") {
      try {
        dlopen(CORE_LIB, {});
      } catch {
        // Core lib may already be loaded or not needed on this platform
      }
    }
    _library = dlopen(C_API_LIB, symbols);
    return _library;
  } catch {
    // Fall through to error
  }

  const searched = searchPaths.length > 0
    ? searchPaths.join(", ") + ", system PATH"
    : "system PATH";

  throw new QuiverError(
    `Cannot load native library '${C_API_LIB}'. Searched: ${searched}`
  );
}

export function getSymbols(): Library["symbols"] {
  return loadLibrary().symbols;
}
