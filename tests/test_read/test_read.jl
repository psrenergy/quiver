module TestRead

using PSRDatabase
using SQLite
using Dates
using DataFrames
using Test

function test_read_parameters()
    path_schema = joinpath(@__DIR__, "test_read.sql")
    db_path = joinpath(@__DIR__, "test_read.sqlite")
    db = PSRDatabase.create_empty_db_from_schema(db_path, path_schema; force = true)
    PSRDatabase.create_element!(
        db,
        "Configuration";
        label = "Toy Case",
        date_initial = DateTime(2020, 1, 1),
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
        date_some_date = [DateTime(2020, 1, 1)],
    )
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 2",
        capacity = 53.0,
        some_factor = [1.0, 2.0],
        date_some_date = [DateTime(2020, 1, 1), DateTime(2020, 1, 2)],
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
          [DateTime(2020, 1, 1)]
    @test PSRDatabase.read_scalar_parameters(db, "Resource", "label") ==
          ["Resource 1", "Resource 2"]
    @test PSRDatabase.read_scalar_parameter(db, "Resource", "label", "Resource 1") ==
          "Resource 1"
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
          [DateTime(2020, 1, 1), DateTime(2020, 1, 2)]
    @test PSRDatabase.read_vector_parameter(
        db,
        "Plant",
        "date_some_date",
        "Plant 3",
    ) ==
          DateTime[]
    @test PSRDatabase.read_vector_parameter(
        db,
        "Plant",
        "date_some_date",
        "Plant 4",
    ) ==
          DateTime[typemin(DateTime), typemin(DateTime)]
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

    PSRDatabase.update_scalar_parameter!(db, "Plant", "capacity", "Plant 1", 2.0)
    @test PSRDatabase.read_scalar_parameters(db, "Plant", "capacity") ==
          [2.0, 53.0, 54.0, 53.0]
    PSRDatabase.delete_element!(db, "Resource", "Resource 1")
    @test PSRDatabase.read_scalar_parameters(db, "Resource", "label") ==
          ["Resource 2"]

    PSRDatabase.close!(db)
    return rm(db_path)
end

function test_read_relations()
    path_schema = joinpath(@__DIR__, "test_read.sql")
    db_path = joinpath(@__DIR__, "test_read.sqlite")
    db = PSRDatabase.create_empty_db_from_schema(db_path, path_schema; force = true)
    PSRDatabase.create_element!(db, "Configuration"; label = "Toy Case")
    PSRDatabase.create_element!(
        db,
        "Resource";
        label = "Resource 1",
        some_value = [1, 2, 3.0],
    )
    PSRDatabase.create_element!(
        db,
        "Resource";
        label = "Resource 2",
        some_value = [1, 2, 4.0],
    )
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 1")
    PSRDatabase.create_element!(db, "Cost"; label = "Cost 2")
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 1",
        capacity = 2.02,
        some_factor = [1.0],
        some_other_factor = [0.5],
    )
    PSRDatabase.create_element!(
        db,
        "Plant";
        label = "Plant 2",
        capacity = 53.0,
        some_factor = [1.0, 2.0],
        some_other_factor = [0.7, 0.8],
    )
    PSRDatabase.create_element!(db, "Plant"; label = "Plant 3", capacity = 54.0)

    PSRDatabase.set_scalar_relation!(
        db,
        "Plant",
        "Resource",
        "Plant 1",
        "Resource 1",
        "id",
    )
    PSRDatabase.set_scalar_relation!(
        db,
        "Plant",
        "Plant",
        "Plant 3",
        "Plant 2",
        "turbine_to",
    )
    PSRDatabase.set_vector_relation!(db, "Plant", "Cost", "Plant 1", ["Cost 1"], "id")
    PSRDatabase.set_vector_relation!(
        db,
        "Plant",
        "Cost",
        "Plant 2",
        ["Cost 1", "Cost 2"],
        "id",
    )

    PSRDatabase.set_set_relation!(db, "Plant", "Cost", "Plant 1", ["Cost 1"], "rel")
    PSRDatabase.set_set_relation!(
        db,
        "Plant",
        "Cost",
        "Plant 2",
        ["Cost 1", "Cost 2"],
        "rel",
    )

    @test PSRDatabase.read_scalar_relations(db, "Plant", "Resource", "id") ==
          ["Resource 1", "", ""]
    @test PSRDatabase.read_scalar_relations(db, "Plant", "Plant", "turbine_to") ==
          ["", "", "Plant 2"]
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_relations(
        db,
        "Plant",
        "Cost",
        "id",
    )
    @test PSRDatabase.read_vector_relations(db, "Plant", "Cost", "id") ==
          [["Cost 1"], ["Cost 1", "Cost 2"], String[]]
    PSRDatabase.set_vector_relation!(db, "Plant", "Cost", "Plant 1", ["Cost 2"], "id")
    @test PSRDatabase.read_vector_relations(db, "Plant", "Cost", "id") ==
          [["Cost 2"], ["Cost 1", "Cost 2"], String[]]
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_vector_relations(
        db,
        "Plant",
        "Resource",
        "id",
    )
    @test PSRDatabase.read_vector_relation(db, "Plant", "Cost", "Plant 1", "id") ==
          ["Cost 2"]
    @test PSRDatabase.read_vector_relation(db, "Plant", "Cost", "Plant 2", "id") ==
          ["Cost 1", "Cost 2"]

    @test PSRDatabase.read_set_relations(db, "Plant", "Cost", "rel") ==
          [["Cost 1"], ["Cost 1", "Cost 2"], String[]]
    PSRDatabase.set_set_relation!(db, "Plant", "Cost", "Plant 1", ["Cost 2"], "rel")
    @test PSRDatabase.read_set_relations(db, "Plant", "Cost", "rel") ==
          [["Cost 2"], ["Cost 1", "Cost 2"], String[]]
    @test PSRDatabase.read_set_relation(db, "Plant", "Cost", "Plant 1", "rel") ==
          ["Cost 2"]
    @test PSRDatabase.read_set_relation(db, "Plant", "Cost", "Plant 2", "rel") ==
          ["Cost 1", "Cost 2"]

    PSRDatabase.close!(db)
    return rm(db_path)
end

function test_read_time_series_files()
    path_schema = joinpath(@__DIR__, "test_read.sql")
    db_path = joinpath(@__DIR__, "test_read.sqlite")
    db = PSRDatabase.create_empty_db_from_schema(db_path, path_schema; force = true)
    PSRDatabase.create_element!(db, "Configuration"; label = "Toy Case")
    PSRDatabase.create_element!(db, "Plant"; label = "Plant 1")

    PSRDatabase.set_time_series_file!(
        db,
        "Plant";
        wind_speed = "some_file.txt",
        wind_direction = "some_file2",
    )
    @test PSRDatabase.read_time_series_file(db, "Plant", "wind_speed") ==
          "some_file.txt"
    @test PSRDatabase.read_time_series_file(db, "Plant", "wind_direction") ==
          "some_file2"
    @test_throws PSRDatabase.DatabaseException PSRDatabase.read_time_series_file(
        db,
        "Plant",
        "spill",
    )
    PSRDatabase.set_time_series_file!(db, "Plant"; wind_speed = "some_file3.txt")
    @test PSRDatabase.read_time_series_file(db, "Plant", "wind_speed") ==
          "some_file3.txt"
    @test PSRDatabase.read_time_series_file(db, "Plant", "wind_direction") ==
          "some_file2"
    PSRDatabase.close!(db)
    return rm(db_path)
end

function runtests()
    Base.GC.gc()
    Base.GC.gc()
    for name in names(@__MODULE__; all = true)
        if startswith("$name", "test_")
            @testset "$(name)" begin
                getfield(@__MODULE__, name)()
            end
        end
    end
end

TestRead.runtests()

end
