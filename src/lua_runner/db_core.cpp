#include "lua_runner/internal.h"
#include "quiver/database.h"
#include "quiver/options.h"
#include "quiver/value.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace quiver::lua_internal {

namespace {

// Collect a nested option table's entries before any is checked (the same collect-then-validate rule), after
// checking the value really is a table.
std::vector<std::pair<sol::object, sol::object>> table_entries(
    const sol::object& value,
    const std::string& operation,
    const std::string& what
) {
    if (value.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": option '" + what + "' must be a table");
    }
    std::vector<std::pair<sol::object, sol::object>> entries;
    value.as<sol::table>().for_each([&](sol::object key, sol::object entry) {
        entries.emplace_back(std::move(key), std::move(entry));
    });
    return entries;
}

std::string string_key(const sol::object& key, const std::string& operation, const std::string& what) {
    if (key.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + operation + ": keys of option '" + what + "' must be strings");
    }
    return key.as<std::string>();
}

// Strict decoder for db:export_csv / db:import_csv options, the same collect-then-validate walk
// as the read_csv/write_csv decoders: nil/missing means defaults; anything else must be a table
// with only known keys of the right types.
CSVOptions parse_csv_options(const sol::object& options, const std::string& operation) {
    CSVOptions result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        return result;
    }
    if (options.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": options must be a table");
    }
    const auto found = csv_options_entries(options, operation, {"date_time_format", "enum_labels"});

    if (const auto& format = found[0]) {
        if (format->get_type() != sol::type::string) {
            throw std::runtime_error("Cannot " + operation + ": option 'date_time_format' must be a string");
        }
        result.date_time_format = format->as<std::string>();
    }
    if (const auto& enums = found[1]) {
        // attribute -> locale -> { label = code }: every level collected before it is checked.
        for (const auto& [attr_key, attr_value] : table_entries(*enums, operation, "enum_labels")) {
            const auto attr = string_key(attr_key, operation, "enum_labels");
            const auto attr_where = "enum_labels['" + attr + "']";
            auto& locales = result.enum_labels[attr];
            for (const auto& [locale_key, locale_value] : table_entries(attr_value, operation, attr_where)) {
                const auto locale = string_key(locale_key, operation, attr_where);
                const auto where = attr_where + "['" + locale + "']";
                auto& labels = locales[locale];
                for (const auto& [label_key, code] : table_entries(locale_value, operation, where)) {
                    const auto label = string_key(label_key, operation, where);
                    labels[label] = lua_cell_as<int64_t>(code, operation, "code for label '" + label + "'");
                }
            }
        }
    }
    return result;
}

// The single place the db:transaction / db:dry_run sequencing lives: open the scope, run the
// callback with the database, and on a Lua error undo the scope (any failure of the undo itself is
// swallowed so the script sees the callback's error) before rethrowing; on success close the scope
// and hand back the callback's first return value, or nil when it returned nothing.
sol::object run_in_scope(
    Database& self,
    const sol::protected_function& fn,
    void (Database::*begin)(),
    void (Database::*finish)(),
    void (Database::*abort)()
) {
    (self.*begin)();
    auto result = fn(std::ref(self));
    if (!result.valid()) {
        sol::error err = result;
        try {
            (self.*abort)();
        } catch (...) {
        }
        throw std::runtime_error(err.what());
    }
    (self.*finish)();
    if (result.return_count() > 0) {
        return result.get<sol::object>(0);
    }
    return sol::make_object(result.lua_state(), sol::lua_nil);
}

std::vector<Value> lua_table_to_values(const std::string& caller, const sol::table& parameters) {
    std::vector<Value> values;
    for (size_t i = 1; i <= parameters.size(); ++i) {
        // A skipped parameter would shift every later placeholder, so anything unsupported throws.
        values.push_back(lua_to_value(parameters.get<sol::object>(i), caller, "parameter #" + std::to_string(i)));
    }
    return values;
}

sol::object query_string_lua(
    Database& db,
    const std::string& sql,
    sol::optional<sol::table> parameters,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto values = parameters ? lua_table_to_values("query_string", *parameters) : std::vector<Value>{};
    auto result = db.query_string(sql, values);
    if (result.has_value()) {
        return sol::make_object(lua, *result);
    }
    return sol::make_object(lua, sol::lua_nil);
}

sol::object query_integer_lua(
    Database& db,
    const std::string& sql,
    sol::optional<sol::table> parameters,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto values = parameters ? lua_table_to_values("query_integer", *parameters) : std::vector<Value>{};
    auto result = db.query_integer(sql, values);
    if (result.has_value()) {
        return sol::make_object(lua, *result);
    }
    return sol::make_object(lua, sol::lua_nil);
}

sol::object query_float_lua(
    Database& db,
    const std::string& sql,
    sol::optional<sol::table> parameters,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto values = parameters ? lua_table_to_values("query_float", *parameters) : std::vector<Value>{};
    auto result = db.query_float(sql, values);
    if (result.has_value()) {
        return sol::make_object(lua, *result);
    }
    return sol::make_object(lua, sol::lua_nil);
}

}  // namespace

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
void bind_core(sol::usertype<Database>& bind) {
    bind.set_function("is_healthy", &Database::is_healthy);
    bind.set_function("current_version", &Database::current_version);
    bind.set_function("path", &Database::path);
    bind.set_function("begin_transaction", &Database::begin_transaction);
    bind.set_function("commit", &Database::commit);
    bind.set_function("rollback", &Database::rollback);
    bind.set_function("in_transaction", &Database::in_transaction);
    bind.set_function("transaction", [](Database& self, sol::protected_function fn) -> sol::object {
        return run_in_scope(self, fn, &Database::begin_transaction, &Database::commit, &Database::rollback);
    });
    bind.set_function("begin_dry_run", &Database::begin_dry_run);
    bind.set_function("end_dry_run", &Database::end_dry_run);
    bind.set_function("in_dry_run", &Database::in_dry_run);
    bind.set_function("dry_run", [](Database& self, sol::protected_function fn) -> sol::object {
        return run_in_scope(self, fn, &Database::begin_dry_run, &Database::end_dry_run, &Database::end_dry_run);
    });
    bind.set_function(
        "export_csv",
        [](Database& self,
           const std::string& collection,
           const std::string& group,
           const std::string& path,
           const sol::object& options) {
            // Sandbox checks before the options table, as in db:write_csv.
            const auto resolved = resolve_sandboxed_path(self, "export_csv", path);
            self.export_csv(collection, group, resolved, parse_csv_options(options, "export_csv"));
        }
    );
    bind.set_function(
        "import_csv",
        [](Database& self,
           const std::string& collection,
           const std::string& group,
           const std::string& path,
           const sol::object& options) {
            const auto resolved = resolve_sandboxed_path(self, "import_csv", path);
            self.import_csv(collection, group, resolved, parse_csv_options(options, "import_csv"));
        }
    );

    bind.set_function("number_of_elements", &Database::number_of_elements);

    bind.set_function("describe", &Database::describe);
    bind.set_function("describe_collection", &Database::describe_collection);
    bind.set_function("summarize_collection", &Database::summarize_collection);

    bind.set_function("query_string", &query_string_lua);
    bind.set_function("query_integer", &query_integer_lua);
    bind.set_function("query_float", &query_float_lua);

    // Migration round-trip validation — db-scoped and sandboxed like the file I/O in binary.cpp.
    bind.set_function("validate_migrations", [](Database& self, const std::string& path) {
        Database::validate_migrations(resolve_sandboxed_path(self, "validate_migrations", path));
    });
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
