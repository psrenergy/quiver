#! format: off

using CEnum
using Libdl

function library_name()
    if Sys.iswindows()
        return "libquiver_database_c.dll"
    elseif Sys.isapple()
        return "libquiver_database_c.dylib"
    else
        return "libquiver_database_c.so"
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

const libquiver_database_c = joinpath(@__DIR__, "..", "..", "..", "build", library_dir(), library_name())
