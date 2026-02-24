using Quiver

db_path = ":memory:"
schema_path = joinpath(@__DIR__, "debug1.sql")
collection = "Collection"

db = Quiver.from_schema(db_path, schema_path)

# Algumas regras
# 1 - Dentro de um grupo todas as datas tem que ser passadas e os vetores de um mesmo grupo precisam ter o mesmo tamanho.
# 2 - As séries temporais de grupos diferentes não precisam estar indexadas na mesma data.

# Alternativa 1 - Time series são criadas na hora do create element
id = Quiver.create_element!(
    db,
    collection;
    label="Item 1",
    group1 = [
        Dict(:date_time => "2024-01-01T00:00:00", :some_vector1 => 10.0, :some_vector3 => 1),
        Dict(:date_time => "2024-02-01T00:00:00", :some_vector1 => 11.0, :some_vector3 => null / missing),
        Dict(:date_time => "2024-03-01T00:00:00", :some_vector1 => 12.0, :some_vector3 => 2),
    ],
    group2 = [
        Dict(:date_time => "2024-01-01T00:00:00", :some_vector2 => 2.0),
    ]
)

# Alternativa 2 - Time series são criadas depois do elemento ser criado
id = Quiver.create_element!(
    db,
    collection;
    label="Item 1",
)

Quiver.update_time_series!(
    db,
    collection,
    id,
    "group1",
    [
        Dict(:date_time => "2024-01-01T00:00:00", :some_vector1 => 10.0, :some_vector3 => 1),
        Dict(:date_time => "2024-02-01T00:00:00", :some_vector1 => 11.0, :some_vector3 => missing),
        Dict(:date_time => "2024-03-01T00:00:00", :some_vector1 => 12.0, :some_vector3 => 2),
    ]
)

Quiver.update_time_series!(
    db,
    collection,
    id,
    "group2",
    [
        Dict(:date_time => "2024-01-01T00:00:00", :some_vector2 => 2.0),
    ]
)