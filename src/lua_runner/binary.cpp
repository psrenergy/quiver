#include "lua_runner/internal.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/binary_metadata.h"
#include "quiver/binary/csv_converter.h"
#include "quiver/binary/time_properties.h"
#include "quiver/database.h"
#include "quiver/element.h"
// Kept although nothing here names Expression: sol2 derives BinaryFile's automatic __lt/__le/__eq from the
// Expression operators (through the implicit Expression(const BinaryFile&)) when the usertype is created.
#include "quiver/expression/expression.h"
#include "utils/datetime.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace quiver::lua_internal {

namespace {

std::unordered_map<std::string, int64_t> lua_table_to_dim_map(const sol::object& t, const std::string& caller) {
    std::unordered_map<std::string, int64_t> dims;
    for (auto& pair : require_table(t, caller, "dims")) {
        auto key = lua_string_key(pair.first, caller, "dimension name");
        dims[key] = lua_cell_as<int64_t>(pair.second, caller, "dimension '" + key + "'");
    }
    return dims;
}

// One quiver.metadata{...} field, as option_entries returned it (absent = disengaged).
std::string metadata_string(const std::optional<sol::object>& value, const char* key, const std::string& fallback) {
    return value ? lua_cell_as<std::string>(*value, "metadata", std::string("field '") + key + "'") : fallback;
}

template <typename T>
std::vector<T> metadata_array(const std::optional<sol::object>& value, const char* key) {
    if (!value) {
        return {};
    }
    const auto field = std::string("field '") + key + "'";
    return lua_table_to_vector<T>(require_table(*value, "metadata", field), "metadata: " + field);
}

// Build BinaryMetadata from a Lua kwargs table, mirroring the Julia Metadata(; ...) constructor:
// assemble an Element and delegate to from_element (which computes time-dimension initial values).
// Strict: a table (option_entries checks it, nil included, so the error is a Pattern 1 message)
// with only these eight keys, each of the right type. This function stays
// above bind_binary: the sync test reads a bare quoted name on its own line below a usertype as
// one of that usertype's methods, and the wrapped key list here is such lines.
BinaryMetadata build_metadata_from_lua(const sol::object& t) {
    const auto& [version, initial_datetime, unit, labels, dimensions, dimension_sizes, time_dimensions, frequencies] =
        option_entries(
            t,
            "metadata",
            {"version",
             "initial_datetime",
             "unit",
             "labels",
             "dimensions",
             "dimension_sizes",
             "time_dimensions",
             "frequencies"}
        );
    Element el;
    el.set("version", metadata_string(version, "version", "1"));
    el.set("initial_datetime", metadata_string(initial_datetime, "initial_datetime", ""));
    el.set("unit", metadata_string(unit, "unit", ""));
    el.set("labels", metadata_array<std::string>(labels, "labels"));
    el.set("dimensions", metadata_array<std::string>(dimensions, "dimensions"));
    el.set("dimension_sizes", metadata_array<int64_t>(dimension_sizes, "dimension_sizes"));
    el.set("time_dimensions", metadata_array<std::string>(time_dimensions, "time_dimensions"));
    el.set("frequencies", metadata_array<std::string>(frequencies, "frequencies"));
    return BinaryMetadata::from_element(el);
}

sol::table dimension_to_lua(sol::state_view& lua, const Dimension& dim) {
    auto t = lua.create_table();
    t["name"] = dim.name;
    t["size"] = dim.size;
    t["is_time_dimension"] = dim.is_time_dimension();
    if (dim.is_time_dimension()) {
        t["frequency"] = frequency_to_string(dim.time->frequency);
        t["initial_value"] = dim.time->initial_value;
        t["parent_dimension_index"] = dim.time->parent_dimension_index;
    }
    return t;
}

}  // namespace

// ========================================================================
// Binary subsystem bindings (mirrors the Julia Binary.* surface)
// ========================================================================

sol::usertype<BinaryFile> bind_binary(
    sol::state& state,
    sol::usertype<Database>& bind,
    sol::table& ns,
    RunHandles& handles
) {
    // Binary subsystem file I/O — db-scoped and sandboxed: paths resolve against the directory
    // containing the database file and must stay inside it.
    bind.set_function(
        "open_file",
        [&handles](Database& self, const std::string& path, const std::string& mode, const sol::object& metadata)
            -> std::shared_ptr<BinaryFile> {
            if (mode.size() != 1 || (mode[0] != 'r' && mode[0] != 'w')) {
                throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
            }
            const auto resolved = resolve_sandboxed_path(self, "open_file", path);
            // After containment, so the order stays mode, then path, then metadata.
            const auto md = optional_from_lua<BinaryMetadata>(metadata, "open_file", "metadata", "a BinaryMetadata");
            auto file = std::make_shared<BinaryFile>(BinaryFile::open_file(resolved, mode[0], md));
            handles.add_binary_file(file);
            return file;
        }
    );
    bind.set_function("bin_to_csv", [](Database& self, const std::string& path, const sol::object& aggregate) {
        // Containment first, as in every file operation, then the flag.
        const auto resolved = resolve_sandboxed_path(self, "bin_to_csv", path);
        const bool by_agent = optional_from_lua<bool>(aggregate, "bin_to_csv", "aggregate", "a boolean").value_or(true);
        CSVConverter::bin_to_csv(resolved, by_agent);
    });
    bind.set_function("csv_to_bin", [](Database& self, const std::string& path) {
        CSVConverter::csv_to_bin(resolve_sandboxed_path(self, "csv_to_bin", path));
    });

    state.new_usertype<BinaryMetadata>(
        "BinaryMetadata",
        sol::no_constructor,
        "get_unit",
        [](BinaryMetadata& self) -> std::string { return self.unit; },
        "get_version",
        [](BinaryMetadata& self) -> std::string { return self.version; },
        "get_initial_datetime",
        [](BinaryMetadata& self) -> std::string { return quiver::datetime::format_utc(self.initial_datetime); },
        "get_labels",
        [](BinaryMetadata& self, sol::this_state s) {
            sol::state_view lua(s);
            return to_lua_table(lua, self.labels);
        },
        "get_dimensions",
        [](BinaryMetadata& self, sol::this_state s) {
            sol::state_view lua(s);
            auto t = lua.create_table();
            for (size_t i = 0; i < self.dimensions.size(); ++i) {
                t[i + 1] = dimension_to_lua(lua, self.dimensions[i]);
            }
            return t;
        },
        "get_number_of_time_dimensions",
        [](BinaryMetadata& self) { return self.number_of_time_dimensions(); },
        "to_toml",
        [](BinaryMetadata& self) -> std::string { return self.to_toml(); }
    );

    auto binary_file_type = state.new_usertype<BinaryFile>(
        "BinaryFile",
        sol::no_constructor,
        "read",
        [](BinaryFile& self, const sol::object& dims, const sol::object& allow_nulls, sol::this_state s) {
            sol::state_view lua(s);
            const auto coordinates = lua_table_to_dim_map(dims, "read");
            const bool nulls = optional_from_lua<bool>(allow_nulls, "read", "allow_nulls", "a boolean").value_or(false);
            auto data = self.read(coordinates, nulls);
            return to_lua_table(lua, data);
        },
        "write",
        [](BinaryFile& self, const sol::object& data, const sol::object& dims) {
            // Decoded in argument order, so data is the one reported when both are wrong.
            const auto values = lua_table_to_vector<double>(require_table(data, "write", "data"), "write");
            const auto coordinates = lua_table_to_dim_map(dims, "write");
            self.write(values, coordinates);
        },
        "close",
        [](BinaryFile& self) { self.close(); },
        "is_open",
        [](BinaryFile& self) { return self.is_open(); },
        "get_metadata",
        [](BinaryFile& self) -> BinaryMetadata { return self.get_metadata(); },
        "get_file_path",
        [](BinaryFile& self) -> std::string { return self.get_file_path(); }
    );

    ns.set_function("metadata", [](const sol::object& t) { return build_metadata_from_lua(t); });
    ns.set_function("metadata_from_toml", [](const std::string& content) {
        return BinaryMetadata::from_toml_content(content);
    });
    ns.set_function("metadata_from_element", [](const sol::object& t) {
        return BinaryMetadata::from_element(table_to_element("metadata_from_element", t));
    });

    return binary_file_type;
}

}  // namespace quiver::lua_internal
