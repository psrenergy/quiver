module TestExpression

using Quiver
using Test

function make_path(name)
    return joinpath(tempdir(), "quiver_julia_expr_$(name)")
end

function cleanup(paths...)
    for p in paths
        for ext in [".qvr", ".toml"]
            f = p * ext
            isfile(f) && rm(f)
        end
    end
end

function make_simple_metadata()
    return Quiver.Binary.Metadata(;
        initial_datetime = "2025-01-01T00:00:00",
        unit = "MW",
        labels = ["val1", "val2"],
        dimensions = ["row", "col"],
        dimension_sizes = Int64[3, 2],
    )
end

function make_metadata(; rows::Int = 3, cols::Int = 2, unit::String = "MW", labels = ["val1", "val2"])
    return Quiver.Binary.Metadata(;
        initial_datetime = "2025-01-01T00:00:00",
        unit = unit,
        labels = labels,
        dimensions = ["row", "col"],
        dimension_sizes = Int64[rows, cols],
    )
end

# Writes a 3 x 2 fixture file using the default metadata. fill(r, c, k) returns the cell value.
function write_fixture(path::String, fill::Function)
    md = make_simple_metadata()
    write_fixture_with_metadata(path, md, fill)
    return nothing
end

# Writes a fixture with caller-provided metadata (rows × 2). fill(r, c, k) returns the cell value.
function write_fixture_with_metadata(path::String, md, fill::Function; rows::Int = 3, cols::Int = 2)
    file = Quiver.Binary.open_file(path; mode = :write, metadata = md)
    for r in 1:rows, c in 1:cols
        Quiver.Binary.write!(file; data = [Float64(fill(r, c, 1)), Float64(fill(r, c, 2))], row = r, col = c)
    end
    Quiver.Binary.close!(file)
    return nothing
end

# Reads all 3 × 2 × 2 cells in (r, c, k) order.
function read_all_cells(path::String; rows::Int = 3, cols::Int = 2)
    file = Quiver.Binary.open_file(path; mode = :read)
    out = Float64[]
    for r in 1:rows, c in 1:cols
        append!(out, Quiver.Binary.read(file; row = r, col = c))
    end
    Quiver.Binary.close!(file)
    return out
end

@testset "Expression" begin
    # ==========================================================================
    # Identity / save round-trip
    # ==========================================================================

    @testset "Identity round-trip" begin
        path_a = make_path("a")
        path_out = make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            file = Quiver.Binary.open_file(path_a; mode = :read)
            e = Quiver.Expression.Expression(file)
            Quiver.Expression.save(e, path_out)
            Quiver.Binary.close!(file)

            orig = read_all_cells(path_a)
            copy = read_all_cells(path_out)
            @test orig == copy
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Save twice produces same output" begin
        path_a = make_path("a")
        path_out = make_path("out")
        path_out2 = make_path("out2")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            file = Quiver.Binary.open_file(path_a; mode = :read)
            e = Quiver.Expression.Expression(file)
            Quiver.Expression.save(e, path_out)
            Quiver.Expression.save(e, path_out2)
            Quiver.Binary.close!(file)

            @test read_all_cells(path_out) == read_all_cells(path_out2)
        finally
            cleanup(path_a, path_out, path_out2)
        end
    end

    # ==========================================================================
    # Binary operators (file op file)
    # ==========================================================================

    @testset "Add two files" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            write_fixture(path_b, (r, c, k) -> r * 100 + c * 5 + k * 2)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            sum_expr = Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb)
            Quiver.Expression.save(sum_expr, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            va = read_all_cells(path_a)
            vb = read_all_cells(path_b)
            vo = read_all_cells(path_out)
            @test vo == va .+ vb
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Subtract two files" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            write_fixture(path_b, (r, c, k) -> r + c + k)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            diff = Quiver.Expression.Expression(fa) - Quiver.Expression.Expression(fb)
            Quiver.Expression.save(diff, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            va, vb, vo = read_all_cells(path_a), read_all_cells(path_b), read_all_cells(path_out)
            @test vo == va .- vb
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Multiply two files" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k + 1)
            write_fixture(path_b, (r, c, k) -> r * c + k + 1)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            product = Quiver.Expression.Expression(fa) * Quiver.Expression.Expression(fb)
            Quiver.Expression.save(product, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            va, vb, vo = read_all_cells(path_a), read_all_cells(path_b), read_all_cells(path_out)
            @test vo == va .* vb
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Divide two files" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            write_fixture(path_b, (r, c, k) -> r + c + k + 1)  # never zero
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            quotient = Quiver.Expression.Expression(fa) / Quiver.Expression.Expression(fb)
            Quiver.Expression.save(quotient, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            va, vb, vo = read_all_cells(path_a), read_all_cells(path_b), read_all_cells(path_out)
            @test vo ≈ va ./ vb
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Chained expression (a + b - c) / 2" begin
        path_a, path_b, path_c, path_out =
            make_path("a"), make_path("b"), make_path("c"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            write_fixture(path_b, (r, c, k) -> r * 2 + c + k)
            write_fixture(path_c, (r, c, k) -> r + c * 2 + k)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            fc = Quiver.Binary.open_file(path_c; mode = :read)
            result = (Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb) -
                      Quiver.Expression.Expression(fc)) / 2.0
            Quiver.Expression.save(result, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)
            Quiver.Binary.close!(fc)

            va, vb, vc, vo =
                read_all_cells(path_a), read_all_cells(path_b), read_all_cells(path_c), read_all_cells(path_out)
            @test vo ≈ (va .+ vb .- vc) ./ 2.0
        finally
            cleanup(path_a, path_b, path_c, path_out)
        end
    end

    # ==========================================================================
    # Scalar broadcasts (right)
    # ==========================================================================

    @testset "Scalar broadcast: a + 2.0" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            result = Quiver.Expression.Expression(fa) + 2.0
            Quiver.Expression.save(result, path_out)
            Quiver.Binary.close!(fa)

            @test read_all_cells(path_out) == read_all_cells(path_a) .+ 2.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: a - 5.0" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            Quiver.Expression.save(Quiver.Expression.Expression(fa) - 5.0, path_out)
            Quiver.Binary.close!(fa)
            @test read_all_cells(path_out) == read_all_cells(path_a) .- 5.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: a * 3.0" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            Quiver.Expression.save(Quiver.Expression.Expression(fa) * 3.0, path_out)
            Quiver.Binary.close!(fa)
            @test read_all_cells(path_out) == read_all_cells(path_a) .* 3.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: a / 4.0" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k + 1)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            Quiver.Expression.save(Quiver.Expression.Expression(fa) / 4.0, path_out)
            Quiver.Binary.close!(fa)
            @test read_all_cells(path_out) ≈ read_all_cells(path_a) ./ 4.0
        finally
            cleanup(path_a, path_out)
        end
    end

    # ==========================================================================
    # Scalar broadcasts (left)
    # ==========================================================================

    @testset "Scalar broadcast: 7.0 + a" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            Quiver.Expression.save(7.0 + Quiver.Expression.Expression(fa), path_out)
            Quiver.Binary.close!(fa)
            @test read_all_cells(path_out) == 7.0 .+ read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: 100.0 - a" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            Quiver.Expression.save(100.0 - Quiver.Expression.Expression(fa), path_out)
            Quiver.Binary.close!(fa)
            @test read_all_cells(path_out) == 100.0 .- read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: 5.0 * a" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k + 1)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            Quiver.Expression.save(5.0 * Quiver.Expression.Expression(fa), path_out)
            Quiver.Binary.close!(fa)
            @test read_all_cells(path_out) == 5.0 .* read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: 60.0 / a" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k + 1)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            Quiver.Expression.save(60.0 / Quiver.Expression.Expression(fa), path_out)
            Quiver.Binary.close!(fa)
            @test read_all_cells(path_out) ≈ 60.0 ./ read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    # ==========================================================================
    # Validation errors
    # ==========================================================================

    @testset "Mismatched shapes throws" begin
        path_a, path_b = make_path("a"), make_path("b")
        try
            md_a = make_metadata(rows = 3)
            md_b = make_metadata(rows = 4)
            write_fixture_with_metadata(path_a, md_a, (_, _, _) -> 1.0; rows = 3)
            write_fixture_with_metadata(path_b, md_b, (_, _, _) -> 1.0; rows = 4)

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            @test_throws Quiver.DatabaseException Quiver.Expression.Expression(fa) +
                                                  Quiver.Expression.Expression(fb)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)
        finally
            cleanup(path_a, path_b)
        end
    end

    @testset "Unit mismatch throws" begin
        path_a, path_b = make_path("a"), make_path("b")
        try
            md_a = make_metadata(unit = "MW")
            md_b = make_metadata(unit = "GWh")
            write_fixture_with_metadata(path_a, md_a, (_, _, _) -> 1.0)
            write_fixture_with_metadata(path_b, md_b, (_, _, _) -> 1.0)

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            @test_throws Quiver.DatabaseException Quiver.Expression.Expression(fa) +
                                                  Quiver.Expression.Expression(fb)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)
        finally
            cleanup(path_a, path_b)
        end
    end

    @testset "Label mismatch throws" begin
        path_a, path_b = make_path("a"), make_path("b")
        try
            md_a = make_metadata(labels = ["v1", "v2"])
            md_b = make_metadata(labels = ["v1", "v3"])
            write_fixture_with_metadata(path_a, md_a, (_, _, _) -> 1.0)
            write_fixture_with_metadata(path_b, md_b, (_, _, _) -> 1.0)

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            @test_throws Quiver.DatabaseException Quiver.Expression.Expression(fa) +
                                                  Quiver.Expression.Expression(fb)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)
        finally
            cleanup(path_a, path_b)
        end
    end

    @testset "Self-save collision throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 42.0)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            e = Quiver.Expression.Expression(fa)
            @test_throws Quiver.DatabaseException Quiver.Expression.save(e, path_a)
            Quiver.Binary.close!(fa)
        finally
            cleanup(path_a)
        end
    end

    # ==========================================================================
    # Lifecycle
    # ==========================================================================

    @testset "Explicit destroy! is idempotent" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            e = Quiver.Expression.Expression(fa)
            Quiver.Expression.destroy!(e)
            Quiver.Expression.destroy!(e)  # second call is no-op
            @test e.ptr == C_NULL
            Quiver.Binary.close!(fa)
        finally
            cleanup(path_a)
        end
    end

    # ==========================================================================
    # Metadata access
    # ==========================================================================

    @testset "get_metadata exposes unit and labels" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            e = Quiver.Expression.Expression(fa)
            md = Quiver.Expression.get_metadata(e)
            @test Quiver.Binary.get_unit(md) == "MW"
            @test Quiver.Binary.get_labels(md) == ["val1", "val2"]
            Quiver.Binary.close!(fa)
        finally
            cleanup(path_a)
        end
    end

    @testset "get_metadata after scalar op returns broadcast metadata" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            e = Quiver.Expression.Expression(fa) * 2.0
            md = Quiver.Expression.get_metadata(e)
            @test Quiver.Binary.get_unit(md) == "MW"  # unit preserved through scalar op
            Quiver.Binary.close!(fa)
        finally
            cleanup(path_a)
        end
    end
end

end
