#include "quiver/lua_runner.h"

#include "quiver/binary/binary_file.h"
#include "quiver/binary/binary_metadata.h"
#include "quiver/binary/csv_converter.h"
#include "quiver/binary/time_properties.h"
#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/expression/expression.h"
#include "quiver/options.h"
#include "quiver/value.h"
#include "utils/datetime.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <sol/sol.hpp>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace quiver {

struct LuaRunner::Impl {
    Database& db;
    sol::state lua;

    explicit Impl(Database& database) : db(database) {
        lua.open_libraries(
            sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::coroutine, sol::lib::utf8);
        // Scripts may not load Lua source from disk; string-form load() stays available.
        lua["dofile"] = sol::lua_nil;
        lua["loadfile"] = sol::lua_nil;
        lua.create_named_table("quiver");
        bind_database();
        bind_binary();
        bind_expression();
        lua["db"] = &db;
    }

    void bind_database() {
        // NOLINTBEGIN(performance-unnecessary-value-parameter) sol2 lambda bindings require pass-by-value for type
        // deduction
        auto bind = lua.new_usertype<Database>(
            "Database",
            "delete_element",
            [](Database& self, const std::string& collection, int64_t id) { self.delete_element(collection, id); },
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
                self.export_csv(collection,
                                group,
                                resolve_sandboxed_path(self, "export_csv", path),
                                parse_csv_options(std::move(options_table)));
            },
            // Group 12: CSV import
            "import_csv",
            [](Database& self,
               const std::string& collection,
               const std::string& group,
               const std::string& path,
               sol::optional<sol::table> options_table) {
                self.import_csv(collection,
                                group,
                                resolve_sandboxed_path(self, "import_csv", path),
                                parse_csv_options(std::move(options_table)));
            });
        // NOLINTEND(performance-unnecessary-value-parameter)

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
        bind.set_function("read_time_series_row", &read_time_series_row_lua);
        bind.set_function("read_time_series_files", &read_time_series_files_lua);

        bind.set_function("read_scalars_by_id", &read_scalars_by_id_lua);
        bind.set_function("read_vectors_by_id", &read_vectors_by_id_lua);
        bind.set_function("read_sets_by_id", &read_sets_by_id_lua);
        bind.set_function("read_element_by_id", &read_element_by_id_lua);

        bind.set_function("update_element", &update_element_lua);
        bind.set_function("update_time_series_group", &update_time_series_group_lua);
        bind.set_function("upsert_time_series_row", &upsert_time_series_row_lua);
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

        bind.set_function("describe", [](Database& self) { return self.describe(); });
        bind.set_function("describe_collection", [](Database& self, const std::string& collection) {
            return self.describe_collection(collection);
        });
        bind.set_function("summarize_collection", [](Database& self, const std::string& collection) {
            return self.summarize_collection(collection);
        });

        bind.set_function("query_string", &query_string_lua);
        bind.set_function("query_integer", &query_integer_lua);
        bind.set_function("query_float", &query_float_lua);

        // Binary subsystem file I/O — db-scoped and sandboxed: paths resolve against the directory
        // containing the database file and must stay inside it.
        bind.set_function(
            "open_file",
            [](Database& self, const std::string& path, const std::string& mode, sol::optional<BinaryMetadata> metadata)
                -> std::unique_ptr<BinaryFile> {
                if (mode.size() != 1 || (mode[0] != 'r' && mode[0] != 'w')) {
                    throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
                }
                const auto resolved = resolve_sandboxed_path(self, "open_file", path);
                std::optional<BinaryMetadata> md = metadata ? std::optional<BinaryMetadata>(*metadata) : std::nullopt;
                return std::make_unique<BinaryFile>(BinaryFile::open_file(resolved, mode[0], md));
            });
        bind.set_function("bin_to_csv", [](Database& self, const std::string& path, sol::optional<bool> aggregate) {
            CSVConverter::bin_to_csv(resolve_sandboxed_path(self, "bin_to_csv", path), aggregate.value_or(true));
        });
        bind.set_function("csv_to_bin", [](Database& self, const std::string& path) {
            CSVConverter::csv_to_bin(resolve_sandboxed_path(self, "csv_to_bin", path));
        });
    }

    // ========================================================================
    // Binary subsystem bindings (mirrors the Julia Binary.* surface)
    // ========================================================================

    void bind_binary() {
        sol::table ns = lua["quiver"];

        // NOLINTBEGIN(performance-unnecessary-value-parameter) sol2 lambda bindings require pass-by-value for type
        // deduction
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
            [](BinaryMetadata& self) -> std::string { return self.to_toml(); });

        lua.new_usertype<BinaryFile>(
            "BinaryFile",
            sol::no_constructor,
            "read",
            [](BinaryFile& self, const sol::table& dims, sol::optional<bool> allow_nulls, sol::this_state s) {
                sol::state_view lua(s);
                auto data = self.read(lua_table_to_dim_map(dims), allow_nulls.value_or(false));
                return to_lua_table(lua, data);
            },
            "write",
            [](BinaryFile& self, const sol::table& data, const sol::table& dims) {
                self.write(lua_table_to_double_vector(data), lua_table_to_dim_map(dims));
            },
            "close",
            [](BinaryFile& self) { self.close(); },
            "is_open",
            [](BinaryFile& self) { return self.is_open(); },
            "get_metadata",
            [](BinaryFile& self) -> BinaryMetadata { return self.get_metadata(); },
            "get_file_path",
            [](BinaryFile& self) -> std::string { return self.get_file_path(); },
            // Arithmetic on files mirrors Julia: file_a + file_b, -file, file * 2.0 (auto-wrap to Expression)
            sol::meta_function::addition,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Add, a, b); },
            sol::meta_function::subtraction,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Subtract, a, b); },
            sol::meta_function::multiplication,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Multiply, a, b); },
            sol::meta_function::division,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Divide, a, b); },
            sol::meta_function::unary_minus,
            [](sol::object a, sol::object) { return -to_expression(a); },
            // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
            sol::meta_function::bitwise_and,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::And, a, b); },
            sol::meta_function::bitwise_or,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Or, a, b); },
            sol::meta_function::bitwise_not,
            [](sol::object a, sol::object) { return !to_expression(a); });

        ns.set_function("metadata", [](const sol::table& t) { return build_metadata_from_lua(t); });
        ns.set_function("metadata_from_toml",
                        [](const std::string& content) { return BinaryMetadata::from_toml_content(content); });
        ns.set_function("metadata_from_element",
                        [](const sol::table& t) { return BinaryMetadata::from_element(table_to_element(t)); });
        // NOLINTEND(performance-unnecessary-value-parameter)
    }

    // ========================================================================
    // Expression subsystem bindings (mirrors the Julia Expression surface)
    // ========================================================================

    void bind_expression() {
        sol::table ns = lua["quiver"];

        // NOLINTBEGIN(performance-unnecessary-value-parameter) sol2 lambda bindings require pass-by-value for type
        // deduction
        lua.new_usertype<Expression>(
            "Expression",
            sol::no_constructor,
            "save",
            [this](Expression& self, const std::string& path) { self.save(resolve_sandboxed_path(db, "save", path)); },
            "metadata",
            [](Expression& self) -> BinaryMetadata { return self.metadata(); },
            "aggregate",
            [](Expression& self, const std::string& dimension, const std::string& op, sol::optional<double> parameter) {
                return self.aggregate(
                    dimension, parse_aggregate_op(op), parameter ? std::optional<double>(*parameter) : std::nullopt);
            },
            "aggregate_agents",
            [](Expression& self, const std::string& op, sol::optional<double> parameter) {
                return self.aggregate_agents(parse_aggregate_agents_op(op),
                                             parameter ? std::optional<double>(*parameter) : std::nullopt);
            },
            "select_agents",
            [](Expression& self, const sol::table& labels) {
                return self.select_agents(lua_table_to_string_vector(labels));
            },
            "rename_agents",
            [](Expression& self, const sol::table& mapping) {
                std::vector<std::pair<std::string, std::string>> pairs;
                for (auto& kv : mapping) {
                    pairs.emplace_back(kv.first.as<std::string>(), kv.second.as<std::string>());
                }
                return self.rename_agents(pairs);
            },
            sol::meta_function::addition,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Add, a, b); },
            sol::meta_function::subtraction,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Subtract, a, b); },
            sol::meta_function::multiplication,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Multiply, a, b); },
            sol::meta_function::division,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Divide, a, b); },
            sol::meta_function::unary_minus,
            [](sol::object a, sol::object) { return -to_expression(a); },
            // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
            sol::meta_function::bitwise_and,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::And, a, b); },
            sol::meta_function::bitwise_or,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Or, a, b); },
            sol::meta_function::bitwise_not,
            [](sol::object a, sol::object) { return !to_expression(a); });

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
        // NOLINTEND(performance-unnecessary-value-parameter)
    }

    // ========================================================================
    // Conversion helpers
    // ========================================================================

    static std::unordered_map<std::string, int64_t> lua_table_to_dim_map(const sol::table& t) {
        std::unordered_map<std::string, int64_t> dims;
        for (auto& pair : t) {
            dims[pair.first.as<std::string>()] = pair.second.as<int64_t>();
        }
        return dims;
    }

    static std::string lua_opt_string(const sol::table& t, const char* key, const std::string& fallback) {
        auto opt = t.get<sol::optional<std::string>>(key);
        return opt ? *opt : fallback;
    }

    static std::vector<std::string> lua_opt_string_vector(const sol::table& t, const char* key) {
        auto opt = t.get<sol::optional<sol::table>>(key);
        return opt ? lua_table_to_string_vector(*opt) : std::vector<std::string>{};
    }

    static std::vector<int64_t> lua_opt_int64_vector(const sol::table& t, const char* key) {
        auto opt = t.get<sol::optional<sol::table>>(key);
        return opt ? lua_table_to_int64_vector(*opt) : std::vector<int64_t>{};
    }

    // Build BinaryMetadata from a Lua kwargs table, mirroring the Julia Metadata(; ...) constructor:
    // assemble an Element and delegate to from_element (which computes time-dimension initial values).
    static BinaryMetadata build_metadata_from_lua(const sol::table& t) {
        Element el;
        el.set("version", lua_opt_string(t, "version", "1"));
        el.set("initial_datetime", lua_opt_string(t, "initial_datetime", ""));
        el.set("unit", lua_opt_string(t, "unit", ""));
        el.set("labels", lua_opt_string_vector(t, "labels"));
        el.set("dimensions", lua_opt_string_vector(t, "dimensions"));
        el.set("dimension_sizes", lua_opt_int64_vector(t, "dimension_sizes"));
        el.set("time_dimensions", lua_opt_string_vector(t, "time_dimensions"));
        el.set("frequencies", lua_opt_string_vector(t, "frequencies"));
        return BinaryMetadata::from_element(el);
    }

    static sol::table dimension_to_lua(sol::state_view& lua, const Dimension& dim) {
        auto t = lua.create_table();
        t["name"] = dim.name;
        t["size"] = dim.size;
        t["is_time_dimension"] = dim.is_time_dimension();
        if (dim.is_time_dimension()) {
            t["frequency"] = frequency_to_string(dim.time->frequency);
            t["initial_value"] = dim.time->initial_value;
            t["parent_dimension_index"] = dim.time->parent_dimension_index;
        } else {
            t["frequency"] = sol::lua_nil;
            t["initial_value"] = sol::lua_nil;
            t["parent_dimension_index"] = sol::lua_nil;
        }
        return t;
    }

    // ------------------------------------------------------------------------
    // Expression operator dispatch (shared by Expression and BinaryFile metamethods)
    // ------------------------------------------------------------------------

    enum class BinOp { Add, Subtract, Multiply, Divide, Gt, Lt, Gte, Lte, Eq, Neq, And, Or };

    static bool is_number(const sol::object& o) { return o.get_type() == sol::type::number; }

    // A Lua operand in arithmetic is either an Expression, a BinaryFile (auto-wrapped), or a number
    // (handled by the scalar operator overloads). Numbers are rejected here on purpose.
    static Expression to_expression(const sol::object& o) {
        if (o.is<Expression>()) {
            return o.as<Expression>();
        }
        if (o.is<BinaryFile>()) {
            return Expression(o.as<BinaryFile&>());
        }
        throw std::runtime_error("Cannot build expression: operand must be an expression or a binary file");
    }

    static Expression apply_binop(BinOp op, const Expression& l, const Expression& r) {
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

    static Expression apply_binop(BinOp op, const Expression& l, double r) {
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

    static Expression apply_binop(BinOp op, double l, const Expression& r) {
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

    static Expression binop_dispatch(BinOp op, const sol::object& lhs, const sol::object& rhs) {
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

    static ExpressionAggregate::Operation parse_aggregate_op(const std::string& op) {
        if (op == "sum")
            return ExpressionAggregate::Operation::Sum;
        if (op == "mean")
            return ExpressionAggregate::Operation::Mean;
        if (op == "min")
            return ExpressionAggregate::Operation::Min;
        if (op == "max")
            return ExpressionAggregate::Operation::Max;
        if (op == "percentile")
            return ExpressionAggregate::Operation::Percentile;
        throw std::runtime_error("Cannot aggregate: unknown operation '" + op + "'");
    }

    static ExpressionAggregateAgents::Operation parse_aggregate_agents_op(const std::string& op) {
        if (op == "sum")
            return ExpressionAggregateAgents::Operation::Sum;
        if (op == "mean")
            return ExpressionAggregateAgents::Operation::Mean;
        if (op == "min")
            return ExpressionAggregateAgents::Operation::Min;
        if (op == "max")
            return ExpressionAggregateAgents::Operation::Max;
        if (op == "percentile")
            return ExpressionAggregateAgents::Operation::Percentile;
        throw std::runtime_error("Cannot aggregate_agents: unknown operation '" + op + "'");
    }

    // Resolves a script-supplied path against the database file's directory and enforces that the
    // result stays strictly inside it (subdirectories allowed). Returns the resolved absolute path.
    // `operation` is the public method name the user called (threaded into Pattern 1 messages).
    static std::string
    resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path) {
        namespace fs = std::filesystem;

        const std::string& db_path = db.path();
        if (db_path == ":memory:") {
            throw std::runtime_error("Cannot " + operation +
                                     ": database is in-memory, file operations are unavailable");
        }

        // Same root derivation as create_database_logger: a bare filename has an empty
        // parent_path and resolves against the current working directory.
        auto root = fs::path(db_path).parent_path();
        if (root.empty()) {
            root = fs::current_path();
        }
        root = fs::weakly_canonical(root);

        auto candidate = fs::path(path);
        if (candidate.is_relative()) {
            candidate = root / candidate;
        }
        candidate = fs::weakly_canonical(candidate);

        // Strict containment: candidate == root is rejected too — the binary subsystem appends
        // ".qvr"/".toml" by string concatenation, so the root itself would yield "<root>.qvr"
        // outside the sandbox.
        const auto rel = candidate.lexically_relative(root);
        if (rel.empty() || rel == "." || rel.begin()->string() == "..") {
            throw std::runtime_error("Cannot " + operation + ": path '" + path + "' escapes the database directory '" +
                                     root.string() + "'");
        }

        return candidate.string();
    }

    static CSVOptions parse_csv_options(sol::optional<sol::table> options_table) {
        CSVOptions options;
        if (!options_table) {
            return options;
        }
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
                    locale_value.as<sol::table>().for_each(
                        [&](sol::object k, sol::object v) { label_map[k.as<std::string>()] = v.as<int64_t>(); });
                });
            });
        }
        return options;
    }

    template <typename T>
    static sol::table to_lua_table(sol::state_view& lua, const std::vector<T>& values) {
        auto t = lua.create_table();
        for (size_t i = 0; i < values.size(); ++i) {
            t[i + 1] = values[i];
        }
        return t;
    }

    // Scalar bulk reads: a SQL NULL becomes a nil hole, preserving positional index.
    // #t over holes is unreliable — read_element_ids is the count/position authority.
    template <typename T>
    static sol::table to_lua_table(sol::state_view& lua, const std::vector<std::optional<T>>& values) {
        auto t = lua.create_table();
        for (size_t i = 0; i < values.size(); ++i) {
            if (values[i]) {
                t[i + 1] = *values[i];
            }
            // else: leave index i + 1 absent (nil).
        }
        return t;
    }

    template <typename T>
    static sol::table to_lua_table(sol::state_view& lua, const std::vector<std::vector<T>>& values) {
        auto outer = lua.create_table();
        for (size_t i = 0; i < values.size(); ++i) {
            outer[i + 1] = to_lua_table(lua, values[i]);
        }
        return outer;
    }

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
        // Column order in the resulting map is alphabetical (std::map invariant),
        // which is fine because the C++ layer indexes by column name rather than
        // relying on positional order. Callers should not depend on insertion order.
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
            } else {
                // Surface typos / nested tables / unsupported Lua types loudly
                // instead of silently dropping the column (would cause confusing
                // downstream "column missing" or NULL-stored errors).
                throw std::runtime_error("Cannot lua_table_to_value_map: column '" + key +
                                         "' has unsupported Lua type");
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
                        element.set(k, lua_table_to_int64_vector(arr));
                    } else if (first.is<double>()) {
                        element.set(k, lua_table_to_double_vector(arr));
                    } else if (first.is<std::string>()) {
                        element.set(k, lua_table_to_string_vector(arr));
                    } else {
                        // Surface unsupported element types loudly instead of silently
                        // dropping the attribute (same policy as lua_table_to_value_map)
                        throw std::runtime_error("Cannot table_to_element: array '" + k +
                                                 "' has unsupported element type");
                    }
                }
            } else if (val.is<int64_t>()) {
                element.set(k, val.as<int64_t>());
            } else if (val.is<double>()) {
                element.set(k, val.as<double>());
            } else if (val.is<std::string>()) {
                element.set(k, val.as<std::string>());
            } else {
                // Surface typos / booleans / nested structures loudly instead of
                // silently dropping the attribute (same policy as lua_table_to_value_map)
                throw std::runtime_error("Cannot table_to_element: attribute '" + k + "' has unsupported Lua type");
            }
        }
        return element;
    }

    static int64_t create_element_lua(Database& db, const std::string& collection, const sol::table& values) {
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
        return to_lua_table(lua, db.read_scalar_strings(collection, attribute));
    }

    static sol::table read_scalar_integers_lua(Database& db,
                                               const std::string& collection,
                                               const std::string& attribute,
                                               sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_scalar_integers(collection, attribute));
    }

    static sol::table read_scalar_floats_lua(Database& db,
                                             const std::string& collection,
                                             const std::string& attribute,
                                             sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_scalar_floats(collection, attribute));
    }

    static sol::table read_vector_integers_lua(Database& db,
                                               const std::string& collection,
                                               const std::string& attribute,
                                               sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_vector_integers(collection, attribute));
    }

    static sol::table read_vector_floats_lua(Database& db,
                                             const std::string& collection,
                                             const std::string& attribute,
                                             sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_vector_floats(collection, attribute));
    }

    static sol::table read_vector_strings_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& attribute,
                                              sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_vector_strings(collection, attribute));
    }

    static sol::table read_element_ids_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_element_ids(collection));
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
            throw std::runtime_error("Cannot data_type_to_string: unknown data type " +
                                     std::to_string(static_cast<int>(type)));
        }
    }

    static sol::table get_scalar_metadata_lua(Database& db,
                                              const std::string& collection,
                                              const std::string& attribute,
                                              sol::this_state s) {
        sol::state_view lua(s);
        auto metadata = db.get_scalar_metadata(collection, attribute);
        return scalar_metadata_lua(lua, metadata);
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
        t["is_foreign_key"] = attribute.is_foreign_key;
        if (attribute.references_collection.has_value()) {
            t["references_collection"] = *attribute.references_collection;
        } else {
            t["references_collection"] = sol::lua_nil;
        }
        if (attribute.references_column.has_value()) {
            t["references_column"] = *attribute.references_column;
        } else {
            t["references_column"] = sol::lua_nil;
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

    static std::vector<Value> lua_table_to_values(const sol::table& parameters) {
        std::vector<Value> values;
        for (size_t i = 1; i <= parameters.size(); ++i) {
            sol::object val = parameters[i];
            if (val.is<sol::lua_nil_t>()) {
                values.emplace_back(nullptr);
            } else if (val.is<int64_t>()) {
                values.emplace_back(val.as<int64_t>());
            } else if (val.is<double>()) {
                values.emplace_back(val.as<double>());
            } else if (val.is<std::string>()) {
                values.emplace_back(val.as<std::string>());
            } else {
                // A silently skipped parameter would shift every later positional
                // parameter left and bind NULL to the trailing placeholder.
                throw std::runtime_error("Cannot lua_table_to_values: parameter #" + std::to_string(i) +
                                         " has unsupported Lua type");
            }
        }
        return values;
    }

    static sol::object
    query_string_lua(Database& db, const std::string& sql, sol::optional<sol::table> parameters, sol::this_state s) {
        sol::state_view lua(s);
        auto values = parameters ? lua_table_to_values(*parameters) : std::vector<Value>{};
        auto result = db.query_string(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    static sol::object
    query_integer_lua(Database& db, const std::string& sql, sol::optional<sol::table> parameters, sol::this_state s) {
        sol::state_view lua(s);
        auto values = parameters ? lua_table_to_values(*parameters) : std::vector<Value>{};
        auto result = db.query_integer(sql, values);
        if (result.has_value()) {
            return sol::make_object(lua, *result);
        }
        return sol::make_object(lua, sol::lua_nil);
    }

    static sol::object
    query_float_lua(Database& db, const std::string& sql, sol::optional<sol::table> parameters, sol::this_state s) {
        sol::state_view lua(s);
        auto values = parameters ? lua_table_to_values(*parameters) : std::vector<Value>{};
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
                throw std::runtime_error("Cannot read_scalars_by_id: unknown data type " +
                                         std::to_string(static_cast<int>(attribute.data_type)));
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
                    throw std::runtime_error("Cannot read_vectors_by_id: unknown data type " +
                                             std::to_string(static_cast<int>(col.data_type)));
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
                    throw std::runtime_error("Cannot read_sets_by_id: unknown data type " +
                                             std::to_string(static_cast<int>(col.data_type)));
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
        return to_lua_table(lua, db.read_set_integers(collection, attribute));
    }

    static sol::table
    read_set_floats_lua(Database& db, const std::string& collection, const std::string& attribute, sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_set_floats(collection, attribute));
    }

    static sol::table
    read_set_strings_lua(Database& db, const std::string& collection, const std::string& attribute, sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.read_set_strings(collection, attribute));
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
        // Column-oriented result: { name = {v1, v2, ...}, ... }
        auto t = lua.create_table();
        if (rows.empty()) {
            return t;
        }
        for (const auto& [name, _] : rows[0]) {
            auto column = lua.create_table();
            for (size_t i = 0; i < rows.size(); ++i) {
                column[i + 1] = value_to_lua_object(lua, rows[i].at(name));
            }
            t[name] = column;
        }
        return t;
    }

    static sol::table read_time_series_row_lua(Database& db,
                                               const std::string& collection,
                                               const std::string& group,
                                               const std::string& attribute,
                                               const std::string& date_time,
                                               sol::this_state s) {
        sol::state_view lua(s);
        auto values = db.read_time_series_row(collection, group, attribute, date_time);
        auto t = lua.create_table();
        for (size_t i = 0; i < values.size(); ++i) {
            t[i + 1] = value_to_lua_object(lua, values[i]);
        }
        return t;
    }

    // One named column of the column-oriented update payload: its Lua table plus the extent
    // (largest 1-based integer key; 0 when empty) and the number of non-nil cells. nil cells
    // never appear during table iteration, so count < extent means the column has holes.
    // Computed via iteration instead of sol::table::size() because lua_rawlen on a table with
    // holes returns an arbitrary border.
    struct TimeSeriesColumn {
        std::string name;
        sol::table values;
        size_t extent = 0;
        size_t count = 0;
    };

    static std::vector<TimeSeriesColumn> collect_time_series_columns(const sol::table& columns) {
        std::vector<TimeSeriesColumn> result;
        for (auto& pair : columns) {
            auto name = pair.first.as<std::string>();
            if (!pair.second.is<sol::table>()) {
                throw std::runtime_error("Cannot update_time_series_group: column '" + name +
                                         "' must be an array of values");
            }
            TimeSeriesColumn column{name, pair.second.as<sol::table>()};
            for (auto& cell : column.values) {
                if (!cell.first.is<int64_t>() || cell.first.as<int64_t>() < 1) {
                    throw std::runtime_error("Cannot update_time_series_group: column '" + name +
                                             "' must be an array of values");
                }
                column.extent = std::max(column.extent, static_cast<size_t>(cell.first.as<int64_t>()));
                ++column.count;
            }
            result.push_back(std::move(column));
        }
        return result;
    }

    static void update_time_series_group_lua(Database& db,
                                             const std::string& collection,
                                             const std::string& group,
                                             int64_t id,
                                             sol::table columns) {
        // Column-oriented input: { name = {v1, v2, ...}, ... } transposed into the row maps the
        // C++ API takes. The dimension column(s) are the row count authority -- PK members are
        // implicitly NOT NULL in STRICT tables, so they must be present and dense. Value columns
        // may be shorter, sparse, or empty: every cell missing at a dimension index is written as
        // NULL, which round-trips the nil holes that read_time_series_group produces. An empty
        // table (no columns) clears all rows; named columns whose dimension transposes to zero
        // rows still throw instead of silently clearing the group.
        auto lua_columns = collect_time_series_columns(columns);
        if (lua_columns.empty()) {
            db.update_time_series_group(collection, group, id, {});
            return;
        }

        // The date_ ordering column plus any extra PK dimensions (multi-dim groups, e.g.
        // (date_time, block)): get_time_series_metadata reports the extras as value_columns
        // with primary_key set.
        auto metadata = db.get_time_series_metadata(collection, group);
        std::vector<std::string> dimension_columns;
        dimension_columns.push_back(metadata.dimension_column);
        for (const auto& vc : metadata.value_columns) {
            if (vc.primary_key) {
                dimension_columns.push_back(vc.name);
            }
        }

        const auto find_column = [&](const std::string& name) -> const TimeSeriesColumn* {
            for (const auto& column : lua_columns) {
                if (column.name == name) {
                    return &column;
                }
            }
            return nullptr;
        };

        for (const auto& dim : dimension_columns) {
            if (find_column(dim) == nullptr) {
                throw std::runtime_error("Cannot update_time_series_group: missing dimension column '" + dim + "'");
            }
        }

        const size_t row_count = find_column(dimension_columns.front())->extent;
        for (const auto& dim : dimension_columns) {
            const auto* column = find_column(dim);
            if (column->extent != row_count) {
                throw std::runtime_error("Cannot update_time_series_group: column '" + dim + "' has length " +
                                         std::to_string(column->extent) + " but expected " + std::to_string(row_count));
            }
            if (column->count != column->extent) {
                for (size_t i = 1; i <= column->extent; ++i) {
                    if (!column->values[i].valid()) {
                        throw std::runtime_error("Cannot update_time_series_group: dimension column '" + dim +
                                                 "' has nil at index " + std::to_string(i));
                    }
                }
            }
        }

        for (const auto& column : lua_columns) {
            if (column.extent > row_count) {
                throw std::runtime_error("Cannot update_time_series_group: column '" + column.name + "' has length " +
                                         std::to_string(column.extent) + " but expected " + std::to_string(row_count));
            }
        }

        if (row_count == 0) {
            std::string joined;
            for (size_t i = 0; i < lua_columns.size(); ++i) {
                if (i > 0) {
                    joined += ", ";
                }
                joined += lua_columns[i].name;
            }
            throw std::runtime_error("Cannot update_time_series_group: columns [" + joined +
                                     "] contain no rows; pass an empty table {} to clear the group");
        }

        // Uniform rows: the C++ core derives the INSERT column list from rows[0], so every row
        // carries every named column, with explicit NULL for the cells the caller left out.
        std::vector<std::map<std::string, Value>> cpp_rows(row_count);
        for (const auto& column : lua_columns) {
            for (auto& row : cpp_rows) {
                row[column.name] = nullptr;
            }
            for (auto& cell : column.values) {
                const auto index = static_cast<size_t>(cell.first.as<int64_t>());
                sol::object val = cell.second;
                if (val.is<int64_t>()) {
                    cpp_rows[index - 1][column.name] = val.as<int64_t>();
                } else if (val.is<double>()) {
                    cpp_rows[index - 1][column.name] = val.as<double>();
                } else if (val.is<std::string>()) {
                    cpp_rows[index - 1][column.name] = val.as<std::string>();
                } else {
                    throw std::runtime_error("Cannot update_time_series_group: column '" + column.name +
                                             "' has unsupported Lua type");
                }
            }
        }

        db.update_time_series_group(collection, group, id, cpp_rows);
    }

    static void upsert_time_series_row_lua(Database& db,
                                           const std::string& collection,
                                           const std::string& group,
                                           int64_t id,
                                           sol::table row) {
        db.upsert_time_series_row(collection, group, id, lua_table_to_value_map(row));
    }

    // ========================================================================
    // Time series files
    // ========================================================================

    static sol::table
    list_time_series_files_columns_lua(Database& db, const std::string& collection, sol::this_state s) {
        sol::state_view lua(s);
        return to_lua_table(lua, db.list_time_series_files_columns(collection));
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
        throw std::runtime_error(std::string("Failed to run Lua script: ") + err.what());
    }
}

}  // namespace quiver
