module TestBlobMetadata

using Quiver
using Test

include("fixture.jl")

@testset "BlobMetadata" begin
    @testset "Builder constructor" begin
        md = Quiver.BlobMetadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            version = "1",
            labels = ["val1", "val2"],
            dimensions = ["row", "col"],
            dimension_sizes = Int64[3, 2],
        )

        @test Quiver.get_unit(md) == "MW"
        @test Quiver.get_version(md) == "1"
        @test Quiver.get_labels(md) == ["val1", "val2"]

        dims = Quiver.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "row"
        @test dims[1].size == 3
        @test dims[1].is_time_dimension == false
        @test dims[2].name == "col"
        @test dims[2].size == 2
    end

    @testset "Builder constructor with time dimensions" begin
        md = Quiver.BlobMetadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            version = "1",
            labels = ["plant_1", "plant_2", "plant_3"],
            dimensions = ["stage", "block"],
            dimension_sizes = Int64[4, 31],
            time_dimensions = ["stage", "block"],
            frequencies = ["monthly", "daily"],
        )

        @test Quiver.get_unit(md) == "MW"
        @test Quiver.get_labels(md) == ["plant_1", "plant_2", "plant_3"]
        @test Quiver.get_number_of_time_dimensions(md) == 2

        dims = Quiver.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "stage"
        @test dims[1].size == 4
        @test dims[1].is_time_dimension == true
        @test dims[1].frequency == "monthly"
        @test dims[2].name == "block"
        @test dims[2].size == 31
        @test dims[2].is_time_dimension == true
        @test dims[2].frequency == "daily"
    end

    @testset "Get initial datetime" begin
        md = Quiver.BlobMetadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            labels = ["val1"],
            dimensions = ["row"],
            dimension_sizes = Int64[3],
        )

        dt = Quiver.get_initial_datetime(md)
        @test occursin("2025-01-01", dt)
    end

    @testset "TOML round-trip" begin
        toml_input = """
            version = "1"
            dimensions = ["row", "col"]
            dimension_sizes = [3, 2]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val1", "val2"]
            """

        md = Quiver.from_toml(toml_input)
        @test Quiver.get_unit(md) == "MW"

        toml_str = Quiver.to_toml(md)
        @test occursin("MW", toml_str)

        md2 = Quiver.from_toml(toml_str)
        @test Quiver.get_unit(md2) == "MW"
        @test Quiver.get_version(md2) == "1"
        @test Quiver.get_labels(md2) == ["val1", "val2"]

        dims = Quiver.get_dimensions(md2)
        @test length(dims) == 2
        @test dims[1].name == "row"
        @test dims[1].size == 3
    end

    @testset "From Element" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["row", "col"]
        el["dimension_sizes"] = Int64[3, 2]
        el["labels"] = ["val1", "val2"]

        md = Quiver.from_element(el)
        @test Quiver.get_unit(md) == "MW"
        @test Quiver.get_version(md) == "1"

        dims = Quiver.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "row"
    end
end

end
