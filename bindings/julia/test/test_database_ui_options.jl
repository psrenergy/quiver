module TestDatabaseUiOptions

using Quiver
using Test

include("fixture.jl")

# OPT-01/OPT-02/OPT-03/OPT-04 (Phase 2, plan 02-05): the two new `quiver_database_options_t`
# fields threaded through `build_quiver_database_options` (shared by `open`/`from_schema`/
# `from_migrations`), plus `has_ui_config`. Reuses the shared `tests/schemas/ui/foresight_like`
# fixture (never copied into this binding) -- its `enum.toml` is the corpus's only
# locale-varying data (see D-06 in 02-CONTEXT.md).

foresight_schema_path() = joinpath(tests_path(), "schemas", "ui", "foresight_like", "schema.sql")
foresight_ui_dir() = joinpath(tests_path(), "schemas", "ui", "foresight_like", "ui")

# A fresh scratch directory with NO `ui/` sibling of its own -- what makes the explicit
# `ui_config_dir` proof real: the convention path (`<db_dir>/ui/`) cannot resolve here.
scratch_db_path(stem) = joinpath(mktempdir(), "$(stem).sqlite")

@testset "UI Options" begin
    @testset "options struct is 24 bytes" begin
        @test sizeof(Quiver.C.quiver_database_options_t) == 24
        @test fieldoffset(Quiver.C.quiver_database_options_t, 1) == 0
        @test fieldoffset(Quiver.C.quiver_database_options_t, 2) == 4
        @test fieldoffset(Quiver.C.quiver_database_options_t, 3) == 8
        @test fieldoffset(Quiver.C.quiver_database_options_t, 4) == 16
    end

    @testset "explicit ui_config_dir loads a config not beside the database" begin
        db = Quiver.from_schema(
            scratch_db_path("julia_options_explicit"),
            foresight_schema_path();
            ui_config_dir = foresight_ui_dir(),
        )
        @test Quiver.has_ui_config(db)
        report = Quiver.describe_collection(db, "EconomicDriver")
        @test occursin("Seasonal Naïve", report)
        Quiver.close!(db)
    end

    @testset "spanish locale renders spanish labels" begin
        db = Quiver.from_schema(
            scratch_db_path("julia_options_es"),
            foresight_schema_path();
            ui_config_dir = foresight_ui_dir(),
            ui_locale = "es",
        )
        report = Quiver.describe_collection(db, "EconomicDriver")
        @test occursin("Ingenuo Estacional", report)
        @test occursin("Tendencia Lineal Local", report)
        @test !occursin("Seasonal Naïve", report)
        Quiver.close!(db)
    end

    @testset "default locale renders english labels" begin
        db = Quiver.from_schema(
            scratch_db_path("julia_options_default_locale"),
            foresight_schema_path();
            ui_config_dir = foresight_ui_dir(),
        )
        report = Quiver.describe_collection(db, "EconomicDriver")
        @test occursin("Seasonal Naïve", report)
        @test !occursin("Ingenuo Estacional", report)
        @test !occursin("Tendencia Lineal Local", report)
        Quiver.close!(db)
    end

    @testset "missing ui_config_dir degrades without throwing" begin
        db = Quiver.from_schema(
            scratch_db_path("julia_options_missing"),
            foresight_schema_path();
            ui_config_dir = joinpath(foresight_ui_dir(), "does_not_exist"),
        )
        @test !Quiver.has_ui_config(db)
        Quiver.close!(db)
    end
end

end
