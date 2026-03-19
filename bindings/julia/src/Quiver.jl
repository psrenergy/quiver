module Quiver

using Dates

include("c_api.jl")
import .C

include("exceptions.jl")
include("date_time.jl")
include("element.jl")
include("database.jl")
include("database_create.jl")
include("database_options.jl")
include("database_csv_export.jl")
include("database_csv_import.jl")
include("database_metadata.jl")
include("database_query.jl")
include("database_read.jl")
include("database_update.jl")
include("database_delete.jl")
include("database_transaction.jl")
include("helper_maps.jl")
include("lua_runner.jl")
include("binary/Binary.jl")

export Element, Database, LuaRunner, DatabaseException
export ScalarMetadata, GroupMetadata
export QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_FLOAT, QUIVER_DATA_TYPE_STRING
export QUIVER_COMPARE_FILE_MATCH, QUIVER_COMPARE_METADATA_MISMATCH, QUIVER_COMPARE_DATA_MISMATCH

# Re-export C enum constants for data types
const QUIVER_DATA_TYPE_INTEGER = C.QUIVER_DATA_TYPE_INTEGER
const QUIVER_DATA_TYPE_FLOAT = C.QUIVER_DATA_TYPE_FLOAT
const QUIVER_DATA_TYPE_STRING = C.QUIVER_DATA_TYPE_STRING

# Re-export C enum constants for compare status
const QUIVER_COMPARE_FILE_MATCH = C.QUIVER_COMPARE_FILE_MATCH
const QUIVER_COMPARE_METADATA_MISMATCH = C.QUIVER_COMPARE_METADATA_MISMATCH
const QUIVER_COMPARE_DATA_MISMATCH = C.QUIVER_COMPARE_DATA_MISMATCH

end
