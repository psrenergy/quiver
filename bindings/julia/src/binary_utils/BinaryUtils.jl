module BinaryUtils

# This module contains higher-level utilities for working with binary data, implemented only in Julia.
# It relies only on the public API provided by the Binary module, and does not call any C functions directly.

using Quiver: Binary

include("operations.jl")

end
