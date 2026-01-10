module TestRead

using PSRDatabase
using Test

@testset "Read Parameters" begin
    path_schema = joinpath(@__DIR__, "test_read.sql")

    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(
        db,
        "Configuration";
        label = "Toy Case",
        date_initial = "2020-01-01 00:00:00",
    )
    PSRDatabase.create_element!(
        db,
        "Resource";
        label = "Resource 1",
        some_value = [1, 2, 3.0],
        some_other_value = [4, 5, 6.0],
    )
    PSRDatabase.create_element!(
        db,
        "Resource";
        label = "Resource 2",
        some_value = [1, 2, 4.0],
        some_other_value = [4, 5, 7.0],
    )
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 1")
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 2", value = 10.0)
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 1",
        capacity = 2.02,
        some_factor = [1.0],
        some_other_factor = [0.5],
        date_some_date = ["2020-01-01 00:00:00"],
    )
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 2",
        capacity = 53.0,
        some_factor = [1.0, 2.0],
        date_some_date = ["2020-01-01 00:00:00", "2020-01-02 00:00:00"],
    )
    PSRDatabase.create_element!(db, "Plant"; label = "Plant 3", capacity = 54.0)
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 4",
        capacity = 53.0,
        some_factor = [1.0, 2.0],
    )

    @test PSRDatabase.read_scalar_parameters(db, "Configuration", "label") ==
          ["Toy Case"]
    @test PSRDatabase.read_scalar_parameters(db, "Configuration", "date_initial") ==
          ["2020-01-01 00:00:00"]
    @test PSRDatabase.read_scalar_parameters(db, "Resource", "label") ==
          ["Resource 1", "Resource 2"]
    @test PSRDatabase.read_scalar_parameter(db, "Resource", "label", "Resource 1") ==
          "Resource 1"
    @test PSRDatabase.read_scalar_parameters(db, "Cost", "value") == [100.0, 10.0]
    @test any(
        isnothing,
        PSRDatabase.read_scalar_parameters(db, "Cost", "value_without_default"),
    )
    @test PSRDatabase.read_scalar_parameters(
        db,
        "Cost",
        "value_without_default";
        default = 2.0,
    ) == [2.0, 2.0]
    @test PSRDatabase.read_scalar_parameter(db, "Plant", "capacity", "Plant 3") ==
          54.0
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_parameter(
        db,
        "Plant",
        "capacity",
        "Plant 5",
    )
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_parameters(
        db,
        "Resource",
        "capacity",
    )
    @test PSRDatabase.read_scalar_parameters(db, "Plant", "label") ==
          ["Plant 1", "Plant 2", "Plant 3", "Plant 4"]
    @test PSRDatabase.read_scalar_parameters(db, "Plant", "capacity") ==
          [2.02, 53.0, 54.0, 53.0]
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_parameters(
        db,
        "Resource",
        "some_value",
    )
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_vector_parameters(
        db,
        "Plant",
        "capacity",
    )
    @test PSRDatabase.read_vector_parameters(db, "Resource", "some_value") ==
          [[1, 2, 3.0], [1, 2, 4.0]]
    @test PSRDatabase.read_vector_parameters(db, "Plant", "some_factor") ==
          [[1.0], [1.0, 2.0], Float64[], [1.0, 2.0]]
    @test PSRDatabase.read_vector_parameter(db, "Plant", "some_factor", "Plant 1") ==
          [1.0]
    @test PSRDatabase.read_vector_parameter(db, "Plant", "some_factor", "Plant 2") ==
          [1.0, 2.0]
    @test PSRDatabase.read_vector_parameter(db, "Plant", "some_factor", "Plant 3") ==
          Float64[]
    @test PSRDatabase.read_vector_parameter(
        db,
        "Plant",
        "date_some_date",
        "Plant 2",
    ) ==
          ["2020-01-01 00:00:00", "2020-01-02 00:00:00"]
    @test PSRDatabase.read_vector_parameter(
        db,
        "Plant",
        "date_some_date",
        "Plant 3",
    ) ==
          String[]
    # @test PSRDatabase.read_vector_parameter(
    #     db,
    #     "Plant",
    #     "date_some_date",
    #     "Plant 4",
    # ) ==
    #       DateTime[typemin(DateTime), typemin(DateTime)]
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_vector_parameter(
        db,
        "Plant",
        "some_factor",
        "Plant 500",
    )

    # It is a set not a vector
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_vector_parameters(
        db,
        "Resource",
        "some_other_value",
    )
    @test PSRDatabase.read_set_parameter(db, "Resource", "some_other_value", "Resource 1") ==
          [4, 5, 6.0]
    @test PSRDatabase.read_set_parameter(db, "Resource", "some_other_value", "Resource 2") ==
          [4, 5, 7.0]
    @test PSRDatabase.read_set_parameters(db, "Resource", "some_other_value") ==
          [[4, 5, 6.0], [4, 5, 7.0]]
    @test PSRDatabase.read_set_parameter(db, "Plant", "some_other_factor", "Plant 1") ==
          [0.5]
    @test PSRDatabase.read_set_parameters(db, "Plant", "some_other_factor") ==
          [[0.5], Float64[], Float64[], Float64[]]

    # PSRDatabase.update_scalar_parameter!(db, "Plant", "capacity", "Plant 1", 2.0)
    # @test PSRDatabase.read_scalar_parameters(db, "Plant", "capacity") ==
    #       [2.0, 53.0, 54.0, 53.0]
    # PSRDatabase.delete_element!(db, "Resource", "Resource 1")
    # @test PSRDatabase.read_scalar_parameters(db, "Resource", "label") ==
    #       ["Resource 2"]

    PSRDatabase.close!(db)
end

end
