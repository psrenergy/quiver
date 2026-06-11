#ifndef QUIVER_TEST_LUA_RUNNER_H
#define QUIVER_TEST_LUA_RUNNER_H

#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <quiver/lua_runner.h>
#include <string>

class LuaRunnerTest : public ::testing::Test {
protected:
    void SetUp() override { collections_schema = VALID_SCHEMA("collections.sql"); }
    std::string collections_schema;
};

#endif  // QUIVER_TEST_LUA_RUNNER_H
