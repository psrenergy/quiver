module TestDatabaseDescribe

using Quiver
using Test

include("fixture.jl")

function open_db()
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    return Quiver.from_schema(":memory:", path_schema)
end

# Shared `tests/schemas/ui/` fixture corpus (CORPUS-03) -- never copied into this binding.
ui_fixture_path(name) = joinpath(tests_path(), "schemas", "ui", name)

function open_ui_fixture(name, stem)
    dir = ui_fixture_path(name)
    return Quiver.from_schema(joinpath(dir, "julia_$(stem).sqlite"), joinpath(dir, "schema.sql"))
end

@testset "Describe" begin
    @testset "describe" begin
        db = open_db()
        @test Quiver.describe(db) isa String
        Quiver.close!(db)
    end

    @testset "describe_collection" begin
        db = open_db()
        @test Quiver.describe_collection(db, "Collection") isa String
        Quiver.close!(db)
    end

    @testset "summarize_collection" begin
        db = open_db()
        @test Quiver.summarize_collection(db, "Collection") isa String
        Quiver.close!(db)
    end

    # DESC-07: exact-string enum rendering against the shared tests/schemas/ui/ fixtures --
    # a "returns a String" assertion alone cannot catch a per-binding decoding bug (D-31).

    @testset "enum vocabulary declared with zero elements" begin
        db = open_ui_fixture("enum_basic", "declared")
        report = Quiver.describe_collection(db, "Storage")
        @test occursin("enum bool {0: Disabled, 1: Enabled}", report)
        Quiver.close!(db)
    end

    @testset "enum histogram over twelve elements" begin
        db = open_ui_fixture("enum_basic", "histogram")
        for i in 1:8
            Quiver.create_element!(db, "Storage"; label = "Disabled $(i)", has_commitment = 0)
        end
        for i in 1:4
            Quiver.create_element!(db, "Storage"; label = "Enabled $(i)", has_commitment = 1)
        end
        report = Quiver.summarize_collection(db, "Storage")
        @test occursin("values {0: 8 (Disabled), 1: 4 (Enabled)}", report)
        Quiver.close!(db)
    end

    @testset "UI config header line" begin
        db = open_ui_fixture("enum_basic", "header")
        report = Quiver.describe(db)
        @test occursin("UI config: ", report)
        @test occursin(" (locale: en)", report)
        Quiver.close!(db)
    end

    @testset "unit and hidden decoration" begin
        db = open_ui_fixture("enum_basic", "unit_hidden")
        report = Quiver.describe_collection(db, "Storage")
        @test occursin("[MW]", report)
        @test occursin("[hidden]", report)
        Quiver.close!(db)
    end

    @testset "accented enum label renders byte-for-byte" begin
        db = open_ui_fixture("foresight_like", "accented")
        report = Quiver.describe_collection(db, "EconomicDriver")
        @test occursin("Seasonal Naïve", report)
        Quiver.close!(db)
    end

    @testset "no ui/ directory leaves the header off" begin
        db = open_ui_fixture("no_ui_dir", "no_sidecar")
        report = Quiver.describe(db)
        @test !occursin("UI config: ", report)
        Quiver.close!(db)
    end
end

end
