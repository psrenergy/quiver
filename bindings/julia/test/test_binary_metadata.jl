module TestMetadata

using Quiver
using Test

include("fixture.jl")

function make_valid_toml()
    return """
        version = "1"
        dimensions = ["stage", "block"]
        dimension_sizes = [4, 31]
        time_dimensions = ["stage", "block"]
        frequencies = ["monthly", "daily"]
        initial_datetime = "2025-01-01T00:00:00"
        unit = "MW"
        labels = ["plant_1", "plant_2"]
        """
end

@testset "Metadata" begin
    # ==========================================================================
    # from_toml
    # ==========================================================================

    @testset "from_toml all fields populated" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        @test Quiver.Binary.get_version(md) == "1"
        @test Quiver.Binary.get_unit(md) == "MW"
        @test Quiver.Binary.get_labels(md) == ["plant_1", "plant_2"]

        dims = Quiver.Binary.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "stage"
        @test dims[1].size == 4
        @test dims[2].name == "block"
        @test dims[2].size == 31
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "from_toml time dimensions marked" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].is_time_dimension == true
        @test dims[2].is_time_dimension == true
    end

    @testset "from_toml frequencies assigned" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].frequency == "monthly"
        @test dims[2].frequency == "daily"
    end

    @testset "from_toml parent indices set" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].parent_dimension_index == -1
        @test dims[2].parent_dimension_index == 0
    end

    @testset "from_toml initial values Jan 1st" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].initial_value == 1
        @test dims[2].initial_value == 1
    end

    @testset "from_toml initial values non-Jan 1st" begin
        toml = """
            version = "1"
            dimensions = ["stage", "block"]
            dimension_sizes = [4, 31]
            time_dimensions = ["stage", "block"]
            frequencies = ["monthly", "daily"]
            initial_datetime = "2025-03-15T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        md = Quiver.Binary.from_toml(toml)
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].initial_value == 1
        @test dims[2].initial_value == 15
    end

    @testset "from_toml initial values hourly offset" begin
        toml = """
            version = "1"
            dimensions = ["month", "day", "hour"]
            dimension_sizes = [12, 31, 24]
            time_dimensions = ["month", "day", "hour"]
            frequencies = ["monthly", "daily", "hourly"]
            initial_datetime = "2025-01-01T10:00:00"
            unit = "MW"
            labels = ["val"]
            """
        md = Quiver.Binary.from_toml(toml)
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].initial_value == 1
        @test dims[2].initial_value == 1
        @test dims[3].initial_value == 11  # hour 10 -> 10+1=11
    end

    @testset "from_toml mixed time and non-time" begin
        toml = """
            version = "1"
            dimensions = ["stage", "scenario", "block"]
            dimension_sizes = [4, 3, 31]
            time_dimensions = ["stage", "block"]
            frequencies = ["monthly", "daily"]
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        md = Quiver.Binary.from_toml(toml)
        dims = Quiver.Binary.get_dimensions(md)
        @test length(dims) == 3
        @test dims[1].is_time_dimension == true
        @test dims[2].is_time_dimension == false
        @test dims[3].is_time_dimension == true
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "from_toml error time dimension not in dimensions" begin
        toml = """
            version = "1"
            dimensions = ["stage", "block"]
            dimension_sizes = [4, 31]
            time_dimensions = ["stage", "nonexistent"]
            frequencies = ["monthly", "daily"]
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "from_toml error time dimensions out of order" begin
        toml = """
            version = "1"
            dimensions = ["stage", "block"]
            dimension_sizes = [4, 31]
            time_dimensions = ["block", "stage"]
            frequencies = ["daily", "monthly"]
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "from_toml no time dimensions" begin
        toml = """
            version = "1"
            dimensions = ["row", "col"]
            dimension_sizes = [3, 2]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        md = Quiver.Binary.from_toml(toml)
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 0
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].is_time_dimension == false
        @test dims[2].is_time_dimension == false
    end

    # ==========================================================================
    # Builder constructor
    # ==========================================================================

    @testset "Builder constructor without time dimensions" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            version = "1",
            labels = ["val1", "val2"],
            dimensions = ["row", "col"],
            dimension_sizes = Int64[3, 2],
        )

        @test Quiver.Binary.get_unit(md) == "MW"
        @test Quiver.Binary.get_version(md) == "1"
        @test Quiver.Binary.get_labels(md) == ["val1", "val2"]

        dims = Quiver.Binary.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "row"
        @test dims[1].size == 3
        @test dims[1].is_time_dimension == false
        @test dims[2].name == "col"
        @test dims[2].size == 2
    end

    @testset "Builder constructor with time dimensions" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            version = "1",
            labels = ["plant_1", "plant_2", "plant_3"],
            dimensions = ["stage", "block"],
            dimension_sizes = Int64[4, 31],
            time_dimensions = ["stage", "block"],
            frequencies = ["monthly", "daily"],
        )

        @test Quiver.Binary.get_unit(md) == "MW"
        @test Quiver.Binary.get_labels(md) == ["plant_1", "plant_2", "plant_3"]
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2

        dims = Quiver.Binary.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "stage"
        @test dims[1].size == 4
        @test dims[1].is_time_dimension == true
        @test dims[1].frequency == "monthly"
        @test dims[2].name == "block"
        @test dims[2].size == 31
        @test dims[2].is_time_dimension == true
        @test dims[2].frequency == "daily"
    end

    @testset "Builder constructor mixed time and non-time" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            labels = ["val"],
            dimensions = ["stage", "scenario", "block"],
            dimension_sizes = Int64[4, 3, 31],
            time_dimensions = ["stage", "block"],
            frequencies = ["monthly", "daily"],
        )

        dims = Quiver.Binary.get_dimensions(md)
        @test length(dims) == 3
        @test dims[1].is_time_dimension == true
        @test dims[2].is_time_dimension == false
        @test dims[3].is_time_dimension == true
    end

    # ==========================================================================
    # from_element
    # ==========================================================================

    @testset "from_element valid with time dimensions" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["stage", "block"]
        el["dimension_sizes"] = Int64[4, 31]
        el["time_dimensions"] = ["stage", "block"]
        el["frequencies"] = ["monthly", "daily"]
        el["labels"] = ["plant_1", "plant_2"]

        md = Quiver.Binary.from_element(el)
        @test Quiver.Binary.get_unit(md) == "MW"
        @test Quiver.Binary.get_version(md) == "1"
        @test Quiver.Binary.get_labels(md) == ["plant_1", "plant_2"]
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2

        dims = Quiver.Binary.get_dimensions(md)
        @test length(dims) == 2
        @test dims[1].name == "stage"
        @test dims[1].size == 4
        @test dims[1].is_time_dimension == true
        @test dims[1].frequency == "monthly"
        @test dims[1].parent_dimension_index == -1
        @test dims[2].name == "block"
        @test dims[2].size == 31
        @test dims[2].is_time_dimension == true
        @test dims[2].frequency == "daily"
        @test dims[2].parent_dimension_index == 0
    end

    @testset "from_element valid without time dimensions" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["row", "col"]
        el["dimension_sizes"] = Int64[3, 2]
        el["labels"] = ["val"]

        md = Quiver.Binary.from_element(el)
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 0
        dims = Quiver.Binary.get_dimensions(md)
        @test dims[1].is_time_dimension == false
        @test dims[2].is_time_dimension == false
    end

    @testset "from_element round-trip equivalence with toml" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["stage", "block"]
        el["dimension_sizes"] = Int64[4, 31]
        el["time_dimensions"] = ["stage", "block"]
        el["frequencies"] = ["monthly", "daily"]
        el["labels"] = ["plant_1", "plant_2"]

        md_element = Quiver.Binary.from_element(el)
        md_toml = Quiver.Binary.from_toml(make_valid_toml())

        @test Quiver.Binary.get_version(md_element) == Quiver.Binary.get_version(md_toml)
        @test Quiver.Binary.get_unit(md_element) == Quiver.Binary.get_unit(md_toml)
        @test Quiver.Binary.get_labels(md_element) == Quiver.Binary.get_labels(md_toml)
        @test Quiver.Binary.get_number_of_time_dimensions(md_element) ==
              Quiver.Binary.get_number_of_time_dimensions(md_toml)

        dims_el = Quiver.Binary.get_dimensions(md_element)
        dims_toml = Quiver.Binary.get_dimensions(md_toml)
        @test length(dims_el) == length(dims_toml)
        for i in eachindex(dims_el)
            @test dims_el[i].name == dims_toml[i].name
            @test dims_el[i].size == dims_toml[i].size
            @test dims_el[i].is_time_dimension == dims_toml[i].is_time_dimension
            if dims_el[i].is_time_dimension
                @test dims_el[i].frequency == dims_toml[i].frequency
                @test dims_el[i].initial_value == dims_toml[i].initial_value
                @test dims_el[i].parent_dimension_index == dims_toml[i].parent_dimension_index
            end
        end
    end

    # ==========================================================================
    # from_element -- Error cases (missing required fields)
    # ==========================================================================

    @testset "from_element missing version" begin
        el = Quiver.Element()
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["row"]
        el["dimension_sizes"] = Int64[3]
        el["labels"] = ["val"]
        @test_throws Quiver.DatabaseException Quiver.Binary.from_element(el)
    end

    @testset "from_element missing unit" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["dimensions"] = ["row"]
        el["dimension_sizes"] = Int64[3]
        el["labels"] = ["val"]
        @test_throws Quiver.DatabaseException Quiver.Binary.from_element(el)
    end

    @testset "from_element missing initial_datetime" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["unit"] = "MW"
        el["dimensions"] = ["row"]
        el["dimension_sizes"] = Int64[3]
        el["labels"] = ["val"]
        @test_throws Quiver.DatabaseException Quiver.Binary.from_element(el)
    end

    @testset "from_element missing dimensions" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimension_sizes"] = Int64[3]
        el["labels"] = ["val"]
        @test_throws Quiver.DatabaseException Quiver.Binary.from_element(el)
    end

    @testset "from_element missing dimension_sizes" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["row"]
        el["labels"] = ["val"]
        @test_throws Quiver.DatabaseException Quiver.Binary.from_element(el)
    end

    @testset "from_element missing labels" begin
        el = Quiver.Element()
        el["version"] = "1"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["row"]
        el["dimension_sizes"] = Int64[3]
        @test_throws Quiver.DatabaseException Quiver.Binary.from_element(el)
    end

    @testset "from_element invalid version propagates validation" begin
        el = Quiver.Element()
        el["version"] = "2"
        el["initial_datetime"] = "2025-01-01T00:00:00"
        el["unit"] = "MW"
        el["dimensions"] = ["row"]
        el["dimension_sizes"] = Int64[3]
        el["labels"] = ["val"]
        @test_throws Quiver.DatabaseException Quiver.Binary.from_element(el)
    end

    # ==========================================================================
    # to_toml
    # ==========================================================================

    @testset "to_toml round-trip" begin
        md1 = Quiver.Binary.from_toml(make_valid_toml())
        toml_str = Quiver.Binary.to_toml(md1)
        md2 = Quiver.Binary.from_toml(toml_str)

        @test Quiver.Binary.get_version(md1) == Quiver.Binary.get_version(md2)
        @test Quiver.Binary.get_unit(md1) == Quiver.Binary.get_unit(md2)
        @test Quiver.Binary.get_labels(md1) == Quiver.Binary.get_labels(md2)

        dims1 = Quiver.Binary.get_dimensions(md1)
        dims2 = Quiver.Binary.get_dimensions(md2)
        @test length(dims1) == length(dims2)
        for i in eachindex(dims1)
            @test dims1[i].name == dims2[i].name
            @test dims1[i].size == dims2[i].size
        end
    end

    @testset "to_toml output contains all keys" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        toml_str = Quiver.Binary.to_toml(md)

        @test occursin("version", toml_str)
        @test occursin("dimensions", toml_str)
        @test occursin("dimension_sizes", toml_str)
        @test occursin("time_dimensions", toml_str)
        @test occursin("frequencies", toml_str)
        @test occursin("initial_datetime", toml_str)
        @test occursin("unit", toml_str)
        @test occursin("labels", toml_str)
    end

    @testset "to_toml ISO8601 datetime format" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        toml_str = Quiver.Binary.to_toml(md)
        @test occursin("2025-01-01T00:00:00", toml_str)
    end

    @testset "Get initial datetime" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            labels = ["val1"],
            dimensions = ["row"],
            dimension_sizes = Int64[3],
        )

        dt = Quiver.Binary.get_initial_datetime(md)
        @test occursin("2025-01-01", dt)
    end

    # ==========================================================================
    # Validation errors (via from_toml / constructor)
    # ==========================================================================

    @testset "Validation: wrong version" begin
        toml = """
            version = "2"
            dimensions = ["row"]
            dimension_sizes = [3]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Validation: empty version" begin
        toml = """
            version = ""
            dimensions = ["row"]
            dimension_sizes = [3]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Validation: empty dimensions" begin
        toml = """
            version = "1"
            dimensions = []
            dimension_sizes = []
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Validation: empty labels" begin
        toml = """
            version = "1"
            dimensions = ["row"]
            dimension_sizes = [3]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = []
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Validation: zero dimension size" begin
        toml = """
            version = "1"
            dimensions = ["row"]
            dimension_sizes = [0]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Validation: negative dimension size" begin
        toml = """
            version = "1"
            dimensions = ["row"]
            dimension_sizes = [-1]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Validation: duplicate dimension names" begin
        toml = """
            version = "1"
            dimensions = ["row", "row"]
            dimension_sizes = [3, 2]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Validation: duplicate labels" begin
        toml = """
            version = "1"
            dimensions = ["row"]
            dimension_sizes = [3]
            time_dimensions = []
            frequencies = []
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val", "val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    # ==========================================================================
    # Time dimension validation (via from_toml)
    # ==========================================================================

    @testset "Time validation: duplicate frequencies" begin
        toml = """
            version = "1"
            dimensions = ["stage", "block"]
            dimension_sizes = [12, 31]
            time_dimensions = ["stage", "block"]
            frequencies = ["monthly", "monthly"]
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Time validation: wrong frequency order" begin
        toml = """
            version = "1"
            dimensions = ["block", "stage"]
            dimension_sizes = [31, 12]
            time_dimensions = ["block", "stage"]
            frequencies = ["daily", "monthly"]
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        @test_throws Quiver.DatabaseException Quiver.Binary.from_toml(toml)
    end

    @testset "Time validation: valid monthly daily" begin
        md = Quiver.Binary.from_toml(make_valid_toml())
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Time validation: valid yearly monthly daily hourly" begin
        toml = """
            version = "1"
            dimensions = ["year", "month", "day", "hour"]
            dimension_sizes = [3, 12, 31, 24]
            time_dimensions = ["year", "month", "day", "hour"]
            frequencies = ["yearly", "monthly", "daily", "hourly"]
            initial_datetime = "2025-01-01T00:00:00"
            unit = "MW"
            labels = ["val"]
            """
        md = Quiver.Binary.from_toml(toml)
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 4
    end

    # ==========================================================================
    # Time dimension size validation (via from_toml)
    # ==========================================================================

    # --- Hourly under Daily ---
    @testset "Size: hourly under daily valid" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["daily", "hourly"], dimension_sizes = Int64[31, 24],
            time_dimensions = ["daily", "hourly"], frequencies = ["daily", "hourly"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: hourly under daily below min" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["daily", "hourly"], dimension_sizes = Int64[31, 23],
            time_dimensions = ["daily", "hourly"], frequencies = ["daily", "hourly"],
        )
    end

    @testset "Size: hourly under daily above max" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["daily", "hourly"], dimension_sizes = Int64[31, 25],
            time_dimensions = ["daily", "hourly"], frequencies = ["daily", "hourly"],
        )
    end

    # --- Hourly under Monthly ---
    @testset "Size: hourly under monthly valid min" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "hourly"], dimension_sizes = Int64[12, 672],
            time_dimensions = ["monthly", "hourly"], frequencies = ["monthly", "hourly"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: hourly under monthly valid max" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "hourly"], dimension_sizes = Int64[12, 744],
            time_dimensions = ["monthly", "hourly"], frequencies = ["monthly", "hourly"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: hourly under monthly below min" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "hourly"], dimension_sizes = Int64[12, 671],
            time_dimensions = ["monthly", "hourly"], frequencies = ["monthly", "hourly"],
        )
    end

    @testset "Size: hourly under monthly above max" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "hourly"], dimension_sizes = Int64[12, 745],
            time_dimensions = ["monthly", "hourly"], frequencies = ["monthly", "hourly"],
        )
    end

    # --- Hourly under Yearly ---
    @testset "Size: hourly under yearly valid min" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "hourly"], dimension_sizes = Int64[3, 8760],
            time_dimensions = ["yearly", "hourly"], frequencies = ["yearly", "hourly"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: hourly under yearly valid max" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "hourly"], dimension_sizes = Int64[3, 8784],
            time_dimensions = ["yearly", "hourly"], frequencies = ["yearly", "hourly"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: hourly under yearly below min" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "hourly"], dimension_sizes = Int64[3, 8759],
            time_dimensions = ["yearly", "hourly"], frequencies = ["yearly", "hourly"],
        )
    end

    @testset "Size: hourly under yearly above max" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "hourly"], dimension_sizes = Int64[3, 8785],
            time_dimensions = ["yearly", "hourly"], frequencies = ["yearly", "hourly"],
        )
    end

    # --- Daily under Monthly ---
    @testset "Size: daily under monthly valid min" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "daily"], dimension_sizes = Int64[12, 28],
            time_dimensions = ["monthly", "daily"], frequencies = ["monthly", "daily"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: daily under monthly valid max" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "daily"], dimension_sizes = Int64[12, 31],
            time_dimensions = ["monthly", "daily"], frequencies = ["monthly", "daily"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: daily under monthly below min" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "daily"], dimension_sizes = Int64[12, 27],
            time_dimensions = ["monthly", "daily"], frequencies = ["monthly", "daily"],
        )
    end

    @testset "Size: daily under monthly above max" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["monthly", "daily"], dimension_sizes = Int64[12, 32],
            time_dimensions = ["monthly", "daily"], frequencies = ["monthly", "daily"],
        )
    end

    # --- Daily under Yearly ---
    @testset "Size: daily under yearly valid min" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "daily"], dimension_sizes = Int64[3, 365],
            time_dimensions = ["yearly", "daily"], frequencies = ["yearly", "daily"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: daily under yearly valid max" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "daily"], dimension_sizes = Int64[3, 366],
            time_dimensions = ["yearly", "daily"], frequencies = ["yearly", "daily"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: daily under yearly below min" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "daily"], dimension_sizes = Int64[3, 364],
            time_dimensions = ["yearly", "daily"], frequencies = ["yearly", "daily"],
        )
    end

    @testset "Size: daily under yearly above max" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "daily"], dimension_sizes = Int64[3, 367],
            time_dimensions = ["yearly", "daily"], frequencies = ["yearly", "daily"],
        )
    end

    # --- Monthly under Yearly ---
    @testset "Size: monthly under yearly valid" begin
        md = Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "monthly"], dimension_sizes = Int64[3, 12],
            time_dimensions = ["yearly", "monthly"], frequencies = ["yearly", "monthly"],
        )
        @test Quiver.Binary.get_number_of_time_dimensions(md) == 2
    end

    @testset "Size: monthly under yearly below min" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "monthly"], dimension_sizes = Int64[3, 11],
            time_dimensions = ["yearly", "monthly"], frequencies = ["yearly", "monthly"],
        )
    end

    @testset "Size: monthly under yearly above max" begin
        @test_throws Quiver.DatabaseException Quiver.Binary.Metadata(;
            initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = ["val"],
            dimensions = ["yearly", "monthly"], dimension_sizes = Int64[3, 13],
            time_dimensions = ["yearly", "monthly"], frequencies = ["yearly", "monthly"],
        )
    end
end

end
