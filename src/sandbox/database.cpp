#include "quiver/database.h"

#include "sandbox/internal.h"

#include <sol/sol.hpp>

#include <stdexcept>
#include <string>
#include <utility>

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

}  // namespace

void bind_database(sol::usertype<Database>& bind, RunHandles& handles) {
    // A scope open when run() started is the host's: ending it from the script would commit the
    // host's work early, or turn a host dry run into real writes. Under a host dry run, commit and
    // rollback still reach Database, which absorbs them. db:transaction / db:dry_run need no check:
    // their begin already throws under a host scope.
    const auto refuse_host_transaction = [&handles](const char* operation) {
        if (handles.host_transaction) {
            throw std::runtime_error(
                std::string("Cannot ") + operation +
                ": the transaction was open before the script started, so it belongs to the host"
            );
        }
    };
    bind.set_function("is_healthy", &Database::is_healthy);
    bind.set_function("current_version", &Database::current_version);
    bind.set_function("path", &Database::path);
    bind.set_function("begin_transaction", &Database::begin_transaction);
    bind.set_function("commit", [refuse_host_transaction](Database& self) {
        refuse_host_transaction("commit");
        self.commit();
    });
    bind.set_function("rollback", [refuse_host_transaction](Database& self) {
        refuse_host_transaction("rollback");
        self.rollback();
    });
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
    bind.set_function("end_dry_run", [&handles](Database& self) {
        if (handles.host_dry_run) {
            throw std::runtime_error(
                "Cannot end_dry_run: the dry run was open before the script started, so it belongs to the host"
            );
        }
        self.end_dry_run();
    });
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

    // Migration round-trip validation — db-scoped and sandbox like the file I/O in binary.cpp.
    bind.set_function("validate_migrations", [](Database& self, const std::string& path) {
        Database::validate_migrations(resolve_sandbox_path(self, "validate_migrations", path));
    });
}

}  // namespace quiver::lua_internal
