#include "quiver/database.h"
#include "sandbox/internal.h"
#include "xlsx/xlsx_read.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace quiver::lua_internal {

namespace {

xlsx_read::Options read_xlsx_options(const sol::object& options, const std::string& operation) {
    xlsx_read::Options result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        return result;
    }

    const auto& [sheet, header_row] = option_entries(options, operation, {"sheet", "header_row"});
    if (sheet) {
        if (sheet->get_type() == sol::type::string) {
            result.sheet = sheet->as<std::string>();
        } else if (sheet->is<int64_t>()) {
            const auto index = sheet->as<int64_t>();
            if (index < 1) {
                throw std::runtime_error("Cannot " + operation + ": option 'sheet' index must be positive");
            }
            result.sheet = index;
        } else {
            throw lua_type_error(operation, "option 'sheet'", "a string or an integer", *sheet);
        }
    }
    if (header_row) {
        if (!header_row->is<int64_t>()) {
            throw lua_type_error(operation, "option 'header_row'", "an integer", *header_row);
        }
        const auto number = header_row->as<int64_t>();
        if (number < 0) {
            throw std::runtime_error("Cannot " + operation + ": option 'header_row' must not be negative");
        }
        result.header_row = number;
    }
    return result;
}

}  // namespace

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda type deduction
void bind_xlsx(sol::usertype<Database>& bind) {
    bind.set_function(
        "read_xlsx",
        [](Database& self, const std::string& path, sol::object options, sol::this_state s) -> sol::table {
            sol::state_view lua(s);
            const auto resolved = resolve_sandbox_path(self, "read_xlsx", path);
            xlsx_read::Reader reader(resolved, path, "read_xlsx", read_xlsx_options(options, "read_xlsx"));
            auto rows = lua.create_table();
            reader.for_each_row([&](std::vector<std::string>&& cells, int64_t index) {
                rows[index] = to_lua_table(lua, cells);
                return true;
            });
            auto result = lua.create_table();
            result["header"] = row_header_to_lua(lua, reader.header());
            result["rows"] = rows;
            return result;
        }
    );
    bind.set_function(
        "read_xlsx_stream",
        [](Database& self, const std::string& path, sol::object callback, sol::object options, sol::this_state s)
            -> int64_t {
            if (callback.get_type() != sol::type::function) {
                throw std::runtime_error("Cannot read_xlsx_stream: on_row must be a function");
            }
            const auto on_row = callback.as<sol::protected_function>();
            sol::state_view lua(s);
            const auto resolved = resolve_sandbox_path(self, "read_xlsx_stream", path);
            xlsx_read::Reader
                reader(resolved, path, "read_xlsx_stream", read_xlsx_options(options, "read_xlsx_stream"));
            const auto header = row_header_to_lua(lua, reader.header());
            return reader.for_each_row([&](std::vector<std::string>&& cells, int64_t index) {
                return call_row_callback(on_row, to_lua_table(lua, cells), index, header);
            });
        }
    );
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
