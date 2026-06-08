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

# Directory holding libquiver_c (and its libquiver dependency). Resolved (at module
# load) in priority order:
#   1. QUIVER_LIB_DIR     -- explicit override (CI / advanced users; set before load)
#   2. the S3 artifact    -- when an Artifacts.toml is present (published Quiver.jl mirror)
#   3. the in-tree build/ -- monorepo local development
function quiver_lib_dir()
    haskey(ENV, "QUIVER_LIB_DIR") && return ENV["QUIVER_LIB_DIR"]
    artifacts_toml = Artifacts.find_artifacts_toml(@__DIR__)
    if artifacts_toml !== nothing
        hash = Artifacts.artifact_hash("quiver", artifacts_toml)
        hash !== nothing && return joinpath(Artifacts.artifact_path(hash), library_dir())
    end
    return joinpath(@__DIR__, "..", "..", "..", "build", library_dir())
end

const libquiver_c = joinpath(quiver_lib_dir(), library_name())

function __init__()
    # Pre-load the transitive dependency (libquiver) from the same directory (Windows robustness).
    dep = Sys.iswindows() ? "libquiver.dll" : Sys.isapple() ? "libquiver.dylib" : "libquiver.so"
    Libdl.dlopen(joinpath(dirname(libquiver_c), dep); throw_error = false)
end
