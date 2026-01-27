module Quiver

using Dates

include("c_api.jl")
import .C

include("exceptions.jl")
include("date_time.jl")
include("element.jl")
include("database.jl")
include("database_create.jl")
include("database_metadata.jl")
include("database_read.jl")
include("database_update.jl")
include("database_delete.jl")
include("database_time_series.jl")
include("lua_runner.jl")

export Element, Database, LuaRunner, DatabaseException
export ScalarMetadata, VectorMetadata, SetMetadata, TimeSeriesMetadata
export QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_STRING

# Re-export C enum constants for data types
const QUIVER_DATA_TYPE_INTEGER = C.QUIVER_DATA_TYPE_INTEGER
const QUIVER_DATA_TYPE_FLOAT = C.QUIVER_DATA_TYPE_FLOAT
const QUIVER_DATA_TYPE_STRING = C.QUIVER_DATA_TYPE_STRING

end
