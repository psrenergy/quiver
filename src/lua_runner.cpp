#include "quiver/lua_runner.h"

#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/options.h"
#include "quiver/value.h"

#include <map>
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
        // NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
        // deduction
        auto bind = lua.new_usertype<Database>(
            "Database",
            "delete_element",
            [](Database& self, const std::string& collection, int64_t id) { self.delete_element(collection, id); },
            "describe",
            [](Database& self) { self.describe(); },
            // Group 1: Database info
            "is_healthy",
            [](Database& self) { return self.is_healthy(); },
            "current_version",
            [](Database& self) { return self.current_version(); },
            "path",
            [](Database& self) -> const std::string& { return self.path(); },
            // Group 9: Time series files
            "has_time_series_files",
            [](Database& self, const std::string& collection) { return self.has_time_series_files(collection); },
            // Group 10: Transactions
            "begin_transaction",
            [](Database& self) { self.begin_transaction(); },
            "commit",
            [](Database& self) { self.commit(); },
            "rollback",
            [](Database& self) { self.rollback(); },
            "in_transaction",
            [](Database& self) { return self.in_transaction(); },
            "transaction",
            [](Database& self, sol::protected_function fn) -> sol::object {
                self.begin_transaction();
                auto result = fn(std::ref(self));
                if (!result.valid()) {
                    sol::error err = result;
                    try {
                        self.rollback();
                    } catch (...) {
                    }
                    throw std::runtime_error(err.what());
                }
                self.commit();
                if (result.return_count() > 0) {
                    return result.get<sol::object>(0);
                }
                return sol::make_object(result.lua_state(), sol::lua_nil);
            },
            // Group 11: CSV export
            "export_csv",
            [](Database& self,
               const std::string& collection,
               const std::string& group,
               const std::string& path,
               sol::optional<sol::table> options_table) {
                CSVOptions options;
                if (options_table) {
                    auto& t = *options_table;
                    if (auto fmt = t.get<sol::optional<std::string>>("date_time_format")) {
                        options.date_time_format = *fmt;
                    }
                    if (auto enums = t.get<sol::optional<sol::table>>("enum_labels")) {
                        enums->for_each([&](sol::object attr_key, sol::object attr_value) {
                            auto attr_name = attr_key.as<std::string>();
                            auto& attr_locale_map = options.enum_labels[attr_name];
                            attr_value.as<sol::table>().for_each([&](sol::object locale_key, sol::object locale_value) {
                                auto locale_name = locale_key.as<std::string>();
                                auto& label_map = attr_locale_map[locale_name];
                                locale_value.as<sol::table>().for_each([&](sol::object k, sol::object v) {
                                    label_map[k.as<std::string>()] = v.as<int64_t>();
                                });
                            });
                        });
                    }
                }
                self.export_csv(collection, group, path, options);
            },
            // Group 12: CSV import
            "import_csv",
            [](Database& self,
               const std::string& collection,
               const std::string& group,
               const std::string& path,
               sol::optional<sol::table> options_table) {
                CSVOptions options;
                if (options_table) {
                    auto& t = *options_table;
                    if (auto fmt = t.get<sol::optional<std::string>>("date_time_format")) {
                        options.date_time_format = *fmt;
                    }
                    if (auto enums = t.get<sol::optional<sol::table>>("enum_labels")) {
                        enums->for_each([&](sol::object attr_key, sol::object attr_value) {
                            auto attr_name = attr_key.as<std::string>();
                            auto& attr_locale_map = options.enum_labels[attr_name];
                            attr_value.as<sol::table>().for_each([&](sol::object locale_key, sol::object locale_value) {
                                auto locale_name = locale_key.as<std::string>();
                                auto& label_map = attr_locale_map[locale_name];
                                locale_value.as<sol::table>().for_each([&](sol::object k, sol::object v) {
                                    label_map[k.as<std::string>()] = v.as<int64_t>();
                                });
                            });
                        });
                    }
                }
                self.import_csv(collection, group, path, options);
            });
        // NOLINTEND(performance-unnecessary-value-param)

        bind.set_function("create_element", &create_element_lua);

        bind.set_function("read_element_ids", &read_element_ids_lua);

        bind.set_function("read_scalar_strings", &read_scalar_strings_lua);
        bind.set_function("read_scalar_integers", &read_scalar_integers_lua);
        bind.set_function("read_scalar_floats", &read_scalar_floats_lua);

        bind.set_function("read_vector_integers", &read_vector_integers_lua);
        bind.set_function("read_vector_floats", &read_vector_floats_lua);
        bind.set_function("read_vector_strings", &read_vector_strings_lua);

        bind.set_function("read_set_integers", &read_set_integers_lua);
        bind.set_function("read_set_floats", &read_set_floats_lua);
        bind.set_function("read_set_strings", &read_set_strings_lua);

        bind.set_function("read_time_series_group", &read_time_series_group_lua);
        bind.set_function("read_time_series_files", &read_time_series_files_lua);

        bind.set_function("read_scalars_by_id", &read_scalars_by_id_lua);
        bind.set_function("read_vectors_by_id", &read_vectors_by_id_lua);
        bind.set_function("read_sets_by_id", &read_sets_by_id_lua);
        bind.set_function("read_element_by_id", &read_element_by_id_lua);

        bind.set_function("update_element", &update_element_lua);
        bind.set_function("update_time_series_group", &update_time_series_group_lua);
        bind.set_function("update_time_series_files", &update_time_series_files_lua);

        bind.set_function("get_scalar_metadata", &get_scalar_metadata_lua);
        bind.set_function("get_vector_metadata", &get_vector_metadata_lua);
        bind.set_function("get_set_metadata", &get_set_metadata_lua);
        bind.set_function("get_time_series_metadata", &get_time_series_metadata_lua);

        bind.set_function("list_scalar_attributes", &list_scalar_metadata_lua);
        bind.set_function("list_vector_groups", &list_vector_metadata_lua);
        bind.set_function("list_set_groups", &list_set_metadata_lua);
        bind.set_function("list_time_series_groups", &list_time_series_groups_lua);
        bind.set_function("list_time_series_files_columns", &list_time_series_files_columns_lua);

        bind.set_function("query_string", &query_string_lua);
        bind.set_function("query_integer", &query_integer_lua);
        bind.set_function("query_float", &query_float_lua);
    }

    // ========================================================================
    // Conversion helpers
    // ========================================================================

    static std::vector<int64_t> lua_table_to_int64_vector(const sol::table& t) {
        std::vector<int64_t> result;
        for (size_t i = 1; i <= t.size(); ++i) {
            result.push_back(t.get<int64_t>(i));
        }
        return result;
    }

    static std::vector<double> lua_table_to_double_vector(const sol::table& t) {
        std::vector<double> result;
        for (size_t i = 1; i <= t.size(); ++i) {
            result.push_back(t.get<double>(i));
        }
        return result;
    }

    static std::vector<std::string> lua_table_to_string_vector(const sol::table& t) {
        std::vector<std::string> result;
        for (size_t i = 1; i <= t.size(); ++i) {
            result.push_back(t.get<std::string>(i));
        }
        return result;
    }

    static sol::object value_to_lua_object(sol::state_view& lua, const Value& val) {
        return std::visit(
            [&](auto&& arg) -> sol::object {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    return sol::make_object(lua, sol::lua_nil);
                } else {
                    return sol::make_object(lua, arg);
                }
            },
            val);
    }

    static std::map<std::string, Value> lua_table_to_value_map(const sol::table& t) {
        std::map<std::string, Value> result;
        for (auto& pair : t) {
            auto key = pair.first.as<std::string>();
            sol::object val = pair.second;
            if (val.is<sol::lua_nil_t>()) {
                result[key] = nullptr;
            } else if (val.is<int64_t>()) {
                result[key] = val.as<int64_t>();
            } else if (val.is<double>()) {
                result[key] = val.as<double>();
            } else if (val.is<std::string>()) {
                result[key] = val.as<std::string>();
            }
        }
        return result;
    }

    static Element table_to_element(const sol::table& values) {
        Element element;
        for (const auto& pair : values) {
            auto key = pair.first;
            auto val = pair.second;
            auto k = key.as<std::string>();

            if (val.is<sol::table>()) {
                auto arr = val.as<sol::table>();
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
    create_element_lua(Database& db, const std::string& collection, const sol::table& values, sol::this_state) {
        auto element = table_to_element(values);
        return db.create_element(collection, element);
    }

    static void update_element_lua(Database& db, const std::string& collection, int64_t id, const sol::table& values) {
        auto element = table_to_element(values);
        db.update_element(collection, id, element);
    }

    static sol::table read_scalar_strings_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& attribute,
                                              sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_strings(collection, attribute);
        auto t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_scalar_integers_lua(Database& db,
                                               const std::string& collection,
                                               const std::string& attribute,
                                               sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_integers(collection, attribute);
        auto t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_scalar_floats_lua(Database& db,
                                             const std::string& collection,
                                             const std::string& attribute,
                                             sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_scalar_floats(collection, attribute);
        auto t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table read_vector_integers_lua(Database& db,
                                               const std::string& collection,
                                               const std::string& attribute,
                                               sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_integers(collection, attribute);
        auto outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            auto inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    static sol::table read_vector_floats_lua(Database& db,
                                             const std::string& collection,
                                             const std::string& attribute,
                                             sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_floats(collection, attribute);
        auto outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            auto inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    static sol::table read_vector_strings_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& attribute,
                                              sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_vector_strings(collection, attribute);
        auto outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            auto inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    static sol::table read_element_ids_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_element_ids(collection);
        auto t = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            t[i + 1] = result[i];
        }
        return t;
    }

    static sol::table list_scalar_metadata_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto metadata_list = db.list_scalar_attributes(collection);
        auto t = lua.create_table();
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            t[i + 1] = scalar_metadata_lua(lua, metadata_list[i]);
        }
        return t;
    }

    static sol::table list_vector_metadata_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto metadata_list = db.list_vector_groups(collection);
        auto t = lua.create_table();
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            auto metadata = lua.create_table();
            metadata["group_name"] = metadata_list[i].group_name;
            auto cols = lua.create_table();
            for (size_t j = 0; j < metadata_list[i].value_columns.size(); ++j) {
                cols[j + 1] = scalar_metadata_lua(lua, metadata_list[i].value_columns[j]);
            }
            metadata["value_columns"] = cols;
            t[i + 1] = metadata;
        }
        return t;
    }

    static sol::table list_set_metadata_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto metadata_list = db.list_set_groups(collection);
        auto t = lua.create_table();
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            auto metadata = lua.create_table();
            metadata["group_name"] = metadata_list[i].group_name;
            auto cols = lua.create_table();
            for (size_t j = 0; j < metadata_list[i].value_columns.size(); ++j) {
                cols[j + 1] = scalar_metadata_lua(lua, metadata_list[i].value_columns[j]);
            }
            metadata["value_columns"] = cols;
            t[i + 1] = metadata;
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
        default:
            throw std::runtime_error("Cannot data_type_to_string: unknown data type " + std::to_string(static_cast<int>(type)));
        }
    }

    static sol::table get_scalar_metadata_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& attribute,
                                              sol::this_state s) {
        sol::state_view lua(s);
        auto metadata = db.get_scalar_metadata(collection, attribute);
        auto t = lua.create_table();
        t["name"] = metadata.name;
        t["data_type"] = data_type_to_string(metadata.data_type);
        t["not_null"] = metadata.not_null;
        t["primary_key"] = metadata.primary_key;
        if (metadata.default_value.has_value()) {
            t["default_value"] = *metadata.default_value;
        } else {
            t["default_value"] = sol::lua_nil;
        }
        return t;
    }

    static sol::table scalar_metadata_lua(sol::state_view& lua, const ScalarMetadata& attribute) {
        auto t = lua.create_table();
        t["name"] = attribute.name;
        t["data_type"] = data_type_to_string(attribute.data_type);
        t["not_null"] = attribute.not_null;
        t["primary_key"] = attribute.primary_key;
        if (attribute.default_value.has_value()) {
            t["default_value"] = *attribute.default_value;
        } else {
            t["default_value"] = sol::lua_nil;
        }
        return t;
    }

    static sol::table get_vector_metadata_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& group_name,
                                              sol::this_state s) {
        sol::state_view lua(s);
        auto metadata = db.get_vector_metadata(collection, group_name);
        auto t = lua.create_table();
        t["group_name"] = metadata.group_name;

        auto cols = lua.create_table();
        for (size_t i = 0; i < metadata.value_columns.size(); ++i) {
            cols[i + 1] = scalar_metadata_lua(lua, metadata.value_columns[i]);
        }
        t["value_columns"] = cols;

        return t;
    }

    static sol::table get_set_metadata_lua(Database& db,
                                           const std::string& collection,
                                           const std::string& group_name,
                                           sol::this_state s) {
        sol::state_view lua(s);
        auto metadata = db.get_set_metadata(collection, group_name);
        auto t = lua.create_table();
        t["group_name"] = metadata.group_name;

        auto cols = lua.create_table();
        for (size_t i = 0; i < metadata.value_columns.size(); ++i) {
            cols[i + 1] = scalar_metadata_lua(lua, metadata.value_columns[i]);
        }
        t["value_columns"] = cols;

        return t;
    }

    static std::vector<Value> lua_table_to_values(const sol::table& params) {
        std::vector<Value> values;
        for (size_t i = 1; i <= params.size(); ++i) {
            sol::object val = params[i];
            if (val.is<sol::lua_nil_t>()) {
                values.emplace_back(nullptr);
            } else if (val.is<int64_t>()) {
                values.emplace_back(val.as<int64_t>());
            } else if (val.is<double>()) {
                values.emplace_back(val.as<double>());
            } else if (val.is<std::string>()) {
                values.emplace_back(val.as<std::string>());
            }
        }
        return values;
    }

    static sol::object
    query_string_lua(Database& db, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
        sol::state_view lua(s);
        auto values = params ? lua_table_to_values(*params) : std::vector<Value>{};
        auto result = db.query_string(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    static sol::object
    query_integer_lua(Database& db, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
        sol::state_view lua(s);
        auto values = params ? lua_table_to_values(*params) : std::vector<Value>{};
        auto result = db.query_integer(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    static sol::object
    query_float_lua(Database& db, const std::string& sql, sol::optional<sol::table> params, sol::this_state s) {
        sol::state_view lua(s);
        auto values = params ? lua_table_to_values(*params) : std::vector<Value>{};
        auto result = db.query_float(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    static sol::table
    read_scalars_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
        sol::state_view lua(s);
        auto result = lua.create_table();

        for (const auto& attribute : db.list_scalar_attributes(collection)) {
            switch (attribute.data_type) {
            case DataType::Integer: {
                auto val = db.read_scalar_integer_by_id(collection, attribute.name, id);
                result[attribute.name] = val.has_value() ? sol::make_object(lua, *val) : sol::lua_nil;
                break;
            }
            case DataType::Real: {
                auto val = db.read_scalar_float_by_id(collection, attribute.name, id);
                result[attribute.name] = val.has_value() ? sol::make_object(lua, *val) : sol::lua_nil;
                break;
            }
            case DataType::Text:
            case DataType::DateTime: {
                auto val = db.read_scalar_string_by_id(collection, attribute.name, id);
                result[attribute.name] = val.has_value() ? sol::make_object(lua, *val) : sol::lua_nil;
                break;
            }
            default:
                throw std::runtime_error("Cannot read_scalars_by_id: unknown data type " + std::to_string(static_cast<int>(attribute.data_type)));
            }
        }
        return result;
    }

    static sol::table
    read_vectors_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
        sol::state_view lua(s);
        auto result = lua.create_table();

        for (const auto& group : db.list_vector_groups(collection)) {
            for (const auto& col : group.value_columns) {
                auto t = lua.create_table();
                switch (col.data_type) {
                case DataType::Integer: {
                    auto values = db.read_vector_integers_by_id(collection, col.name, id);
                    for (size_t i = 0; i < values.size(); ++i)
                        t[i + 1] = values[i];
                    break;
                }
                case DataType::Real: {
                    auto values = db.read_vector_floats_by_id(collection, col.name, id);
                    for (size_t i = 0; i < values.size(); ++i)
                        t[i + 1] = values[i];
                    break;
                }
                case DataType::Text:
                case DataType::DateTime: {
                    auto values = db.read_vector_strings_by_id(collection, col.name, id);
                    for (size_t i = 0; i < values.size(); ++i)
                        t[i + 1] = values[i];
                    break;
                }
                default:
                    throw std::runtime_error("Cannot read_vectors_by_id: unknown data type " + std::to_string(static_cast<int>(col.data_type)));
                }
                result[col.name] = t;
            }
        }
        return result;
    }

    static sol::table read_sets_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
        sol::state_view lua(s);
        auto result = lua.create_table();

        for (const auto& group : db.list_set_groups(collection)) {
            for (const auto& col : group.value_columns) {
                auto t = lua.create_table();
                switch (col.data_type) {
                case DataType::Integer: {
                    auto values = db.read_set_integers_by_id(collection, col.name, id);
                    for (size_t i = 0; i < values.size(); ++i)
                        t[i + 1] = values[i];
                    break;
                }
                case DataType::Real: {
                    auto values = db.read_set_floats_by_id(collection, col.name, id);
                    for (size_t i = 0; i < values.size(); ++i)
                        t[i + 1] = values[i];
                    break;
                }
                case DataType::Text:
                case DataType::DateTime: {
                    auto values = db.read_set_strings_by_id(collection, col.name, id);
                    for (size_t i = 0; i < values.size(); ++i)
                        t[i + 1] = values[i];
                    break;
                }
                default:
                    throw std::runtime_error("Cannot read_sets_by_id: unknown data type " + std::to_string(static_cast<int>(col.data_type)));
                }
                result[col.name] = t;
            }
        }
        return result;
    }

    static sol::table
    read_element_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
        auto scalars = read_scalars_by_id_lua(db, collection, id, s);
        auto vectors = read_vectors_by_id_lua(db, collection, id, s);
        auto sets = read_sets_by_id_lua(db, collection, id, s);

        // Merge vectors and sets into scalars
        for (auto& pair : vectors) {
            scalars[pair.first] = pair.second;
        }
        for (auto& pair : sets) {
            scalars[pair.first] = pair.second;
        }
        return scalars;
    }

    // ========================================================================
    // Bulk set reads (same pattern as read_vector_*_lua)
    // ========================================================================

    static sol::table read_set_integers_lua(Database& db,
                                            const std::string& collection,
                                            const std::string& attribute,
                                            sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_set_integers(collection, attribute);
        auto outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            auto inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    static sol::table
    read_set_floats_lua(Database& db, const std::string& collection, const std::string& attribute, sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_set_floats(collection, attribute);
        auto outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            auto inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    static sol::table
    read_set_strings_lua(Database& db, const std::string& collection, const std::string& attribute, sol::this_state s) {
        sol::state_view lua(s);
        auto result = db.read_set_strings(collection, attribute);
        auto outer = lua.create_table();
        for (size_t i = 0; i < result.size(); ++i) {
            auto inner = lua.create_table();
            for (size_t j = 0; j < result[i].size(); ++j) {
                inner[j + 1] = result[i][j];
            }
            outer[i + 1] = inner;
        }
        return outer;
    }

    // ========================================================================
    // Time series metadata
    // ========================================================================

    static sol::table time_series_metadata_lua(sol::state_view& lua, const GroupMetadata& metadata) {
        auto t = lua.create_table();
        t["group_name"] = metadata.group_name;
        t["dimension_column"] = metadata.dimension_column;
        auto cols = lua.create_table();
        for (size_t i = 0; i < metadata.value_columns.size(); ++i) {
            cols[i + 1] = scalar_metadata_lua(lua, metadata.value_columns[i]);
        }
        t["value_columns"] = cols;
        return t;
    }

    static sol::table get_time_series_metadata_lua(Database& db,
                                                   const std::string& collection,
                                                   const std::string& group_name,
                                                   sol::this_state s) {
        sol::state_view lua(s);
        auto metadata = db.get_time_series_metadata(collection, group_name);
        return time_series_metadata_lua(lua, metadata);
    }

    static sol::table list_time_series_groups_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto metadata_list = db.list_time_series_groups(collection);
        auto t = lua.create_table();
        for (size_t i = 0; i < metadata_list.size(); ++i) {
            t[i + 1] = time_series_metadata_lua(lua, metadata_list[i]);
        }
        return t;
    }

    // ========================================================================
    // Time series data
    // ========================================================================

    static sol::table read_time_series_group_lua(Database& db,
                                                 const std::string& collection,
                                                 const std::string& group,
                                                 int64_t id,
                                                 sol::this_state s) {
        sol::state_view lua(s);
        auto rows = db.read_time_series_group(collection, group, id);
        auto t = lua.create_table();
        for (size_t i = 0; i < rows.size(); ++i) {
            auto row = lua.create_table();
            for (const auto& [key, val] : rows[i]) {
                row[key] = value_to_lua_object(lua, val);
            }
            t[i + 1] = row;
        }
        return t;
    }

    static void update_time_series_group_lua(Database& db,
                                             const std::string& collection,
                                             const std::string& group,
                                             int64_t id,
                                             sol::table rows) {
        std::vector<std::map<std::string, Value>> cpp_rows;
        for (size_t i = 1; i <= rows.size(); ++i) {
            sol::table row = rows[i];
            cpp_rows.push_back(lua_table_to_value_map(row));
        }
        db.update_time_series_group(collection, group, id, cpp_rows);
    }

    // ========================================================================
    // Time series files
    // ========================================================================

    static sol::table
    list_time_series_files_columns_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto columns = db.list_time_series_files_columns(collection);
        auto t = lua.create_table();
        for (size_t i = 0; i < columns.size(); ++i) {
            t[i + 1] = columns[i];
        }
        return t;
    }

    static sol::table read_time_series_files_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        auto files = db.read_time_series_files(collection);
        auto t = lua.create_table();
        for (const auto& [key, val] : files) {
            if (val.has_value()) {
                t[key] = *val;
            } else {
                t[key] = sol::lua_nil;
            }
        }
        return t;
    }

    static void update_time_series_files_lua(Database& db, const std::string& collection, const sol::table& paths) {
        std::map<std::string, std::optional<std::string>> cpp_paths;
        for (auto& pair : paths) {
            auto key = pair.first.as<std::string>();
            sol::object val = pair.second;
            if (val.is<sol::lua_nil_t>()) {
                cpp_paths[key] = std::nullopt;
            } else {
                cpp_paths[key] = val.as<std::string>();
            }
        }
        db.update_time_series_files(collection, cpp_paths);
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
