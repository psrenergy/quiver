module PSRDatabase

include("c_api.jl")
import .C

include("exceptions.jl")
include("element.jl")
include("database.jl")
include("lua_runner.jl")

export Element, Database, LuaRunner, DatabaseException
export get_attribute_type
export PSR_ATTRIBUTE_SCALAR, PSR_ATTRIBUTE_VECTOR, PSR_ATTRIBUTE_SET
export PSR_DATA_TYPE_INTEGER, PSR_DATA_TYPE_REAL, PSR_DATA_TYPE_TEXT

# Re-export enums from C module
const PSR_ATTRIBUTE_SCALAR = C.PSR_ATTRIBUTE_SCALAR
const PSR_ATTRIBUTE_VECTOR = C.PSR_ATTRIBUTE_VECTOR
const PSR_ATTRIBUTE_SET = C.PSR_ATTRIBUTE_SET
const PSR_DATA_TYPE_INTEGER = C.PSR_DATA_TYPE_INTEGER
const PSR_DATA_TYPE_REAL = C.PSR_DATA_TYPE_REAL
const PSR_DATA_TYPE_TEXT = C.PSR_DATA_TYPE_TEXT

end
