module TestElement

using Aqua
using PSRDatabase
using Test

@testset "Element" begin
    e = Element()

    e["integer"] = 42
    e["double"] = 3.14
    e["string"] = "Hello, World!"
    e["vector_int"] = [1, 2, 3, 4, 5]
    e["vector_double"] = [1.1, 2.2, 3.3]
    # e["vector_string"] = ["one", "two", "three"]

    println(e)

    return nothing
end

end
