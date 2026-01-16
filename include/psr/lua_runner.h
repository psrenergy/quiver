#ifndef PSR_LUA_RUNNER_H
#define PSR_LUA_RUNNER_H

#include "export.h"

#include <memory>
#include <string>

namespace psr {

class Database;

/// Runs Lua scripts with access to a Database instance.
///
/// Example:
/// @code
/// auto db = psr::Database::from_schema(":memory:", "schema.sql");
/// psr::LuaRunner lua(db);
/// lua.run(R"(
///     db:create_element("Collection", { label = "Item 1" })
///     local labels = db:read_scalar_strings("Collection", "label")
/// )");
/// @endcode
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

}  // namespace psr

#endif  // PSR_LUA_RUNNER_H
