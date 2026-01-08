module TestCreate

using PSRDatabase
using Dates
using Test

@testset "Create Empty Parameters" begin
    path_schema = joinpath(@__DIR__, "test_create_parameters.sql")
    db_path = joinpath(@__DIR__, "test_create_empty_parameters.sqlite")

    if isfile(db_path)
        rm(db_path)
    end

    db = PSRDatabase.create_empty_db_from_schema(db_path, path_schema; force = true)
    PSRDatabase.create_element!(db, "Configuration"; label = "Toy Case")
    # @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
    #     db,
    #     "Resource",
    # )
    PSRDatabase.create_element!(
        db,
        "Resource",
    )    
    PSRDatabase.close!(db)
    rm(db_path)

    return nothing
end


end
