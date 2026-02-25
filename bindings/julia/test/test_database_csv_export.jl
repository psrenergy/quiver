module TestDatabaseCSVExport

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
            Quiver.update_element!(db, "Items", id1; measurement = [1.1, 2.2, 3.3])
            Quiver.update_element!(db, "Items", id2; measurement = [4.4, 5.5])

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
                enum_labels = Dict("status" => Dict("en" => Dict("Active" => 1, "Inactive" => 2))),
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

    @testset "Invalid datetime returns raw value" begin
        db = Quiver.from_schema(":memory:", path_schema)
        csv_path = tempname() * ".csv"
        try
            Quiver.create_element!(db, "Items";
                label = "Item1",
                name = "Alpha",
                date_created = "not-a-date",
            )

            Quiver.export_csv(db, "Items", "", csv_path;
                date_time_format = "%Y/%m/%d",
            )
            content = read(csv_path, String)

            # Invalid datetime should be returned as-is
            @test occursin("not-a-date", content)
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
                enum_labels = Dict("status" => Dict("en" => Dict("Active" => 1, "Inactive" => 2))),
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

end # module TestDatabaseCSVExport
