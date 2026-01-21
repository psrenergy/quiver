module Margaux

include("c_api.jl")
import .C

include("exceptions.jl")
include("element.jl")
include("database.jl")
include("database_create.jl")
include("database_read.jl")
include("database_update.jl")
include("database_delete.jl")
include("lua_runner.jl")

export Element, Database, LuaRunner, DatabaseException

# Re-export enums from C module
const MARGAUX_DATA_STRUCTURE_SCALAR = C.MARGAUX_DATA_STRUCTURE_SCALAR
const MARGAUX_DATA_STRUCTURE_VECTOR = C.MARGAUX_DATA_STRUCTURE_VECTOR
const MARGAUX_DATA_STRUCTURE_SET = C.MARGAUX_DATA_STRUCTURE_SET
const MARGAUX_DATA_TYPE_INTEGER = C.MARGAUX_DATA_TYPE_INTEGER
const MARGAUX_DATA_TYPE_FLOAT = C.MARGAUX_DATA_TYPE_FLOAT
const MARGAUX_DATA_TYPE_STRING = C.MARGAUX_DATA_TYPE_STRING

end
