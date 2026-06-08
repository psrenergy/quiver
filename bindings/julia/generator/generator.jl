
import Pkg
Pkg.instantiate()

using Clang.Generators
using Libdl

cd(@__DIR__)

database_dir = joinpath(@__DIR__, "..", "..", "..")
include_dir = joinpath(database_dir, "include", "quiver", "c")

function libquiver_c_path()
    dir = if haskey(ENV, "QUIVER_LIB_DIR")
        ENV["QUIVER_LIB_DIR"]
    elseif Sys.iswindows()
        joinpath(database_dir, "build", "bin")
    else
        joinpath(database_dir, "build", "lib")
    end
    name = Sys.iswindows() ? "libquiver_c.dll" : Sys.isapple() ? "libquiver_c.dylib" : "libquiver_c.so"
    return joinpath(dir, name)
end

Libdl.dlopen(libquiver_c_path())

headers = [
    joinpath(root, file)
    for (root, _, files) in walkdir(include_dir)
    for file in files
    if endswith(file, ".h")
]
args = get_default_args()
options = load_options(joinpath(@__DIR__, "generator.toml"))
ctx = create_context(headers, args, options)
build!(ctx)
