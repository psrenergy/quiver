const QUIVER_DATE_TIME_FORMAT = dateformat"yyyy-mm-ddTHH:MM:SS"
# The core accepts a space in place of the `T` (src/utils/datetime.h parse_iso8601), and import_csv
# canonicalizes to it, so a stored value can legally carry either separator. Julia's parser matches
# one format literally, so the space form needs its own. Truncated input needs nothing: Julia fills
# missing trailing components, so "2024-01-01" already parses as midnight.
const QUIVER_DATE_TIME_FORMAT_SPACE = dateformat"yyyy-mm-dd HH:MM:SS"

function string_to_date_time(s::String)::DateTime
    return DateTime(s, occursin(' ', s) ? QUIVER_DATE_TIME_FORMAT_SPACE : QUIVER_DATE_TIME_FORMAT)
end

function date_time_to_string(dt::DateTime)::String
    return Dates.format(dt, QUIVER_DATE_TIME_FORMAT)
end
