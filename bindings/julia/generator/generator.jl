
import Pkg
Pkg.instantiate()

using Clang.Generators
using Libdl

cd(@__DIR__)

database_dir = joinpath(@__DIR__, "..", "..", "..")
include_dir = joinpath(database_dir, "include", "quiver", "c")

Libdl.dlopen(joinpath(database_dir, "build", "bin", "libquiver_c.dll"))

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
