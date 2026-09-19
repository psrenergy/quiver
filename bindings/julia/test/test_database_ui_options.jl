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
malformed_ui_dir() = joinpath(tests_path(), "schemas", "ui", "malformed", "ui")

# A fresh scratch directory with NO `ui/` sibling of its own -- what makes the explicit
# `ui_config_dir` proof real: the convention path (`<db_dir>/ui/`) cannot resolve here.
scratch_db_path(stem) = joinpath(mktempdir(), "$(stem).sqlite")

# Gap 7 (02-VERIFICATION.md): `from_migrations` needs a migrations directory that actually
# creates the EconomicDriver schema the foresight_like sidecar labels -- no such fixture is
# checked in (the plan's files_modified list is the six test files only), so this writes one at
# runtime into a scratch directory, mirroring foresight_like/schema.sql's own DDL. Not a tracked
# fixture: nothing here is committed.
function scratch_migrations_dir()
    dir = mktempdir()
    up_dir = joinpath(dir, "1")
    mkpath(up_dir)
    write(joinpath(up_dir, "up.sql"), read(foresight_schema_path(), String))
    write(joinpath(up_dir, "down.sql"), "DROP TABLE EconomicDriver;\nDROP TABLE Configuration;\n")
    return dir
end

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

    # --- Gap 5 (02-VERIFICATION.md): malformed polarity, previously C++-only ---
    @testset "malformed sidecar reports false without throwing" begin
        db = Quiver.from_schema(
            scratch_db_path("julia_options_malformed"),
            foresight_schema_path();
            ui_config_dir = malformed_ui_dir(),
        )
        @test !Quiver.has_ui_config(db)
        Quiver.close!(db)
    end

    # --- Gap 5 (02-VERIFICATION.md): the D-01 :memory: distinction, previously C++-only ---
    @testset ":memory: distinction" begin
        with_dir = Quiver.from_schema(
            ":memory:",
            foresight_schema_path();
            ui_config_dir = foresight_ui_dir(),
        )
        @test Quiver.has_ui_config(with_dir)
        Quiver.close!(with_dir)

        without_dir = Quiver.from_schema(":memory:", foresight_schema_path())
        @test !Quiver.has_ui_config(without_dir)
        Quiver.close!(without_dir)
    end

    # --- Gap 7 (02-VERIFICATION.md): open()/from_migrations() with ui_config_dir + ui_locale,
    # previously proven only through from_schema() by a committed test (open/from_migrations were
    # proven only by ad-hoc runtime probe per 02-VERIFICATION.md SC1). ---
    @testset "open() threads ui_config_dir and ui_locale" begin
        db_path = scratch_db_path("julia_options_open")
        seed = Quiver.from_schema(db_path, foresight_schema_path())
        Quiver.close!(seed)

        db = Quiver.open(db_path; ui_config_dir = foresight_ui_dir(), ui_locale = "es")
        report = Quiver.describe_collection(db, "EconomicDriver")
        @test occursin("Ingenuo Estacional", report)
        @test occursin("Tendencia Lineal Local", report)
        @test !occursin("Seasonal Naïve", report)
        Quiver.close!(db)
    end

    @testset "from_migrations() threads ui_config_dir and ui_locale" begin
        migrations_dir = scratch_migrations_dir()
        db = Quiver.from_migrations(
            scratch_db_path("julia_options_migrations"),
            migrations_dir;
            ui_config_dir = foresight_ui_dir(),
            ui_locale = "es",
        )
        report = Quiver.describe_collection(db, "EconomicDriver")
        @test occursin("Ingenuo Estacional", report)
        @test occursin("Tendencia Lineal Local", report)
        @test !occursin("Seasonal Naïve", report)
        Quiver.close!(db)
    end

    # --- Gap 7 (02-VERIFICATION.md): a locale label through whole-database describe(), not only
    # describe_collection(). ---
    @testset "describe() carries the locale-specific label" begin
        db = Quiver.from_schema(
            scratch_db_path("julia_options_describe"),
            foresight_schema_path();
            ui_config_dir = foresight_ui_dir(),
            ui_locale = "es",
        )
        report = Quiver.describe(db)
        @test occursin("Ingenuo Estacional", report)
        @test !occursin("Seasonal Naïve", report)
        Quiver.close!(db)
    end

    # --- OPT-03/empty (02-09 edge lift): an empty-string ui_locale is unset (D-03), not a locale
    # named "". ---
    @testset "empty-string ui_locale matches the unset/en output" begin
        db = Quiver.from_schema(
            scratch_db_path("julia_options_empty_locale"),
            foresight_schema_path();
            ui_config_dir = foresight_ui_dir(),
            ui_locale = "",
        )
        report = Quiver.describe_collection(db, "EconomicDriver")
        @test occursin("Seasonal Naïve", report)
        @test !occursin("Ingenuo Estacional", report)
        Quiver.close!(db)
    end
end

end
