#include "csv/csv_read.h"
#include "csv/csv_write.h"
#include "lua_runner/internal.h"
#include "quiver/database.h"
#include "utils/number.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace quiver::lua_internal {

// Lua-visible handle behind db:write_csv. Wraps the internal writer; sol2 owns
// it via std::unique_ptr, registered with sol::no_constructor and no explicit finalizer.
// db:open_file's BinaryFile is sol2-owned through a std::shared_ptr instead, for the same reason
// `writer` below is one: RunHandles keeps a weak_ptr to it (open_binary_files).
struct CsvWriter {
    // shared_ptr, not a value: RunHandles keeps a weak_ptr to every writer it hands out so run()
    // can close the ones the script never closed, whether or not Lua can still reach them
    // (collect_garbage() alone only finalizes unreachable ones -- see close_open_handles).
    // It also means the Writer is constructed in place and never moved.
    std::shared_ptr<quiver::csv_write::Writer> writer;
    // 1-based ordinal of the NEXT data row to attempt, so the non-finite-number error
    // can name which w:write_row call failed (a row of forty cells with one bad value is
    // otherwise unfindable). Incremented only after a row is accepted -- a rejected row keeps
    // the ordinal unchanged, so a script that retries the same logical row after a pcall sees
    // the same number.
    std::int64_t next_row_index = 1;
    // Width of the header this writer was opened with (write_csv's decoded
    // csv_options.header.size()); 0 means no header was given and therefore no width
    // enforcement -- header = {} already means "no header row", so 0 is unambiguous.
    // Stored once at construction; the header vector itself is never read again.
    std::size_t header_width = 0;

    CsvWriter(std::shared_ptr<quiver::csv_write::Writer> w, std::size_t header_width_)
        : writer(std::move(w)), header_width(header_width_) {}

    // w:write_row(row) and w:close(), bound as member pointers (defined below the decoders they use).
    void write_row(const sol::object& row);
    void close();
};

namespace {

// The MAXIMUM integer key of a Lua table, never sol::table::size()/lua_rawlen, plus
// the key rule and the width cap. Shared by csv_row_cells_from_lua and csv_header_from_lua so
// all three live in one place; `what` names the offending container in the messages ("row",
// "option 'header'").
std::int64_t csv_max_integer_key(const sol::table& t, const std::string& operation, const std::string& what) {
    std::int64_t max_index = 0;
    for (auto& pair : t) {
        if (!pair.first.is<std::int64_t>() || pair.first.as<std::int64_t>() < 1) {
            throw std::runtime_error("Cannot " + operation + ": " + what + " key must be a positive integer");
        }
        max_index = std::max(max_index, pair.first.as<std::int64_t>());
    }
    // Both callers materialize a dense vector up to max_index, so a single stray large key
    // ({ [1e9] = "x" }) would allocate that whole range -- tens of gigabytes, or a raw
    // std::bad_alloc/std::length_error reaching the script with no Pattern 1 prefix.
    // No real CSV record is this wide; reject it as a precondition failure instead.
    constexpr std::int64_t kMaxWidth = 1'000'000;
    if (max_index > kMaxWidth) {
        throw std::runtime_error(
            "Cannot " + operation + ": " + what + " key " + std::to_string(max_index) +
            " exceeds the maximum width of " + std::to_string(kMaxWidth)
        );
    }
    return max_index;
}

// One sol::object cell -> the std::string quiver::csv_write::Writer takes, through lua_to_value:
// nil (a hole from csv_row_cells_from_lua below) is an empty cell, a boolean is INTEGER 1/0 and so
// writes the text "1"/"0", and anything else that is not an int64, double or string is
// lua_to_value's own Pattern 1 rejection naming the 1-based cell. The dispatch order lives there
// (do not reorder it or add a get_type() guard to it): under SOL_SAFE_NUMERICS=1
// (src/CMakeLists.txt) a numeric-looking Lua STRING such as "0012" answers false to
// is<std::int64_t>()/is<double>() and stays a string, written verbatim.
std::string csv_cell_to_string(
    const sol::object& cell,
    const std::string& operation,
    std::int64_t index,
    std::int64_t row_index
) {
    return std::visit(
        [&](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return {};
            } else if constexpr (std::is_same_v<T, std::string>) {
                return value;
            } else {
                // Reject a non-finite double BEFORE append_number/to_chars is reached, so no
                // platform-specific spelling (MSVC's "-nan(ind)"/"nan"/"inf" vs. glibc's
                // "nan"/"inf") can reach a cell. Both the row and the cell are named: a row of
                // many cells with one bad value is otherwise unfindable.
                if constexpr (std::is_same_v<T, double>) {
                    if (!std::isfinite(value)) {
                        throw std::runtime_error(
                            "Cannot " + operation + ": row " + std::to_string(row_index) + " cell #" +
                            std::to_string(index) + " is not a finite number"
                        );
                    }
                }
                // An int64 goes to its own overload, never through double, so an integer past
                // double's 53-bit mantissa survives exactly; a double gets to_chars' shortest
                // round-trip form with no synthetic decimal point.
                std::string out;
                quiver::utils::append_number(value, out);
                return out;
            }
        },
        lua_to_value(cell, operation, "cell #" + std::to_string(index))
    );
}

// Converts one Lua row table to the ordered std::vector<std::string> quiver::csv_write::Writer
// takes. Row width is the table's maximum integer key: an interior hole is exactly the
// shape a nullable db:read_csv result produces, and must become an empty cell rather than
// collapse the row. A missing key reads back as Lua nil, which csv_cell_to_string above turns
// into an empty cell -- so an interior nil and an absent key are structurally identical,
// exactly as they are in Lua itself.
std::vector<std::string> csv_row_cells_from_lua(
    const sol::table& row,
    const std::string& operation,
    std::int64_t row_index
) {
    const std::int64_t max_index = csv_max_integer_key(row, operation, "row");

    std::vector<std::string> cells(static_cast<std::size_t>(max_index));
    for (std::int64_t i = 1; i <= max_index; ++i) {
        cells[static_cast<std::size_t>(i - 1)] = csv_cell_to_string(row[i], operation, i, row_index);
    }
    return cells;
}

// Converts a `header` option table to an ordered list of column names, through the same
// csv_max_integer_key walk csv_row_cells_from_lua uses. Every present entry must be a
// string; an empty table (zero integer keys) yields an empty result, which csv_write::Options
// treats as no header row.
// A bad KEY gets csv_max_integer_key's own message ("option 'header' key must be a positive
// integer"), never the entry-type one below -- `{ header = { name = "a" } }` would otherwise
// be told to fix a value it already got right.
std::vector<std::string> csv_header_from_lua(const sol::table& header, const std::string& operation) {
    const std::int64_t max_index = csv_max_integer_key(header, operation, "option 'header'");

    std::vector<std::string> names(static_cast<std::size_t>(max_index));
    for (std::int64_t i = 1; i <= max_index; ++i) {
        const sol::object cell = header[i];
        if (!cell.is<std::string>()) {
            throw std::runtime_error("Cannot " + operation + ": option 'header' entry must be a string");
        }
        names[static_cast<std::size_t>(i - 1)] = cell.as<std::string>();
    }
    return names;
}

// The `separator` branch both decoders share, so the byte-vs-character rule and the messages
// the tests pin exist once.
char csv_separator_from_lua(const sol::object& value, const std::string& operation) {
    // Check the Lua type explicitly rather than routing through the checked-conversion
    // helper every other converter uses: that helper surfaces sol2's own stack-index
    // message, which is neither Pattern 1 nor stable across build types.
    if (value.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + operation + ": option 'separator' must be a string");
    }
    const auto separator = value.as<std::string>();
    // Measured in BYTES -- a multi-byte UTF-8 character is rejected as multi-character.
    if (separator.size() != 1) {
        throw std::runtime_error("Cannot " + operation + ": option 'separator' must be a single character");
    }
    // A quote, a line terminator or a NUL is one byte but not a delimiter: csv-parser refuses
    // a delimiter that overlaps its quote character, and a CR/LF/NUL delimiter makes the
    // writer emit records this project's own reader can never put back together. Rejecting
    // here keeps db:write_csv from silently producing a file db:read_csv cannot parse.
    const char c = separator[0];
    if (c == '"' || c == '\r' || c == '\n' || c == '\0') {
        throw std::runtime_error(
            "Cannot " + operation + ": option 'separator' must not be a quote, carriage return, newline or NUL"
        );
    }
    return c;
}

// Strict decoder for db:write_csv's trailing options table, on the two shared helpers above.
csv_write::Options write_csv_options_from_lua(const sol::object& options, const std::string& operation) {
    csv_write::Options result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        // Missing parameter or explicit nil -- same as an empty table, both valid.
        return result;
    }

    const auto& [separator, header] = option_entries(options, operation, {"separator", "header"});

    if (separator) {
        result.separator = csv_separator_from_lua(*separator, operation);
    }

    if (header) {
        result.header = csv_header_from_lua(option_table(*header, operation, "header"), operation);
    }

    return result;
}

// Shared strict decoder for db:read_csv / db:read_csv_stream's trailing options table
// (one decoder so the two entry points cannot diverge on any
// option). `options` is `sol::object`, not `sol::optional<sol::table>`: sol2's optional checker
// never raises on a type mismatch on its own (see relation_target_from_lua, the pattern
// this copies), so a wrong type would otherwise silently fall through to defaults instead of
// throwing. `operation` is the
// caller's own method name ("read_csv" / "read_csv_stream"), so the same bad table reports
// whichever entry point the script actually called.
csv_read::Options read_csv_options_from_lua(const sol::object& options, const std::string& operation) {
    csv_read::Options result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        // Missing parameter or explicit nil -- same as an empty table, both valid.
        return result;
    }

    const auto& [separator, header_row_value] = option_entries(options, operation, {"separator", "header_row"});

    if (separator) {
        result.separator = csv_separator_from_lua(*separator, operation);
    }

    if (header_row_value) {
        // One test, not lua_cell_as (whose text is not this stable Pattern 1 one): is<int64_t>() is
        // lua_isinteger under SOL_SAFE_NUMERICS=1 (set unconditionally in src/CMakeLists.txt, so
        // Release too), false for a string -- even a quoted "2", which Lua's own string->number
        // coercion would let through --, a boolean, and a fractional or whole float (2.5, 2.0).
        if (!header_row_value->is<int64_t>()) {
            throw std::runtime_error("Cannot " + operation + ": option 'header_row' must be an integer");
        }
        const auto header_row = header_row_value->as<int64_t>();
        if (header_row < 0) {
            // A separate message from the type check above: "-1" IS an integer, so telling
            // the caller otherwise would be a lie.
            throw std::runtime_error("Cannot " + operation + ": option 'header_row' must not be negative");
        }
        result.header_row = header_row;
    }

    return result;
}

// The header both read forms hand a script: nil when the file has no header (header_row = 0),
// else the name list. Never an empty table: {} is truthy in Lua and nil is falsy, so with no
// header `result.header` must be absent and on_row's third argument nil, or a script written as
// `if header then ... end` would take opposite branches in the whole-file and streaming forms.
sol::object header_object(sol::state_view& lua, const std::vector<std::string>& header) {
    return header.empty() ? sol::object(sol::lua_nil) : sol::object(to_lua_table(lua, header));
}

}  // namespace

void CsvWriter::write_row(const sol::object& row) {
    // The row is a sol::object checked by require_table, first: a userdata (which iterates as
    // no keys, so w:write_row(db) once appended an empty record) is rejected like any other
    // non-table, and the type error wins over the closed-writer one.
    const auto table = require_table(row, "write_row", "row");
    // Check the closed state BEFORE formatting a single cell -- cells were
    // previously formatted as csv_write::Writer::write_row's argument, evaluated before
    // the call, so a write after close on a bad row raised the wrong error. Delegating
    // to Writer with an empty vector reuses its own closed-writer message verbatim
    // (never reached: Writer checks closed_ before touching cells) instead of
    // duplicating the text here.
    if (writer->is_closed()) {
        writer->write_row({}, "write_row");
        return;
    }
    const auto row_index = next_row_index;
    auto cells = csv_row_cells_from_lua(table, "write_row", row_index);
    // header_width == 0 means no header was given, so no enforcement applies.
    // A row wider than the header is never truncated -- it throws, naming the 1-based
    // data-row ordinal and both counts (pinned in src/csv/csv_write.cpp's
    // message catalogue comment -- reword both together). A short row is padded BEFORE
    // Writer::write_row ever sees it -- append_record is a pure function of the vector
    // it receives, so padding after the call would be too late.
    if (header_width != 0) {
        if (cells.size() > header_width) {
            throw std::runtime_error(
                "Cannot write_row: row " + std::to_string(row_index) + " has " + std::to_string(cells.size()) +
                " cells but header declares " + std::to_string(header_width)
            );
        }
        if (cells.size() < header_width) {
            cells.resize(header_width);
        }
    }
    writer->write_row(cells, "write_row");
    ++next_row_index;
}

void CsvWriter::close() {
    writer->close("close");
}

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
void bind_csv(sol::state& state, sol::usertype<Database>& bind, RunHandles& handles) {
    // CSV file reading/writing -- db-scoped and sandboxed like the file I/O in binary.cpp. The two
    // reading entry points below construct the same csv_read reader and drive it through
    // header()/for_each_row(), so they cannot diverge on any input. Writing
    // (db:write_csv) is streaming-only -- there is no whole-file counterpart, by decision.
    bind.set_function(
        "read_csv",
        [](Database& self, const std::string& path, sol::object options, sol::this_state s) -> sol::table {
            sol::state_view lua(s);
            // Evaluation order: sandbox checks (in-memory db, path escape) before the
            // options table, so a bad separator never masks an escaping path.
            const auto resolved = resolve_sandboxed_path(self, "read_csv", path);
            auto csv_options = read_csv_options_from_lua(options, "read_csv");
            csv_read::Reader reader(resolved, path, "read_csv", csv_options);

            // Each row is handed to Lua as it is parsed, exactly as read_csv_stream does, so
            // the file exists once (in Lua) rather than twice -- staging every row in a
            // std::vector first and converting afterwards held a complete C++ copy alongside
            // the complete Lua copy for the whole conversion.
            auto rows = lua.create_table();
            reader.for_each_row([&lua, &rows](std::vector<std::string>&& cells, int64_t index) {
                rows[index] = to_lua_table(lua, cells);
                return true;
            });

            auto result = lua.create_table();
            result["header"] = header_object(lua, reader.header());
            result["rows"] = rows;
            return result;
        }
    );
    bind.set_function(
        "read_csv_stream",
        [](Database& self, const std::string& path, sol::object on_row_arg, sol::object options, sol::this_state s)
            -> int64_t {
            sol::state_view lua(s);
            // sol::object plus an explicit type check, not a typed
            // sol::protected_function parameter: a typed one surfaces sol2's own
            // "stack index 3, expected function" text, which is neither Pattern 1
            // nor stable across build types -- the same reason `options`
            // is decoded by hand.
            if (on_row_arg.get_type() != sol::type::function) {
                throw std::runtime_error("Cannot read_csv_stream: on_row must be a function");
            }
            const sol::protected_function on_row = on_row_arg.as<sol::protected_function>();
            // Evaluation order: sandbox checks before the options table.
            const auto resolved = resolve_sandboxed_path(self, "read_csv_stream", path);
            auto csv_options = read_csv_options_from_lua(options, "read_csv_stream");
            csv_read::Reader reader(resolved, path, "read_csv_stream", csv_options);

            // Built once, before the loop, and passed by reference into every callback
            // invocation -- reachable during the stream so a script can find a column by
            // name before processing row 1.
            const sol::object header_table = header_object(lua, reader.header());

            return reader.for_each_row([&](std::vector<std::string>&& cells, int64_t index) -> bool {
                const auto row_table = to_lua_table(lua, cells);
                auto result = on_row(row_table, index, header_table);
                if (!result.valid()) {
                    // Propagate the Lua error verbatim and unwrapped: the reader is a
                    // stack local and ~CSVReader() joins its scheduler during normal C++
                    // unwinding, so no manual cleanup is needed here.
                    sol::error err = result;
                    throw std::runtime_error(err.what());
                }
                // sol::optional<bool> is a strict LUA_TBOOLEAN check, Debug/Release-identical.
                // Only an exact `false` stops the read -- get<bool>() would be
                // lua_toboolean truthiness and misread a no-return callback's nil as "stop".
                if (result.return_count() > 0 && result.get<sol::optional<bool>>(0) == false) {
                    return false;
                }
                return true;
            });
        }
    );
    bind.set_function(
        "write_csv",
        [&handles](Database& self, const std::string& path, sol::object options) -> std::unique_ptr<CsvWriter> {
            // Evaluation order: sandbox checks (in-memory db, path escape) before
            // the options table, so a bad separator never masks an escaping path.
            const auto resolved = resolve_sandboxed_path(self, "write_csv", path);
            auto csv_options = write_csv_options_from_lua(options, "write_csv");
            if (handles.path_has_open_writer(resolved)) {
                throw std::runtime_error("Cannot write_csv: file is already open for writing: " + path);
            }
            const auto header_width = csv_options.header.size();
            auto writer = std::make_shared<quiver::csv_write::Writer>(resolved, path, "write_csv", csv_options);
            // Registered so close_open_handles() can flush it at run()'s exit even
            // when the script leaves it reachable (a global), which the GC cannot.
            handles.add_writer(resolved, writer);
            return std::make_unique<CsvWriter>(std::move(writer), header_width);
        }
    );

    // sol::no_constructor + std::unique_ptr return (above), no explicit finalizer.
    // BinaryFile is the same except for its holder, a std::shared_ptr (see db:open_file).
    state.new_usertype<CsvWriter>(
        "CsvWriter",
        sol::no_constructor,
        "write_row",
        &CsvWriter::write_row,
        "close",
        &CsvWriter::close
    );
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
