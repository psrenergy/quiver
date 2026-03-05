module TestBlobCSV

using Quiver
using Test

include("fixture.jl")

function make_blob_path()
    return joinpath(tempdir(), "quiver_julia_blob_csv_test")
end

function cleanup_blob(path)
    for ext in [".qvr", ".toml", ".csv"]
        f = path * ext
        isfile(f) && rm(f)
    end
end

@testset "Blob CSV" begin
    @testset "bin_to_csv creates file" begin
        path = make_blob_path()
        try
            md = Quiver.BlobMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val1", "val2"],
                dimensions = ["row" => Int64(3), "col" => Int64(2)],
            )

            blob = Quiver.open_blob(path, :write; metadata = md)
            Quiver.close!(blob)
            Quiver.destroy!(md)

            Quiver.blob_bin_to_csv(path; aggregate_time_dimensions = true)
            @test isfile(path * ".csv")
        finally
            cleanup_blob(path)
        end
    end

    @testset "Round-trip" begin
        path = make_blob_path()
        try
            md = Quiver.BlobMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val1", "val2"],
                dimensions = ["row" => Int64(3), "col" => Int64(2)],
            )

            blob = Quiver.open_blob(path, :write; metadata = md)
            Quiver.blob_write!(blob, Dict("row" => Int64(1), "col" => Int64(1)), [42.5, 99.5])
            Quiver.close!(blob)
            Quiver.destroy!(md)

            Quiver.blob_bin_to_csv(path; aggregate_time_dimensions = false)
            rm(path * ".qvr")

            Quiver.blob_csv_to_bin(path)
            @test isfile(path * ".qvr")

            blob = Quiver.open_blob(path, :read)
            data = Quiver.blob_read(blob, Dict("row" => Int64(1), "col" => Int64(1)))
            @test length(data) == 2
            @test data[1] ≈ 42.5
            @test data[2] ≈ 99.5
            Quiver.close!(blob)
        finally
            cleanup_blob(path)
        end
    end
end

end
