module TestExpression

using Quiver
using Test

function make_path(name)
    return joinpath(tempdir(), "quiver_julia_expr_$(name)")
end

function cleanup(paths...)
    # Expression's internal FileNode opens its own BinaryFile that stays open until the
    # Expression handle is finalized. Locals like `e` are still in scope inside `finally`,
    # so we retry rm() across GC.gc() calls until handles are released.
    for p in paths
        for ext in [".qvr", ".toml"]
            f = p * ext
            for _ in 1:10
                isfile(f) || break
                try
                    rm(f)
                    break
                catch
                    GC.gc()
                end
            end
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

# Generic metadata builder. Pass empty time_dimensions for non-time data.
function make_metadata_full(;
    dimensions,
    dimension_sizes,
    labels = ["v1", "v2"],
    unit::String = "MW",
    initial_datetime::String = "2025-01-01T00:00:00",
    time_dimensions = String[],
    frequencies = String[],
)
    return Quiver.Binary.Metadata(;
        initial_datetime = initial_datetime,
        unit = unit,
        labels = labels,
        dimensions = dimensions,
        dimension_sizes = Int64.(dimension_sizes),
        time_dimensions = time_dimensions,
        frequencies = frequencies,
    )
end

# Writes a single cell at the given dim positions; rest of the file stays NaN.
function write_one_cell(path::String, md; data, dims...)
    file = Quiver.Binary.open_file(path; mode = :write, metadata = md)
    Quiver.Binary.write!(file; data = data, dims...)
    Quiver.Binary.close!(file)
    return nothing
end

# Writes every cell with row-major iteration. Caller must ensure no nested
# time dimensions (sizes are constant across the grid).
# fill(dims_tuple, label_idx) returns the value (label_idx is 1-based).
function write_dense(
    path::String,
    md,
    dim_names::Vector{Symbol},
    dim_sizes::Vector{Int},
    label_count::Int,
    fill::Function,
)
    file = Quiver.Binary.open_file(path; mode = :write, metadata = md)
    dims = ones(Int64, length(dim_sizes))
    while true
        row = [Float64(fill(dims, k)) for k in 1:label_count]
        Quiver.Binary.write!(file; data = row, NamedTuple{Tuple(dim_names)}(Tuple(dims))...)

        i = length(dims)
        while i >= 1
            dims[i] += 1
            dims[i] <= dim_sizes[i] && break
            dims[i] = 1
            i -= 1
        end
        i < 1 && break
    end
    Quiver.Binary.close!(file)
    return nothing
end

# Reads a single cell at the given dim positions, allowing nulls.
function read_one_cell(path::String; dims...)
    file = Quiver.Binary.open_file(path; mode = :read)
    cell = Quiver.Binary.read(file; allow_nulls = true, dims...)
    Quiver.Binary.close!(file)
    return cell
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
            result =
                (
                    Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb) -
                    Quiver.Expression.Expression(fc)
                ) / 2.0
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

    @testset "Explicit close! is idempotent" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            fa = Quiver.Binary.open_file(path_a; mode = :read)
            e = Quiver.Expression.Expression(fa)
            Quiver.Expression.close!(e)
            Quiver.Expression.close!(e)  # second call is no-op
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

    # ==========================================================================
    # Same path twice
    # ==========================================================================

    @testset "Same path twice (a + a, each FileNode opens independently)" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            f1 = Quiver.Binary.open_file(path_a; mode = :read)
            f2 = Quiver.Binary.open_file(path_a; mode = :read)
            sum_expr = Quiver.Expression.Expression(f1) + Quiver.Expression.Expression(f2)
            Quiver.Expression.save(sum_expr, path_out)
            Quiver.Binary.close!(f1)
            Quiver.Binary.close!(f2)

            @test read_all_cells(path_out) == 2.0 .* read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    # ==========================================================================
    # Broadcasting
    # ==========================================================================

    @testset "Broadcast size-1 dim" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            md_a = make_metadata_full(dimensions = ["row", "col"], dimension_sizes = [3, 2])
            md_b = make_metadata_full(dimensions = ["row", "col"], dimension_sizes = [1, 2])

            write_dense(path_a, md_a, [:row, :col], [3, 2], 2,
                        (dims, k) -> dims[1] * 100 + dims[2] * 10 + (k - 1))
            write_dense(path_b, md_b, [:row, :col], [1, 2], 2, (dims, k) -> dims[2] * 1000 + (k - 1))

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            sum_expr = Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb)
            Quiver.Expression.save(sum_expr, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            # out[r,c,k] = a[r,c,k] + b[1,c,k]
            cell_111 = read_one_cell(path_out; row = 1, col = 1)
            cell_211 = read_one_cell(path_out; row = 2, col = 1)
            cell_322 = read_one_cell(path_out; row = 3, col = 2)
            @test cell_111[1] == (1 * 100 + 1 * 10 + 0) + (1 * 1000 + 0)
            @test cell_211[1] == (2 * 100 + 1 * 10 + 0) + (1 * 1000 + 0)
            @test cell_322[2] == (3 * 100 + 2 * 10 + 1) + (2 * 1000 + 1)
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Broadcast labels axis (1 label vs 3)" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            md_a = make_metadata_full(dimensions = ["row", "col"], dimension_sizes = [2, 2], labels = ["single"])
            md_b = make_metadata_full(
                dimensions = ["row", "col"],
                dimension_sizes = [2, 2],
                labels = ["l1", "l2", "l3"],
            )

            write_dense(path_a, md_a, [:row, :col], [2, 2], 1, (dims, _) -> dims[1] * 10 + dims[2])
            write_dense(path_b, md_b, [:row, :col], [2, 2], 3,
                        (dims, k) -> dims[1] * 100 + dims[2] * 10 + (k - 1))

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            sum_expr = Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb)
            Quiver.Expression.save(sum_expr, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            cell_11 = read_one_cell(path_out; row = 1, col = 1)
            @test length(cell_11) == 3  # labels promoted from 1 to 3
            @test cell_11[1] == (1 * 10 + 1) + (1 * 100 + 1 * 10 + 0)
            @test cell_11[2] == (1 * 10 + 1) + (1 * 100 + 1 * 10 + 1)
            @test cell_11[3] == (1 * 10 + 1) + (1 * 100 + 1 * 10 + 2)
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Union dims across operands" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            md_a = make_metadata_full(
                dimensions = ["scenario", "time"],
                dimension_sizes = [2, 4],
                labels = ["v1"],
                time_dimensions = ["time"],
                frequencies = ["monthly"],
            )
            md_b = make_metadata_full(
                dimensions = ["time", "stage"],
                dimension_sizes = [4, 3],
                labels = ["v1"],
                time_dimensions = ["time"],
                frequencies = ["monthly"],
            )

            write_dense(path_a, md_a, [:scenario, :time], [2, 4], 1, (dims, _) -> dims[1] * 10 + dims[2])
            write_dense(path_b, md_b, [:time, :stage], [4, 3], 1, (dims, _) -> dims[1] * 100 + dims[2])

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            sum_expr = Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb)
            Quiver.Expression.save(sum_expr, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            # Output: [scenario=2, time=4, stage=3]. out[s,t,st] = a[s,t] + b[t,st].
            @test read_one_cell(path_out; scenario = 1, time = 1, stage = 1)[1] == (1 * 10 + 1) + (1 * 100 + 1)
            @test read_one_cell(path_out; scenario = 2, time = 4, stage = 2)[1] == (2 * 10 + 4) + (4 * 100 + 2)
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Operand dims in different order" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            md_a = make_metadata_full(dimensions = ["scenario", "time"], dimension_sizes = [2, 4], labels = ["v1"])
            md_b = make_metadata_full(dimensions = ["time", "scenario"], dimension_sizes = [4, 2], labels = ["v1"])

            write_dense(path_a, md_a, [:scenario, :time], [2, 4], 1, (dims, _) -> dims[1] * 10 + dims[2])
            write_dense(path_b, md_b, [:time, :scenario], [4, 2], 1, (dims, _) -> dims[1] * 100 + dims[2])

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            sum_expr = Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb)
            Quiver.Expression.save(sum_expr, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            # out[s,t] = a[s,t] + b[t,s] = (s*10+t) + (t*100+s)
            for (s, t) in [(1, 1), (2, 4), (1, 4), (2, 2)]
                expected = (s * 10 + t) + (t * 100 + s)
                @test read_one_cell(path_out; scenario = s, time = t)[1] == expected
            end
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    # ==========================================================================
    # Time-property mismatches
    # ==========================================================================

    @testset "Time-property mismatch throws" begin
        path_a, path_b = make_path("a"), make_path("b")
        try
            md_a = make_metadata_full(
                dimensions = ["scenario", "block"],
                dimension_sizes = [3, 12],
                time_dimensions = ["block"],
                frequencies = ["monthly"],
            )
            md_b = make_metadata_full(dimensions = ["scenario", "block"], dimension_sizes = [3, 12])
            write_one_cell(path_a, md_a; data = [1.0, 1.0], scenario = 1, block = 1)
            write_one_cell(path_b, md_b; data = [1.0, 1.0], scenario = 1, block = 1)

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

    @testset "Initial datetime mismatch throws" begin
        path_a, path_b = make_path("a"), make_path("b")
        try
            md_a = make_metadata_full(
                dimensions = ["month", "block"],
                dimension_sizes = [4, 31],
                time_dimensions = ["month", "block"],
                frequencies = ["monthly", "daily"],
                initial_datetime = "2025-01-01T00:00:00",
            )
            md_b = make_metadata_full(
                dimensions = ["month", "block"],
                dimension_sizes = [4, 31],
                time_dimensions = ["month", "block"],
                frequencies = ["monthly", "daily"],
                initial_datetime = "2025-02-01T00:00:00",
            )
            write_one_cell(path_a, md_a; data = [1.0, 1.0], month = 1, block = 1)
            write_one_cell(path_b, md_b; data = [1.0, 1.0], month = 1, block = 1)

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

    @testset "Parent dim name mismatch throws" begin
        path_a, path_b = make_path("a"), make_path("b")
        try
            md_a = make_metadata_full(
                dimensions = ["month", "block"],
                dimension_sizes = [2, 31],
                time_dimensions = ["month", "block"],
                frequencies = ["monthly", "daily"],
            )
            md_b = make_metadata_full(
                dimensions = ["stage", "block"],
                dimension_sizes = [2, 31],
                time_dimensions = ["stage", "block"],
                frequencies = ["monthly", "daily"],
            )
            write_one_cell(path_a, md_a; data = [1.0, 1.0], month = 1, block = 1)
            write_one_cell(path_b, md_b; data = [1.0, 1.0], stage = 1, block = 1)

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

    @testset "Parent dim match by name accepts cross position" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            md_a = make_metadata_full(
                dimensions = ["month", "extra", "day"],
                dimension_sizes = [2, 3, 31],
                time_dimensions = ["month", "day"],
                frequencies = ["monthly", "daily"],
            )
            md_b = make_metadata_full(
                dimensions = ["extra", "month", "day"],
                dimension_sizes = [3, 2, 31],
                time_dimensions = ["month", "day"],
                frequencies = ["monthly", "daily"],
            )
            write_one_cell(path_a, md_a; data = [1.0, 1.0], month = 1, extra = 1, day = 1)
            write_one_cell(path_b, md_b; data = [1.0, 1.0], extra = 1, month = 1, day = 1)

            fa = Quiver.Binary.open_file(path_a; mode = :read)
            fb = Quiver.Binary.open_file(path_b; mode = :read)
            sum_expr = Quiver.Expression.Expression(fa) + Quiver.Expression.Expression(fb)
            Quiver.Expression.save(sum_expr, path_out)
            Quiver.Binary.close!(fa)
            Quiver.Binary.close!(fb)

            reopened = Quiver.Binary.open_file(path_out; mode = :read)
            Quiver.Binary.close!(reopened)
            @test isfile(path_out * ".qvr")
        finally
            cleanup(path_a, path_b, path_out)
        end
    end
end

end
