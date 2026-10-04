#include "lua_runner/internal.h"
#include "quiver/database.h"
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

// The single place the db:transaction / db:dry_run sequencing lives: check the argument is a
// function before the scope opens (a callable table is refused, as for on_row), open the scope, run
// the callback with the database, then close the scope. A Lua error or a failed close (a COMMIT
// that fails on a deferred foreign key) undoes the scope best-effort (any failure of the undo
// itself is swallowed so the script sees the original error) and rethrows. On success it hands
// back the callback's first return value, or nil when it returned nothing.
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
    try {
        if (!result.valid()) {
            sol::error err = result;
            throw std::runtime_error(err.what());
        }
        (self.*finish)();
    } catch (...) {
        try {
            (self.*abort)();
        } catch (...) {
        }
        throw;
    }
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
