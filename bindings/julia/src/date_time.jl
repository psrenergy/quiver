const QUIVER_DATE_TIME_FORMAT = dateformat"yyyy-mm-ddTHH:MM:SS"

# The core accepts a space in place of the `T` (src/utils/datetime.h parse_iso8601), so a stored
# value can legally carry either separator, and Julia's parser matches one format literally.
# Normalizing the separator handles both with one format. Truncated input needs nothing: Julia
# fills missing trailing components, so "2024-01-01" already parses as midnight.
function string_to_date_time(s::String)::DateTime
    return DateTime(replace(s, ' ' => 'T'), QUIVER_DATE_TIME_FORMAT)
end

function date_time_to_string(dt::DateTime)::String
    return Dates.format(dt, QUIVER_DATE_TIME_FORMAT)
end
