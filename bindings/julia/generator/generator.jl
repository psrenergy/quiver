
import Pkg
Pkg.instantiate()

using Clang.Generators
using Libdl

function get_headers_recursive(dir::String)
    headers = String[]
    for entry in readdir(dir, join=true)
        if isdir(entry)
            append!(headers, get_headers_recursive(entry))
        elseif endswith(entry, ".h")
            push!(headers, entry)
        end
    end
    return headers
end

cd(@__DIR__)

include_dir = raw"C:\Development\Database\database\include"

options = load_options(joinpath(@__DIR__, "generator.toml"))

@show args = get_default_args()

@show headers = get_headers_recursive(include_dir)

Libdl.dlopen(raw"C:\Development\Database\database\build\bin\libpsr_database_c.dll")

ctx = create_context(headers, args, options)

build!(ctx)
