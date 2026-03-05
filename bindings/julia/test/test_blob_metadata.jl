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
            dimensions = ["row" => Int64(3), "col" => Int64(2)],
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

        Quiver.destroy!(md)
    end

    @testset "Get initial datetime" begin
        md = Quiver.BlobMetadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            dimensions = ["row" => Int64(3)],
        )

        dt = Quiver.get_initial_datetime(md)
        @test occursin("2025-01-01", dt)

        Quiver.destroy!(md)
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

        md = Quiver.blob_metadata_from_toml(toml_input)
        @test Quiver.get_unit(md) == "MW"

        toml_str = Quiver.to_toml(md)
        @test occursin("MW", toml_str)

        md2 = Quiver.blob_metadata_from_toml(toml_str)
        @test Quiver.get_unit(md2) == "MW"
        @test Quiver.get_version(md2) == "1"
        @test Quiver.get_labels(md2) == ["val1", "val2"]

        dims = Quiver.get_dimensions(md2)
        @test length(dims) == 2
        @test dims[1].name == "row"
        @test dims[1].size == 3

        Quiver.destroy!(md)
        Quiver.destroy!(md2)
    end

    @testset "From Element" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["row", "col"]
        el["dimension_sizes"] = Int64[3, 2]
        el["labels"] = ["val1", "val2"]

        md = Quiver.blob_metadata_from_element(el)
        @test Quiver.get_unit(md) == "MW"
        @test Quiver.get_version(md) == "1"

        dims = Quiver.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "row"

        Quiver.destroy!(md)
        Quiver.destroy!(el)
    end
end

end
