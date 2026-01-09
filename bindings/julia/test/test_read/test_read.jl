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
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 1")
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 2", value = 10.0)
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 1",
        capacity = 2.02,
    )
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 2",
        capacity = 53.0,
    )
    PSRDatabase.create_element!(db, "Plant"; label = "Plant 3", capacity = 54.0)
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 4",
        capacity = 53.0,
    )

    @test PSRDatabase.read_scalar_parameters(db, "Configuration", "label") ==
          ["Toy Case"]
    @test PSRDatabase.read_scalar_parameters(db, "Configuration", "date_initial") ==
          ["2020-01-01 00:00:00"]
    @test PSRDatabase.read_scalar_parameters(db, "Cost", "value") == [100.0, 10.0]
    @test any(
        isnan,
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

    PSRDatabase.close!(db)
end

end
