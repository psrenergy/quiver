module TestBlob

using Quiver
using Test

include("fixture.jl")

function make_blob_path()
    return joinpath(tempdir(), "quiver_julia_blob_test")
end

function cleanup_blob(path)
    for ext in [".qvr", ".toml", ".csv"]
        f = path * ext
        isfile(f) && rm(f)
    end
end

@testset "Blob" begin
    @testset "Open write and close" begin
        path = make_blob_path()
        try
            md = Quiver.BlobMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val1", "val2"],
                dimensions = ["row", "col"],
                dimension_sizes = Int64[3, 2],
            )

            blob = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(blob)

            @test isfile(path * ".qvr")
            @test isfile(path * ".toml")
        finally
            cleanup_blob(path)
        end
    end

    @testset "Write and read round-trip" begin
        path = make_blob_path()
        try
            md = Quiver.BlobMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val1", "val2"],
                dimensions = ["row", "col"],
                dimension_sizes = Int64[3, 2],
            )

            blob = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(blob; data = [1.5, 2.5], row = 1, col = 1)
            Quiver.close!(blob)

            blob = Quiver.open_file(path; mode = :read)
            data = Quiver.read(blob; row = 1, col = 1)
            @test length(data) == 2
            @test data[1] ≈ 1.5
            @test data[2] ≈ 2.5
            Quiver.close!(blob)
        finally
            cleanup_blob(path)
        end
    end

    @testset "Get metadata" begin
        path = make_blob_path()
        try
            md = Quiver.BlobMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val1", "val2"],
                dimensions = ["row", "col"],
                dimension_sizes = Int64[3, 2],
            )

            blob = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(blob)

            blob = Quiver.open_file(path; mode = :read)
            read_md = Quiver.get_metadata(blob)
            @test Quiver.get_unit(read_md) == "MW"

            dims = Quiver.get_dimensions(read_md)
            @test length(dims) == 2

            Quiver.close!(blob)
        finally
            cleanup_blob(path)
        end
    end

    @testset "Get file path" begin
        path = make_blob_path()
        try
            md = Quiver.BlobMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val1", "val2"],
                dimensions = ["row", "col"],
                dimension_sizes = Int64[3, 2],
            )

            blob = Quiver.open_file(path; mode = :write, metadata = md)
            @test Quiver.get_file_path(blob) == path
            Quiver.close!(blob)
        finally
            cleanup_blob(path)
        end
    end

    @testset "Allow nulls" begin
        path = make_blob_path()
        try
            md = Quiver.BlobMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val1", "val2"],
                dimensions = ["row", "col"],
                dimension_sizes = Int64[3, 2],
            )

            blob = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(blob)

            blob = Quiver.open_file(path; mode = :read)

            # Without allow_nulls should fail
            @test_throws Quiver.DatabaseException Quiver.read(blob; row = 1, col = 1)

            # With allow_nulls should succeed
            data = Quiver.read(blob; row = 1, col = 1, allow_nulls = true)
            @test length(data) == 2
            @test isnan(data[1])
            @test isnan(data[2])

            Quiver.close!(blob)
        finally
            cleanup_blob(path)
        end
    end

    @testset "Invalid mode" begin
        @test_throws ArgumentError Quiver.open_file("test"; mode = :invalid)
    end

    @testset "Write without metadata" begin
        @test_throws ArgumentError Quiver.open_file("test"; mode = :write)
    end
end

end
