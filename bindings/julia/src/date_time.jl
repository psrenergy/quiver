const QUIVER_DATE_TIME_FORMAT = dateformat"yyyy-mm-ddTHH:MM:SS"

function string_to_date_time(s::String)::DateTime
    return DateTime(s, QUIVER_DATE_TIME_FORMAT)
end

function string_to_date_time(::Nothing)::Nothing
    return nothing
end

function date_time_to_string(dt::DateTime)::String
    return Dates.format(dt, QUIVER_DATE_TIME_FORMAT)
end
