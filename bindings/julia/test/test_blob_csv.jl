module TestCSVConverter

using Quiver
using Test

include("fixture.jl")

function make_binary_path()
    return joinpath(tempdir(), "quiver_julia_binary_csv_test")
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

function make_hourly_metadata()
    return Quiver.BinaryMetadata(;
        initial_datetime = "2025-01-01T00:00:00",
        unit = "MW",
        labels = ["val"],
        dimensions = ["day", "hour"],
        dimension_sizes = Int64[3, 24],
        time_dimensions = ["day", "hour"],
        frequencies = ["daily", "hourly"],
    )
end

function write_toml(path, md)
    toml_str = Quiver.to_toml(md)
    open(path * ".toml", "w") do f
        return write(f, toml_str)
    end
end

function write_csv(path, content)
    open(path * ".csv", "w") do f
        return write(f, content)
    end
end

function read_csv(path)
    return Base.read(path * ".csv", String)
end

function csv_lines(path)
    content = read_csv(path)
    return filter(!isempty, [strip(l) for l in split(content, '\n')])
end

@testset "Binary CSV" begin
    # ==========================================================================
    # bin_to_csv -- Header tests
    # ==========================================================================

    @testset "Non-time metadata header" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            lines = csv_lines(path)
            @test length(lines) >= 1
            @test lines[1] == "row,col,val1,val2"
        finally
            cleanup_binary(path)
        end
    end

    @testset "Aggregated no hourly header" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            lines = csv_lines(path)
            @test length(lines) >= 1
            @test lines[1] == "date,plant_1,plant_2"
        finally
            cleanup_binary(path)
        end
    end

    @testset "Aggregated with hourly header" begin
        path = make_binary_path()
        try
            md = make_hourly_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            lines = csv_lines(path)
            @test length(lines) >= 1
            @test lines[1] == "datetime,val"
        finally
            cleanup_binary(path)
        end
    end

    @testset "Non-aggregated header" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            lines = csv_lines(path)
            @test length(lines) >= 1
            @test lines[1] == "stage,block,plant_1,plant_2"
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # bin_to_csv -- Data correctness
    # ==========================================================================

    @testset "Creates CSV file" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            @test isfile(path * ".csv")
        finally
            cleanup_binary(path)
        end
    end

    @testset "Data values match" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.5, 2.5], row = 1, col = 1)
            Quiver.write!(binary; data = [3.5, 4.5], row = 1, col = 2)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            lines = csv_lines(path)
            @test length(lines) >= 3
            @test lines[2] == "1,1,1.5,2.5"
            @test lines[3] == "1,2,3.5,4.5"
        finally
            cleanup_binary(path)
        end
    end

    @testset "NaN values appear as null" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            lines = csv_lines(path)
            @test length(lines) >= 2
            @test occursin("null", lines[2])
        finally
            cleanup_binary(path)
        end
    end

    @testset "Float precision" begin
        path = make_binary_path()
        try
            md = Quiver.BinaryMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val"],
                dimensions = ["row"],
                dimension_sizes = Int64[1],
            )
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.23456789], row = 1)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            lines = csv_lines(path)
            @test length(lines) >= 2
            @test lines[2] == "1,1.23457"
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # bin_to_csv -- Row count
    # ==========================================================================

    @testset "Non-time row count equals product" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            lines = csv_lines(path)
            # 3 * 2 = 6 data rows + 1 header
            @test length(lines) == 7
        finally
            cleanup_binary(path)
        end
    end

    @testset "Time metadata row count less than product" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            lines = csv_lines(path)
            data_rows = length(lines) - 1
            # Must be less than 4 * 31 = 124 (variable month lengths)
            @test data_rows < 124
            # Jan(31) + Feb(28) + Mar(31) + Apr(30) = 120
            @test data_rows == 120
        finally
            cleanup_binary(path)
        end
    end

    @testset "Hourly metadata row count" begin
        path = make_binary_path()
        try
            md = make_hourly_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            lines = csv_lines(path)
            data_rows = length(lines) - 1
            # 3 days * 24 hours = 72
            @test data_rows == 72
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # csv_to_bin -- Happy paths
    # ==========================================================================

    @testset "csv_to_bin creates qvr file" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            csv = "row,col,val1,val2\n"
            csv *= "1,1,1.0,2.0\n1,2,3.0,4.0\n"
            csv *= "2,1,5.0,6.0\n2,2,7.0,8.0\n"
            csv *= "3,1,9.0,10.0\n3,2,11.0,12.0\n"
            write_csv(path, csv)
            Quiver.csv_to_bin(path)
            @test isfile(path * ".qvr")
        finally
            cleanup_binary(path)
        end
    end

    @testset "csv_to_bin non-time round-trip" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            csv = "row,col,val1,val2\n"
            csv *= "1,1,1.5,2.5\n1,2,3.5,4.5\n"
            csv *= "2,1,5.5,6.5\n2,2,7.5,8.5\n"
            csv *= "3,1,9.5,10.5\n3,2,11.5,12.5\n"
            write_csv(path, csv)
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v = Quiver.read(binary; row = 1, col = 1)
            @test v[1] ≈ 1.5
            @test v[2] ≈ 2.5
            v2 = Quiver.read(binary; row = 3, col = 2)
            @test v2[1] ≈ 11.5
            @test v2[2] ≈ 12.5
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "csv_to_bin aggregated datetime with hourly" begin
        path = make_binary_path()
        try
            md = make_hourly_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [42.0], day = 1, hour = 1)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v = Quiver.read(binary; day = 1, hour = 1)
            @test v[1] ≈ 42.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "csv_to_bin aggregated date without hourly" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [10.0, 20.0], stage = 1, block = 1)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v = Quiver.read(binary; stage = 1, block = 1)
            @test v[1] ≈ 10.0
            @test v[2] ≈ 20.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "csv_to_bin non-aggregated time dimensions" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [5.0, 6.0], stage = 1, block = 1)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v = Quiver.read(binary; stage = 1, block = 1)
            @test v[1] ≈ 5.0
            @test v[2] ≈ 6.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # csv_to_bin -- Error cases
    # ==========================================================================

    @testset "Header mismatch wrong column name" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,bad_name,val1,val2\n1,1,1.0,2.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Header too few columns" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col\n1,1\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Header too many columns" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2,extra\n1,1,1.0,2.0,3.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Dimension value mismatch" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2\n2,1,1.0,2.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Non-numeric data value" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2\n1,1,abc,2.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Empty data field" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2\n1,1,,2.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Empty CSV file" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Round-trip tests
    # ==========================================================================

    @testset "Bin to CSV to bin without hourly" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [100.0, 200.0], stage = 1, block = 1)
            Quiver.write!(binary; data = [300.0, 400.0], stage = 2, block = 15)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v1 = Quiver.read(binary; stage = 1, block = 1)
            @test v1[1] ≈ 100.0
            @test v1[2] ≈ 200.0
            v2 = Quiver.read(binary; stage = 2, block = 15)
            @test v2[1] ≈ 300.0
            @test v2[2] ≈ 400.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "CSV to bin to CSV without hourly" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], stage = 1, block = 1)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            csv1 = read_csv(path)

            rm(path * ".qvr")
            Quiver.csv_to_bin(path)
            rm(path * ".csv")
            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            csv2 = read_csv(path)

            @test csv1 == csv2
        finally
            cleanup_binary(path)
        end
    end

    @testset "Bin to CSV to bin with hourly" begin
        path = make_binary_path()
        try
            md = make_hourly_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [77.0], day = 1, hour = 12)
            Quiver.write!(binary; data = [88.0], day = 3, hour = 24)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)

            # Verify datetime strings include time component
            lines = csv_lines(path)
            @test length(lines) >= 2
            @test lines[1] == "datetime,val"
            has_time_component = any(occursin("T", l) for l in lines[2:end])
            @test has_time_component

            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            @test Quiver.read(binary; day = 1, hour = 12)[1] ≈ 77.0
            @test Quiver.read(binary; day = 3, hour = 24)[1] ≈ 88.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "CSV to bin to CSV with hourly" begin
        path = make_binary_path()
        try
            md = make_hourly_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0], day = 1, hour = 1)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            csv1 = read_csv(path)

            rm(path * ".qvr")
            Quiver.csv_to_bin(path)
            rm(path * ".csv")
            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            csv2 = read_csv(path)

            @test csv1 == csv2
        finally
            cleanup_binary(path)
        end
    end

    @testset "Round-trip with null values" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)

            lines = csv_lines(path)
            @test length(lines) >= 2
            @test occursin("null", lines[2])

            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v = Quiver.read(binary; row = 1, col = 1, allow_nulls = true)
            @test isnan(v[1])
            @test isnan(v[2])
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Aggregated and non-aggregated produce same binary" begin
        path = make_binary_path()
        try
            md = make_time_metadata()

            # Aggregated round-trip
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], stage = 1, block = 1)
            Quiver.write!(binary; data = [3.0, 4.0], stage = 1, block = 5)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v1a = Quiver.read(binary; stage = 1, block = 1)
            v1b = Quiver.read(binary; stage = 1, block = 5)
            Quiver.close!(binary)

            # Reset
            rm(path * ".qvr")
            rm(path * ".csv")

            # Non-aggregated round-trip
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], stage = 1, block = 1)
            Quiver.write!(binary; data = [3.0, 4.0], stage = 1, block = 5)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            v2a = Quiver.read(binary; stage = 1, block = 1)
            v2b = Quiver.read(binary; stage = 1, block = 5)
            Quiver.close!(binary)

            @test v1a[1] ≈ v2a[1]
            @test v1a[2] ≈ v2a[2]
            @test v1b[1] ≈ v2b[1]
            @test v1b[2] ≈ v2b[2]
        finally
            cleanup_binary(path)
        end
    end

    @testset "Round-trip mixed time and non-time" begin
        path = make_binary_path()
        try
            md = Quiver.BinaryMetadata(;
                initial_datetime = "2025-01-01T00:00:00",
                unit = "MW",
                labels = ["val"],
                dimensions = ["month", "scenario", "day"],
                dimension_sizes = Int64[2, 2, 31],
                time_dimensions = ["month", "day"],
                frequencies = ["monthly", "daily"],
            )

            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [99.0], month = 1, scenario = 1, day = 1)
            Quiver.write!(binary; data = [77.0], month = 2, scenario = 2, day = 15)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = true)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)

            binary = Quiver.open_file(path; mode = :read)
            @test Quiver.read(binary; month = 1, scenario = 1, day = 1)[1] ≈ 99.0
            @test Quiver.read(binary; month = 2, scenario = 2, day = 15)[1] ≈ 77.0
            Quiver.close!(binary)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Validate header
    # ==========================================================================

    @testset "Validate header correct" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            binary = Quiver.open_file(path; mode = :write, metadata = md)
            Quiver.write!(binary; data = [1.0, 2.0], row = 1, col = 1)
            Quiver.close!(binary)

            Quiver.bin_to_csv(path; aggregate_time_dimensions = false)
            rm(path * ".qvr")
            Quiver.csv_to_bin(path)
            @test isfile(path * ".qvr")
        finally
            cleanup_binary(path)
        end
    end

    @testset "Validate header wrong column" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,wrong,val1,val2\n1,1,1.0,2.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Validate header extra column" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2,extra\n1,1,1.0,2.0,3.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Validate header missing column" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1\n1,1,1.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    # ==========================================================================
    # Validate dimensions
    # ==========================================================================

    @testset "Validate dimensions correct" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            csv = "row,col,val1,val2\n"
            csv *= "1,1,1.0,2.0\n1,2,3.0,4.0\n"
            csv *= "2,1,5.0,6.0\n2,2,7.0,8.0\n"
            csv *= "3,1,9.0,10.0\n3,2,11.0,12.0\n"
            write_csv(path, csv)
            Quiver.csv_to_bin(path)
            @test isfile(path * ".qvr")
        finally
            cleanup_binary(path)
        end
    end

    @testset "Validate dimensions wrong value" begin
        path = make_binary_path()
        try
            md = make_simple_metadata()
            write_toml(path, md)
            write_csv(path, "row,col,val1,val2\n1,2,1.0,2.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end

    @testset "Validate dimensions wrong aggregated datetime" begin
        path = make_binary_path()
        try
            md = make_time_metadata()
            write_toml(path, md)
            write_csv(path, "date,plant_1,plant_2\n2025-02-01,1.0,2.0\n")
            @test_throws Quiver.DatabaseException Quiver.csv_to_bin(path)
        finally
            cleanup_binary(path)
        end
    end
end

end
