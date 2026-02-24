using Quiver

# db_path = joinpath(@__DIR__, "debug1.db")
db_path = ":memory:"
# schema_path = raw"C:\Development\DatabaseTmp\quiver\tests\schemas\valid\collections.sql"
schema_path = joinpath(@__DIR__, "debug1.sql")
collection = "Collection"

# rm(db_path, force = true)

db = Quiver.from_schema(db_path, schema_path)

for group in Quiver.list_time_series_groups(db, collection)
    # @show group
end

id = Quiver.create_element!(
    db,
    collection;
    label="Item 1",
    date_time=["2024-01-01T10:00:00", "2024-01-01T11:00:00", "2024-01-01T12:00:00"],
    some_vector1=Float64[10.0, 11.0, 12.0],
    some_vector2 = Float64[2.0, 2.0, 2.0],
)

@show rows = Quiver.read_time_series_group(db, "Collection", "group1", id)

Quiver.update_time_series_group!(
    db,
    collection,
    "group1",
    id,
    [
        Dict{String,Any}("date_time" => "2024-01-01T07:00:00", "value" => 7.0),
        Dict{String,Any}("date_time" => "2024-01-01T08:00:00", "value" => 8.0),
        Dict{String,Any}("date_time" => "2024-01-01T09:00:00", "value" => 9.0),
    ],
)

@show rows = Quiver.read_time_series_group(db, "Collection", "group1", id)

Quiver.close!(db)