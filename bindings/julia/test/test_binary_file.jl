module TestBinaryFile

using Quiver
using Test

include("fixture.jl")

function make_binary_file_path()
    return joinpath(tempdir(), "quiver_julia_binary_test")
end

function cleanup_binary_file(path)
    for ext in [".qvr", ".toml", ".csv"]
        f = path * ext
        isfile(f) && rm(f)
    end
end

function make_simple_metadata()
    return Quiver.Binary.Metadata(;
        initial_datetime = "2025-01-01T00:00:00",
        unit = "MW",
        labels = ["val1", "val2"],
        dimensions = ["row", "col"],
        dimension_sizes = Int64[3, 2],
    )
end

function make_time_metadata()
    return Quiver.Binary.Metadata(;
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
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.close!(file)

            @test isfile(path * ".qvr")
            @test isfile(path * ".toml")
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Read mode after write" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            Quiver.Binary.close!(file)
            @test true
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Read mode on missing file" begin
        path = make_binary_file_path()
        cleanup_binary_file(path)
        @test_throws Quiver.DatabaseException Quiver.Binary.open(path; mode = :read)
    end

    @testset "Write mode without metadata" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.open("test"; mode = :write)
    end

    @testset "Invalid mode" begin
        @test_throws ArgumentError Quiver.Binary.open("test"; mode = :invalid)
    end

    @testset "Read mode returns correct metadata" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            read_md = Quiver.Binary.get_metadata(file)
            @test Quiver.Binary.get_unit(read_md) == "MW"
            @test Quiver.Binary.get_labels(read_md) == ["val1", "val2"]

            dims = Quiver.Binary.get_dimensions(read_md)
            @test length(dims) == 2
            @test dims[1].name == "row"
            @test dims[1].size == 3
            @test dims[2].name == "col"
            @test dims[2].size == 2

            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Get file path" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test Quiver.Binary.get_file_path(file) == path
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Write mode initializes with NaN" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1, allow_nulls = true)
            @test length(data) == 2
            @test isnan(data[1])
            @test isnan(data[2])
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    # ==========================================================================
    # Write / Read
    # ==========================================================================

    @testset "Write and read single position" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1)
            @test length(data) == 2
            @test data[1] ≈ 1.0
            @test data[2] ≈ 2.0
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Write and read multiple positions" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.write!(file; data = [3.0, 4.0], row = 2, col = 2)
            Quiver.Binary.write!(file; data = [5.0, 6.0], row = 3, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            @test Quiver.Binary.read(file; row = 1, col = 1)[1] ≈ 1.0
            @test Quiver.Binary.read(file; row = 2, col = 2)[1] ≈ 3.0
            @test Quiver.Binary.read(file; row = 3, col = 1)[1] ≈ 5.0
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Write and read all positions" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            counter = 0
            for r in 1:3, c in 1:2
                Quiver.Binary.write!(file; data = [Float64(counter), Float64(counter + 1)], row = r, col = c)
                counter += 2
            end
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            counter = 0
            for r in 1:3, c in 1:2
                data = Quiver.Binary.read(file; row = r, col = c)
                @test data[1] ≈ Float64(counter)
                @test data[2] ≈ Float64(counter + 1)
                counter += 2
            end
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Overwrite existing data" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.write!(file; data = [99.0, 100.0], row = 1, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1)
            @test data[1] ≈ 99.0
            @test data[2] ≈ 100.0
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Data persists after reopen" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.write!(file; data = [3.0, 4.0], row = 2, col = 2)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            @test Quiver.Binary.read(file; row = 1, col = 1)[1] ≈ 1.0
            @test Quiver.Binary.read(file; row = 2, col = 2)[1] ≈ 3.0
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Negative values" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [-1.5, -999.99], row = 1, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1)
            @test data[1] ≈ -1.5
            @test data[2] ≈ -999.99
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Zero values" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [0.0, 0.0], row = 1, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1)
            @test data[1] ≈ 0.0
            @test data[2] ≈ 0.0
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Large double values" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            big = 1e300
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [big, -big], row = 1, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1)
            @test data[1] ≈ big
            @test data[2] ≈ -big
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    # ==========================================================================
    # Null handling
    # ==========================================================================

    @testset "Read unwritten position throws when nulls not allowed" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            @test_throws Quiver.DatabaseException Quiver.Binary.read(file; row = 1, col = 1)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Read unwritten position returns NaN when nulls allowed" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1, allow_nulls = true)
            @test isnan(data[1])
            @test isnan(data[2])
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Read written position succeeds with nulls not allowed" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1)
            @test data[1] ≈ 1.0
            @test data[2] ≈ 2.0
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Explicit NaN write" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [NaN, NaN], row = 1, col = 1)
            Quiver.Binary.close!(file)

            file = Quiver.Binary.open(path; mode = :read)
            data = Quiver.Binary.read(file; row = 1, col = 1, allow_nulls = true)
            @test isnan(data[1])
            @test isnan(data[2])
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    # ==========================================================================
    # Dimension validation
    # ==========================================================================

    @testset "Wrong dimension count too few" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.read(file; row = 1, allow_nulls = true)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Wrong dimension count too many" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.read(
                file;
                row = 1,
                col = 1,
                z = 1,
                allow_nulls = true,
            )
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Missing dimension name" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.read(file; row = 1, bad = 1, allow_nulls = true)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Value below one" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.read(file; row = 0, col = 1, allow_nulls = true)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Value above max" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.read(file; row = 4, col = 1, allow_nulls = true)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Boundary min valid" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            data = Quiver.Binary.read(file; row = 1, col = 1, allow_nulls = true)
            @test length(data) == 2
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Boundary max valid" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            data = Quiver.Binary.read(file; row = 3, col = 2, allow_nulls = true)
            @test length(data) == 2
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    # ==========================================================================
    # Data length validation
    # ==========================================================================

    @testset "Data too short" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.write!(file; data = [1.0], row = 1, col = 1)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Data too long" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.write!(file; data = [1.0, 2.0, 3.0], row = 1, col = 1)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Data exact match" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(file)
            @test true
        finally
            cleanup_binary_file(path)
        end
    end

    # ==========================================================================
    # Write registry
    # ==========================================================================

    @testset "Writer blocks reader" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            writer = Quiver.Binary.open(path; mode = :write, metadata = md)
            @test_throws Quiver.DatabaseException Quiver.Binary.open(path; mode = :read)
            Quiver.Binary.close!(writer)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Closed writer allows reader" begin
        path = make_binary_file_path()
        try
            md = make_simple_metadata()
            writer = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.close!(writer)

            reader = Quiver.Binary.open(path; mode = :read)
            Quiver.Binary.close!(reader)
            @test true
        finally
            cleanup_binary_file(path)
        end
    end

    # ==========================================================================
    # Time dimension validation
    # ==========================================================================

    @testset "Valid time dimension coordinates" begin
        path = make_binary_file_path()
        try
            md = make_time_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0, 2.0], stage = 1, block = 1)
            Quiver.Binary.close!(file)
            @test true
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Invalid time dimension coordinates" begin
        path = make_binary_file_path()
        try
            md = make_time_metadata()
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            # stage=2 (Feb), block=30: Feb doesn't have 30 days
            @test_throws Quiver.DatabaseException Quiver.Binary.write!(file; data = [1.0, 2.0], stage = 2, block = 30)
            Quiver.Binary.close!(file)
        finally
            cleanup_binary_file(path)
        end
    end

    @testset "Single time dimension skips consistency check" begin
        path = make_binary_file_path()
        try
            md = Quiver.Binary.Metadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val"],
                dimensions = ["month", "scenario"],
                dimension_sizes = Int64[12, 3],
                time_dimensions = ["month"],
                frequencies = ["monthly"],
            )
            file = Quiver.Binary.open(path; mode = :write, metadata = md)
            Quiver.Binary.write!(file; data = [1.0], month = 12, scenario = 3)
            Quiver.Binary.close!(file)
            @test true
        finally
            cleanup_binary_file(path)
        end
    end
end

end
