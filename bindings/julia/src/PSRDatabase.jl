module PSRDatabase

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
export get_attribute_type
export PSR_DATA_STRUCTURE_SCALAR, PSR_DATA_STRUCTURE_VECTOR, PSR_DATA_STRUCTURE_SET
export PSR_DATA_TYPE_INTEGER, PSR_DATA_TYPE_FLOAT, PSR_DATA_TYPE_STRING

# Re-export enums from C module
const PSR_DATA_STRUCTURE_SCALAR = C.PSR_DATA_STRUCTURE_SCALAR
const PSR_DATA_STRUCTURE_VECTOR = C.PSR_DATA_STRUCTURE_VECTOR
const PSR_DATA_STRUCTURE_SET = C.PSR_DATA_STRUCTURE_SET
const PSR_DATA_TYPE_INTEGER = C.PSR_DATA_TYPE_INTEGER
const PSR_DATA_TYPE_FLOAT = C.PSR_DATA_TYPE_FLOAT
const PSR_DATA_TYPE_STRING = C.PSR_DATA_TYPE_STRING

end
