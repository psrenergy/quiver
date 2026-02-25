module TestHelperMaps

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Helper Maps" begin
    @testset "scalar_relation_map" begin
        @testset "Basic Mapping" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2
            Quiver.create_element!(db, "Parent"; label = "Parent 3")  # id=3

            # Create children with parent relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = 1)  # relates to Parent 1
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_id = 2)  # relates to Parent 2
            Quiver.create_element!(db, "Child"; label = "Child 3")                  # no relation

            # Get mapping: Child -> Parent via "id" relation type
            result = Quiver.scalar_relation_map(db, "Child", "Parent", "id")

            # Child 1 -> Parent 1 (position 1), Child 2 -> Parent 2 (position 2), Child 3 -> no relation (-1)
            @test result == [1, 2, -1]

            Quiver.close!(db)
        end

        @testset "After Deleting Parents" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2
            Quiver.create_element!(db, "Parent"; label = "Parent 3")  # id=3
            Quiver.create_element!(db, "Parent"; label = "Parent 4")  # id=4

            # Create children with parent relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = 1)  # relates to Parent 1
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_id = 2)  # relates to Parent 2
            Quiver.create_element!(db, "Child"; label = "Child 3", parent_id = 4)  # relates to Parent 4
            Quiver.create_element!(db, "Child"; label = "Child 4")                  # no relation

            # Delete Parent 2 (FK action SET NULL will clear Child 2's parent_id)
            Quiver.delete_element!(db, "Parent", Int64(2))

            # Now Parent IDs are [1, 3, 4] at positions [1, 2, 3]
            # Child 1 still points to Parent id=1 (position 1)
            # Child 2's parent_id was SET NULL (no relation, -1)
            # Child 3 still points to Parent id=4 (position 3)
            # Child 4 has no relation (-1)

            result = Quiver.scalar_relation_map(db, "Child", "Parent", "id")
            @test result == [1, -1, 3, -1]

            Quiver.close!(db)
        end

        @testset "After Deleting Children" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2

            # Create children with parent relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = 1)  # id=1, relates to Parent 1
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_id = 2)  # id=2, relates to Parent 2
            Quiver.create_element!(db, "Child"; label = "Child 3", parent_id = 1)  # id=3, relates to Parent 1
            Quiver.create_element!(db, "Child"; label = "Child 4", parent_id = 2)  # id=4, relates to Parent 2

            # Delete Child 2
            Quiver.delete_element!(db, "Child", Int64(2))

            # Now Child IDs are [1, 3, 4]
            # Child 1 -> Parent 1 (position 1)
            # Child 3 -> Parent 1 (position 1)
            # Child 4 -> Parent 2 (position 2)

            result = Quiver.scalar_relation_map(db, "Child", "Parent", "id")
            @test result == [1, 1, 2]

            Quiver.close!(db)
        end

        @testset "After Deleting Both Parents and Children" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2
            Quiver.create_element!(db, "Parent"; label = "Parent 3")  # id=3
            Quiver.create_element!(db, "Parent"; label = "Parent 4")  # id=4
            Quiver.create_element!(db, "Parent"; label = "Parent 5")  # id=5

            # Create children with parent relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = 1)  # relates to Parent 1
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_id = 2)  # relates to Parent 2
            Quiver.create_element!(db, "Child"; label = "Child 3", parent_id = 3)  # relates to Parent 3
            Quiver.create_element!(db, "Child"; label = "Child 4", parent_id = 4)  # relates to Parent 4
            Quiver.create_element!(db, "Child"; label = "Child 5", parent_id = 5)  # relates to Parent 5

            # Delete Parent 2 and Parent 4 (FK SET NULL will clear related children's parent_id)
            Quiver.delete_element!(db, "Parent", Int64(2))
            Quiver.delete_element!(db, "Parent", Int64(4))

            # Delete Child 3
            Quiver.delete_element!(db, "Child", Int64(3))

            # Parent IDs remaining: [1, 3, 5] at positions [1, 2, 3]
            # Child IDs remaining: [1, 2, 4, 5]
            # Child 1 -> Parent 1 (position 1)
            # Child 2 -> SET NULL (Parent 2 deleted), -1
            # Child 4 -> SET NULL (Parent 4 deleted), -1
            # Child 5 -> Parent 5 (position 3)

            result = Quiver.scalar_relation_map(db, "Child", "Parent", "id")
            @test result == [1, -1, -1, 3]

            Quiver.close!(db)
        end
    end

    @testset "set_relation_map" begin
        @testset "Basic Mapping" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents (mentors)
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2
            Quiver.create_element!(db, "Parent"; label = "Parent 3")  # id=3

            # Create children with mentor set relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_ref = [1, 2])    # mentored by Parent 1, 2
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_ref = [2, 3])    # mentored by Parent 2, 3
            Quiver.create_element!(db, "Child"; label = "Child 3")                         # no mentors
            
            result = Quiver.set_relation_map(db, "Child", "Parent", "ref")
            @test result == [Int64[1, 2], Int64[2, 3], Int64[]]

            Quiver.close!(db)
        end

        @testset "With Set Relations" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2
            Quiver.create_element!(db, "Parent"; label = "Parent 3")  # id=3

            # Create children with parent_ref set relations (using Child_set_parents table)
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_ref = [1, 2])    # refs Parent 1, 2
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_ref = [2, 3])    # refs Parent 2, 3
            Quiver.create_element!(db, "Child"; label = "Child 3", parent_ref = [1])       # refs Parent 1
            Quiver.create_element!(db, "Child"; label = "Child 4")                          # no refs

            # Get mapping: Child -> Parent via "ref" relation type
            result = Quiver.set_relation_map(db, "Child", "Parent", "ref")

            # Child 1 refs Parents [1, 2] -> positions [1, 2]
            # Child 2 refs Parents [2, 3] -> positions [2, 3]
            # Child 3 refs Parent [1] -> position [1]
            # Child 4 has no refs -> []
            @test sort(result[1]) == [1, 2]
            @test sort(result[2]) == [2, 3]
            @test result[3] == [1]
            @test result[4] == Int[]

            Quiver.close!(db)
        end

        @testset "After Deleting Parents (CASCADE)" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2
            Quiver.create_element!(db, "Parent"; label = "Parent 3")  # id=3
            Quiver.create_element!(db, "Parent"; label = "Parent 4")  # id=4

            # Create children with parent_ref set relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_ref = [1, 2])    # refs Parent 1, 2
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_ref = [2, 3])    # refs Parent 2, 3
            Quiver.create_element!(db, "Child"; label = "Child 3", parent_ref = [3, 4])    # refs Parent 3, 4
            Quiver.create_element!(db, "Child"; label = "Child 4", parent_ref = [1, 4])    # refs Parent 1, 4

            # Delete Parent 2 (CASCADE will remove its refs from set relations)
            Quiver.delete_element!(db, "Parent", Int64(2))

            # Now Parent IDs are [1, 3, 4] at positions [1, 2, 3]
            # Child 1 refs [1] (2 was deleted) -> position [1]
            # Child 2 refs [3] (2 was deleted) -> position [2]
            # Child 3 refs [3, 4] -> positions [2, 3]
            # Child 4 refs [1, 4] -> positions [1, 3]

            result = Quiver.set_relation_map(db, "Child", "Parent", "ref")
            @test result[1] == [1]
            @test result[2] == [2]
            @test sort(result[3]) == [2, 3]
            @test sort(result[4]) == [1, 3]

            Quiver.close!(db)
        end

        @testset "After Deleting Children" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2

            # Create children with parent_ref set relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_ref = [1, 2])    # id=1
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_ref = [1])       # id=2
            Quiver.create_element!(db, "Child"; label = "Child 3", parent_ref = [2])       # id=3
            Quiver.create_element!(db, "Child"; label = "Child 4", parent_ref = [1, 2])    # id=4

            # Delete Child 2
            Quiver.delete_element!(db, "Child", Int64(2))

            # Now Child IDs are [1, 3, 4]
            # Child 1 refs [1, 2] -> positions [1, 2]
            # Child 3 refs [2] -> position [2]
            # Child 4 refs [1, 2] -> positions [1, 2]

            result = Quiver.set_relation_map(db, "Child", "Parent", "ref")
            @test length(result) == 3
            @test sort(result[1]) == [1, 2]
            @test result[2] == [2]
            @test sort(result[3]) == [1, 2]

            Quiver.close!(db)
        end

        @testset "After Deleting Both Parents and Children" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")  # id=1
            Quiver.create_element!(db, "Parent"; label = "Parent 2")  # id=2
            Quiver.create_element!(db, "Parent"; label = "Parent 3")  # id=3
            Quiver.create_element!(db, "Parent"; label = "Parent 4")  # id=4
            Quiver.create_element!(db, "Parent"; label = "Parent 5")  # id=5

            # Create children with parent_ref set relations
            Quiver.create_element!(db, "Child"; label = "Child 1", parent_ref = [1, 2, 3])  # id=1
            Quiver.create_element!(db, "Child"; label = "Child 2", parent_ref = [2, 4])     # id=2
            Quiver.create_element!(db, "Child"; label = "Child 3", parent_ref = [3, 5])     # id=3
            Quiver.create_element!(db, "Child"; label = "Child 4", parent_ref = [1, 5])     # id=4
            Quiver.create_element!(db, "Child"; label = "Child 5", parent_ref = [4])        # id=5

            # Delete Parent 2 and Parent 4 (CASCADE removes refs)
            Quiver.delete_element!(db, "Parent", Int64(2))
            Quiver.delete_element!(db, "Parent", Int64(4))

            # Delete Child 3
            Quiver.delete_element!(db, "Child", Int64(3))

            # Parent IDs remaining: [1, 3, 5] at positions [1, 2, 3]
            # Child IDs remaining: [1, 2, 4, 5]
            # Child 1 refs [1, 3] (2 deleted) -> positions [1, 2]
            # Child 2 refs [] (2 and 4 deleted) -> []
            # Child 4 refs [1, 5] -> positions [1, 3]
            # Child 5 refs [] (4 deleted) -> []

            result = Quiver.set_relation_map(db, "Child", "Parent", "ref")
            @test length(result) == 4
            @test sort(result[1]) == [1, 2]
            @test result[2] == Int[]
            @test sort(result[3]) == [1, 3]
            @test result[4] == Int[]

            Quiver.close!(db)
        end

        @testset "Empty Relations" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Config")

            # Create parents
            Quiver.create_element!(db, "Parent"; label = "Parent 1")
            Quiver.create_element!(db, "Parent"; label = "Parent 2")

            # Create children without set relations
            Quiver.create_element!(db, "Child"; label = "Child 1")
            Quiver.create_element!(db, "Child"; label = "Child 2")
            Quiver.create_element!(db, "Child"; label = "Child 3")

            result = Quiver.set_relation_map(db, "Child", "Parent", "ref")
            @test result == [Int[], Int[], Int[]]

            Quiver.close!(db)
        end
    end
end

end