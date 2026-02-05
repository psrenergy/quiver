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

quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner) {
    if (!db || !out_runner) {
        quiver_set_last_error("Null argument");
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        *out_runner = new quiver_lua_runner(db->db);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    } catch (...) {
        quiver_set_last_error("Unknown error creating LuaRunner");
        return QUIVER_ERROR_DATABASE;
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
