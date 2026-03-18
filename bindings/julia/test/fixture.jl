function tests_path()
    local_schemas = joinpath(@__DIR__, "schemas")
    if haskey(ENV, "QUIVER_JULIA_USE_LOCAL_SHARED_LIBRARY")
        return @__DIR__
    else
        return joinpath(@__DIR__, "..", "..", "..", "tests")
    end
end
