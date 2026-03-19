module TestBinaryComparator

using Quiver
using Test

include("fixture.jl")

function make_binary_path()
    return joinpath(tempdir(), "quiver_julia_comparator_test")
end

function cleanup_binary(path)
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

@testset "Binary Comparator" begin
    @testset "compare_files returns file_match for identical files" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            result = Quiver.Binary.compare_files(path1, path2)
            @test result.status == Quiver.QUIVER_COMPARE_FILE_MATCH
            @test result.report === nothing
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files detects data mismatch" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [99.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            result = Quiver.Binary.compare_files(path1, path2)
            @test result.status == Quiver.QUIVER_COMPARE_DATA_MISMATCH
            @test result.report === nothing
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files detects metadata mismatch" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md1 = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md1)
            Quiver.Binary.write!(b1; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            md2 = Quiver.Binary.Metadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "GWh",
                labels = ["val1", "val2"],
                dimensions = ["row", "col"],
                dimension_sizes = Int64[3, 2],
            )
            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md2)
            Quiver.Binary.write!(b2; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            result = Quiver.Binary.compare_files(path1, path2)
            @test result.status == Quiver.QUIVER_COMPARE_METADATA_MISMATCH
            @test result.report === nothing
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files with detailed report on match" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            result = Quiver.Binary.compare_files(path1, path2; detailed_report = true)
            @test result.status == Quiver.QUIVER_COMPARE_FILE_MATCH
            @test result.report !== nothing
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files with detailed report on data mismatch" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [99.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            result = Quiver.Binary.compare_files(path1, path2; detailed_report = true)
            @test result.status == Quiver.QUIVER_COMPARE_DATA_MISMATCH
            @test result.report !== nothing
            @test occursin("val1", result.report)
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files non-existent file" begin
        path1 = make_binary_path() * "_cmp1"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            @test_throws Quiver.DatabaseException Quiver.Binary.compare_files(path1, "nonexistent")
        finally
            cleanup_binary(path1)
        end
    end

    @testset "compare_files with absolute tolerance matches within threshold" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [10.0, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [10.05, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            result = Quiver.Binary.compare_files(path1, path2; absolute_tolerance = 0.1)
            @test result.status == Quiver.QUIVER_COMPARE_FILE_MATCH
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files with absolute tolerance fails outside threshold" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [10.0, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [10.05, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            result = Quiver.Binary.compare_files(path1, path2; absolute_tolerance = 0.01)
            @test result.status == Quiver.QUIVER_COMPARE_DATA_MISMATCH
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files with relative tolerance matches within threshold" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [100.0, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [100.05, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            # diff = 0.05, threshold = 0.0 + 0.001 * 100.05 = 0.10005
            result = Quiver.Binary.compare_files(path1, path2; absolute_tolerance = 0.0, relative_tolerance = 0.001)
            @test result.status == Quiver.QUIVER_COMPARE_FILE_MATCH
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end

    @testset "compare_files with relative tolerance fails outside threshold" begin
        path1 = make_binary_path() * "_cmp1"
        path2 = make_binary_path() * "_cmp2"
        try
            md = make_simple_metadata()
            b1 = Quiver.Binary.open_file(path1; mode = :write, metadata = md)
            Quiver.Binary.write!(b1; data = [100.0, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b1)

            b2 = Quiver.Binary.open_file(path2; mode = :write, metadata = md)
            Quiver.Binary.write!(b2; data = [100.2, 20.0], row = 1, col = 1)
            Quiver.Binary.close!(b2)

            # diff = 0.2, threshold = 0.0 + 0.001 * 100.2 = 0.1002
            result = Quiver.Binary.compare_files(path1, path2; absolute_tolerance = 0.0, relative_tolerance = 0.001)
            @test result.status == Quiver.QUIVER_COMPARE_DATA_MISMATCH
        finally
            cleanup_binary(path1)
            cleanup_binary(path2)
        end
    end
end

end
