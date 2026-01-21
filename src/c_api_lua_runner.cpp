#include "c_api_internal.h"
#include "margaux/c/lua_runner.h"
#include "margaux/lua_runner.h"

#include <new>
#include <string>

struct lua_runner {
    margaux::LuaRunner runner;
    std::string last_error;

    explicit lua_runner(margaux::Database& db) : runner(db) {}
};

lua_runner_t* lua_runner_new(database_t* db) {
    if (!db) {
        return nullptr;
    }
    try {
        return new lua_runner(db->db);
    } catch (...) {
        return nullptr;
    }
}

void lua_runner_free(lua_runner_t* runner) {
    delete runner;
}

margaux_error_t lua_runner_run(lua_runner_t* runner, const char* script) {
    if (!runner || !script) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        runner->last_error.clear();
        runner->runner.run(script);
        return MARGAUX_OK;
    } catch (const std::exception& e) {
        runner->last_error = e.what();
        return MARGAUX_ERROR_DATABASE;
    } catch (...) {
        runner->last_error = "Unknown error";
        return MARGAUX_ERROR_DATABASE;
    }
}

const char* lua_runner_get_error(lua_runner_t* runner) {
    if (!runner || runner->last_error.empty()) {
        return nullptr;
    }
    return runner->last_error.c_str();
}
