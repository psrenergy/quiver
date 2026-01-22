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
    void run(const std::string& script);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace quiver

#endif  // QUIVER_LUA_RUNNER_H
