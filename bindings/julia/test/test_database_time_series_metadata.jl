module TestDatabaseTimeSeriesMetadata

using Quiver
using Test
using Dates

include("fixture.jl")

@testset "Time Series Metadata" begin
    @testset "Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # get_time_series_metadata
        metadata = Quiver.get_time_series_metadata(db, "Collection", "data")
        @test metadata.group_name == "data"
        @test length(metadata.value_columns) == 1
        @test metadata.value_columns[1].name == "value"

        # list_time_series_groups
        groups = Quiver.list_time_series_groups(db, "Collection")
        @test length(groups) == 1
        @test groups[1].group_name == "data"

        Quiver.close!(db)
    end

    @testset "Metadata Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Configuration has no time series tables
        groups = Quiver.list_time_series_groups(db, "Configuration")
        @test isempty(groups)

        Quiver.close!(db)
    end
end

end  # module
