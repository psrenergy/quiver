// MSVC /O2 reports C4702 (unreachable code) inside sol2's call wrappers for the always-throwing fallback
// candidates below. The warning state at each template's definition decides, so the guard covers the includes.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include "quiver/expression/expression.h"

#include "lua_runner/internal.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/binary_metadata.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace quiver::lua_internal {

namespace {

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

// ------------------------------------------------------------------------
// Expression operand dispatch (shared by the Expression and BinaryFile usertypes and quiver.*)
// ------------------------------------------------------------------------

// The last candidate of every expression overload set. sol2 reaches it only after every typed
// candidate failed (a wrong type or a wrong count), so it only words the error: the leftmost bad
// operand, where a number beside an expression is a valid operand (`numbers`), else too many
// arguments. `operation` is what the script called: Lua's event name for a metamethod ("add",
// "unm", ...), the function name for quiver.* ("gt", "abs", ...).
Expression operand_error(const char* operation, std::size_t arity, bool numbers, const sol::variadic_args& args) {
    const std::size_t count = args.size();
    auto operand = [&](std::size_t i) {
        return i < count ? sol::object(args[i]) : sol::make_object(args.lua_state(), sol::lua_nil);
    };
    std::size_t number_operands = 0;
    for (std::size_t i = 0; i < arity; ++i) {
        number_operands += operand(i).get_type() == sol::type::number ? 1 : 0;
    }
    for (std::size_t i = 0; i < arity; ++i) {
        const auto o = operand(i);
        const bool valid =
            o.is<AbstractExpression>() || (numbers && number_operands < arity && o.get_type() == sol::type::number);
        if (!valid) {
            throw lua_type_error(operation, "operand", "an expression or a binary file", o);
        }
    }
    throw std::runtime_error(
        "Cannot " + std::string(operation) + ": too many arguments (expected " + std::to_string(arity) + ", got " +
        std::to_string(count) + ")"
    );
}

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
// One overload set for every binary Expression operator. Op is a transparent functor (std::plus<>,
// std::greater_equal<>, ...), so overload resolution on its arguments picks the matching Expression
// operator for each operand combination (expr/expr, expr/double, double/expr). std::logical_and<> /
// std::logical_or<> call the overloaded && / ||, which, unlike the built-in ones, evaluate both
// operands. number/number matches no typed candidate and reaches operand_error.
template <typename Op>
auto binop(const char* operation) {
    return sol::overload(
        [](const AbstractExpression& lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); },
        [](const AbstractExpression& lhs, double rhs) { return Op{}(lhs, rhs); },
        [](double lhs, const AbstractExpression& rhs) { return Op{}(lhs, rhs); },
        [operation](sol::variadic_args args) { return operand_error(operation, 2, true, args); }
    );
}

// Lua passes a unary metamethod its operand twice (OP_UNM / OP_BNOT), hence arity 2.
template <typename F>
auto unary_metamethod(const char* operation, F f) {
    return sol::overload(
        [f](const AbstractExpression& operand, const AbstractExpression&) { return f(operand); },
        [operation](sol::variadic_args args) { return operand_error(operation, 2, false, args); }
    );
}

template <typename F>
auto unary_function(const char* operation, F f) {
    return sol::overload(f, [operation](sol::variadic_args args) { return operand_error(operation, 1, false, args); });
}

// The arithmetic, unary-minus and logical metamethods shared by every usertype that is an expression
// (Expression and BinaryFile). One table, so a new operator is added once.
template <typename T>
void bind_expression_operators(sol::usertype<T>& type) {
    type[sol::meta_function::addition] = binop<std::plus<>>("add");
    type[sol::meta_function::subtraction] = binop<std::minus<>>("sub");
    type[sol::meta_function::multiplication] = binop<std::multiplies<>>("mul");
    type[sol::meta_function::division] = binop<std::divides<>>("div");
    type[sol::meta_function::unary_minus] =
        unary_metamethod("unm", [](const AbstractExpression& operand) { return -operand; });
    // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
    type[sol::meta_function::bitwise_and] = binop<std::logical_and<>>("band");
    type[sol::meta_function::bitwise_or] = binop<std::logical_or<>>("bor");
    type[sol::meta_function::bitwise_not] =
        unary_metamethod("bnot", [](const AbstractExpression& operand) { return !operand; });
}

}  // namespace

void bind_expression(sol::state& state, sol::table& ns, sol::usertype<BinaryFile>& binary_file_type, Database& db) {
    // The expression methods, one body each for every usertype that is an expression. Saving from a
    // file reads it by path and leaves the handle open.
    auto save = [&db](const AbstractExpression& self, const std::string& path) {
        self.save(resolve_sandboxed_path(db, "save", path));
    };
    auto get_metadata = [](const AbstractExpression& self) -> BinaryMetadata { return self.get_metadata(); };
    auto aggregate = [](const AbstractExpression& self,
                        const std::string& dimension,
                        const std::string& op,
                        const sol::object& parameter) {
        const auto operation = parse_aggregate_op(op, "aggregate");
        const auto value = optional_from_lua<double>(parameter, "aggregate", "parameter", "a number");
        return self.aggregate(dimension, operation, value);
    };
    auto aggregate_agents = [](const AbstractExpression& self, const std::string& op, const sol::object& parameter) {
        const auto operation = parse_aggregate_op(op, "aggregate_agents");
        const auto value = optional_from_lua<double>(parameter, "aggregate_agents", "parameter", "a number");
        return self.aggregate_agents(operation, value);
    };
    auto select_agents = [](const AbstractExpression& self, const sol::object& labels) {
        return self.select_agents(
            lua_table_to_vector<std::string>(require_table(labels, "select_agents", "labels"), "select_agents")
        );
    };
    auto rename_agents = [](const AbstractExpression& self, const sol::object& mapping) {
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
    };

    // Expression subsystem bindings (mirrors the Julia Expression surface)
    auto expression_type = state.new_usertype<Expression>(
        "Expression",
        sol::no_constructor,
        "save",
        save,
        "get_metadata",
        get_metadata,
        "aggregate",
        aggregate,
        "aggregate_agents",
        aggregate_agents,
        "select_agents",
        select_agents,
        "rename_agents",
        rename_agents
    );
    bind_expression_operators(expression_type);

    // A BinaryFile is an expression: the same operators and methods (file_a + file_b, -file, file * 2.0,
    // f:aggregate(...), f:save(...)). Through the indexer, not set_function: the sync test reads
    // set_function only on bind / ns, and lists these methods once, from the Expression usertype above.
    bind_expression_operators(binary_file_type);
    binary_file_type["save"] = save;
    binary_file_type["get_metadata"] = get_metadata;
    binary_file_type["aggregate"] = aggregate;
    binary_file_type["aggregate_agents"] = aggregate_agents;
    binary_file_type["select_agents"] = select_agents;
    binary_file_type["rename_agents"] = rename_agents;

    ns.set_function("expression", unary_function("expression", [](const AbstractExpression& operand) {
                        return Expression(operand);
                    }));
    ns.set_function("abs", unary_function("abs", [](const AbstractExpression& operand) {
                        return quiver::abs(operand);
                    }));
    ns.set_function("sqrt", unary_function("sqrt", [](const AbstractExpression& operand) {
                        return quiver::sqrt(operand);
                    }));
    ns.set_function("log", unary_function("log", [](const AbstractExpression& operand) {
                        return quiver::log(operand);
                    }));
    ns.set_function("exp", unary_function("exp", [](const AbstractExpression& operand) {
                        return quiver::exp(operand);
                    }));
    ns.set_function(
        "ifelse",
        sol::overload(
            [](const AbstractExpression& condition,
               const AbstractExpression& then_value,
               const AbstractExpression& else_value) { return quiver::ifelse(condition, then_value, else_value); },
            [](sol::variadic_args args) { return operand_error("ifelse", 3, false, args); }
        )
    );
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
