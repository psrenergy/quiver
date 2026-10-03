#include "lua_runner/internal.h"
#include "quiver/database.h"
#include "quiver/options.h"
#include "quiver/value.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace quiver::lua_internal {

namespace {

std::string string_key(const sol::object& key, const std::string& operation, const std::string& what) {
    if (key.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + operation + ": keys of option '" + what + "' must be strings");
    }
    return key.as<std::string>();
}

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

// The single place the db:transaction / db:dry_run sequencing lives: check the argument is a
// function before the scope opens (a callable table is refused, as for on_row), open the scope, run
// the callback with the database, and on a Lua error undo the scope (any failure of the undo itself
// is swallowed so the script sees the callback's error) before rethrowing; on success close the
// scope and hand back the callback's first return value, or nil when it returned nothing.
sol::object run_in_scope(
    Database& self,
    const char* operation,
    const sol::object& fn_arg,
    void (Database::*begin)(),
    void (Database::*finish)(),
    void (Database::*abort)()
) {
    if (fn_arg.get_type() != sol::type::function) {
        throw lua_type_error(operation, "fn", "a function", fn_arg);
    }
    const auto fn = fn_arg.as<sol::protected_function>();
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

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
std::optional<std::string> query_string_lua(Database& db, const std::string& sql, const sol::object& parameters) {
    const auto params = optional_from_lua<sol::table>(parameters, "query_string", "params", "a table");
    return db.query_string(sql, params ? lua_table_to_values("query_string", *params) : std::vector<Value>{});
}

std::optional<int64_t> query_integer_lua(Database& db, const std::string& sql, const sol::object& parameters) {
    const auto params = optional_from_lua<sol::table>(parameters, "query_integer", "params", "a table");
    return db.query_integer(sql, params ? lua_table_to_values("query_integer", *params) : std::vector<Value>{});
}

std::optional<double> query_float_lua(Database& db, const std::string& sql, const sol::object& parameters) {
    const auto params = optional_from_lua<sol::table>(parameters, "query_float", "params", "a table");
    return db.query_float(sql, params ? lua_table_to_values("query_float", *params) : std::vector<Value>{});
}

}  // namespace

void bind_core(sol::usertype<Database>& bind) {
    bind.set_function("is_healthy", &Database::is_healthy);
    bind.set_function("current_version", &Database::current_version);
    bind.set_function("path", &Database::path);
    bind.set_function("begin_transaction", &Database::begin_transaction);
    bind.set_function("commit", &Database::commit);
    bind.set_function("rollback", &Database::rollback);
    bind.set_function("in_transaction", &Database::in_transaction);
    bind.set_function("transaction", [](Database& self, const sol::object& fn) -> sol::object {
        return run_in_scope(
            self,
            "transaction",
            fn,
            &Database::begin_transaction,
            &Database::commit,
            &Database::rollback
        );
    });
    bind.set_function("begin_dry_run", &Database::begin_dry_run);
    bind.set_function("end_dry_run", &Database::end_dry_run);
    bind.set_function("in_dry_run", &Database::in_dry_run);
    bind.set_function("dry_run", [](Database& self, const sol::object& fn) -> sol::object {
        return run_in_scope(
            self,
            "dry_run",
            fn,
            &Database::begin_dry_run,
            &Database::end_dry_run,
            &Database::end_dry_run
        );
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
