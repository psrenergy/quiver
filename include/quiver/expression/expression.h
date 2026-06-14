#ifndef QUIVER_EXPRESSION_H
#define QUIVER_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"
#include "expression_node.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

class QUIVER_API Expression {
public:
    Expression(const BinaryFile& file);

    explicit Expression(std::shared_ptr<ExpressionNode> node);

    const BinaryMetadata& metadata() const;

    void save(const std::string& path) const;

    Expression aggregate(const std::string& dimension,
                         ExpressionAggregate::Operation operation,
                         std::optional<double> parameter = std::nullopt) const;

    Expression aggregate_agents(ExpressionAggregateAgents::Operation operation,
                                std::optional<double> parameter = std::nullopt) const;

    Expression select_agents(const std::vector<std::string>& labels) const;

    Expression rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const;

private:
    friend QUIVER_API Expression operator+(const Expression&, const Expression&);
    friend QUIVER_API Expression operator+(const Expression&, double);
    friend QUIVER_API Expression operator+(double, const Expression&);
    friend QUIVER_API Expression operator-(const Expression&, const Expression&);
    friend QUIVER_API Expression operator-(const Expression&, double);
    friend QUIVER_API Expression operator-(double, const Expression&);
    friend QUIVER_API Expression operator*(const Expression&, const Expression&);
    friend QUIVER_API Expression operator*(const Expression&, double);
    friend QUIVER_API Expression operator*(double, const Expression&);
    friend QUIVER_API Expression operator/(const Expression&, const Expression&);
    friend QUIVER_API Expression operator/(const Expression&, double);
    friend QUIVER_API Expression operator/(double, const Expression&);

    friend QUIVER_API Expression operator-(const Expression&);
    friend QUIVER_API Expression abs(const Expression&);
    friend QUIVER_API Expression sqrt(const Expression&);
    friend QUIVER_API Expression log(const Expression&);
    friend QUIVER_API Expression exp(const Expression&);

    friend QUIVER_API Expression ifelse(const Expression&, const Expression&, const Expression&);

    friend QUIVER_API Expression gt(const Expression&, const Expression&);
    friend QUIVER_API Expression gt(const Expression&, double);
    friend QUIVER_API Expression gt(double, const Expression&);
    friend QUIVER_API Expression lt(const Expression&, const Expression&);
    friend QUIVER_API Expression lt(const Expression&, double);
    friend QUIVER_API Expression lt(double, const Expression&);
    friend QUIVER_API Expression gte(const Expression&, const Expression&);
    friend QUIVER_API Expression gte(const Expression&, double);
    friend QUIVER_API Expression gte(double, const Expression&);
    friend QUIVER_API Expression lte(const Expression&, const Expression&);
    friend QUIVER_API Expression lte(const Expression&, double);
    friend QUIVER_API Expression lte(double, const Expression&);
    friend QUIVER_API Expression eq(const Expression&, const Expression&);
    friend QUIVER_API Expression eq(const Expression&, double);
    friend QUIVER_API Expression eq(double, const Expression&);
    friend QUIVER_API Expression neq(const Expression&, const Expression&);
    friend QUIVER_API Expression neq(const Expression&, double);
    friend QUIVER_API Expression neq(double, const Expression&);

    std::shared_ptr<ExpressionNode> node_;
};

QUIVER_API Expression operator+(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator+(const Expression& lhs, double rhs);
QUIVER_API Expression operator+(double lhs, const Expression& rhs);

QUIVER_API Expression operator-(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator-(const Expression& lhs, double rhs);
QUIVER_API Expression operator-(double lhs, const Expression& rhs);

QUIVER_API Expression operator*(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator*(const Expression& lhs, double rhs);
QUIVER_API Expression operator*(double lhs, const Expression& rhs);

QUIVER_API Expression operator/(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator/(const Expression& lhs, double rhs);
QUIVER_API Expression operator/(double lhs, const Expression& rhs);

QUIVER_API Expression operator-(const Expression& operand);
QUIVER_API Expression abs(const Expression& operand);
QUIVER_API Expression sqrt(const Expression& operand);
QUIVER_API Expression log(const Expression& operand);
QUIVER_API Expression exp(const Expression& operand);

QUIVER_API Expression ifelse(const Expression& condition, const Expression& then_value, const Expression& else_value);

// Element-wise comparisons producing 1.0 (true) / 0.0 (false); a NaN operand propagates as NaN.
// Free functions (not operators) so the surface is identical in C++ and Lua, where comparison
// metamethods cannot return an Expression.
QUIVER_API Expression gt(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression gt(const Expression& lhs, double rhs);
QUIVER_API Expression gt(double lhs, const Expression& rhs);
QUIVER_API Expression lt(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression lt(const Expression& lhs, double rhs);
QUIVER_API Expression lt(double lhs, const Expression& rhs);
QUIVER_API Expression gte(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression gte(const Expression& lhs, double rhs);
QUIVER_API Expression gte(double lhs, const Expression& rhs);
QUIVER_API Expression lte(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression lte(const Expression& lhs, double rhs);
QUIVER_API Expression lte(double lhs, const Expression& rhs);
QUIVER_API Expression eq(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression eq(const Expression& lhs, double rhs);
QUIVER_API Expression eq(double lhs, const Expression& rhs);
QUIVER_API Expression neq(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression neq(const Expression& lhs, double rhs);
QUIVER_API Expression neq(double lhs, const Expression& rhs);

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_H
