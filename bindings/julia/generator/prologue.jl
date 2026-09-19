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

# Wiring-evidence list (mirrors Python's _CHECKED_STRUCTS from 02-08): cleared on entry to
# _assert_struct_sizes and appended after each struct's check passes. Lets a test observe that
# the gate actually RAN (and in what order) without re-driving _check_struct_size itself -- which
# would recreate the vacuous-pass defect this list exists to prevent.
const _CHECKED_STRUCTS = String[]

# SAFE-02/SAFE-03/D-08/D-09: a load-time struct-layout gate. `c_api.jl`'s `__init__` is verbatim
# `generator/prologue.jl` content (generator.toml's `prologue_file_path`), so this is the ONE
# place a hand-written safety check survives `generator.bat` -- writing it into `c_api.jl` itself
# would be silently deleted by the next regeneration while every happy-path test stayed green.
#
# D-09: this message is locally crafted (one of the binding's few) because the C API cannot
# diagnose a disagreement about its own struct layout.
function _check_struct_size(name::AbstractString, expected::Integer, native::Integer)
    if expected != native
        error(
            "Quiver native library layout mismatch for $(name): Julia expects $(expected) " *
            "bytes, the loaded native library reports $(native) bytes. Reinstall a matching " *
            "Quiver native library.",
        )
    end
    return nothing
end

# Calls a zero-argument native size accessor (an @ccall closure) and re-raises a lookup/dlopen
# failure -- e.g. a native library predating this phase, missing the `*_sizeof` symbol -- with a
# message distinguishing "can't even ask the native library" from "asked, and it disagrees".
function _native_struct_size(get_size::Function, name::AbstractString)
    try
        return Int(get_size())
    catch e
        error(
            "Cannot verify the layout of $(name): failed to call its native size accessor " *
            "(the loaded Quiver native library may predate this API or failed to load). " *
            "Reinstall a matching Quiver native library. Original error: $(e)",
        )
    end
end

# Fixed order: options, scalar metadata, group metadata, csv options -- short-circuits on the
# first mismatch (`error` throws, so later checks never run). Promoted rule (02-08): every struct
# a binding hand-allocates a raw buffer for joins this list by default.
function _assert_struct_sizes()
    empty!(_CHECKED_STRUCTS)
    _check_struct_size(
        "quiver_database_options_t",
        sizeof(quiver_database_options_t),
        _native_struct_size(
            () -> (@ccall libquiver_c.quiver_database_options_sizeof()::Csize_t),
            "quiver_database_options_t",
        ),
    )
    push!(_CHECKED_STRUCTS, "quiver_database_options_t")
    _check_struct_size(
        "quiver_scalar_metadata_t",
        sizeof(quiver_scalar_metadata_t),
        _native_struct_size(
            () -> (@ccall libquiver_c.quiver_scalar_metadata_sizeof()::Csize_t),
            "quiver_scalar_metadata_t",
        ),
    )
    push!(_CHECKED_STRUCTS, "quiver_scalar_metadata_t")
    _check_struct_size(
        "quiver_group_metadata_t",
        sizeof(quiver_group_metadata_t),
        _native_struct_size(
            () -> (@ccall libquiver_c.quiver_group_metadata_sizeof()::Csize_t),
            "quiver_group_metadata_t",
        ),
    )
    push!(_CHECKED_STRUCTS, "quiver_group_metadata_t")
    _check_struct_size(
        "quiver_csv_options_t",
        sizeof(quiver_csv_options_t),
        _native_struct_size(
            () -> (@ccall libquiver_c.quiver_csv_options_sizeof()::Csize_t),
            "quiver_csv_options_t",
        ),
    )
    push!(_CHECKED_STRUCTS, "quiver_csv_options_t")
    return nothing
end

function __init__()
    dir = quiver_lib_dir()
    # Pre-load the transitive dependency (libquiver) from the same directory (Windows robustness).
    # On macOS the artifact ships libquiver only under its install name (the .0 tracks
    # SOVERSION = major version).
    dep = Sys.iswindows() ? "libquiver.dll" : Sys.isapple() ? "libquiver.0.dylib" : "libquiver.so"
    Libdl.dlopen(joinpath(dir, dep); throw_error = false)
    global libquiver_c = joinpath(dir, library_name())
    _assert_struct_sizes()
end
