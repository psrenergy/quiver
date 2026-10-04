#include "quiver/expression/expression.h"

#include "lua_runner/internal.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/binary_metadata.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace quiver::lua_internal {

namespace {

// ------------------------------------------------------------------------
// Expression operator dispatch (shared by Expression and BinaryFile metamethods)
// ------------------------------------------------------------------------

bool is_number(const sol::object& o) {
    return o.get_type() == sol::type::number;
}

// A Lua operand in arithmetic is either an Expression, a BinaryFile (auto-wrapped), or a number
// (handled by the scalar operator overloads). Numbers are rejected here on purpose. `operation` is
// what the script called: Lua's event name for a metamethod ("add", "unm", ...), the function name
// for quiver.* ("gt", "abs", ...).
Expression to_expression(const sol::object& o, const char* operation) {
    if (o.is<Expression>()) {
        return o.as<Expression>();
    }
    if (o.is<BinaryFile>()) {
        return Expression(o.as<BinaryFile&>());
    }
    throw lua_type_error(operation, "operand", "an expression or a binary file", o);
}

// One body for every binary Expression operator. Op is a transparent functor (std::plus<>,
// std::greater_equal<>, ...), so overload resolution on its arguments picks the matching
// Expression operator for each operand combination (expr/expr, expr/double, double/expr).
// std::logical_and<> / std::logical_or<> call the overloaded && / ||, which, unlike the built-in
// ones, evaluate both operands. number/number reaches to_expression, which throws. binop builds
// the callable for one operation, so the operand error names it (`operation`, as in to_expression).
template <typename Op>
auto binop(const char* operation) {
    return [operation](const sol::object& lhs, const sol::object& rhs) -> Expression {
        const bool lnum = is_number(lhs);
        const bool rnum = is_number(rhs);
        if (lnum && !rnum) {
            return Op{}(lhs.as<double>(), to_expression(rhs, operation));
        }
        if (!lnum && rnum) {
            return Op{}(to_expression(lhs, operation), rhs.as<double>());
        }
        // Locals, not arguments: argument order is unspecified, and the leftmost bad operand is reported.
        auto a = to_expression(lhs, operation);
        auto b = to_expression(rhs, operation);
        return Op{}(a, b);
    };
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
    type[sol::meta_function::addition] = binop<std::plus<>>("add");
    type[sol::meta_function::subtraction] = binop<std::minus<>>("sub");
    type[sol::meta_function::multiplication] = binop<std::multiplies<>>("mul");
    type[sol::meta_function::division] = binop<std::divides<>>("div");
    type[sol::meta_function::unary_minus] = [](sol::object a, sol::object) { return -to_expression(a, "unm"); };
    // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
    type[sol::meta_function::bitwise_and] = binop<std::logical_and<>>("band");
    type[sol::meta_function::bitwise_or] = binop<std::logical_or<>>("bor");
    type[sol::meta_function::bitwise_not] = [](sol::object a, sol::object) { return !to_expression(a, "bnot"); };
}

}  // namespace

void bind_expression(sol::state& state, sol::table& ns, sol::usertype<BinaryFile>& binary_file_type, Database& db) {
    // Arithmetic on files mirrors Julia: file_a + file_b, -file, file * 2.0 (auto-wrap to Expression)
    bind_expression_operators(binary_file_type);

    // Expression subsystem bindings (mirrors the Julia Expression surface)
    auto expression_type = state.new_usertype<Expression>(
        "Expression",
        sol::no_constructor,
        "save",
        [&db](Expression& self, const std::string& path) { self.save(resolve_sandboxed_path(db, "save", path)); },
        "metadata",
        [](Expression& self) -> BinaryMetadata { return self.get_metadata(); },
        "aggregate",
        [](Expression& self, const std::string& dimension, const std::string& op, const sol::object& parameter) {
            const auto operation = parse_aggregate_op(op, "aggregate");
            const auto value = optional_from_lua<double>(parameter, "aggregate", "parameter", "a number");
            return self.aggregate(dimension, operation, value);
        },
        "aggregate_agents",
        [](Expression& self, const std::string& op, const sol::object& parameter) {
            const auto operation = parse_aggregate_op(op, "aggregate_agents");
            const auto value = optional_from_lua<double>(parameter, "aggregate_agents", "parameter", "a number");
            return self.aggregate_agents(operation, value);
        },
        "select_agents",
        [](Expression& self, const sol::object& labels) {
            return self.select_agents(
                lua_table_to_vector<std::string>(require_table(labels, "select_agents", "labels"), "select_agents")
            );
        },
        "rename_agents",
        [](Expression& self, const sol::object& mapping) {
            // The explicit table check rejects a userdata too. Then collect, then check both
            // halves with lua_cell_as: an unchecked as<std::string>() spelled a number key as
            // text and gave "" for a boolean, since sol2's getter is unchecked.
            std::vector<std::pair<std::string, std::string>> pairs;
            for (const auto& [key, value] : collect_entries(require_table(mapping, "rename_agents", "mapping"))) {
                auto old_name = lua_cell_as<std::string>(key, "rename_agents", "key");
                auto new_name = lua_cell_as<std::string>(value, "rename_agents", "value for '" + old_name + "'");
                pairs.emplace_back(std::move(old_name), std::move(new_name));
            }
            return self.rename_agents(pairs);
        }
    );
    bind_expression_operators(expression_type);

    ns.set_function("expression", [](sol::object o) { return to_expression(o, "expression"); });
    ns.set_function("abs", [](sol::object o) { return quiver::abs(to_expression(o, "abs")); });
    ns.set_function("sqrt", [](sol::object o) { return quiver::sqrt(to_expression(o, "sqrt")); });
    ns.set_function("log", [](sol::object o) { return quiver::log(to_expression(o, "log")); });
    ns.set_function("exp", [](sol::object o) { return quiver::exp(to_expression(o, "exp")); });
    ns.set_function("ifelse", [](sol::object c, sol::object t, sol::object e) {
        auto cond = to_expression(c, "ifelse");
        auto then_value = to_expression(t, "ifelse");
        auto else_value = to_expression(e, "ifelse");
        return quiver::ifelse(cond, then_value, else_value);
    });
    // Comparisons produce 1.0/0.0 per element (NaN operand -> NaN). Free functions because Lua
    // comparison metamethods are coerced to bool and cannot return an Expression.
    ns.set_function("gt", binop<std::greater<>>("gt"));
    ns.set_function("lt", binop<std::less<>>("lt"));
    ns.set_function("gte", binop<std::greater_equal<>>("gte"));
    ns.set_function("lte", binop<std::less_equal<>>("lte"));
    ns.set_function("eq", binop<std::equal_to<>>("eq"));
    ns.set_function("neq", binop<std::not_equal_to<>>("neq"));
    // Logical ops on boolean-valued expressions are the `&` / `|` / `~` metamethods bound on the
    // Expression and BinaryFile usertypes (`and`/`or`/`not` are Lua keywords, so no free functions).
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
