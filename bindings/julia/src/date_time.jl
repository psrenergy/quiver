# DateTime format used by Quiver (ISO 8601 with T separator)
const QUIVER_DATETIME_FORMAT = dateformat"yyyy-mm-ddTHH:MM:SS"

function _string_to_datetime(s::String)::DateTime
    DateTime(s, QUIVER_DATETIME_FORMAT)
end

function _datetime_to_string(dt::DateTime)::String
    Dates.format(dt, QUIVER_DATETIME_FORMAT)
end
