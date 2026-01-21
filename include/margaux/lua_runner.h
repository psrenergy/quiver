#ifndef DECK_DATABASE_LUA_RUNNER_H
#define DECK_DATABASE_LUA_RUNNER_H

#include "export.h"

#include <memory>
#include <string>

namespace margaux {

class Database;

class DECK_DATABASE_API LuaRunner {
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

#endif  // DECK_DATABASE_LUA_RUNNER_H
