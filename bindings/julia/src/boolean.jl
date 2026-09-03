_integer_to_boolean(value::Nothing, collection::String = "", attribute::String = "") = nothing

function _integer_to_boolean(value::Integer, collection::String = "", attribute::String = "")
    value == 0 && return false
    value == 1 && return true
    source = isempty(collection) ? "" : " in '$collection.$attribute'"
    throw(ArgumentError("Cannot convert integer $value to boolean$source: expected 0 or 1"))
end
