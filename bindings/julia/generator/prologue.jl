#! format: off

using CEnum
using Libdl

function library_name()
    if Sys.iswindows()
        return "libmargaux_c.dll"
    elseif Sys.isapple()
        return "libmargaux_c.dylib"
    else
        return "libmargaux_c.so"
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

const libmargaux_c = joinpath(@__DIR__, "..", "..", "..", "build", library_dir(), library_name())
