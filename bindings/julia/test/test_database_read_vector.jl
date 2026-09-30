module TestDatabaseReadVector

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Read Vector" begin
    @testset "Vector Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(
            db,
            "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )
        Quiver.create_element!(
            db,
            "Collection";
            label = "Item 2",
            value_int = [10, 20],
            value_float = [10.5, 20.5],
        )

        @test Quiver.read_vector_integers(db, "Collection", "value_int") == [[1, 2, 3], [10, 20]]
        @test Quiver.read_vector_floats(db, "Collection", "value_float") == [[1.5, 2.5, 3.5], [10.5, 20.5]]

        Quiver.close!(db)
    end

    @testset "Vector Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Quiver.read_vector_integers(db, "Collection", "value_int") == Vector{Int64}[]
        @test Quiver.read_vector_floats(db, "Collection", "value_float") == Vector{Float64}[]

        Quiver.close!(db)
    end

    @testset "Vector Includes Elements With No Rows" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vectors
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        # Create element without vectors (no vector data inserted)
        Quiver.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with vectors
        Quiver.create_element!(db, "Collection"; label = "Item 3", value_int = [4, 5])

        # One entry per element: the element with no rows is an empty vector, not a gap
        result = Quiver.read_vector_integers(db, "Collection", "value_int")
        @test length(result) == 3
        @test result[1] == [1, 2, 3]
        @test isempty(result[2])
        @test result[3] == [4, 5]

        Quiver.close!(db)
    end
    @testset "Vector Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Quiver.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 1) == [1, 2, 3]
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 2) == [10, 20]

        Quiver.close!(db)
    end

    @testset "Vector Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        @test Quiver.read_vector_floats_by_id(db, "Collection", "value_float", 1) == [1.5, 2.5, 3.5]

        Quiver.close!(db)
    end
    @testset "Vector by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1")  # No vector data

        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 1) == Int64[]

        Quiver.close!(db)
    end
    @testset "Vector Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.read_vector_integers(
            db,
            "NonexistentCollection",
            "value_int",
        )
        @test_throws Quiver.DatabaseException Quiver.read_vector_floats(
            db,
            "NonexistentCollection",
            "value_float",
        )

        Quiver.close!(db)
    end
    @testset "Read Vector Strings Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", 1; label_value = ["alpha", "beta"])
        Quiver.update_element!(db, "AllTypes", 2; label_value = ["gamma"])

        result = Quiver.read_vector_strings(db, "AllTypes", "label_value")
        @test length(result) == 2
        @test result[1] == ["alpha", "beta"]
        @test result[2] == ["gamma"]

        Quiver.close!(db)
    end

    @testset "Read Vector DateTimes Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test Quiver.read_vector_date_times(db, "AllTypes", "label_value") == Vector{DateTime}[]

        Quiver.create_element!(db, "AllTypes";
            label = "Item 1",
            label_value = ["2024-01-15T10:30:00", "2024-01-16"],
        )
        Quiver.create_element!(db, "AllTypes";
            label = "Item 2",
            label_value = ["2024-06-20 14:45:30"],
        )
        Quiver.create_element!(db, "AllTypes"; label = "No vector")

        @test Quiver.read_vector_date_times(db, "AllTypes", "label_value") == [
            [DateTime(2024, 1, 15, 10, 30, 0), DateTime(2024, 1, 16)],
            [DateTime(2024, 6, 20, 14, 45, 30)],
            [],
        ]

        Quiver.close!(db)
    end

    # The core's DATE_TIME write gate only fires on `date_`-prefixed columns, so a plain TEXT
    # column is the reachable path for a value outside the grammar. Every binding's parser must
    # reject the same set, or the same stored bytes read back differently per language.
    @testset "Read Vector DateTimes Rejects A Malformed Cell" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1", label_value = ["2024-01-15", "2024-01"])

        @test_throws ArgumentError Quiver.read_vector_date_times(db, "AllTypes", "label_value")
        @test_throws "AllTypes.label_value" Quiver.read_vector_date_times(db, "AllTypes", "label_value")
        @test_throws ArgumentError Quiver.read_vector_date_times_by_id(db, "AllTypes", "label_value", 1)

        Quiver.close!(db)
    end

    @testset "Singular date-time by-id names are gone" begin
        @test !isdefined(Quiver, :read_vector_date_time_by_id)
        @test !isdefined(Quiver, :read_set_date_time_by_id)
    end

    @testset "Read Vector Strings By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_element!(db, "AllTypes", 1; label_value = ["alpha", "beta", "gamma"])

        result = Quiver.read_vector_strings_by_id(db, "AllTypes", "label_value", 1)
        @test result == ["alpha", "beta", "gamma"]

        Quiver.close!(db)
    end
    @testset "Read Vector Strings By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_vector_strings_by_id(db, "AllTypes", "label_value", 1)
        @test isempty(result)

        Quiver.close!(db)
    end
    @testset "Vector Group by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )

        rows = Quiver.read_vector_group_by_id(db, "Collection", "values", 1)
        @test length(rows) == 3
        @test rows[1]["value_int"] == 1
        @test rows[1]["value_float"] == 1.5
        @test rows[3]["value_int"] == 3
        @test rows[3]["value_float"] == 3.5

        Quiver.close!(db)
    end

    @testset "Vector Group by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        rows = Quiver.read_vector_group_by_id(db, "Collection", "values", 999)
        @test isempty(rows)

        Quiver.close!(db)
    end

    @testset "Vector Group by ID Keeps NULL Cells In Place" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_column_groups.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Items"; label = "Item 1")

        Quiver.update_vector_group!(db, "Items", "readings", id; amount = [1.5, 2.5], score = [nothing, 20.5])
        rows = Quiver.read_vector_group_by_id(db, "Items", "readings", id)
        @test length(rows) == 2
        @test rows[1]["amount"] == 1.5
        @test rows[1]["score"] === nothing
        @test rows[2]["amount"] == 2.5
        @test rows[2]["score"] == 20.5

        Quiver.update_vector_group!(db, "Items", "readings", id; amount = [nothing, 2.5], score = [10.5, 20.5])
        rows = Quiver.read_vector_group_by_id(db, "Items", "readings", id)
        @test length(rows) == 2
        @test rows[1]["amount"] === nothing
        @test rows[1]["score"] == 10.5
        @test rows[2]["amount"] == 2.5
        @test rows[2]["score"] == 20.5

        Quiver.close!(db)
    end

    @testset "Vector Group by ID Parses DateTime Columns" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_column_groups.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Items"; label = "Item 1")
        Quiver.update_vector_group!(db, "Items", "events", id;
            date_event = [DateTime(2024, 1, 15, 10, 30, 0), nothing, DateTime(2024, 3, 1)],
            note = [nothing, "second", "third"],
        )

        rows = Quiver.read_vector_group_by_id(db, "Items", "events", id)
        @test length(rows) == 3
        @test rows[1]["date_event"] == DateTime(2024, 1, 15, 10, 30, 0)
        @test rows[1]["note"] === nothing
        @test rows[2]["date_event"] === nothing
        @test rows[2]["note"] == "second"
        @test rows[3]["date_event"] == DateTime(2024, 3, 1)
        @test rows[3]["note"] == "third"

        Quiver.close!(db)
    end

    @testset "Vector Group by ID Reads Its Own Table When Groups Share A Column" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "shared_group_columns.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        parent_a = Quiver.create_element!(db, "Parent"; label = "Parent A")
        parent_b = Quiver.create_element!(db, "Parent"; label = "Parent B")
        child = Quiver.create_element!(db, "Child"; label = "Child 1")

        # links and routes share parent_ref, and a per-column read of that name resolves to links.
        Quiver.update_vector_group!(db, "Child", "links", child; parent_ref = [parent_a])
        Quiver.update_vector_group!(db, "Child", "routes", child; parent_ref = [parent_b, parent_b], cost = [1.5, 2.5])

        rows = Quiver.read_vector_group_by_id(db, "Child", "routes", child)
        @test [(row["parent_ref"], row["cost"]) for row in rows] == [(parent_b, 1.5), (parent_b, 2.5)]

        Quiver.close!(db)
    end

    @testset "read_vectors_by_id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "composite_helpers.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Items";
            label = "Item 1",
            amount = [10, 20, 30],
            score = [1.1, 2.2],
            note = ["hello", "world"],
        )

        result = Quiver.read_vectors_by_id(db, "Items", id)

        @test length(result) == 3
        @test haskey(result, "amount")
        @test haskey(result, "score")
        @test haskey(result, "note")
        @test result["amount"] == [10, 20, 30]
        @test result["score"] == [1.1, 2.2]
        @test result["note"] == ["hello", "world"]

        # Verify element types (Dict values are Vector{Any}, check individual elements)
        @test all(v -> v isa Int64, result["amount"])
        @test all(v -> v isa Float64, result["score"])
        @test all(v -> v isa String, result["note"])

        Quiver.close!(db)
    end

    @testset "NULL cells and element types" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")
        Quiver.create_element!(db, "Collection"; label = "Item 2")  # no vector rows
        # Julia's Element keeps a non-null array write surface, so the NULL cell is written
        # through the group writer.
        Quiver.update_vector_group!(db, "Collection", "values", id; value_int = [10, nothing, 30])

        # A NULL cell keeps its slot; an element with no rows is an empty inner vector. Those two
        # are different things, which is what the presence column in the core's LEFT JOIN buys.
        @test Quiver.read_vector_integers(db, "Collection", "value_int") == [[10, nothing, 30], []]
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", id) == [10, nothing, 30]

        # value_int is nullable -> Optional element type; label is NOT NULL -> concrete.
        @test Quiver.read_vector_integers(db, "Collection", "value_int") isa
              Vector{Vector{Union{Int64, Nothing}}}
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", id) isa
              Vector{Union{Int64, Nothing}}

        Quiver.close!(db)
    end

    @testset "Nullable booleans keep NULL cells" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")
        Quiver.update_vector_group!(db, "Collection", "values", id; value_int = [1, nothing, 0])

        @test Quiver.read_vector_booleans(db, "Collection", "value_int") == [[true, nothing, false]]
        @test Quiver.read_vector_booleans(db, "Collection", "value_int") isa Vector{Vector{Union{Bool, Nothing}}}
        @test Quiver.read_vector_booleans_by_id(db, "Collection", "value_int", id) == [true, nothing, false]
        @test Quiver.read_vector_booleans_by_id(db, "Collection", "value_int", id) isa Vector{Union{Bool, Nothing}}

        Quiver.close!(db)
    end

    @testset "Concrete element types for NOT NULL columns" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        id = Quiver.create_element!(db, "AllTypes"; label = "Item 1", count_value = [1, 0])

        # AllTypes_vector_counts.count_value is INTEGER NOT NULL -> concrete element type.
        @test Quiver.read_vector_integers(db, "AllTypes", "count_value") isa Vector{Vector{Int64}}
        @test Quiver.read_vector_integers_by_id(db, "AllTypes", "count_value", id) isa Vector{Int64}
        @test Quiver.read_vector_booleans(db, "AllTypes", "count_value") isa Vector{Vector{Bool}}

        Quiver.close!(db)
    end

    @testset "Errors name the reader" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # The nullability lookup runs after the read, so it never reports list_vector_groups.
        exc = @test_throws Quiver.DatabaseException Quiver.read_vector_integers(db, "Nope", "value_int")
        @test exc.value.msg == "Cannot read_vector_integers: collection not found: Nope"
        exc = @test_throws Quiver.DatabaseException Quiver.read_vector_strings_by_id(db, "Nope", "value_int", 1)
        @test exc.value.msg == "Cannot read_vector_strings_by_id: collection not found: Nope"

        Quiver.close!(db)
    end

    @testset "Nullable read round-trips into create_element!" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        # value_int is nullable, so the read is Vector{Union{Nothing, Int64}} even without a NULL.
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", id)
        copy_id = Quiver.create_element!(db, "Collection"; label = "Item 2", value_int = values)
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", copy_id) == [1, 2, 3]

        # A real NULL cell is refused: NULL cells are written through the group writers.
        Quiver.update_vector_group!(db, "Collection", "values", id; value_int = [1, nothing])
        with_null = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", id)
        @test_throws ArgumentError Quiver.update_element!(db, "Collection", copy_id; value_int = with_null)
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", copy_id) == [1, 2, 3]

        # The boolean wrapper's nullable read (Vector{Union{Nothing, Bool}}) round-trips the same way.
        flags_id = Quiver.create_element!(db, "Collection"; label = "Item 3", value_int = [1, 0])
        flags = Quiver.read_vector_booleans_by_id(db, "Collection", "value_int", flags_id)
        flags_copy = Quiver.create_element!(db, "Collection"; label = "Item 4", value_int = flags)
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", flags_copy) == [1, 0]
        Quiver.update_vector_group!(db, "Collection", "values", flags_id; value_int = [1, nothing])
        flags_with_null = Quiver.read_vector_booleans_by_id(db, "Collection", "value_int", flags_id)
        @test_throws ArgumentError Quiver.update_element!(db, "Collection", flags_copy; value_int = flags_with_null)

        Quiver.close!(db)
    end
end

end
