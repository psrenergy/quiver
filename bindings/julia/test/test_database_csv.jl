module TestDatabaseCSV

using Quiver
using Test

include("fixture.jl")

@testset "CSV Export" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "csv_export.sql")

    @testset "Scalar export with defaults" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
                status = 1,
                price = 9.99,
                date_created = "2024-01-15T10:30:00",
                notes = "first",
            )
            Quiver.create_element!(db, "Items";
                label = "Item2",
                name = "Beta",
                status = 2,
                price = 19.5,
                date_created = "2024-02-20T08:00:00",
                notes = "second",
            )

            Quiver.export_csv(db, "Items", "", csv_path)
            content = read(csv_path, String)

            @test occursin("label,name,status,price,date_created,notes\n", content)
            @test occursin("Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n", content)
            @test occursin("Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n", content)
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Group export (vector)" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            id1 = Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
            )
            id2 = Quiver.create_element!(db, "Items";
                label = "Item2",
                name = "Beta",
            )
            Quiver.update_vector_floats!(db, "Items", "measurement", id1, [1.1, 2.2, 3.3])
            Quiver.update_vector_floats!(db, "Items", "measurement", id2, [4.4, 5.5])

            Quiver.export_csv(db, "Items", "measurements", csv_path)
            content = read(csv_path, String)

            @test occursin("sep=,\nid,vector_index,measurement\n", content)
            @test occursin("Item1,1,1.1\n", content)
            @test occursin("Item1,2,2.2\n", content)
            @test occursin("Item1,3,3.3\n", content)
            @test occursin("Item2,1,4.4\n", content)
            @test occursin("Item2,2,5.5\n", content)
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Enum labels" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
                status = 1,
            )
            Quiver.create_element!(db, "Items";
                label = "Item2",
                name = "Beta",
                status = 2,
            )

            Quiver.export_csv(db, "Items", "", csv_path;
                enum_labels = Dict("status" => Dict(1 => "Active", 2 => "Inactive")),
            )
            content = read(csv_path, String)

            @test occursin("Item1,Alpha,Active,", content)
            @test occursin("Item2,Beta,Inactive,", content)
            # Raw integers should NOT be present as status values
            @test !occursin("Item1,Alpha,1,", content)
            @test !occursin("Item2,Beta,2,", content)
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Date formatting" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
                status = 1,
                date_created = "2024-01-15T10:30:00",
            )

            Quiver.export_csv(db, "Items", "", csv_path;
                date_time_format = "%Y/%m/%d",
            )
            content = read(csv_path, String)

            @test occursin("2024/01/15", content)
            # Raw ISO format should NOT appear
            @test !occursin("2024-01-15T10:30:00", content)
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Combined options (enum_labels + date_time_format)" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
                status = 1,
                date_created = "2024-01-15T10:30:00",
            )
            Quiver.create_element!(db, "Items";
                label = "Item2",
                name = "Beta",
                status = 2,
                date_created = "2024-02-20T08:00:00",
            )

            Quiver.export_csv(db, "Items", "", csv_path;
                enum_labels = Dict("status" => Dict(1 => "Active", 2 => "Inactive")),
                date_time_format = "%Y/%m/%d",
            )
            content = read(csv_path, String)

            # Enum labels applied
            @test occursin("Item1,Alpha,Active,", content)
            @test occursin("Item2,Beta,Inactive,", content)
            # Date formatting applied
            @test occursin("2024/01/15", content)
            @test occursin("2024/02/20", content)
            # Raw values should NOT appear
            @test !occursin("Item1,Alpha,1,", content)
            @test !occursin("2024-01-15T10:30:00", content)
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end
end

@testset "CSV Import" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "csv_export.sql")

    @testset "Scalar round-trip" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
                status = 1,
                price = 9.99,
                date_created = "2024-01-15T10:30:00",
                notes = "first",
            )
            Quiver.create_element!(db, "Items";
                label = "Item2",
                name = "Beta",
                status = 2,
                price = 19.5,
                date_created = "2024-02-20T08:00:00",
                notes = "second",
            )

            Quiver.export_csv(db, "Items", "", csv_path)

            # Import into fresh DB
            db2 = Quiver.from_schema(":memory:", path_schema)
            try
                Quiver.import_csv(db2, "Items", "", csv_path)

                names = Quiver.read_scalar_strings(db2, "Items", "name")
                @test length(names) == 2
                @test names[1] == "Alpha"
                @test names[2] == "Beta"
            finally
                Quiver.close!(db2)
            end
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Vector group round-trip" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            id1 = Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
            )
            Quiver.update_vector_floats!(db, "Items", "measurement", id1, [1.1, 2.2, 3.3])

            Quiver.export_csv(db, "Items", "measurements", csv_path)

            # Clear and re-import
            Quiver.update_vector_floats!(db, "Items", "measurement", id1, Float64[])
            Quiver.import_csv(db, "Items", "measurements", csv_path)

            vals = Quiver.read_vector_floats_by_id(db, "Items", "measurement", id1)
            @test length(vals) == 3
            @test vals[1] ≈ 1.1 atol=0.001
            @test vals[2] ≈ 2.2 atol=0.001
            @test vals[3] ≈ 3.3 atol=0.001
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Scalar header-only clears table" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
            )

            # Write header-only CSV
            open(csv_path, "w") do f
                return write(f, "sep=,\nlabel,name,status,price,date_created,notes\n")
            end

            Quiver.import_csv(db, "Items", "", csv_path)

            names = Quiver.read_scalar_strings(db, "Items", "name")
            @test isempty(names)
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Enum resolution" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            open(csv_path, "w") do f
                return write(f, "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,Active,,,\n")
            end

            Quiver.import_csv(db, "Items", "", csv_path;
                enum_labels = Dict("status" => Dict("en" => Dict("Active" => 1, "Inactive" => 2))),
            )

            status = Quiver.read_scalar_integer_by_id(db, "Items", "status", 1)
            @test !isnothing(status)
            @test status == 1
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "DateTime format" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            open(csv_path, "w") do f
                return write(f, "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,2024/01/15,\n")
            end

            Quiver.import_csv(db, "Items", "", csv_path;
                date_time_format = "%Y/%m/%d",
            )

            date = Quiver.read_scalar_string_by_id(db, "Items", "date_created", 1)
            @test !isnothing(date)
            @test date == "2024-01-15T00:00:00"
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end
end

end # module TestDatabaseCSV
