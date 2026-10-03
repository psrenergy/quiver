#include "lua_runner/internal.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/binary_metadata.h"
#include "quiver/binary/csv_converter.h"
#include "quiver/binary/time_properties.h"
#include "quiver/database.h"
#include "quiver/element.h"
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

std::unordered_map<std::string, int64_t> lua_table_to_dim_map(const sol::table& t, const std::string& caller) {
    std::unordered_map<std::string, int64_t> dims;
    for (auto& pair : t) {
        auto key = pair.first.as<std::string>();
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
    if (value->get_type() != sol::type::table) {
        throw std::runtime_error("Cannot metadata: " + field + " must be a table");
    }
    return lua_table_to_vector<T>(value->as<sol::table>(), "metadata: " + field);
}

// Build BinaryMetadata from a Lua kwargs table, mirroring the Julia Metadata(; ...) constructor:
// assemble an Element and delegate to from_element (which computes time-dimension initial values).
// Strict: a table (option_entries checks it, nil included, since sol2 does not check a table
// parameter in Release) with only these eight keys, each of the right type. This function stays
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

// ------------------------------------------------------------------------
// Expression operator dispatch (shared by Expression and BinaryFile metamethods)
// ------------------------------------------------------------------------

enum class BinOp { Add, Subtract, Multiply, Divide, Gt, Lt, Gte, Lte, Eq, Neq, And, Or };

bool is_number(const sol::object& o) {
    return o.get_type() == sol::type::number;
}

// A Lua operand in arithmetic is either an Expression, a BinaryFile (auto-wrapped), or a number
// (handled by the scalar operator overloads). Numbers are rejected here on purpose.
Expression to_expression(const sol::object& o) {
    if (o.is<Expression>()) {
        return o.as<Expression>();
    }
    if (o.is<BinaryFile>()) {
        return Expression(o.as<BinaryFile&>());
    }
    throw std::runtime_error("Cannot build expression: operand must be an expression or a binary file");
}

// One body for all three operand combos (expr/expr, expr/double, double/expr); overload
// resolution on l/r picks the matching Expression operator per instantiation. double/double
// is never instantiated (binop_dispatch routes numbers through to_expression, which throws).
template <typename L, typename R>
Expression apply_binop(BinOp op, const L& l, const R& r) {
    switch (op) {
    case BinOp::Add:
        return l + r;
    case BinOp::Subtract:
        return l - r;
    case BinOp::Multiply:
        return l * r;
    case BinOp::Divide:
        return l / r;
    case BinOp::Gt:
        return l > r;
    case BinOp::Lt:
        return l < r;
    case BinOp::Gte:
        return l >= r;
    case BinOp::Lte:
        return l <= r;
    case BinOp::Eq:
        return l == r;
    case BinOp::Neq:
        return l != r;
    case BinOp::And:
        return l && r;
    case BinOp::Or:
        return l || r;
    }
    throw std::runtime_error("Cannot apply operator: unknown operation");
}

Expression binop_dispatch(BinOp op, const sol::object& lhs, const sol::object& rhs) {
    bool lnum = is_number(lhs);
    bool rnum = is_number(rhs);
    if (lnum && !rnum) {
        return apply_binop(op, lhs.as<double>(), to_expression(rhs));
    }
    if (!lnum && rnum) {
        return apply_binop(op, to_expression(lhs), rhs.as<double>());
    }
    return apply_binop(op, to_expression(lhs), to_expression(rhs));
}

// `caller` is the public method ("aggregate" / "aggregate_agents") named in the Pattern 1 message.
ExpressionAggregate::Operation parse_aggregate_op(const std::string& op, const std::string& caller) {
    if (op == "sum") {
        return ExpressionAggregate::Operation::Sum;
    }
    if (op == "mean") {
        return ExpressionAggregate::Operation::Mean;
    }
    if (op == "min") {
        return ExpressionAggregate::Operation::Min;
    }
    if (op == "max") {
        return ExpressionAggregate::Operation::Max;
    }
    if (op == "percentile") {
        return ExpressionAggregate::Operation::Percentile;
    }
    throw std::runtime_error("Cannot " + caller + ": unknown operation '" + op + "'");
}

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
// The arithmetic, unary-minus and logical metamethods shared by every usertype that behaves like an
// expression (BinaryFile auto-wraps to Expression). One table, so a new operator is added once.
template <typename T>
void bind_expression_operators(sol::usertype<T>& type) {
    type[sol::meta_function::addition] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Add, a, b); };
    type[sol::meta_function::subtraction] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::Subtract, a, b);
    };
    type[sol::meta_function::multiplication] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::Multiply, a, b);
    };
    type[sol::meta_function::division] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::Divide, a, b);
    };
    type[sol::meta_function::unary_minus] = [](sol::object a, sol::object) { return -to_expression(a); };
    // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
    type[sol::meta_function::bitwise_and] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::And, a, b);
    };
    type[sol::meta_function::bitwise_or] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Or, a, b); };
    type[sol::meta_function::bitwise_not] = [](sol::object a, sol::object) { return !to_expression(a); };
}

}  // namespace

// ========================================================================
// Binary subsystem bindings (mirrors the Julia Binary.* surface)
// ========================================================================

void bind_binary(sol::state& lua, sol::usertype<Database>& bind, sol::table& ns, Database& db, RunHandles& handles) {
    // Binary subsystem file I/O — db-scoped and sandboxed: paths resolve against the directory
    // containing the database file and must stay inside it.
    bind.set_function(
        "open_file",
        [&handles](
            Database& self,
            const std::string& path,
            const std::string& mode,
            sol::optional<BinaryMetadata> metadata
        ) -> std::shared_ptr<BinaryFile> {
            if (mode.size() != 1 || (mode[0] != 'r' && mode[0] != 'w')) {
                throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
            }
            const auto resolved = resolve_sandboxed_path(self, "open_file", path);
            std::optional<BinaryMetadata> md = metadata ? std::optional<BinaryMetadata>(*metadata) : std::nullopt;
            auto file = std::make_shared<BinaryFile>(BinaryFile::open_file(resolved, mode[0], md));
            handles.add_binary_file(file);
            return file;
        }
    );
    bind.set_function("bin_to_csv", [](Database& self, const std::string& path, sol::optional<bool> aggregate) {
        CSVConverter::bin_to_csv(resolve_sandboxed_path(self, "bin_to_csv", path), aggregate.value_or(true));
    });
    bind.set_function("csv_to_bin", [](Database& self, const std::string& path) {
        CSVConverter::csv_to_bin(resolve_sandboxed_path(self, "csv_to_bin", path));
    });

    lua.new_usertype<BinaryMetadata>(
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

    auto binary_file_type = lua.new_usertype<BinaryFile>(
        "BinaryFile",
        sol::no_constructor,
        "read",
        [](BinaryFile& self, const sol::table& dims, sol::optional<bool> allow_nulls, sol::this_state s) {
            sol::state_view lua(s);
            auto data = self.read(lua_table_to_dim_map(dims, "read"), allow_nulls.value_or(false));
            return to_lua_table(lua, data);
        },
        "write",
        [](BinaryFile& self, const sol::table& data, const sol::table& dims) {
            self.write(lua_table_to_vector<double>(data, "write"), lua_table_to_dim_map(dims, "write"));
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
    // Arithmetic on files mirrors Julia: file_a + file_b, -file, file * 2.0 (auto-wrap to Expression)
    bind_expression_operators(binary_file_type);

    ns.set_function("metadata", [](const sol::object& t) { return build_metadata_from_lua(t); });
    ns.set_function("metadata_from_toml", [](const std::string& content) {
        return BinaryMetadata::from_toml_content(content);
    });
    ns.set_function("metadata_from_element", [](const sol::table& t) {
        return BinaryMetadata::from_element(table_to_element("metadata_from_element", t));
    });

    // Expression subsystem bindings (mirrors the Julia Expression surface)
    auto expression_type = lua.new_usertype<Expression>(
        "Expression",
        sol::no_constructor,
        "save",
        [&db](Expression& self, const std::string& path) { self.save(resolve_sandboxed_path(db, "save", path)); },
        "metadata",
        [](Expression& self) -> BinaryMetadata { return self.metadata(); },
        "aggregate",
        [](Expression& self, const std::string& dimension, const std::string& op, sol::optional<double> parameter) {
            return self.aggregate(
                dimension,
                parse_aggregate_op(op, "aggregate"),
                parameter ? std::optional<double>(*parameter) : std::nullopt
            );
        },
        "aggregate_agents",
        [](Expression& self, const std::string& op, sol::optional<double> parameter) {
            return self.aggregate_agents(
                parse_aggregate_op(op, "aggregate_agents"),
                parameter ? std::optional<double>(*parameter) : std::nullopt
            );
        },
        "select_agents",
        [](Expression& self, const sol::table& labels) {
            return self.select_agents(lua_table_to_vector<std::string>(labels, "select_agents"));
        },
        "rename_agents",
        [](Expression& self, const sol::object& mapping) {
            // sol2 does not check a table parameter in Release, so check it here. Then collect,
            // then check both halves: an unchecked as<std::string>() spelled a number
            // key as text and gave "" for a boolean in Release.
            if (mapping.get_type() != sol::type::table) {
                throw std::runtime_error("Cannot rename_agents: mapping must be a table");
            }
            std::vector<std::pair<std::string, std::string>> pairs;
            for (const auto& [key, value] : collect_entries(mapping.as<sol::table>())) {
                auto old_name = lua_cell_as<std::string>(key, "rename_agents", "key");
                auto new_name = lua_cell_as<std::string>(value, "rename_agents", "value for '" + old_name + "'");
                pairs.emplace_back(std::move(old_name), std::move(new_name));
            }
            return self.rename_agents(pairs);
        }
    );
    bind_expression_operators(expression_type);

    ns.set_function("expression", [](sol::object o) { return to_expression(o); });
    ns.set_function("abs", [](sol::object o) { return quiver::abs(to_expression(o)); });
    ns.set_function("sqrt", [](sol::object o) { return quiver::sqrt(to_expression(o)); });
    ns.set_function("log", [](sol::object o) { return quiver::log(to_expression(o)); });
    ns.set_function("exp", [](sol::object o) { return quiver::exp(to_expression(o)); });
    ns.set_function("ifelse", [](sol::object c, sol::object t, sol::object e) {
        return quiver::ifelse(to_expression(c), to_expression(t), to_expression(e));
    });
    // Comparisons produce 1.0/0.0 per element (NaN operand -> NaN). Free functions because Lua
    // comparison metamethods are coerced to bool and cannot return an Expression.
    ns.set_function("gt", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Gt, a, b); });
    ns.set_function("lt", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Lt, a, b); });
    ns.set_function("gte", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Gte, a, b); });
    ns.set_function("lte", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Lte, a, b); });
    ns.set_function("eq", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Eq, a, b); });
    ns.set_function("neq", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Neq, a, b); });
    // Logical ops on boolean-valued expressions are the `&` / `|` / `~` metamethods bound on the
    // Expression and BinaryFile usertypes (`and`/`or`/`not` are Lua keywords, so no free functions).
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
