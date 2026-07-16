#! format: off

using CEnum
using Artifacts
using Libdl

function library_name()
    if Sys.iswindows()
        return "libquiver_c.dll"
    elseif Sys.isapple()
        return "libquiver_c.dylib"
    else
        return "libquiver_c.so"
    end
end

# On Windows, DLLs go to bin/; on Linux/macOS, shared libs go to lib/
function library_dir()
    if Sys.iswindows()
        return "bin"
    else
        return "lib"
    end
end

# Content-addressed artifact hash, resolved at PRECOMPILE time. A SHA1 is a value (not a path),
# so it survives being baked into a PackageCompiler sysimage and relocated to another machine.
# `nothing` in the monorepo (no Artifacts.toml), so precompilation still succeeds there. The
# library *directory* is resolved at runtime (below) from this hash -- never baked as an absolute
# path, because a baked path freezes the build machine's depot location and breaks
# compiled/relocated apps (the failure this loader is designed to avoid).
const _quiver_artifact_hash = let
    artifacts_toml = Artifacts.find_artifacts_toml(@__DIR__)
    artifacts_toml === nothing ? nothing : Artifacts.artifact_hash("quiver", artifacts_toml)
end

# Directory holding libquiver_c (and its libquiver dependency), resolved at RUNTIME (from
# __init__) in priority order:
#   1. QUIVER_LIB_DIR     -- explicit override (CI / advanced users)
#   2. the S3 artifact    -- located by hash against the runtime DEPOT_PATH (published Quiver.jl
#                            mirror, or bundled inside a PackageCompiler app)
#   3. the in-tree build/ -- monorepo local development
function quiver_lib_dir()
    haskey(ENV, "QUIVER_LIB_DIR") && return ENV["QUIVER_LIB_DIR"]
    _quiver_artifact_hash !== nothing &&
        return joinpath(Artifacts.artifact_path(_quiver_artifact_hash), library_dir())
    return joinpath(@__DIR__, "..", "..", "..", "build", library_dir())
end

# Assigned in __init__ (runtime), never at precompile time -- see _quiver_artifact_hash above.
# Typed global so the @ccall sites keep their efficient codegen.
libquiver_c::String = ""

function __init__()
    dir = quiver_lib_dir()
    # Pre-load the transitive dependency (libquiver) from the same directory (Windows robustness).
    # On macOS the artifact ships libquiver only under its install name (the .0 tracks
    # SOVERSION = major version).
    dep = Sys.iswindows() ? "libquiver.dll" : Sys.isapple() ? "libquiver.0.dylib" : "libquiver.so"
    Libdl.dlopen(joinpath(dir, dep); throw_error = false)
    global libquiver_c = joinpath(dir, library_name())
end
