_integer_to_boolean(value::Nothing) = nothing
_integer_to_boolean(value::Integer)::Bool = Bool(value)
