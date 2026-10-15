module Binary

using ..Quiver: AbstractExpression, C, check, Element, Optional

include("metadata.jl")
include("file.jl")
include("csv_converter.jl")
include("parquet.jl")

end
