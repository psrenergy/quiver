#ifndef QUIVER_LUA_RUNNER_H
#define QUIVER_LUA_RUNNER_H

#include "export.h"

#include <memory>
#include <string>

namespace quiver {

class Database;

class QUIVER_API LuaRunner {
public:
    explicit LuaRunner(Database& db);
    ~LuaRunner();

    // Non-copyable
    LuaRunner(const LuaRunner&) = delete;
    LuaRunner& operator=(const LuaRunner&) = delete;

    // Movable
    LuaRunner(LuaRunner&&) noexcept;
    LuaRunner& operator=(LuaRunner&&) noexcept;

    /// Runs a Lua script with access to the database as 'db'.
    ///
    /// Returns the script's return value encoded as JSON, or an empty string if the script
    /// returned nothing. Only the first returned value is encoded.
    ///
    /// To execute a script without keeping its writes, wrap the call in
    /// Database::begin_dry_run / Database::end_dry_run.
    std::string run(const std::string& script);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver

#endif  // QUIVER_LUA_RUNNER_H
