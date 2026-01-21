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
const DECK_DATABASE_DATA_STRUCTURE_SCALAR = C.DECK_DATABASE_DATA_STRUCTURE_SCALAR
const DECK_DATABASE_DATA_STRUCTURE_VECTOR = C.DECK_DATABASE_DATA_STRUCTURE_VECTOR
const DECK_DATABASE_DATA_STRUCTURE_SET = C.DECK_DATABASE_DATA_STRUCTURE_SET
const DECK_DATABASE_DATA_TYPE_INTEGER = C.DECK_DATABASE_DATA_TYPE_INTEGER
const DECK_DATABASE_DATA_TYPE_FLOAT = C.DECK_DATABASE_DATA_TYPE_FLOAT
const DECK_DATABASE_DATA_TYPE_STRING = C.DECK_DATABASE_DATA_TYPE_STRING

end
