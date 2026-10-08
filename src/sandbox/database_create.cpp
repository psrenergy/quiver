#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/value.h"
#include "sandbox/internal.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace quiver::lua_internal {

namespace {

// A vector/set read hands a NULL cell back as a nil hole, and `#` over a hole is an arbitrary
// border: lua_table_to_vector (bounded by t.size()) would silently cut such an array short, and
// table_to_element would read it as empty when the hole is cell 1. Element arrays stay dense; the
// group writers are the ones that write a hole as NULL.
void require_dense_array(const std::string& caller, const sol::table& arr, const std::string& name) {
    const size_t count = arr.size();
    size_t entries = 0;
    bool valid_keys = true;
    for (const auto& entry : arr) {
        const auto key = entry.first.as<sol::optional<int64_t>>();
        if (!key || *key < 1 || static_cast<size_t>(*key) > count) {
            valid_keys = false;
        }
        ++entries;
    }
    if (!valid_keys || entries != count) {
        throw std::runtime_error(
            "Cannot " + caller + ": array '" + name +
            "' has a nil hole or a non-integer key; write NULL cells with "
            "update_vector_group or update_set_group"
        );
    }
}

}  // namespace

Element table_to_element(const std::string& caller, const sol::object& values) {
    Element element;
    for (const auto& pair : require_table(values, caller, "element_table")) {
        auto key = pair.first;
        auto val = pair.second;
        auto k = lua_string_key(key, caller, "attribute name");

        // A userdata is neither a value nor an array; sol2's loose table test took it for an array.
        if (val.get_type() == sol::type::userdata) {
            throw lua_type_error(caller, "attribute '" + k + "'", "a value or a table", val);
        }
        if (val.get_type() == sol::type::table) {
            auto arr = val.as<sol::table>();
            require_dense_array(caller, arr, k);
            const size_t count = arr.size();
            std::vector<Value> cells;
            cells.reserve(count);
            const std::string array_caller = caller + ": array '" + k + "'";
            for (size_t i = 1; i <= count; ++i) {
                cells.push_back(lua_to_value(arr.get<sol::object>(i), array_caller, "cell #" + std::to_string(i)));
            }
            element.set(k, cells);
        } else {
            std::visit(
                [&](auto&& x) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::nullptr_t>) {
                        element.set_null(k);
                    } else {
                        element.set(k, x);
                    }
                },
                lua_to_value(val, caller, "attribute '" + k + "'")
            );
        }
    }
    return element;
}

namespace {

int64_t create_element_lua(Database& db, const std::string& collection, const sol::object& values) {
    auto element = table_to_element("create_element", values);
    return db.create_element(collection, element);
}

}  // namespace

void bind_create(sol::usertype<Database>& bind) {
    bind.set_function("create_element", &create_element_lua);
}

}  // namespace quiver::lua_internal
