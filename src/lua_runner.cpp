#include "psr/lua_runner.h"

#include "psr/database.h"
#include "psr/element.h"

#include <sol/sol.hpp>
#include <stdexcept>

namespace psr {

struct LuaRunner::Impl {
    Database& db;
    sol::state lua;

    explicit Impl(Database& database) : db(database) {
        lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);
        bind_database();
        lua["db"] = &db;
    }

    void bind_database() {
        lua.new_usertype<Database>(
            "Database",
            "create_element",
            [](Database& self, const std::string& collection, sol::table values, sol::this_state s) {
                return create_element_from_lua(self, collection, values, s);
            },
            "read_scalar_strings",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_scalar_strings_to_lua(self, collection, attribute, s);
            },
            "read_scalar_integers",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_scalar_integers_to_lua(self, collection, attribute, s);
            },
            "read_scalar_doubles",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_scalar_doubles_to_lua(self, collection, attribute, s);
            },
            "read_vector_integers",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_vector_integers_to_lua(self, collection, attribute, s);
            },
            "read_vector_doubles",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_vector_doubles_to_lua(self, collection, attribute, s);
            },
            "read_vector_strings",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_vector_strings_to_lua(self, collection, attribute, s);
            });
    }

    static int64_t
    create_element_from_lua(Database& db, const std::string& collection, sol::table values, sol::this_state) {
        Element element;
        for (auto pair : values) {
            sol::object key = pair.first;
            sol::object val = pair.second;
            std::string k = key.as<std::string>();

            if (val.is<sol::table>()) {
                sol::table arr = val.as<sol::table>();
                if (arr.size() > 0) {
                    sol::object first = arr[1];
                    if (first.is<int64_t>()) {
                        std::vector<int64_t> vec;
                        for (size_t i = 1; i <= arr.size(); ++i) {
                            vec.push_back(arr.get<int64_t>(i));
                        }
                        element.set(k, vec);
                    } else if (first.is<double>()) {
                        std::vector<double> vec;
                        for (size_t i = 1; i <= arr.size(); ++i) {
                            vec.push_back(arr.get<double>(i));
                        }
                        element.set(k, vec);
                    } else if (first.is<std::string>()) {
                        std::vector<std::string> vec;
                        for (size_t i = 1; i <= arr.size(); ++i) {
                            vec.push_back(arr.get<std::string>(i));
                        }
                        element.set(k, vec);
                    }
                }
            } else if (val.is<int64_t>()) {
                element.set(k, val.as<int64_t>());
            } else if (val.is<double>()) {
                element.set(k, val.as<double>());
            } else if (val.is<std::string>()) {
                element.set(k, val.as<std::string>());
            }
        }
        return db.create_element(collection, element);
    }

    static sol::table read_scalar_strings_to_lua(Database& db,
                                                 const std::string& collection,
                                                 const std::string& attribute,
                                                 sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_strings(collection, attribute);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_scalar_integers_to_lua(Database& db,
                                                  const std::string& collection,
                                                  const std::string& attribute,
                                                  sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_integers(collection, attribute);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_scalar_doubles_to_lua(Database& db,
                                                 const std::string& collection,
                                                 const std::string& attribute,
                                                 sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_doubles(collection, attribute);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_vector_integers_to_lua(Database& db,
                                                  const std::string& collection,
                                                  const std::string& attribute,
                                                  sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_integers(collection, attribute);
        sol::table outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            sol::table inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    static sol::table read_vector_doubles_to_lua(Database& db,
                                                 const std::string& collection,
                                                 const std::string& attribute,
                                                 sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_doubles(collection, attribute);
        sol::table outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            sol::table inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    static sol::table read_vector_strings_to_lua(Database& db,
                                                 const std::string& collection,
                                                 const std::string& attribute,
                                                 sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_strings(collection, attribute);
        sol::table outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            sol::table inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }
};

LuaRunner::LuaRunner(Database& db) : impl_(std::make_unique<Impl>(db)) {}

LuaRunner::~LuaRunner() = default;

LuaRunner::LuaRunner(LuaRunner&&) noexcept = default;

LuaRunner& LuaRunner::operator=(LuaRunner&&) noexcept = default;

void LuaRunner::run(const std::string& script) {
    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Lua error: ") + err.what());
    }
}

}  // namespace psr
