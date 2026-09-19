module TestStructSizes

using Quiver
using Test

# SAFE-02/SAFE-03 (Phase 2, plan 02-05): the load-time struct-layout gate lives in
# `generator/prologue.jl` -- NOT `src/c_api.jl` -- because `c_api.jl`'s `__init__` is verbatim
# prologue content (`generator.toml`'s `prologue_file_path`), and a hand edit made directly to
# `c_api.jl` is silently deleted by the next `generator.bat` run. This suite proves both halves:
# the happy path (the three native accessors agree with `sizeof`, and `using Quiver` -- already
# exercised just by loading this test file -- did not error) and the failure path (a deliberately
# wrong expected value actually throws, naming both numbers).

@testset "Struct Sizes" begin
    @testset "using Quiver did not error" begin
        # If __init__'s _assert_struct_sizes() call had failed, `using Quiver` above (module
        # load, at the top of this file) would already have thrown before reaching this line.
        @test true
    end

    @testset "happy path: the three accessors match sizeof" begin
        @test Quiver.C.quiver_database_options_sizeof() == sizeof(Quiver.C.quiver_database_options_t)
        @test Quiver.C.quiver_database_options_sizeof() == 24
        @test Quiver.C.quiver_scalar_metadata_sizeof() == sizeof(Quiver.C.quiver_scalar_metadata_t)
        @test Quiver.C.quiver_scalar_metadata_sizeof() == 56
        @test Quiver.C.quiver_group_metadata_sizeof() == sizeof(Quiver.C.quiver_group_metadata_t)
        @test Quiver.C.quiver_group_metadata_sizeof() == 32
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
    end

    @testset "exact-equal size passes; no tolerance band" begin
        @test Quiver.C._check_struct_size("quiver_database_options_t", 24, 24) === nothing
    end
end

end
