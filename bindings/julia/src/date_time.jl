const QUIVER_DATE_TIME_FORMAT = dateformat"yyyy-mm-ddTHH:MM:SS"

# The core's DATE_TIME grammar (`datetime::is_valid_iso8601`, `src/utils/datetime.h`): `YYYY-MM-DD`
# optionally followed by `THH:MM:SS` or ` HH:MM:SS`, every field fixed-width and zero-padded, year
# 0001-9999. Julia's `dateformat` treats field widths as maxima and fills missing trailing
# components, so without this gate "2024" parses as 2024-01-01 and "20240115" as year 20240115 --
# values Python and Dart reject or read differently.
const QUIVER_DATE_TIME_PATTERN = r"^(?!0000)\d{4}-\d{2}-\d{2}(?:[T ]\d{2}:\d{2}:\d{2})?$"

string_to_date_time(::Nothing, collection::String = "", attribute::String = "") = nothing

function string_to_date_time(s::String, collection::String = "", attribute::String = "")::DateTime
    if occursin(QUIVER_DATE_TIME_PATTERN, s)
        try
            # Only the separator is normalized (`count = 1`), so the message below quotes what was
            # stored rather than a string with every interior space turned into a `T`.
            return DateTime(replace(s, ' ' => 'T'; count = 1), QUIVER_DATE_TIME_FORMAT)
        catch e
            # An out-of-range field ("2024-02-31", hour 25) has the right shape but no valid date;
            # fall through so the rejection still names the column.
            e isa ArgumentError || rethrow()
        end
    end
    source = isempty(collection) ? "" : " in '$collection.$attribute'"
    return throw(
        ArgumentError(
            "Cannot convert \"$s\" to a date time$source: expected a valid YYYY-MM-DD[THH:MM:SS]",
        ),
    )
end

function date_time_to_string(dt::DateTime)::String
    return Dates.format(dt, QUIVER_DATE_TIME_FORMAT)
end
