#include "quiver/c/lua_runner.h"

#include "internal.h"
#include "quiver/lua_runner.h"

#include <new>
#include <string>

struct quiver_lua_runner {
    quiver::LuaRunner runner;
    std::string last_error;

    explicit quiver_lua_runner(quiver::Database& db) : runner(db) {}
};

quiver_lua_runner_t* quiver_lua_runner_new(quiver_database_t* db) {
    if (!db) {
        return nullptr;
    }
    try {
        return new quiver_lua_runner(db->db);
    } catch (...) {
        return nullptr;
    }
}

void quiver_lua_runner_free(quiver_lua_runner_t* runner) {
    delete runner;
}

quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script) {
    if (!runner || !script) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        runner->last_error.clear();
        runner->runner.run(script);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        runner->last_error = e.what();
        return QUIVER_ERROR_DATABASE;
    } catch (...) {
        runner->last_error = "Unknown error";
        return QUIVER_ERROR_DATABASE;
    }
}

const char* quiver_lua_runner_get_error(quiver_lua_runner_t* runner) {
    if (!runner || runner->last_error.empty()) {
        return nullptr;
    }
    return runner->last_error.c_str();
}
