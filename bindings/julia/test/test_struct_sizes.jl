module TestStructSizes

using Quiver
using Test

# SAFE-02/SAFE-03 (Phase 2, plans 02-05/02-11): the load-time struct-layout gate lives in
# `generator/prologue.jl` -- NOT `src/c_api.jl` -- because `c_api.jl`'s `__init__` is verbatim
# prologue content (`generator.toml`'s `prologue_file_path`), and a hand edit made directly to
# `c_api.jl` is silently deleted by the next `generator.bat` run. This suite proves three things:
# the gate actually ran, in the fixed four-struct order, via the `_CHECKED_STRUCTS` wiring-evidence
# record (not by re-driving the gate itself); the happy path (all four native accessors agree with
# `sizeof`); and the failure path (a deliberately wrong expected value actually throws, naming both
# numbers).

@testset "Struct Sizes" begin
    @testset "the gate ran and checked all four structs in order" begin
        # Observes Quiver.C._CHECKED_STRUCTS -- populated by __init__'s _assert_struct_sizes()
        # call, which already ran when `using Quiver` loaded this module above. This does NOT
        # call _assert_struct_sizes itself: doing so would repopulate the record and let the test
        # stay green even if the real __init__ call site were deleted -- the exact vacuous-pass
        # defect this record exists to catch (the old version of this testset was a literal
        # `@test true` whose comment claimed the module load alone proved the gate ran).
        @test Quiver.C._CHECKED_STRUCTS == [
            "quiver_database_options_t",
            "quiver_scalar_metadata_t",
            "quiver_group_metadata_t",
            "quiver_csv_options_t",
        ]
    end

    @testset "happy path: the four accessors match sizeof" begin
        @test Quiver.C.quiver_database_options_sizeof() == sizeof(Quiver.C.quiver_database_options_t)
        @test Quiver.C.quiver_database_options_sizeof() == 24
        @test Quiver.C.quiver_scalar_metadata_sizeof() == sizeof(Quiver.C.quiver_scalar_metadata_t)
        @test Quiver.C.quiver_scalar_metadata_sizeof() == 56
        @test Quiver.C.quiver_group_metadata_sizeof() == sizeof(Quiver.C.quiver_group_metadata_t)
        @test Quiver.C.quiver_group_metadata_sizeof() == 32
        @test Quiver.C.quiver_csv_options_sizeof() == sizeof(Quiver.C.quiver_csv_options_t)
        @test Quiver.C.quiver_csv_options_sizeof() == 56
    end

    @testset "failure path: a wrong expected value throws naming both numbers" begin
        @test_throws ErrorException Quiver.C._check_struct_size("quiver_database_options_t", 24, 8)
        err = try
            Quiver.C._check_struct_size("quiver_database_options_t", 24, 8)
            nothing
        catch e
            e
        end
        @test err isa ErrorException
        @test occursin("quiver_database_options_t", err.msg)
        @test occursin("24", err.msg)
        @test occursin("8", err.msg)

        @test_throws ErrorException Quiver.C._check_struct_size("quiver_scalar_metadata_t", 56, 40)
        err = try
            Quiver.C._check_struct_size("quiver_scalar_metadata_t", 56, 40)
            nothing
        catch e
            e
        end
        @test err isa ErrorException
        @test occursin("quiver_scalar_metadata_t", err.msg)
        @test occursin("56", err.msg)
        @test occursin("40", err.msg)

        @test_throws ErrorException Quiver.C._check_struct_size("quiver_group_metadata_t", 32, 16)
        err = try
            Quiver.C._check_struct_size("quiver_group_metadata_t", 32, 16)
            nothing
        catch e
            e
        end
        @test err isa ErrorException
        @test occursin("quiver_group_metadata_t", err.msg)
        @test occursin("32", err.msg)
        @test occursin("16", err.msg)

        @test_throws ErrorException Quiver.C._check_struct_size("quiver_csv_options_t", 56, 55)
        err = try
            Quiver.C._check_struct_size("quiver_csv_options_t", 56, 55)
            nothing
        catch e
            e
        end
        @test err isa ErrorException
        @test occursin("quiver_csv_options_t", err.msg)
        @test occursin("56", err.msg)
        @test occursin("55", err.msg)

        @test_throws ErrorException Quiver.C._check_struct_size("quiver_csv_options_t", 56, 57)
        err = try
            Quiver.C._check_struct_size("quiver_csv_options_t", 56, 57)
            nothing
        catch e
            e
        end
        @test err isa ErrorException
        @test occursin("quiver_csv_options_t", err.msg)
        @test occursin("56", err.msg)
        @test occursin("57", err.msg)
    end

    @testset "exact-equal size passes; no tolerance band" begin
        @test Quiver.C._check_struct_size("quiver_database_options_t", 24, 24) === nothing
    end
end

end
