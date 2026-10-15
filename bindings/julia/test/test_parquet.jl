module TestParquet

using Quiver
using Test

@testset "Parquet export" begin
    mktempdir() do directory
        path = joinpath(directory, "snapshot")
        metadata = Quiver.Binary.Metadata(;
            initial_datetime = "2024-01-01T00:00:00",
            unit = "MW",
            dimensions = ["row"],
            dimension_sizes = Int64[2],
            labels = ["value"],
        )
        Quiver.Binary.open_file(path; mode = 'w', metadata) do file
            Quiver.Binary.write!(file; data = [1.5], row = 2)
        end
        @test isnothing(Quiver.Binary.bin_to_parquet(path))
        @test read(path * ".parquet", 4) == UInt8[0x50, 0x41, 0x52, 0x31]
        @test isfile(path * ".qvr") && isfile(path * ".toml")
        @test_throws Quiver.DatabaseException Quiver.Binary.bin_to_parquet(joinpath(directory, "missing"))
    end
end

end
