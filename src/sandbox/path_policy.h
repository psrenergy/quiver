#ifndef QUIVER_SRC_LUA_RUNNER_PATH_POLICY_H
#define QUIVER_SRC_LUA_RUNNER_PATH_POLICY_H

#include "quiver/database.h"

#include <string>

namespace quiver::lua_internal {

// The one filesystem gate every file-touching Lua operation routes through. Resolves a
// script-supplied path against the database file's directory and requires the result to stay
// strictly inside it; returns the resolved absolute path. No sol2 here: SandboxedPathTest
// includes this header and compiles path_policy.cpp into quiver_tests.
std::string resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path);

}  // namespace quiver::lua_internal

#endif  // QUIVER_SRC_LUA_RUNNER_PATH_POLICY_H
