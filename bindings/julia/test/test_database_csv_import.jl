module TestDatabaseCSVImport

using Quiver
using Test

include("fixture.jl")

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

    @testset "Scalar label on third column" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            open(csv_path, "w") do f
                return write(
                    f,
                    "sep=,\nname,status,label,price,date_created,notes\nAlpha,1,Item1,10.5,,\nBeta,2,Item2,20.0,,\n",
                )
            end

            Quiver.import_csv(db, "Items", "", csv_path)

            labels = Quiver.read_scalar_strings(db, "Items", "label")
            @test length(labels) == 2
            @test labels[1] == "Item1"
            @test labels[2] == "Item2"

            names = Quiver.read_scalar_strings(db, "Items", "name")
            @test length(names) == 2
            @test names[1] == "Alpha"
            @test names[2] == "Beta"

            statuses = Quiver.read_scalar_integers(db, "Items", "status")
            @test length(statuses) == 2
            @test statuses[1] == 1
            @test statuses[2] == 2

            prices = Quiver.read_scalar_floats(db, "Items", "price")
            @test length(prices) == 2
            @test prices[1] ≈ 10.5 atol=0.001
            @test prices[2] ≈ 20.0 atol=0.001
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
            Quiver.update_element!(db, "Items", id1; measurement = [1.1, 2.2, 3.3])

            Quiver.export_csv(db, "Items", "measurements", csv_path)

            Quiver.update_element!(db, "Items", id1; measurement = Float64[])
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

    @testset "Semicolon sep= header round-trip" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            open(csv_path, "w") do f
                return write(f, "sep=;\nlabel;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n")
            end

            Quiver.import_csv(db, "Items", "", csv_path)

            names = Quiver.read_scalar_strings(db, "Items", "name")
            @test length(names) == 1
            @test names[1] == "Alpha"
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Semicolon auto-detect round-trip" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            open(csv_path, "w") do f
                return write(f, "label;name;status;price;date_created;notes\nItem1;Alpha;1;9.99;;\n")
            end

            Quiver.import_csv(db, "Items", "", csv_path)

            names = Quiver.read_scalar_strings(db, "Items", "name")
            @test length(names) == 1
            @test names[1] == "Alpha"
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Cannot open file throws" begin
        db = Quiver.from_schema(":memory:", path_schema)
        try
            @test_throws Exception Quiver.import_csv(db, "Items", "", "/nonexistent/path/file.csv")
        finally
            Quiver.close!(db)
        end
    end

    @testset "Group duplicate entries throws" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
            )

            open(csv_path, "w") do f
                return write(f, "sep=,\nid,tag\nItem1,red\nItem1,red\n")
            end

            @test_throws Exception Quiver.import_csv(db, "Items", "tags", csv_path)
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Scalar trailing empty columns" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            open(csv_path, "w") do f
                return write(f,
                    "sep=,\n" *
                    "label,name,status,price,date_created,notes,,,,\n" *
                    "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first,,,,\n" *
                    "Item2,Beta,2,19.5,2024-02-20T08:00:00,second,,,,\n")
            end

            Quiver.import_csv(db, "Items", "", csv_path)

            names = Quiver.read_scalar_strings(db, "Items", "name")
            @test length(names) == 2
            @test names[1] == "Alpha"
            @test names[2] == "Beta"
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Vector trailing empty columns" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
            )

            open(csv_path, "w") do f
                return write(f,
                    "sep=,\n" *
                    "id,vector_index,measurement,,,\n" *
                    "Item1,1,1.1,,,\n" *
                    "Item1,2,2.2,,,\n")
            end

            Quiver.import_csv(db, "Items", "measurements", csv_path)

            vals = Quiver.read_vector_floats_by_id(db, "Items", "measurement", 1)
            @test length(vals) == 2
            @test vals[1] ≈ 1.1 atol = 0.001
            @test vals[2] ≈ 2.2 atol = 0.001
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end

    @testset "Group invalid enum throws" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
            )

            open(csv_path, "w") do f
                return write(f, "sep=,\nid,date_time,temperature,humidity\nItem1,2024-01-01T10:00:00,22.5,Unknown\n")
            end

            @test_throws Exception Quiver.import_csv(db, "Items", "readings", csv_path;
                enum_labels = Dict("humidity" => Dict("en" => Dict("Low" => 60, "High" => 90))),
            )
        finally
            isfile(csv_path) && rm(csv_path)
            Quiver.close!(db)
        end
    end
end

end # module TestDatabaseCSVImport
