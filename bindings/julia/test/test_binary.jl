module TestBinary

using Quiver
using Test

include("fixture.jl")

function make_binary_path()
    return joinpath(tempdir(), "quiver_julia_binary_test")
end

function cleanup_binary(path)
    for ext in [".qvr", ".toml", ".csv"]
        f = path * ext
        isfile(f) && rm(f)
    end
end

function make_simple_metadata()
    return Quiver.BinaryMetadata(;
        initial_datetime = "2025-01-01T00:00:00",
        unit = "MW",
        labels = ["val1", "val2"],
        dimensions = ["row", "col"],
        dimension_sizes = Int64[3, 2],
    )
end

function make_time_metadata()
    return Quiver.BinaryMetadata(;
        initial_datetime = "2025-01-01T00:00:00",
        unit = "MW",
        labels = ["plant_1", "plant_2"],
        dimensions = ["stage", "block"],
        dimension_sizes = Int64[4, 31],
        time_dimensions = ["stage", "block"],
        frequencies = ["monthly", "daily"],
    )
end

@testset "Binary" begin
    # ==========================================================================
    # Open / Close
    # ==========================================================================

    @testset "Write mode creates files" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            @test isfile(path * ".qvr")
            @test isfile(path * ".toml")
        finally
            cleanup_binary(path)
        end
    end

    @testset "Read mode after write" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            Quiver.close!(binary)
            @test true
        finally
            cleanup_binary(path)
        end
    end

    @testset "Read mode on missing file" begin
        path = make_binary_path()
        cleanup_binary(path)
        @test_throws Quiver.DatabaseException Quiver.open_file(path; mode = :read)
    end

    @testset "Write mode without metadata" begin
        @test_throws Quiver.DatabaseException Quiver.open_file("test"; mode = :write)
    end

    @testset "Invalid mode" begin
        @test_throws ArgumentError Quiver.open_file("test"; mode = :invalid)
    end

    @testset "Read mode returns correct metadata" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            read_md = Quiver.get_metadata(binary)
            @test Quiver.get_unit(read_md) == "MW"
            @test Quiver.get_labels(read_md) == ["val1", "val2"]

            dims = Quiver.get_dimensions(read_md)
            @test length(dims) == 2
            @test dims[1].name == "row"
            @test dims[1].size == 3
            @test dims[2].name == "col"
            @test dims[2].size == 2

            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Get file path" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test Quiver.get_file_path(binary) == path
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Write mode initializes with NaN" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1, allow_nulls = true)
            @test length(data) == 2
            @test isnan(data[1])
            @test isnan(data[2])
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Write / Read
    # ==========================================================================

    @testset "Write and read single position" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1)
            @test length(data) == 2
            @test data[1] ≈ 1.0
            @test data[2] ≈ 2.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Write and read multiple positions" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.write!(binary; data = [3.0, 4.0], row = 2, col = 2)
            Quiver.write!(binary; data = [5.0, 6.0], row = 3, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            @test Quiver.read(binary; row = 1, col = 1)[1] ≈ 1.0
            @test Quiver.read(binary; row = 2, col = 2)[1] ≈ 3.0
            @test Quiver.read(binary; row = 3, col = 1)[1] ≈ 5.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Write and read all positions" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            counter = 0
            for r in 1:3, c in 1:2
                Quiver.write!(binary; data = [Float64(counter), Float64(counter + 1)], row = r, col = c)
                counter += 2
            end
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            counter = 0
            for r in 1:3, c in 1:2
                data = Quiver.read(binary; row = r, col = c)
                @test data[1] ≈ Float64(counter)
                @test data[2] ≈ Float64(counter + 1)
                counter += 2
            end
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Overwrite existing data" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.write!(binary; data = [99.0, 100.0], row = 1, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1)
            @test data[1] ≈ 99.0
            @test data[2] ≈ 100.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Data persists after reopen" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.write!(binary; data = [3.0, 4.0], row = 2, col = 2)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            @test Quiver.read(binary; row = 1, col = 1)[1] ≈ 1.0
            @test Quiver.read(binary; row = 2, col = 2)[1] ≈ 3.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Negative values" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [-1.5, -999.99], row = 1, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1)
            @test data[1] ≈ -1.5
            @test data[2] ≈ -999.99
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Zero values" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [0.0, 0.0], row = 1, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1)
            @test data[1] ≈ 0.0
            @test data[2] ≈ 0.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Large double values" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            big = 1e300
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [big, -big], row = 1, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1)
            @test data[1] ≈ big
            @test data[2] ≈ -big
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Null handling
    # ==========================================================================

    @testset "Read unwritten position throws when nulls not allowed" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            @test_throws Quiver.DatabaseException Quiver.read(binary; row = 1, col = 1)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Read unwritten position returns NaN when nulls allowed" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1, allow_nulls = true)
            @test isnan(data[1])
            @test isnan(data[2])
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Read written position succeeds with nulls not allowed" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1)
            @test data[1] ≈ 1.0
            @test data[2] ≈ 2.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Explicit NaN write" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [NaN, NaN], row = 1, col = 1)
            Quiver.close!(binary)

            binary = Quiver.open_file(path; mode = :read)
            data = Quiver.read(binary; row = 1, col = 1, allow_nulls = true)
            @test isnan(data[1])
            @test isnan(data[2])
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Dimension validation
    # ==========================================================================

    @testset "Wrong dimension count too few" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.read(binary; row = 1, allow_nulls = true)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Wrong dimension count too many" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.read(binary; row = 1, col = 1, z = 1, allow_nulls = true)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Missing dimension name" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.read(binary; row = 1, bad = 1, allow_nulls = true)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Value below one" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.read(binary; row = 0, col = 1, allow_nulls = true)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Value above max" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.read(binary; row = 4, col = 1, allow_nulls = true)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Boundary min valid" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            data = Quiver.read(binary; row = 1, col = 1, allow_nulls = true)
            @test length(data) == 2
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Boundary max valid" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            data = Quiver.read(binary; row = 3, col = 2, allow_nulls = true)
            @test length(data) == 2
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Data length validation
    # ==========================================================================

    @testset "Data too short" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.write!(binary; data = [1.0], row = 1, col = 1)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Data too long" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.write!(binary; data = [1.0, 2.0, 3.0], row = 1, col = 1)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Data exact match" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.close!(binary)
            @test true
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Time dimension validation
    # ==========================================================================

    @testset "Valid time dimension coordinates" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], stage = 1, block = 1)
            Quiver.close!(binary)
            @test true
        finally
            cleanup_binary(path)
        end
    end

    @testset "Invalid time dimension coordinates" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            # stage=2 (Feb), block=30: Feb doesn't have 30 days
            @test_throws Quiver.DatabaseException Quiver.write!(binary; data = [1.0, 2.0], stage = 2, block = 30)
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Single time dimension skips consistency check" begin
        path = make_binary_path()
        try
            md = Quiver.BinaryMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val"],
                dimensions = ["month", "scenario"],
                dimension_sizes = Int64[12, 3],
                time_dimensions = ["month"],
                frequencies = ["monthly"],
            )
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0], month = 12, scenario = 3)
            Quiver.close!(binary)
            @test true
        finally
            cleanup_binary(path)
        end
    end
end

end
