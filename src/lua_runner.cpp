#include "quiver/lua_runner.h"

#include "quiver/database.h"
#include "quiver/element.h"

#include <sol/sol.hpp>
#include <stdexcept>

namespace quiver {

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
            "read_scalar_floats",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_scalar_floats_to_lua(self, collection, attribute, s);
            },
            "read_vector_integers",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_vector_integers_to_lua(self, collection, attribute, s);
            },
            "read_vector_floats",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_vector_floats_to_lua(self, collection, attribute, s);
            },
            "read_vector_strings",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return read_vector_strings_to_lua(self, collection, attribute, s);
            },
            "read_scalar_string_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_scalar_string_by_id_to_lua(self, collection, attribute, id, s); },
            "read_scalar_integer_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_scalar_integer_by_id_to_lua(self, collection, attribute, id, s); },
            "read_scalar_float_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_scalar_float_by_id_to_lua(self, collection, attribute, id, s); },
            "read_vector_integers_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_vector_integers_by_id_to_lua(self, collection, attribute, id, s); },
            "read_vector_floats_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_vector_floats_by_id_to_lua(self, collection, attribute, id, s); },
            "read_vector_strings_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_vector_strings_by_id_to_lua(self, collection, attribute, id, s); },
            "read_set_integers_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_set_integers_by_id_to_lua(self, collection, attribute, id, s); },
            "read_set_floats_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_set_floats_by_id_to_lua(self, collection, attribute, id, s); },
            "read_set_strings_by_id",
            [](Database& self,
               const std::string& collection,
               const std::string& attribute,
               int64_t id,
               sol::this_state s) { return read_set_strings_by_id_to_lua(self, collection, attribute, id, s); },
            "read_element_ids",
            [](Database& self, const std::string& collection, sol::this_state s) {
                return read_element_ids_to_lua(self, collection, s);
            },
            "delete_element_by_id",
            [](Database& self, const std::string& collection, int64_t id) {
                self.delete_element_by_id(collection, id);
            },
            "update_element",
            [](Database& self, const std::string& collection, int64_t id, sol::table values) {
                update_element_from_lua(self, collection, id, values);
            },
            "get_scalar_metadata",
            [](Database& self, const std::string& collection, const std::string& attribute, sol::this_state s) {
                return get_scalar_metadata_to_lua(self, collection, attribute, s);
            },
            "get_vector_metadata",
            [](Database& self, const std::string& collection, const std::string& group_name, sol::this_state s) {
                return get_vector_metadata_to_lua(self, collection, group_name, s);
            },
            "get_set_metadata",
            [](Database& self, const std::string& collection, const std::string& group_name, sol::this_state s) {
                return get_set_metadata_to_lua(self, collection, group_name, s);
            },
            "list_scalar_attributes",
            [](Database& self, const std::string& collection, sol::this_state s) {
                return list_scalar_metadata_to_lua(self, collection, s);
            },
            "list_vector_groups",
            [](Database& self, const std::string& collection, sol::this_state s) {
                return list_vector_metadata_to_lua(self, collection, s);
            },
            "list_set_groups",
            [](Database& self, const std::string& collection, sol::this_state s) {
                return list_set_metadata_to_lua(self, collection, s);
            },
            "read_all_scalars_by_id",
            [](Database& self, const std::string& collection, int64_t id, sol::this_state s) {
                return read_all_scalars_by_id_to_lua(self, collection, id, s);
            },
            "read_all_vectors_by_id",
            [](Database& self, const std::string& collection, int64_t id, sol::this_state s) {
                return read_all_vectors_by_id_to_lua(self, collection, id, s);
            },
            "read_all_sets_by_id",
            [](Database& self, const std::string& collection, int64_t id, sol::this_state s) {
                return read_all_sets_by_id_to_lua(self, collection, id, s);
            },
            "query_string",
            [](Database& self, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
                return query_string_to_lua(self, sql, params, s);
            },
            "query_integer",
            [](Database& self, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
                return query_integer_to_lua(self, sql, params, s);
            },
            "query_float",
            [](Database& self, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
                return query_float_to_lua(self, sql, params, s);
            },
            "describe",
            [](Database& self) { self.describe(); });
    }

    static Element table_to_element(sol::table values) {
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
        return element;
    }

    static int64_t
    create_element_from_lua(Database& db, const std::string& collection, sol::table values, sol::this_state) {
        Element element = table_to_element(values);
        return db.create_element(collection, element);
    }

    static void update_element_from_lua(Database& db, const std::string& collection, int64_t id, sol::table values) {
        Element element = table_to_element(values);
        db.update_element(collection, id, element);
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

    static sol::table read_scalar_floats_to_lua(Database& db,
                                                const std::string& collection,
                                                const std::string& attribute,
                                                sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_floats(collection, attribute);
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

    static sol::table read_vector_floats_to_lua(Database& db,
                                                const std::string& collection,
                                                const std::string& attribute,
                                                sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_floats(collection, attribute);
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

    // Read scalar by ID helpers - return nil if not found
    static sol::object read_scalar_string_by_id_to_lua(Database& db,
                                                        const std::string& collection,
                                                        const std::string& attribute,
                                                        int64_t id,
                                                        sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_string_by_id(collection, attribute, id);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::nil);
    }

    static sol::object read_scalar_integer_by_id_to_lua(Database& db,
                                                         const std::string& collection,
                                                         const std::string& attribute,
                                                         int64_t id,
                                                         sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_integer_by_id(collection, attribute, id);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::nil);
    }

    static sol::object read_scalar_float_by_id_to_lua(Database& db,
                                                       const std::string& collection,
                                                       const std::string& attribute,
                                                       int64_t id,
                                                       sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_float_by_id(collection, attribute, id);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::nil);
    }

    // Read vector by ID helpers - return table
    static sol::table read_vector_integers_by_id_to_lua(Database& db,
                                                        const std::string& collection,
                                                        const std::string& attribute,
                                                        int64_t id,
                                                        sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_integers_by_id(collection, attribute, id);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_vector_floats_by_id_to_lua(Database& db,
                                                      const std::string& collection,
                                                      const std::string& attribute,
                                                      int64_t id,
                                                      sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_floats_by_id(collection, attribute, id);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_vector_strings_by_id_to_lua(Database& db,
                                                       const std::string& collection,
                                                       const std::string& attribute,
                                                       int64_t id,
                                                       sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_strings_by_id(collection, attribute, id);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    // Read set by ID helpers - return table
    static sol::table read_set_integers_by_id_to_lua(Database& db,
                                                     const std::string& collection,
                                                     const std::string& attribute,
                                                     int64_t id,
                                                     sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_set_integers_by_id(collection, attribute, id);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_set_floats_by_id_to_lua(Database& db,
                                                   const std::string& collection,
                                                   const std::string& attribute,
                                                   int64_t id,
                                                   sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_set_floats_by_id(collection, attribute, id);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_set_strings_by_id_to_lua(Database& db,
                                                    const std::string& collection,
                                                    const std::string& attribute,
                                                    int64_t id,
                                                    sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_set_strings_by_id(collection, attribute, id);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_element_ids_to_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_element_ids(collection);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table list_scalar_metadata_to_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto metadata_list = db.list_scalar_attributes(collection);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            t[i + 1] = scalar_metadata_to_lua(lua, metadata_list[i]);
        }
        return t;
    }

    static sol::table list_vector_metadata_to_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto metadata_list = db.list_vector_groups(collection);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            sol::table meta = lua.create_table();
            meta["group_name"] = metadata_list[i].group_name;
            sol::table cols = lua.create_table();
            for (size_t j = 0; j < metadata_list[i].value_columns.size(); ++j) {
                cols[j + 1] = scalar_metadata_to_lua(lua, metadata_list[i].value_columns[j]);
            }
            meta["value_columns"] = cols;
            t[i + 1] = meta;
        }
        return t;
    }

    static sol::table list_set_metadata_to_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto metadata_list = db.list_set_groups(collection);
        sol::table t = lua.create_table();
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            sol::table meta = lua.create_table();
            meta["group_name"] = metadata_list[i].group_name;
            sol::table cols = lua.create_table();
            for (size_t j = 0; j < metadata_list[i].value_columns.size(); ++j) {
                cols[j + 1] = scalar_metadata_to_lua(lua, metadata_list[i].value_columns[j]);
            }
            meta["value_columns"] = cols;
            t[i + 1] = meta;
        }
        return t;
    }

    static std::string data_type_to_string(DataType type) {
        switch (type) {
        case DataType::Integer:
            return "integer";
        case DataType::Real:
            return "real";
        case DataType::Text:
            return "text";
        case DataType::DateTime:
            return "date_time";
        }
        return "unknown";
    }

    static sol::table get_scalar_metadata_to_lua(Database& db,
                                                 const std::string& collection,
                                                 const std::string& attribute,
                                                 sol::this_state s) {
        sol::state_view lua(s);
        auto meta = db.get_scalar_metadata(collection, attribute);
        sol::table t = lua.create_table();
        t["name"] = meta.name;
        t["data_type"] = data_type_to_string(meta.data_type);
        t["not_null"] = meta.not_null;
        t["primary_key"] = meta.primary_key;
        if (meta.default_value.has_value()) {
            t["default_value"] = *meta.default_value;
        } else {
            t["default_value"] = sol::nil;
        }
        return t;
    }

    static sol::table scalar_metadata_to_lua(sol::state_view& lua, const ScalarMetadata& attr) {
        sol::table t = lua.create_table();
        t["name"] = attr.name;
        t["data_type"] = data_type_to_string(attr.data_type);
        t["not_null"] = attr.not_null;
        t["primary_key"] = attr.primary_key;
        if (attr.default_value.has_value()) {
            t["default_value"] = *attr.default_value;
        } else {
            t["default_value"] = sol::nil;
        }
        return t;
    }

    static sol::table get_vector_metadata_to_lua(Database& db,
                                                 const std::string& collection,
                                                 const std::string& group_name,
                                                 sol::this_state s) {
        sol::state_view lua(s);
        auto meta = db.get_vector_metadata(collection, group_name);
        sol::table t = lua.create_table();
        t["group_name"] = meta.group_name;

        sol::table cols = lua.create_table();
        for (size_t i = 0; i < meta.value_columns.size(); ++i) {
            cols[i + 1] = scalar_metadata_to_lua(lua, meta.value_columns[i]);
        }
        t["value_columns"] = cols;

        return t;
    }

    static sol::table get_set_metadata_to_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& group_name,
                                              sol::this_state s) {
        sol::state_view lua(s);
        auto meta = db.get_set_metadata(collection, group_name);
        sol::table t = lua.create_table();
        t["group_name"] = meta.group_name;

        sol::table cols = lua.create_table();
        for (size_t i = 0; i < meta.value_columns.size(); ++i) {
            cols[i + 1] = scalar_metadata_to_lua(lua, meta.value_columns[i]);
        }
        t["value_columns"] = cols;

        return t;
    }

    static std::vector<Value> lua_table_to_values(sol::table params) {
        std::vector<Value> values;
        for (size_t i = 1; i <= params.size(); ++i) {
            sol::object val = params[i];
            if (val.is<sol::nil_t>()) {
                values.emplace_back(nullptr);
            } else if (val.is<int64_t>()) {
                values.push_back(val.as<int64_t>());
            } else if (val.is<double>()) {
                values.push_back(val.as<double>());
            } else if (val.is<std::string>()) {
                values.push_back(val.as<std::string>());
            }
        }
        return values;
    }

    static sol::object
    query_string_to_lua(Database& db, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
        sol::state_view lua(s);
        auto values = params ? lua_table_to_values(*params) : std::vector<Value>{};
        auto result = db.query_string(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::nil);
    }

    static sol::object
    query_integer_to_lua(Database& db, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
        sol::state_view lua(s);
        auto values = params ? lua_table_to_values(*params) : std::vector<Value>{};
        auto result = db.query_integer(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::nil);
    }

    static sol::object
    query_float_to_lua(Database& db, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
        sol::state_view lua(s);
        auto values = params ? lua_table_to_values(*params) : std::vector<Value>{};
        auto result = db.query_float(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::nil);
    }

    static DataType get_value_data_type(const std::vector<ScalarMetadata>& value_columns) {
        if (!value_columns.empty()) {
            return value_columns[0].data_type;
        }
        return DataType::Text;
    }

    static sol::table
    read_all_scalars_by_id_to_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
        sol::state_view lua(s);
        sol::table result = lua.create_table();

        for (const auto& attr : db.list_scalar_attributes(collection)) {
            switch (attr.data_type) {
            case DataType::Integer: {
                auto val = db.read_scalar_integer_by_id(collection, attr.name, id);
                result[attr.name] = val.has_value() ? sol::make_object(lua, *val) : sol::nil;
                break;
            }
            case DataType::Real: {
                auto val = db.read_scalar_float_by_id(collection, attr.name, id);
                result[attr.name] = val.has_value() ? sol::make_object(lua, *val) : sol::nil;
                break;
            }
            case DataType::Text:
            case DataType::DateTime: {
                auto val = db.read_scalar_string_by_id(collection, attr.name, id);
                result[attr.name] = val.has_value() ? sol::make_object(lua, *val) : sol::nil;
                break;
            }
            }
        }
        return result;
    }

    static sol::table
    read_all_vectors_by_id_to_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
        sol::state_view lua(s);
        sol::table result = lua.create_table();

        for (const auto& group : db.list_vector_groups(collection)) {
            sol::table vec = lua.create_table();
            DataType data_type = get_value_data_type(group.value_columns);
            switch (data_type) {
            case DataType::Integer: {
                auto values = db.read_vector_integers_by_id(collection, group.group_name, id);
                for (size_t i = 0; i < values.size(); ++i) {
                    vec[i + 1] = values[i];
                }
                break;
            }
            case DataType::Real: {
                auto values = db.read_vector_floats_by_id(collection, group.group_name, id);
                for (size_t i = 0; i < values.size(); ++i) {
                    vec[i + 1] = values[i];
                }
                break;
            }
            case DataType::Text:
            case DataType::DateTime: {
                auto values = db.read_vector_strings_by_id(collection, group.group_name, id);
                for (size_t i = 0; i < values.size(); ++i) {
                    vec[i + 1] = values[i];
                }
                break;
            }
            }
            result[group.group_name] = vec;
        }
        return result;
    }

    static sol::table
    read_all_sets_by_id_to_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
        sol::state_view lua(s);
        sol::table result = lua.create_table();

        for (const auto& group : db.list_set_groups(collection)) {
            sol::table set = lua.create_table();
            DataType data_type = get_value_data_type(group.value_columns);
            switch (data_type) {
            case DataType::Integer: {
                auto values = db.read_set_integers_by_id(collection, group.group_name, id);
                for (size_t i = 0; i < values.size(); ++i) {
                    set[i + 1] = values[i];
                }
                break;
            }
            case DataType::Real: {
                auto values = db.read_set_floats_by_id(collection, group.group_name, id);
                for (size_t i = 0; i < values.size(); ++i) {
                    set[i + 1] = values[i];
                }
                break;
            }
            case DataType::Text:
            case DataType::DateTime: {
                auto values = db.read_set_strings_by_id(collection, group.group_name, id);
                for (size_t i = 0; i < values.size(); ++i) {
                    set[i + 1] = values[i];
                }
                break;
            }
            }
            result[group.group_name] = set;
        }
        return result;
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

}  // namespace quiver
