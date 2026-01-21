#ifndef PSR_LUA_RUNNER_H
#define PSR_LUA_RUNNER_H

#include "export.h"

#include <memory>
#include <string>

namespace margaux {

class Database;

class PSR_API LuaRunner {
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

}  // namespace margaux

#endif  // PSR_LUA_RUNNER_H
