module TestDatabaseReadSet

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Read Set" begin
    @testset "Set Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result = Quiver.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2
        # Sets are unordered, so sort before comparison
        @test sort(result[1]) == ["important", "urgent"]
        @test result[2] == ["review"]

        Quiver.close!(db)
    end

    @testset "Set Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Quiver.read_set_strings(db, "Collection", "tag") == Vector{String}[]

        Quiver.close!(db)
    end

    @testset "Set Includes Elements With No Rows" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set data
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        # Create element without set data
        Quiver.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with set data
        Quiver.create_element!(db, "Collection"; label = "Item 3", tag = ["urgent", "review"])

        # One entry per element: the element with no rows is an empty vector, not a gap
        result = Quiver.read_set_strings(db, "Collection", "tag")
        @test length(result) == 3
        @test result[1] == ["important"]
        @test isempty(result[2])
        @test result[3] == ["urgent", "review"]

        Quiver.close!(db)
    end
    @testset "Set Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result1 = Quiver.read_set_strings_by_id(db, "Collection", "tag", 1)
        @test sort(result1) == ["important", "urgent"]
        @test Quiver.read_set_strings_by_id(db, "Collection", "tag", 2) == ["review"]

        Quiver.close!(db)
    end
    @testset "Set Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.read_set_strings(
            db,
            "NonexistentCollection",
            "tag",
        )

        Quiver.close!(db)
    end
    @testset "Read Set Integers Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", 1; code = [10, 20, 30])
        Quiver.update_element!(db, "AllTypes", 2; code = [40, 50])

        result = Quiver.read_set_integers(db, "AllTypes", "code")
        @test length(result) == 2
        @test sort(result[1]) == [10, 20, 30]
        @test sort(result[2]) == [40, 50]

        Quiver.close!(db)
    end

    @testset "Read Set Integers By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_element!(db, "AllTypes", 1; code = [10, 20, 30])

        result = Quiver.read_set_integers_by_id(db, "AllTypes", "code", 1)
        @test sort(result) == [10, 20, 30]

        Quiver.close!(db)
    end

    @testset "Read Set Floats Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", 1; weight = [1.1, 2.2])
        Quiver.update_element!(db, "AllTypes", 2; weight = [3.3, 4.4, 5.5])

        result = Quiver.read_set_floats(db, "AllTypes", "weight")
        @test length(result) == 2
        @test sort(result[1]) == [1.1, 2.2]
        @test sort(result[2]) == [3.3, 4.4, 5.5]

        Quiver.close!(db)
    end

    @testset "Read Set Floats By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_element!(db, "AllTypes", 1; weight = [1.1, 2.2])

        result = Quiver.read_set_floats_by_id(db, "AllTypes", "weight", 1)
        @test sort(result) == [1.1, 2.2]

        Quiver.close!(db)
    end

    @testset "Read Set DateTimes Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test Quiver.read_set_date_times(db, "AllTypes", "tag") == Vector{DateTime}[]

        Quiver.create_element!(db, "AllTypes";
            label = "Item 1",
            tag = ["2024-01-15T10:30:00", "2024-01-16"],
        )
        Quiver.create_element!(db, "AllTypes";
            label = "Item 2",
            tag = ["2024-06-20 14:45:30"],
        )
        Quiver.create_element!(db, "AllTypes"; label = "No set")

        result = Quiver.read_set_date_times(db, "AllTypes", "tag")
        @test length(result) == 3
        @test sort(result[1]) == [DateTime(2024, 1, 15, 10, 30, 0), DateTime(2024, 1, 16)]
        @test result[2] == [DateTime(2024, 6, 20, 14, 45, 30)]
        @test isempty(result[3])

        Quiver.close!(db)
    end

    # The core's DATE_TIME write gate only fires on `date_`-prefixed columns, so a plain TEXT
    # column is the reachable path for a value outside the grammar. Every binding's parser must
    # reject the same set, or the same stored bytes read back differently per language.
    @testset "Read Set DateTimes Rejects A Malformed Cell" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1", tag = ["2024-01-15", "20240115"])

        @test_throws ArgumentError Quiver.read_set_date_times(db, "AllTypes", "tag")
        @test_throws "AllTypes.tag" Quiver.read_set_date_times(db, "AllTypes", "tag")
        @test_throws ArgumentError Quiver.read_set_date_time_by_id(db, "AllTypes", "tag", 1)

        Quiver.close!(db)
    end

    @testset "Read Set Integers By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_set_integers_by_id(db, "AllTypes", "code", 1)
        @test isempty(result)

        Quiver.close!(db)
    end

    @testset "Read Set Floats By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_set_floats_by_id(db, "AllTypes", "weight", 1)
        @test isempty(result)

        Quiver.close!(db)
    end
    @testset "read_sets_by_id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "composite_helpers.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Items";
            label = "Item 1",
            code = [10, 20, 30],
            weight = [1.1, 2.2],
            tag = ["alpha", "beta"],
        )

        result = Quiver.read_sets_by_id(db, "Items", id)

        @test length(result) == 3
        @test haskey(result, "code")
        @test haskey(result, "weight")
        @test haskey(result, "tag")
        @test sort(result["code"]) == [10, 20, 30]
        @test sort(result["weight"]) == [1.1, 2.2]
        @test sort(result["tag"]) == ["alpha", "beta"]

        # Verify element types (Dict values are Vector{Any}, check individual elements)
        @test all(v -> v isa Int64, result["code"])
        @test all(v -> v isa Float64, result["weight"])
        @test all(v -> v isa String, result["tag"])

        Quiver.close!(db)
    end

    @testset "Set Group Columns Pair By Row" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_column_groups.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Items"; label = "Item 1")
        # Unsorted in both columns on purpose: a value-ordered reader would pair the wrong rows
        Quiver.update_set_group!(db, "Items", "codes", id;
            code = ["zeta", "alpha", "mu"],
            weight = [2.5, 3.5, 1.5],
        )

        codes = Quiver.read_set_strings_by_id(db, "Items", "code", id)
        weights = Quiver.read_set_floats_by_id(db, "Items", "weight", id)

        @test length(codes) == 3
        @test length(weights) == length(codes)
        @test sort(collect(zip(codes, weights))) == [("alpha", 3.5), ("mu", 1.5), ("zeta", 2.5)]

        Quiver.close!(db)
    end

    @testset "Set Group by ID Keeps NULL Cells In Place" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_column_groups.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Items"; label = "Item 1")
        Quiver.update_set_group!(db, "Items", "codes", id;
            code = ["alpha", nothing, "mu"],
            weight = [1.5, 2.5, nothing],
        )

        rows = Quiver.read_set_group_by_id(db, "Items", "codes", id)
        @test length(rows) == 3
        # A set's row order is unspecified: compare the (code, weight) pairs, not positions.
        @test Set((row["code"], row["weight"]) for row in rows) ==
              Set([("alpha", 1.5), (nothing, 2.5), ("mu", nothing)])

        Quiver.close!(db)
    end

    @testset "Set Group by ID Reads Its Own Table When Groups Share A Column" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "shared_group_columns.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        parent_a = Quiver.create_element!(db, "Parent"; label = "Parent A")
        parent_b = Quiver.create_element!(db, "Parent"; label = "Parent B")
        child = Quiver.create_element!(db, "Child"; label = "Child 1")

        # mentors and sponsors share parent_ref, and a per-column read of that name resolves to mentors.
        Quiver.update_set_group!(db, "Child", "mentors", child; parent_ref = [parent_a])
        Quiver.update_set_group!(db, "Child", "sponsors", child; parent_ref = [parent_b, parent_b], tier = [1, 2])

        rows = Quiver.read_set_group_by_id(db, "Child", "sponsors", child)
        @test length(rows) == 2
        @test Set((row["parent_ref"], row["tier"]) for row in rows) == Set([(parent_b, 1), (parent_b, 2)])

        Quiver.close!(db)
    end

    @testset "NULL cells and element types" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")
        Quiver.create_element!(db, "Collection"; label = "Item 2")  # no set rows
        # Julia's Element keeps a non-null array write surface, so the NULL cell is written
        # through the group writer.
        Quiver.update_set_group!(db, "Collection", "tags", id; tag = ["a", nothing, "c"])

        # A NULL cell keeps a slot; an element with no rows is an empty inner vector. Set order is
        # unspecified, so pin the agreement between the two readers and the content, not the order.
        sets = Quiver.read_set_strings(db, "Collection", "tag")
        by_id = Quiver.read_set_strings_by_id(db, "Collection", "tag", id)
        @test length(sets) == 2
        @test isempty(sets[2])
        @test isequal(sets[1], by_id)
        @test length(by_id) == 3
        @test Set(by_id) == Set(["a", nothing, "c"])

        # tag is nullable -> Optional element type.
        @test Quiver.read_set_strings(db, "Collection", "tag") isa
              Vector{Vector{Union{String, Nothing}}}
        @test Quiver.read_set_strings_by_id(db, "Collection", "tag", id) isa
              Vector{Union{String, Nothing}}

        # The DateTime wrapper keeps the NULL cell too.
        Quiver.update_set_group!(db, "Collection", "tags", id; tag = ["2024-01-01", nothing])
        @test Set(Quiver.read_set_date_times(db, "Collection", "tag")[1]) == Set([DateTime(2024, 1, 1), nothing])
        @test Set(Quiver.read_set_date_time_by_id(db, "Collection", "tag", id)) == Set([DateTime(2024, 1, 1), nothing])

        Quiver.close!(db)
    end

    @testset "Concrete element types for NOT NULL columns" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        id = Quiver.create_element!(db, "AllTypes"; label = "Item 1", code = [1, 0])

        # AllTypes_set_codes.code is INTEGER NOT NULL -> concrete element type.
        @test Quiver.read_set_integers(db, "AllTypes", "code") isa Vector{Vector{Int64}}
        @test Quiver.read_set_integers_by_id(db, "AllTypes", "code", id) isa Vector{Int64}
        @test Quiver.read_set_booleans(db, "AllTypes", "code") isa Vector{Vector{Bool}}

        Quiver.close!(db)
    end

    @testset "Errors name the reader" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # The nullability lookup runs after the read, so it never reports list_set_groups.
        exc = @test_throws Quiver.DatabaseException Quiver.read_set_strings(db, "Nope", "tag")
        @test exc.value.msg == "Cannot read_set_strings: collection not found: Nope"
        exc = @test_throws Quiver.DatabaseException Quiver.read_set_integers_by_id(db, "Nope", "tag", 1)
        @test exc.value.msg == "Cannot read_set_integers_by_id: collection not found: Nope"

        Quiver.close!(db)
    end
end

end
