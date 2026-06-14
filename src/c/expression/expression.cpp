#include "quiver/c/expression/expression.h"

#include "../internal.h"

#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

template <typename Lhs, typename Rhs>
quiver::Expression dispatch(quiver_expression_operation_t operation, const Lhs& lhs, const Rhs& rhs) {
    switch (operation) {
    case QUIVER_EXPRESSION_OPERATION_ADD:
        return lhs + rhs;
    case QUIVER_EXPRESSION_OPERATION_SUBTRACT:
        return lhs - rhs;
    case QUIVER_EXPRESSION_OPERATION_MULTIPLY:
        return lhs * rhs;
    case QUIVER_EXPRESSION_OPERATION_DIVIDE:
        return lhs / rhs;
    case QUIVER_EXPRESSION_OPERATION_GT:
        return quiver::gt(lhs, rhs);
    case QUIVER_EXPRESSION_OPERATION_LT:
        return quiver::lt(lhs, rhs);
    case QUIVER_EXPRESSION_OPERATION_GTE:
        return quiver::gte(lhs, rhs);
    case QUIVER_EXPRESSION_OPERATION_LTE:
        return quiver::lte(lhs, rhs);
    case QUIVER_EXPRESSION_OPERATION_EQ:
        return quiver::eq(lhs, rhs);
    case QUIVER_EXPRESSION_OPERATION_NEQ:
        return quiver::neq(lhs, rhs);
    }
    throw std::runtime_error("Cannot apply: unknown expression operation");
}

quiver::Expression dispatch_unary(quiver_expression_unary_operation_t operation, const quiver::Expression& operand) {
    switch (operation) {
    case QUIVER_EXPRESSION_UNARY_OPERATION_NEGATE:
        return -operand;
    case QUIVER_EXPRESSION_UNARY_OPERATION_ABS:
        return quiver::abs(operand);
    case QUIVER_EXPRESSION_UNARY_OPERATION_SQRT:
        return quiver::sqrt(operand);
    case QUIVER_EXPRESSION_UNARY_OPERATION_LOG:
        return quiver::log(operand);
    case QUIVER_EXPRESSION_UNARY_OPERATION_EXP:
        return quiver::exp(operand);
    }
    throw std::runtime_error("Cannot apply: unknown expression unary operation");
}

quiver::Expression dispatch_ternary(quiver_expression_ternary_operation_t operation,
                                    const quiver::Expression& condition,
                                    const quiver::Expression& then_value,
                                    const quiver::Expression& else_value) {
    switch (operation) {
    case QUIVER_EXPRESSION_TERNARY_OPERATION_IFELSE:
        return quiver::ifelse(condition, then_value, else_value);
    }
    throw std::runtime_error("Cannot apply: unknown expression ternary operation");
}

quiver::ExpressionAggregate::Operation from_c(quiver_expression_aggregate_operation_t op) {
    switch (op) {
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_SUM:
        return quiver::ExpressionAggregate::Operation::Sum;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_MEAN:
        return quiver::ExpressionAggregate::Operation::Mean;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_MIN:
        return quiver::ExpressionAggregate::Operation::Min;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_MAX:
        return quiver::ExpressionAggregate::Operation::Max;
    case QUIVER_EXPRESSION_AGGREGATE_OPERATION_PERCENTILE:
        return quiver::ExpressionAggregate::Operation::Percentile;
    }
    throw std::runtime_error("Cannot aggregate: unknown operation enum value");
}

quiver::ExpressionAggregateAgents::Operation from_c(quiver_expression_aggregate_agents_operation_t op) {
    switch (op) {
    case QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_SUM:
        return quiver::ExpressionAggregateAgents::Operation::Sum;
    case QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_MEAN:
        return quiver::ExpressionAggregateAgents::Operation::Mean;
    case QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_MIN:
        return quiver::ExpressionAggregateAgents::Operation::Min;
    case QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_MAX:
        return quiver::ExpressionAggregateAgents::Operation::Max;
    case QUIVER_EXPRESSION_AGGREGATE_AGENTS_OPERATION_PERCENTILE:
        return quiver::ExpressionAggregateAgents::Operation::Percentile;
    }
    throw std::runtime_error("Cannot aggregate_agents: unknown operation enum value");
}

}  // namespace

extern "C" {

// Construction

QUIVER_C_API quiver_error_t quiver_expression_from_file(quiver_binary_file_t* file, quiver_expression_t** out) {
    QUIVER_REQUIRE(file, out);

    try {
        *out = new quiver_expression(quiver::Expression(file->binary_file));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Lifecycle

QUIVER_C_API quiver_error_t quiver_expression_close(quiver_expression_t* expression) {
    delete expression;
    return QUIVER_OK;
}

// Operations

QUIVER_C_API quiver_error_t quiver_expression_apply(quiver_expression_operation_t operation,
                                                    quiver_expression_t* lhs,
                                                    quiver_expression_t* rhs,
                                                    quiver_expression_t** out) {
    QUIVER_REQUIRE(lhs, rhs, out);

    try {
        *out = new quiver_expression(dispatch(operation, lhs->expression, rhs->expression));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_expression_apply_scalar_right(quiver_expression_operation_t operation,
                                                                 quiver_expression_t* lhs,
                                                                 double rhs,
                                                                 quiver_expression_t** out) {
    QUIVER_REQUIRE(lhs, out);

    try {
        *out = new quiver_expression(dispatch(operation, lhs->expression, rhs));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_expression_apply_scalar_left(quiver_expression_operation_t operation,
                                                                double lhs,
                                                                quiver_expression_t* rhs,
                                                                quiver_expression_t** out) {
    QUIVER_REQUIRE(rhs, out);

    try {
        *out = new quiver_expression(dispatch(operation, lhs, rhs->expression));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_expression_apply_unary(quiver_expression_unary_operation_t operation,
                                                          quiver_expression_t* operand,
                                                          quiver_expression_t** out) {
    QUIVER_REQUIRE(operand, out);

    try {
        *out = new quiver_expression(dispatch_unary(operation, operand->expression));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_expression_apply_ternary(quiver_expression_ternary_operation_t operation,
                                                            quiver_expression_t* condition,
                                                            quiver_expression_t* then_value,
                                                            quiver_expression_t* else_value,
                                                            quiver_expression_t** out) {
    QUIVER_REQUIRE(condition, then_value, else_value, out);

    try {
        *out = new quiver_expression(
            dispatch_ternary(operation, condition->expression, then_value->expression, else_value->expression));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Materialization

QUIVER_C_API quiver_error_t quiver_expression_save(quiver_expression_t* expression, const char* path) {
    QUIVER_REQUIRE(expression, path);

    try {
        expression->expression.save(path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Metadata

QUIVER_C_API quiver_error_t quiver_expression_get_metadata(quiver_expression_t* expression,
                                                           quiver_binary_metadata_t** out) {
    QUIVER_REQUIRE(expression, out);

    try {
        *out = new quiver_binary_metadata{expression->expression.metadata()};
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    }
}

// Aggregation

QUIVER_C_API quiver_error_t quiver_expression_aggregate(quiver_expression_t* expression,
                                                        const char* dimension,
                                                        quiver_expression_aggregate_operation_t operation,
                                                        const double* parameter,
                                                        quiver_expression_t** out) {
    QUIVER_REQUIRE(expression, dimension, out);

    try {
        std::optional<double> p = parameter ? std::optional<double>(*parameter) : std::nullopt;
        *out = new quiver_expression(expression->expression.aggregate(dimension, from_c(operation), p));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_expression_aggregate_agents(quiver_expression_t* expression,
                                                               quiver_expression_aggregate_agents_operation_t operation,
                                                               const double* parameter,
                                                               quiver_expression_t** out) {
    QUIVER_REQUIRE(expression, out);

    try {
        std::optional<double> p = parameter ? std::optional<double>(*parameter) : std::nullopt;
        *out = new quiver_expression(expression->expression.aggregate_agents(from_c(operation), p));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_expression_select_agents(quiver_expression_t* expression,
                                                            const char* const* labels,
                                                            size_t label_count,
                                                            quiver_expression_t** out) {
    QUIVER_REQUIRE(expression, labels, out);

    try {
        std::vector<std::string> selected;
        selected.reserve(label_count);
        for (size_t i = 0; i < label_count; ++i) {
            selected.emplace_back(labels[i]);
        }
        *out = new quiver_expression(expression->expression.select_agents(selected));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_expression_rename_agents(quiver_expression_t* expression,
                                                            const char* const* old_labels,
                                                            const char* const* new_labels,
                                                            size_t mapping_count,
                                                            quiver_expression_t** out) {
    QUIVER_REQUIRE(expression, old_labels, new_labels, out);

    try {
        std::vector<std::pair<std::string, std::string>> mapping;
        mapping.reserve(mapping_count);
        for (size_t i = 0; i < mapping_count; ++i) {
            mapping.emplace_back(old_labels[i], new_labels[i]);
        }
        *out = new quiver_expression(expression->expression.rename_agents(mapping));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
