module TestExpression

using Quiver
using Test

function make_path(name)
    return joinpath(tempdir(), "quiver_julia_expr_$(name)")
end

function cleanup(paths...)
    # Operator chains like `(a + b - c) / 2.0` create unnamed intermediate Expression
    # objects whose internal FileNodes hold the input files open until Julia GC runs
    # their finalizers. One GC pass before cleanup is enough to release them.
    GC.gc()
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
    file = Quiver.Binary.open_file(path; mode = 'w', metadata = md)
    for r in 1:rows, c in 1:cols
        Quiver.Binary.write!(file; data = [Float64(fill(r, c, 1)), Float64(fill(r, c, 2))], row = r, col = c)
    end
    Quiver.Binary.close!(file)
    return nothing
end

# Reads all 3 × 2 × 2 cells in (r, c, k) order.
function read_all_cells(path::String; rows::Int = 3, cols::Int = 2)
    file = Quiver.Binary.open_file(path; mode = 'r')
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
    file = Quiver.Binary.open_file(path; mode = 'w', metadata = md)
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
    file = Quiver.Binary.open_file(path; mode = 'w', metadata = md)
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
    file = Quiver.Binary.open_file(path; mode = 'r')
    cell = Quiver.Binary.read(file; allow_nulls = true, dims...)
    Quiver.Binary.close!(file)
    return cell
end

# Open a BinaryFile, build an Expression on it, close the file, run the body
# with the Expression, then close the Expression. The Expression's internal
# ExpressionFile keeps its own handle, so the BinaryFile can close immediately.
# Body is the first positional arg so callers can use `do` syntax.
function with_expr(body::Function, path::String)
    file = Quiver.Binary.open_file(path; mode = 'r')
    e = Quiver.Expression(file)
    Quiver.Binary.close!(file)
    try
        body(e)
    finally
        Quiver.close!(e)
    end
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
            with_expr(path_a) do e
                return Quiver.save(e, path_out)
            end
            @test read_all_cells(path_a) == read_all_cells(path_out)
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
            with_expr(path_a) do e
                Quiver.save(e, path_out)
                return Quiver.save(e, path_out2)
            end
            @test read_all_cells(path_out) == read_all_cells(path_out2)
        finally
            cleanup(path_a, path_out, path_out2)
        end
    end

    # ==========================================================================
    # Binary operators (file operation file)
    # ==========================================================================

    @testset "Add two files" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            write_fixture(path_b, (r, c, k) -> r * 100 + c * 5 + k * 2)
            with_expr(path_a) do a
                with_expr(path_b) do b
                    sum_expr = a + b
                    Quiver.save(sum_expr, path_out)
                    return Quiver.close!(sum_expr)
                end
            end
            va, vb, vo = read_all_cells(path_a), read_all_cells(path_b), read_all_cells(path_out)
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
            with_expr(path_a) do a
                with_expr(path_b) do b
                    diff = a - b
                    Quiver.save(diff, path_out)
                    return Quiver.close!(diff)
                end
            end
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
            with_expr(path_a) do a
                with_expr(path_b) do b
                    product = a * b
                    Quiver.save(product, path_out)
                    return Quiver.close!(product)
                end
            end
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
            with_expr(path_a) do a
                with_expr(path_b) do b
                    quotient = a / b
                    Quiver.save(quotient, path_out)
                    return Quiver.close!(quotient)
                end
            end
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
            with_expr(path_a) do a
                with_expr(path_b) do b
                    with_expr(path_c) do c
                        result = (a + b - c) / 2.0
                        Quiver.save(result, path_out)
                        return Quiver.close!(result)
                    end
                end
            end
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
            with_expr(path_a) do a
                result = a + 2.0
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == read_all_cells(path_a) .+ 2.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: a - 5.0" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            with_expr(path_a) do a
                result = a - 5.0
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == read_all_cells(path_a) .- 5.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: a * 3.0" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do a
                result = a * 3.0
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == read_all_cells(path_a) .* 3.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: a / 4.0" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k + 1)
            with_expr(path_a) do a
                result = a / 4.0
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
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
            with_expr(path_a) do a
                result = 7.0 + a
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == 7.0 .+ read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: 100.0 - a" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            with_expr(path_a) do a
                result = 100.0 - a
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == 100.0 .- read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: 5.0 * a" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k + 1)
            with_expr(path_a) do a
                result = 5.0 * a
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == 5.0 .* read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Scalar broadcast: 60.0 / a" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k + 1)
            with_expr(path_a) do a
                result = 60.0 / a
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    @test_throws Quiver.DatabaseException a + b
                end
            end
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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    @test_throws Quiver.DatabaseException a + b
                end
            end
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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    @test_throws Quiver.DatabaseException a + b
                end
            end
        finally
            cleanup(path_a, path_b)
        end
    end

    @testset "Self-save collision throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 42.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.save(e, path_a)
            end
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
            file = Quiver.Binary.open_file(path_a; mode = 'r')
            e = Quiver.Expression(file)
            Quiver.Binary.close!(file)
            Quiver.close!(e)
            Quiver.close!(e)  # second call is no-op
            @test e.ptr == C_NULL
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
            with_expr(path_a) do e
                md = Quiver.get_metadata(e)
                @test Quiver.Binary.get_unit(md) == "MW"
                @test Quiver.Binary.get_labels(md) == ["val1", "val2"]
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "get_metadata after scalar operation returns broadcast metadata" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do a
                e = a * 2.0
                try
                    md = Quiver.get_metadata(e)
                    @test Quiver.Binary.get_unit(md) == "MW"  # unit preserved through scalar operation
                finally
                    Quiver.close!(e)
                end
            end
        finally
            cleanup(path_a)
        end
    end

    # ==========================================================================
    # Same path twice
    # ==========================================================================

    @testset "Same path twice (a + a, each ExpressionFile opens independently)" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do a1
                with_expr(path_a) do a2
                    sum_expr = a1 + a2
                    Quiver.save(sum_expr, path_out)
                    return Quiver.close!(sum_expr)
                end
            end
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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    sum_expr = a + b
                    Quiver.save(sum_expr, path_out)
                    return Quiver.close!(sum_expr)
                end
            end

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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    sum_expr = a + b
                    Quiver.save(sum_expr, path_out)
                    return Quiver.close!(sum_expr)
                end
            end

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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    sum_expr = a + b
                    Quiver.save(sum_expr, path_out)
                    return Quiver.close!(sum_expr)
                end
            end

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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    sum_expr = a + b
                    Quiver.save(sum_expr, path_out)
                    return Quiver.close!(sum_expr)
                end
            end

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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    @test_throws Quiver.DatabaseException a + b
                end
            end
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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    @test_throws Quiver.DatabaseException a + b
                end
            end
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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    @test_throws Quiver.DatabaseException a + b
                end
            end
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

            with_expr(path_a) do a
                with_expr(path_b) do b
                    sum_expr = a + b
                    Quiver.save(sum_expr, path_out)
                    return Quiver.close!(sum_expr)
                end
            end

            reopened = Quiver.Binary.open_file(path_out; mode = 'r')
            Quiver.Binary.close!(reopened)
            @test isfile(path_out * ".qvr")
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    # ==========================================================================
    # Operators on Binary.File directly (no explicit Expression wrap)
    # ==========================================================================

    @testset "Binary.File + Binary.File round-trip" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            write_fixture(path_b, (r, c, k) -> r * 100 + c * 5 + k * 2)
            a = Quiver.Binary.open_file(path_a; mode = 'r')
            b = Quiver.Binary.open_file(path_b; mode = 'r')
            try
                c = a + b
                Quiver.save(c, path_out)
                Quiver.close!(c)
            finally
                Quiver.Binary.close!(a)
                Quiver.Binary.close!(b)
            end
            @test read_all_cells(path_out) == read_all_cells(path_a) .+ read_all_cells(path_b)
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Binary.File * Real and Real * Binary.File" begin
        path_a, path_out, path_out2 = make_path("a"), make_path("out"), make_path("out2")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            a = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                right = a * 2.5
                Quiver.save(right, path_out)
                Quiver.close!(right)
            finally
                Quiver.Binary.close!(a)
            end

            a2 = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                left = 2.5 * a2
                Quiver.save(left, path_out2)
                Quiver.close!(left)
            finally
                Quiver.Binary.close!(a2)
            end

            src = read_all_cells(path_a)
            @test read_all_cells(path_out) == src .* 2.5
            @test read_all_cells(path_out2) == 2.5 .* src
        finally
            cleanup(path_a, path_out, path_out2)
        end
    end

    @testset "example2.jl pattern: c = a + b * 2" begin
        # Mirrors example2.jl's `c = a + b * 2`. We bind the intermediate `b * 2`
        # to a name so it can be closed explicitly — otherwise on Windows the
        # ExpressionFile held by the anonymous intermediate Expression keeps path_b open
        # until Julia GC runs, blocking cleanup's rm(). Production code (example2.jl)
        # never deletes the files, so this is purely a test-harness concern.
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            write_fixture(path_b, (r, c, k) -> r * 2 + c * 3 + k)
            a = Quiver.Binary.open_file(path_a; mode = 'r')
            b = Quiver.Binary.open_file(path_b; mode = 'r')
            b_times_2 = b * 2
            try
                c = a + b_times_2
                Quiver.save(c, path_out)
                Quiver.close!(c)
            finally
                Quiver.close!(b_times_2)
                Quiver.Binary.close!(a)
                Quiver.Binary.close!(b)
            end
            @test read_all_cells(path_out) == read_all_cells(path_a) .+ read_all_cells(path_b) .* 2
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Mixed Binary.File and Expression" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            write_fixture(path_b, (r, c, k) -> r * 10 + c + k)
            a = Quiver.Binary.open_file(path_a; mode = 'r')
            b = Quiver.Binary.open_file(path_b; mode = 'r')
            try
                b_expr = Quiver.Expression(b)
                try
                    # File + Expression
                    c = a + b_expr
                    Quiver.save(c, path_out)
                    Quiver.close!(c)
                finally
                    Quiver.close!(b_expr)
                end
            finally
                Quiver.Binary.close!(a)
                Quiver.Binary.close!(b)
            end
            @test read_all_cells(path_out) == read_all_cells(path_a) .+ read_all_cells(path_b)
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "save on Expression rooted in write-mode Binary.File throws" begin
        # Expression construction only loads metadata; the read-handle open is deferred
        # to save(), which is where the write_registry collision surfaces.
        path_a, path_out = make_path("a"), make_path("out")
        try
            md = make_simple_metadata()
            w = Quiver.Binary.open_file(path_a; mode = 'w', metadata = md)
            try
                e = Quiver.Expression(w)
                @test_throws Quiver.DatabaseException Quiver.save(e, path_out)
            finally
                Quiver.Binary.close!(w)
            end
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "save on Operator over write-mode Binary.Files throws" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            md = make_simple_metadata()
            wa = Quiver.Binary.open_file(path_a; mode = 'w', metadata = md)
            wb = Quiver.Binary.open_file(path_b; mode = 'w', metadata = md)
            try
                e = wa + wb
                @test_throws Quiver.DatabaseException Quiver.save(e, path_out)
            finally
                Quiver.Binary.close!(wa)
                Quiver.Binary.close!(wb)
            end
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    # ==========================================================================
    # Aggregation: dimension reduction (Quiver.aggregate)
    # ==========================================================================

    @testset "Aggregate sum over non-time dim" begin
        # Note: write_fixture indexes labels as k=1,2 (Julia convention).
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.aggregate(e, "row", "sum")
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Sum over r=1..3 of (10r + col + k) = 60 + 3*col + 3*k.
            @test read_one_cell(path_out; col = 1)[1] == 66.0  # col=1, k=1
            @test read_one_cell(path_out; col = 1)[2] == 69.0  # col=1, k=2
            @test read_one_cell(path_out; col = 2)[1] == 69.0  # col=2, k=1
            @test read_one_cell(path_out; col = 2)[2] == 72.0  # col=2, k=2
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate mean over non-time dim" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.aggregate(e, "row", "mean")
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Mean over r=1..3 of (10r + c + k) = 20 + c + k.
            @test read_one_cell(path_out; col = 1)[1] == 22.0  # k=1
            @test read_one_cell(path_out; col = 2)[2] == 24.0  # k=2
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate min and max" begin
        path_a, path_out, path_out2 = make_path("a"), make_path("out"), make_path("out2")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out_min = Quiver.aggregate(e, "row", "min")
                out_max = Quiver.aggregate(e, "row", "max")
                Quiver.save(out_min, path_out)
                Quiver.save(out_max, path_out2)
                Quiver.close!(out_min)
                return Quiver.close!(out_max)
            end
            # Min at r=1: 10 + c + k. Max at r=3: 30 + c + k.
            @test read_one_cell(path_out; col = 1)[1] == 12.0   # min, c=1, k=1
            @test read_one_cell(path_out2; col = 2)[2] == 34.0  # max, c=2, k=2
        finally
            cleanup(path_a, path_out, path_out2)
        end
    end

    @testset "Aggregate percentile with parameter 0.5" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.aggregate(e, "row", "percentile", 0.5)
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Median = middle row (r=2): 20 + c + k.
            @test read_one_cell(path_out; col = 1)[1] == 22.0
            @test read_one_cell(path_out; col = 2)[2] == 24.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate skips NaN inputs" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r == 2 ? NaN : Float64(r * 10 + c + k))
            with_expr(path_a) do e
                out = Quiver.aggregate(e, "row", "sum")
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Sum = row1 + row3 = (10 + c + k) + (30 + c + k) = 40 + 2c + 2k.
            @test read_one_cell(path_out; col = 1)[1] == 44.0  # c=1, k=1
            @test read_one_cell(path_out; col = 2)[2] == 48.0  # c=2, k=2
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate chained" begin
        # 3-dim fixture so chaining still leaves >= 1 dim (metadata validation requires it).
        path_a, path_out = make_path("a"), make_path("out")
        try
            md = make_metadata_full(
                dimensions = ["row", "col", "depth"],
                dimension_sizes = [3, 2, 2],
                labels = ["v1"],
            )
            write_dense(path_a, md, [:row, :col, :depth], [3, 2, 2], 1, (_, _) -> 2.0)
            with_expr(path_a) do e
                out = Quiver.aggregate(Quiver.aggregate(e, "row", "sum"), "col", "sum")
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Each depth cell = 3 rows × 2 cols × 2.0 = 12.0.
            @test read_one_cell(path_out; depth = 1)[1] == 12.0
            @test read_one_cell(path_out; depth = 2)[1] == 12.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate composed with binary" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            write_fixture(path_b, (r, c, k) -> r + c + k)
            with_expr(path_a) do a
                with_expr(path_b) do b
                    out = Quiver.aggregate(a + b, "row", "sum")
                    Quiver.save(out, path_out)
                    return Quiver.close!(out)
                end
            end
            # (a+b) = 11r + 2c + 2k. Sum over r=1..3 = 66 + 6c + 6k.
            @test read_one_cell(path_out; col = 1)[1] == 78.0  # c=1, k=1: 66+6+6
            @test read_one_cell(path_out; col = 2)[2] == 90.0  # c=2, k=2: 66+12+12
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Aggregate dimension not found throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.aggregate(e, "nonexistent", "sum")
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "Aggregate unknown operation throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.aggregate(e, "row", "average")
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "Aggregate percentile missing parameter throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.aggregate(e, "row", "percentile")
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "Aggregate sum with extra parameter throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.aggregate(e, "row", "sum", 0.5)
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "Aggregate percentile out of range throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.aggregate(e, "row", "percentile", 1.5)
                @test_throws Quiver.DatabaseException Quiver.aggregate(e, "row", "percentile", -0.1)
            end
        finally
            cleanup(path_a)
        end
    end

    # ==========================================================================
    # Aggregation: label-axis reduction (Quiver.aggregate_agents)
    # ==========================================================================

    @testset "Aggregate_agents sum reduces labels" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.aggregate_agents(e, "sum")
                md = Quiver.get_metadata(out)
                @test Quiver.Binary.get_labels(md) == ["sum"]
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Each cell = (10r + c + 1) + (10r + c + 2) = 20r + 2c + 3.
            @test read_one_cell(path_out; row = 1, col = 1) == [25.0]  # 20+2+3
            @test read_one_cell(path_out; row = 3, col = 2) == [67.0]  # 60+4+3
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate_agents mean" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.aggregate_agents(e, "mean")
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Mean of (10r + c + 1, 10r + c + 2) = 10r + c + 1.5.
            @test read_one_cell(path_out; row = 1, col = 1)[1] == 12.5
            @test read_one_cell(path_out; row = 3, col = 2)[1] == 33.5
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate_agents percentile 0.5" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.aggregate_agents(e, "percentile", 0.5)
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Median of two values (a, a+1) = a + 0.5 = 10r + c + 1.5.
            @test read_one_cell(path_out; row = 1, col = 1)[1] == 12.5
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate_agents skips NaN" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            # Mark label k=1 as NaN; sum should fall back to the other label.
            write_fixture(path_a, (r, c, k) -> k == 1 ? NaN : Float64(r * 10 + c + k))
            with_expr(path_a) do e
                out = Quiver.aggregate_agents(e, "sum")
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Only label k=2 contributes: sum = 10r + c + 2.
            @test read_one_cell(path_out; row = 1, col = 1)[1] == 13.0
            @test read_one_cell(path_out; row = 3, col = 2)[1] == 34.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate_agents preserves dimensions" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do e
                out = Quiver.aggregate_agents(e, "mean")
                md = Quiver.get_metadata(out)
                @test Quiver.Binary.get_labels(md) == ["mean"]
                @test Quiver.Binary.get_unit(md) == "MW"
                dims = Quiver.Binary.get_dimensions(md)
                @test length(dims) == 2
                @test dims[1].name == "row"
                @test dims[2].name == "col"
                return Quiver.close!(out)
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "Aggregate_agents chained after aggregate" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (_, _, _) -> 3.0)
            with_expr(path_a) do e
                out = Quiver.aggregate_agents(Quiver.aggregate(e, "row", "sum"), "mean")
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # After reducing row(3) and agents(2): each (col) cell = sum 3 rows × 3.0 = 9.0; mean of 2 same labels = 9.0.
            @test read_one_cell(path_out; col = 1)[1] == 9.0
            @test read_one_cell(path_out; col = 2)[1] == 9.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate on Binary.File shortcut" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            file = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                out = Quiver.aggregate(file, "row", "sum")
                Quiver.save(out, path_out)
                Quiver.close!(out)
            finally
                Quiver.Binary.close!(file)
            end
            # col=1, k=1: 60 + 3 + 3 = 66.
            @test read_one_cell(path_out; col = 1)[1] == 66.0
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Aggregate_agents on Binary.File shortcut" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            file = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                out = Quiver.aggregate_agents(file, "mean")
                Quiver.save(out, path_out)
                Quiver.close!(out)
            finally
                Quiver.Binary.close!(file)
            end
            # Mean of (10*1 + 1 + 1, 10*1 + 1 + 2) = 12.5.
            @test read_one_cell(path_out; row = 1, col = 1)[1] == 12.5
        finally
            cleanup(path_a, path_out)
        end
    end

    # ==========================================================================
    # Unary operators (Negate, Abs, Sqrt, Log, Exp)
    # ==========================================================================

    @testset "Unary negate on Expression" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            with_expr(path_a) do a
                result = -a
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == .-read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary abs on Expression" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            # Alternate sign so abs has work to do.
            write_fixture(path_a, (r, c, k) -> (r % 2 == 0 ? -1 : 1) * (r * 10 + c + k))
            with_expr(path_a) do a
                result = abs(a)
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == abs.(read_all_cells(path_a))
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary sqrt on Expression" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k + 1)  # > 0
            with_expr(path_a) do a
                result = sqrt(a)
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) ≈ sqrt.(read_all_cells(path_a))
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary sqrt propagates NaN on negative" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (_, _, _) -> -1.0)
            with_expr(path_a) do a
                result = sqrt(a)
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            # Read with allow_nulls = true since sqrt(-1) produces NaN cells.
            file = Quiver.Binary.open_file(path_out; mode = 'r')
            try
                for r in 1:3, c in 1:2
                    cell = Quiver.Binary.read(file; allow_nulls = true, row = r, col = c)
                    @test all(isnan, cell)
                end
            finally
                Quiver.Binary.close!(file)
            end
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary log on Expression" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k + 1)  # > 0
            with_expr(path_a) do a
                result = log(a)
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) ≈ log.(read_all_cells(path_a))
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary exp on Expression" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            # Small magnitudes to keep exp() comfortable numerically.
            write_fixture(path_a, (r, c, k) -> (r + c + k) * 0.1)
            with_expr(path_a) do a
                result = exp(a)
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) ≈ exp.(read_all_cells(path_a))
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary metadata preserved" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (_, _, _) -> 1.0)
            with_expr(path_a) do a
                neg = -a
                try
                    md = Quiver.get_metadata(neg)
                    @test Quiver.Binary.get_unit(md) == "MW"
                    @test Quiver.Binary.get_labels(md) == ["val1", "val2"]
                    dims = Quiver.Binary.get_dimensions(md)
                    @test length(dims) == 2
                    @test dims[1].name == "row"
                    @test dims[2].name == "col"
                finally
                    Quiver.close!(neg)
                end
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "Unary composes (abs of negate)" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do a
                result = abs(-a)
                Quiver.save(result, path_out)
                return Quiver.close!(result)
            end
            @test read_all_cells(path_out) == abs.(read_all_cells(path_a))
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary composes with binary: -(a + b)" begin
        path_a, path_b, path_out = make_path("a"), make_path("b"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r + c + k)
            write_fixture(path_b, (r, c, k) -> r * 2 + c + k)
            with_expr(path_a) do a
                with_expr(path_b) do b
                    result = -(a + b)
                    Quiver.save(result, path_out)
                    return Quiver.close!(result)
                end
            end
            @test read_all_cells(path_out) == .-(read_all_cells(path_a) .+ read_all_cells(path_b))
        finally
            cleanup(path_a, path_b, path_out)
        end
    end

    @testset "Unary on Binary.File shortcut" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k + 1)
            file = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                result = sqrt(file)
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(file)
            end
            @test read_all_cells(path_out) ≈ sqrt.(read_all_cells(path_a))
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "Unary negate on Binary.File shortcut" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 100 + c * 10 + k)
            file = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                result = -file
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(file)
            end
            @test read_all_cells(path_out) == .-read_all_cells(path_a)
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "ifelse selects by condition" begin
        path_cond, path_then, path_else, path_out =
            make_path("cond"), make_path("then"), make_path("else"), make_path("out")
        try
            write_fixture(path_cond, (r, _c, _k) -> r == 1 ? 1.0 : 0.0)
            write_fixture(path_then, (_r, _c, _k) -> 10.0)
            write_fixture(path_else, (_r, _c, _k) -> 20.0)
            cond = Quiver.Binary.open_file(path_cond; mode = 'r')
            then_v = Quiver.Binary.open_file(path_then; mode = 'r')
            else_v = Quiver.Binary.open_file(path_else; mode = 'r')
            try
                result = ifelse(Quiver.Expression(cond), Quiver.Expression(then_v), Quiver.Expression(else_v))
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(cond)
                Quiver.Binary.close!(then_v)
                Quiver.Binary.close!(else_v)
            end
            vc = read_all_cells(path_cond)
            vo = read_all_cells(path_out)
            for i in eachindex(vo)
                @test vo[i] == (vc[i] != 0.0 ? 10.0 : 20.0)
            end
        finally
            cleanup(path_cond, path_then, path_else, path_out)
        end
    end

    @testset "ifelse propagates NaN in condition" begin
        path_cond, path_then, path_else, path_out =
            make_path("cond"), make_path("then"), make_path("else"), make_path("out")
        try
            write_fixture(path_cond, (r, c, _k) -> (r == 1 && c == 1) ? NaN : 1.0)
            write_fixture(path_then, (_r, _c, _k) -> 7.0)
            write_fixture(path_else, (_r, _c, _k) -> -7.0)
            cond = Quiver.Binary.open_file(path_cond; mode = 'r')
            then_v = Quiver.Binary.open_file(path_then; mode = 'r')
            else_v = Quiver.Binary.open_file(path_else; mode = 'r')
            try
                result = ifelse(Quiver.Expression(cond), Quiver.Expression(then_v), Quiver.Expression(else_v))
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(cond)
                Quiver.Binary.close!(then_v)
                Quiver.Binary.close!(else_v)
            end
            file = Quiver.Binary.open_file(path_out; mode = 'r')
            cell_11 = Quiver.Binary.read(file; allow_nulls = true, row = 1, col = 1)
            cell_22 = Quiver.Binary.read(file; row = 2, col = 2)
            Quiver.Binary.close!(file)
            @test all(isnan, cell_11)
            @test cell_22 == [7.0, 7.0]
        finally
            cleanup(path_cond, path_then, path_else, path_out)
        end
    end

    @testset "ifelse unselected-branch NaN does not propagate" begin
        path_cond, path_then, path_else, path_out =
            make_path("cond"), make_path("then"), make_path("else"), make_path("out")
        try
            write_fixture(path_cond, (_r, _c, _k) -> 1.0)
            write_fixture(path_then, (_r, _c, _k) -> 42.0)
            write_fixture(path_else, (_r, _c, _k) -> NaN)
            cond = Quiver.Binary.open_file(path_cond; mode = 'r')
            then_v = Quiver.Binary.open_file(path_then; mode = 'r')
            else_v = Quiver.Binary.open_file(path_else; mode = 'r')
            try
                result = ifelse(Quiver.Expression(cond), Quiver.Expression(then_v), Quiver.Expression(else_v))
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(cond)
                Quiver.Binary.close!(then_v)
                Quiver.Binary.close!(else_v)
            end
            @test all(v -> v == 42.0, read_all_cells(path_out))
        finally
            cleanup(path_cond, path_then, path_else, path_out)
        end
    end

    @testset "ifelse condition unit ignored" begin
        path_cond, path_then, path_else, path_out =
            make_path("cond"), make_path("then"), make_path("else"), make_path("out")
        try
            md_cond = make_metadata(; unit = "flag")  # cond.unit = flag
            md_branch = make_metadata(; unit = "MW")  # then/else MW
            write_fixture_with_metadata(path_cond, md_cond, (_r, _c, _k) -> 1.0)
            write_fixture_with_metadata(path_then, md_branch, (_r, _c, _k) -> 10.0)
            write_fixture_with_metadata(path_else, md_branch, (_r, _c, _k) -> 20.0)
            cond = Quiver.Binary.open_file(path_cond; mode = 'r')
            then_v = Quiver.Binary.open_file(path_then; mode = 'r')
            else_v = Quiver.Binary.open_file(path_else; mode = 'r')
            try
                result = ifelse(Quiver.Expression(cond), Quiver.Expression(then_v), Quiver.Expression(else_v))
                meta = Quiver.get_metadata(result)
                @test Quiver.Binary.get_unit(meta) == "MW"
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(cond)
                Quiver.Binary.close!(then_v)
                Quiver.Binary.close!(else_v)
            end
            @test all(v -> v == 10.0, read_all_cells(path_out))
        finally
            cleanup(path_cond, path_then, path_else, path_out)
        end
    end

    @testset "ifelse unit mismatch between then and else throws" begin
        path_cond, path_then, path_else = make_path("cond"), make_path("then"), make_path("else")
        try
            md = make_metadata(; unit = "MW")
            md_other = make_metadata(; unit = "kWh")
            write_fixture_with_metadata(path_cond, md, (_r, _c, _k) -> 1.0)
            write_fixture_with_metadata(path_then, md, (_r, _c, _k) -> 1.0)
            write_fixture_with_metadata(path_else, md_other, (_r, _c, _k) -> 1.0)
            cond = Quiver.Binary.open_file(path_cond; mode = 'r')
            then_v = Quiver.Binary.open_file(path_then; mode = 'r')
            else_v = Quiver.Binary.open_file(path_else; mode = 'r')
            try
                @test_throws Quiver.DatabaseException ifelse(
                    Quiver.Expression(cond), Quiver.Expression(then_v), Quiver.Expression(else_v),
                )
            finally
                Quiver.Binary.close!(cond)
                Quiver.Binary.close!(then_v)
                Quiver.Binary.close!(else_v)
            end
        finally
            cleanup(path_cond, path_then, path_else)
        end
    end

    @testset "ifelse on Binary.File shortcuts" begin
        path_cond, path_then, path_else, path_out =
            make_path("cond"), make_path("then"), make_path("else"), make_path("out")
        try
            write_fixture(path_cond, (r, _c, _k) -> r == 1 ? 1.0 : 0.0)
            write_fixture(path_then, (_r, _c, _k) -> 100.0)
            write_fixture(path_else, (_r, _c, _k) -> 200.0)
            cond = Quiver.Binary.open_file(path_cond; mode = 'r')
            then_v = Quiver.Binary.open_file(path_then; mode = 'r')
            else_v = Quiver.Binary.open_file(path_else; mode = 'r')
            try
                # All three as Binary.File
                result = ifelse(cond, then_v, else_v)
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(cond)
                Quiver.Binary.close!(then_v)
                Quiver.Binary.close!(else_v)
            end
            vc = read_all_cells(path_cond)
            vo = read_all_cells(path_out)
            for i in eachindex(vo)
                @test vo[i] == (vc[i] != 0.0 ? 100.0 : 200.0)
            end
        finally
            cleanup(path_cond, path_then, path_else, path_out)
        end
    end

    @testset "ifelse chains with binary ops" begin
        path_cond, path_then, path_else, path_out =
            make_path("cond"), make_path("then"), make_path("else"), make_path("out")
        try
            write_fixture(path_cond, (r, _c, _k) -> r == 1 ? 1.0 : 0.0)
            write_fixture(path_then, (_r, _c, _k) -> 10.0)
            write_fixture(path_else, (_r, _c, _k) -> 20.0)
            cond = Quiver.Binary.open_file(path_cond; mode = 'r')
            then_v = Quiver.Binary.open_file(path_then; mode = 'r')
            else_v = Quiver.Binary.open_file(path_else; mode = 'r')
            try
                # 2 * ifelse(cond, then, else) + 1
                result =
                    2.0 * ifelse(Quiver.Expression(cond), Quiver.Expression(then_v), Quiver.Expression(else_v)) + 1.0
                Quiver.save(result, path_out)
                Quiver.close!(result)
            finally
                Quiver.Binary.close!(cond)
                Quiver.Binary.close!(then_v)
                Quiver.Binary.close!(else_v)
            end
            vc = read_all_cells(path_cond)
            vo = read_all_cells(path_out)
            for i in eachindex(vo)
                base = vc[i] != 0.0 ? 10.0 : 20.0
                @test vo[i] == 2.0 * base + 1.0
            end
        finally
            cleanup(path_cond, path_then, path_else, path_out)
        end
    end

    # =========================================================================
    # select_agents — label-axis projection
    # =========================================================================

    @testset "select_agents subset" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.select_agents(e, ["val2"])
                md = Quiver.get_metadata(out)
                @test Quiver.Binary.get_labels(md) == ["val2"]
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # val2 is k=2 → cell = 10r + c + 2.
            @test read_one_cell(path_out; row = 1, col = 1) == [13.0]
            @test read_one_cell(path_out; row = 3, col = 2) == [34.0]
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "select_agents reorder" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.select_agents(e, ["val2", "val1"])
                md = Quiver.get_metadata(out)
                @test Quiver.Binary.get_labels(md) == ["val2", "val1"]
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # First entry is val2 (k=2 → 10r+c+2), second is val1 (k=1 → 10r+c+1).
            @test read_one_cell(path_out; row = 1, col = 1) == [13.0, 12.0]
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "select_agents missing label throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (r, c, k) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.select_agents(e, ["nope"])
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "select_agents on Binary.File shortcut" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            file = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                out = Quiver.select_agents(file, ["val1"])
                Quiver.save(out, path_out)
                Quiver.close!(out)
            finally
                Quiver.Binary.close!(file)
            end
            @test read_one_cell(path_out; row = 1, col = 1) == [12.0]
        finally
            cleanup(path_a, path_out)
        end
    end

    # =========================================================================
    # rename_agents — label-axis rename
    # =========================================================================

    @testset "rename_agents partial mapping" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> r * 10 + c + k)
            with_expr(path_a) do e
                out = Quiver.rename_agents(e, Dict("val1" => "alpha"))
                md = Quiver.get_metadata(out)
                @test Quiver.Binary.get_labels(md) == ["alpha", "val2"]
                Quiver.save(out, path_out)
                return Quiver.close!(out)
            end
            # Values unchanged; both labels still present in (val1->alpha, val2) order.
            @test read_one_cell(path_out; row = 1, col = 1) == [12.0, 13.0]
        finally
            cleanup(path_a, path_out)
        end
    end

    @testset "rename_agents all labels" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (r, c, k) -> 1.0)
            with_expr(path_a) do e
                out = Quiver.rename_agents(e, Dict("val1" => "alpha", "val2" => "beta"))
                md = Quiver.get_metadata(out)
                labels = Quiver.Binary.get_labels(md)
                @test sort(labels) == ["alpha", "beta"]
                return Quiver.close!(out)
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "rename_agents missing key throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (r, c, k) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.rename_agents(e, Dict("nope" => "x"))
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "rename_agents collision throws" begin
        path_a = make_path("a")
        try
            write_fixture(path_a, (r, c, k) -> 1.0)
            with_expr(path_a) do e
                @test_throws Quiver.DatabaseException Quiver.rename_agents(e, Dict("val1" => "val2"))
            end
        finally
            cleanup(path_a)
        end
    end

    @testset "rename_agents on Binary.File shortcut" begin
        path_a, path_out = make_path("a"), make_path("out")
        try
            write_fixture(path_a, (r, c, k) -> 1.0)
            file = Quiver.Binary.open_file(path_a; mode = 'r')
            try
                out = Quiver.rename_agents(file, Dict("val1" => "alpha"))
                Quiver.save(out, path_out)
                Quiver.close!(out)
            finally
                Quiver.Binary.close!(file)
            end
            reopened = Quiver.Binary.open_file(path_out; mode = 'r')
            try
                md = Quiver.Binary.get_metadata(reopened)
                @test Quiver.Binary.get_labels(md) == ["alpha", "val2"]
            finally
                Quiver.Binary.close!(reopened)
            end
        finally
            cleanup(path_a, path_out)
        end
    end
end

end
