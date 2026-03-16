function tests_path()
    local_schemas = joinpath(@__DIR__, "schemas")
    if isdir(local_schemas)
        return @__DIR__
    else
        return joinpath(@__DIR__, "..", "..", "..", "tests")
    end
end
