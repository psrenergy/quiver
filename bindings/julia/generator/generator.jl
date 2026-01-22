
import Pkg
Pkg.instantiate()

using Clang.Generators
using Libdl

cd(@__DIR__)

database_dir = joinpath(@__DIR__, "..", "..", "..")
include_dir = joinpath(database_dir, "include", "quiver", "c")

Libdl.dlopen(joinpath(database_dir, "build", "bin", "libquiver_database_c.dll"))

headers = [
    joinpath(include_dir, header) for
    header in readdir(include_dir) if endswith(header, ".h") && header != "platform.h"
]
args = get_default_args()
options = load_options(joinpath(@__DIR__, "generator.toml"))
ctx = create_context(headers, args, options)
build!(ctx)
