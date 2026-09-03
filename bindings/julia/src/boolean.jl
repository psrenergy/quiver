_integer_to_boolean(value::Nothing) = nothing

function _integer_to_boolean(value::Integer)::Bool
    value == 0 && return false
    value == 1 && return true
    return throw(ArgumentError("Cannot convert integer $value to boolean: expected 0 or 1"))
end
