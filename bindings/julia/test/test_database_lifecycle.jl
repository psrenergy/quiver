module TestDatabaseLifecycle

using Quiver
using Test

include("fixture.jl")

@testset "Valid Schema" begin
    @testset "Basic" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.close!(db)
        @test true
    end

    @testset "Collections" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.close!(db)
        @test true
    end

    @testset "Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.close!(db)
        @test true
    end
end

@testset "Open Existing" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    mktempdir() do dir
        db_path = joinpath(dir, "test.db")
        db = Quiver.from_schema(db_path, path_schema)
        Quiver.close!(db)

        reopened = Quiver.open(db_path)
        @test Quiver.is_healthy(reopened) == true
        Quiver.close!(reopened)
        return nothing
    end
end

@testset "Scoped database factories" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    path_migrations = joinpath(tests_path(), "schemas", "migrations")

    schema_db = Ref{Union{Nothing, Quiver.Database}}(nothing)
    schema_result = Quiver.from_schema(":memory:", path_schema) do db
        schema_db[] = db
        @test Quiver.is_healthy(db)
        return :schema_result
    end
    @test schema_result === :schema_result
    @test schema_db[].ptr == C_NULL

    migrations_db = Ref{Union{Nothing, Quiver.Database}}(nothing)
    migrations_result = Quiver.from_migrations(":memory:", path_migrations) do db
        migrations_db[] = db
        @test Quiver.current_version(db) == 3
        return :migrations_result
    end
    @test migrations_result === :migrations_result
    @test migrations_db[].ptr == C_NULL

    # File-backed: `:memory:` cannot show a leaked handle, and `mktempdir` downgrades a
    # teardown failure to `@error`, so the closure assertions above prove nothing on their own.
    mktempdir() do dir
        db_path = joinpath(dir, "test.db")
        file_db = Ref{Union{Nothing, Quiver.Database}}(nothing)
        Quiver.from_schema(db_path, path_schema) do db
            file_db[] = db
            Quiver.create_element!(db, "Configuration", label = "config")
        end
        @test file_db[].ptr == C_NULL

        opened_db = Ref{Union{Nothing, Quiver.Database}}(nothing)
        open_result = Quiver.open(db_path; read_only = true) do db
            opened_db[] = db
            # Pins the `kwargs...` forwarding: `is_healthy` is true for any handle that
            # opened at all, whatever the options.
            @test_throws Quiver.DatabaseException Quiver.create_element!(db, "Configuration", label = "denied")
            return :open_result
        end
        @test open_result === :open_result
        @test opened_db[].ptr == C_NULL
        return nothing
    end
end

@testset "Scoped database factories reject a non-callable first argument" begin
    # Untyped `fn` would capture an arity slip and run the factory before the MethodError --
    # `from_schema` removes its target file, and a plain `open` creates one.
    @test_throws MethodError Quiver.from_schema("a.db", "b.db", "schema.sql")
    @test_throws MethodError Quiver.from_migrations("a.db", "b.db", "migrations")
    @test_throws MethodError Quiver.open("out.csv", "w")
end

@testset "Scoped database factory closes after callback error" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    captured_db = Ref{Union{Nothing, Quiver.Database}}(nothing)

    exception = @test_throws ErrorException begin
        Quiver.from_schema(":memory:", path_schema) do db
            captured_db[] = db
            error("scoped callback failed")
        end
    end
    @test exception.value.msg == "scoped callback failed"
    @test captured_db[].ptr == C_NULL
end

@testset "Describe" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = Quiver.from_schema(":memory:", path_schema)

    # Just verify describe runs without error
    Quiver.describe(db)
    @test true

    Quiver.close!(db)
end

@testset "Current Version" begin
    @testset "Schema returns 0" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        @test Quiver.current_version(db) == 0
        Quiver.close!(db)
    end

    @testset "Migrations returns count" begin
        path_migrations = joinpath(tests_path(), "schemas", "migrations")
        db = Quiver.from_migrations(":memory:", path_migrations)
        @test Quiver.current_version(db) == 3
        Quiver.close!(db)
    end
end

@testset "Test Migrations" begin
    @testset "Success" begin
        path_migrations = joinpath(tests_path(), "schemas", "migrations")
        @test Quiver.validate_migrations(path_migrations) === nothing
    end

    @testset "Path validation" begin
        path = joinpath(tests_path(), "schemas", "does_not_exist")
        exc = @test_throws Quiver.DatabaseException Quiver.validate_migrations(path)
        @test exc.value.msg == "Cannot validate_migrations: migrations path not found: " * path
    end
end

@testset "is_healthy" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = Quiver.from_schema(":memory:", path_schema)
    @test Quiver.is_healthy(db) == true
    Quiver.close!(db)
end

@testset "path" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    mktempdir() do dir
        db_path = joinpath(dir, "test.db")
        db = Quiver.from_schema(db_path, path_schema)
        result = Quiver.path(db)
        @test result isa String
        @test occursin("test.db", result)
        Quiver.close!(db)
        return nothing
    end
end

end
