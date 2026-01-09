module TestCreate

using PSRDatabase
using Test

@testset "Create Parameters" begin
    path_schema = joinpath(@__DIR__, "test_create_parameters.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
        db,
        "Configuration";
        label = "Toy Case",
        value1 = "wrong",
    )
    PSRDatabase.create_element!(db, "Configuration"; label = "Toy Case", value1 = 1.0)
    PSRDatabase.create_element!(db, "Resource"; label = "Resource 2")
    PSRDatabase.create_element!(db, "Resource"; label = "Resource 1", type = "E")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
        db,
        "Resource";
        label = "Resource 4",
        type3 = "E",
    )
    PSRDatabase.close!(db)
end

@testset "Create Empty Parameters" begin
    path_schema = joinpath(@__DIR__, "test_create_parameters.sql")

    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)
    PSRDatabase.create_element!(db, "Configuration"; label = "Toy Case")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
        db,
        "Resource",
    )
    PSRDatabase.close!(db)
end

@testset "Create Parameters and Vectors" begin
    path_schema = joinpath(@__DIR__, "test_create_parameters_and_vectors.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)
    PSRDatabase.create_element!(db, "Configuration"; label = "Toy Case", value1 = 1.0)
    PSRDatabase.create_element!(
        db,
        "Resource";
        label = "Resource 1",
        type = "E",
        some_value = [1.0, 2.0, 3.0],
    )
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 1", value = 30.0)
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 2", value = 20.0)
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 1",
        capacity = 50.0,
        some_factor = [0.1, 0.3],
    )
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 2",
        capacity = 50.0,
        some_factor = [0.1, 0.3, 0.5],
    )
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 3",
        generation = "some_file.txt",
    )
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 4",
        capacity = 50.0,
        some_factor = Float64[],
    )
    PSRDatabase.create_element!(db, "Plant"; label = "Plant 3", resource_id = 1)
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
        db,
        "Resource";
        label = "Resource 1",
        type = "E",
        some_value = 1.0,
    )
    PSRDatabase.close!(db)

    return nothing
end

# @testset "Create Sets with Relations" begin
#     path_schema = joinpath(@__DIR__, "test_create_sets_with_relations.sql")
#     db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)
#     PSRDatabase.create_element!(
#         db,
#         "Configuration";
#         label = "Toy Case",
#         some_value = 1.0,
#     )
#     PSRDatabase.create_element!(db, "Product"; label = "Sugar", unit = "Kg")
#     PSRDatabase.create_element!(db, "Product"; label = "Sugarcane", unit = "ton")
#     PSRDatabase.create_element!(db, "Product"; label = "Molasse", unit = "ton")
#     PSRDatabase.create_element!(db, "Product"; label = "Bagasse", unit = "ton")
#     @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
#         db,
#         "Product",
#         label = "Bagasse 2",
#         unit = 30,
#     )
#     PSRDatabase.create_element!(db, "Process";
#         label = "Sugar Mill",
#         product_set = ["Sugarcane"],
#         factor = [1.0],
#     )

#     @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db,
#         "Process";
#         label = "Sugar Mill 2",
#         product_set = ["Sugar"],
#         factor = ["wrong"],
#     )

#     @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db,
#         "Process";
#         label = "Sugar Mill 3",
#         product_set = ["Some Sugar"],
#         factor = [1.0],
#     )

#     @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db,
#         "Process";
#         label = "Sugar Mill 3",
#         product_set = ["Sugear"],
#         factor = [],
#     )

#     PSRDatabase.close!(db)
# end

end
