import Pkg
Pkg.instantiate()

using Style
Style.format(dirname(@__DIR__))
