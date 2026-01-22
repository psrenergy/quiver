module QUIVERDatabase

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
const QUIVER_DATA_STRUCTURE_SCALAR = C.QUIVER_DATA_STRUCTURE_SCALAR
const QUIVER_DATA_STRUCTURE_VECTOR = C.QUIVER_DATA_STRUCTURE_VECTOR
const QUIVER_DATA_STRUCTURE_SET = C.QUIVER_DATA_STRUCTURE_SET
const QUIVER_DATA_TYPE_INTEGER = C.QUIVER_DATA_TYPE_INTEGER
const QUIVER_DATA_TYPE_FLOAT = C.QUIVER_DATA_TYPE_FLOAT
const QUIVER_DATA_TYPE_STRING = C.QUIVER_DATA_TYPE_STRING

end
