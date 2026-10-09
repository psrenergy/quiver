#include "quiver/database.h"
#include "quiver/options.h"
#include "sandbox/internal.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace quiver::lua_internal {

namespace {

std::string string_key(const sol::object& key, const std::string& operation, const std::string& what) {
    if (key.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + operation + ": keys of option '" + what + "' must be strings");
    }
    return key.as<std::string>();
}

}  // namespace

// Strict decoder for db:export_csv / db:import_csv options, on the same option_entries walk as
// the read_csv/write_csv decoders: nil/missing means defaults; anything else must be a table
// with only known keys of the right types.
CSVOptions parse_csv_options(const sol::object& options, const std::string& operation) {
    CSVOptions result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        return result;
    }
    const auto& [date_time_format, enum_labels] =
        option_entries(options, operation, {"date_time_format", "enum_labels"});

    if (date_time_format) {
        if (date_time_format->get_type() != sol::type::string) {
            throw std::runtime_error("Cannot " + operation + ": option 'date_time_format' must be a string");
        }
        result.date_time_format = date_time_format->as<std::string>();
    }
    if (enum_labels) {
        // attribute -> locale -> { label = code }: every level collected before it is checked.
        const auto attributes = option_table(*enum_labels, operation, "enum_labels");
        for (const auto& [attr_key, attr_value] : collect_entries(attributes)) {
            const auto attr = string_key(attr_key, operation, "enum_labels");
            const auto attr_where = "enum_labels['" + attr + "']";
            auto& locales = result.enum_labels[attr];
            const auto locale_tables = option_table(attr_value, operation, attr_where);
            for (const auto& [locale_key, locale_value] : collect_entries(locale_tables)) {
                const auto locale = string_key(locale_key, operation, attr_where);
                const auto where = attr_where + "['" + locale + "']";
                auto& labels = locales[locale];
                const auto codes = option_table(locale_value, operation, where);
                for (const auto& [label_key, code] : collect_entries(codes)) {
                    const auto label = string_key(label_key, operation, where);
                    labels[label] = lua_cell_as<int64_t>(code, operation, "code for label '" + label + "'");
                }
            }
        }
    }
    return result;
}

void bind_csv_export(sol::usertype<Database>& bind) {
    bind.set_function(
        "export_csv",
        [](Database& self,
           const std::string& collection,
           const std::string& group,
           const std::string& path,
           const sol::object& options) {
            // Sandbox checks before the options table, as in db:write_csv.
            const auto resolved = resolve_sandbox_path(self, "export_csv", path);
            self.export_csv(collection, group, resolved, parse_csv_options(options, "export_csv"));
        }
    );
}

}  // namespace quiver::lua_internal
