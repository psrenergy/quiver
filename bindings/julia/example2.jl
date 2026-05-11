using Quiver

data = [
    1.0 2.0;
    2.0 4.0;
    3.0 8.0
]

@show rows = size(data, 1)

md = Quiver.Binary.Metadata(;
    initial_datetime = "2025-01-01T00:00:00",
    unit = "MW",
    labels = ["val1", "val2"],
    dimensions = ["row"],
    dimension_sizes = Int64[rows],
)

a = Quiver.Binary.open_file("a"; mode = 'w', metadata = md)
for row in 1:rows
    Quiver.Binary.write!(a; data = data[row, :], row = row)
end
Quiver.Binary.close!(a)

b = Quiver.Binary.open_file("b"; mode = 'w', metadata = md)
for row in 1:rows
    Quiver.Binary.write!(b; data = ones(2), row = 1)
end
Quiver.Binary.close!(b)

################################################

a = Quiver.Binary.File("a")
b = Quiver.Binary.File("b")

Quiver.Binary.open!(a; mode = 'r')
Quiver.Binary.open!(b; mode = 'r')

@show Quiver.Binary.get_metadata(a)
@show Quiver.Binary.get_metadata(b)

@show c = a + b * 2
Quiver.save(c, "c")

Quiver.Binary.close!(a)
Quiver.Binary.close!(b)

################################################

c = Quiver.Binary.open_file("c"; mode = 'r')
for row in 1:rows
    @show Quiver.Binary.read(c; row = row)
end
Quiver.Binary.close!(c)

return
