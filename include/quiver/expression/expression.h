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

    friend QUIVER_API Expression operator>(const Expression&, const Expression&);
    friend QUIVER_API Expression operator>(const Expression&, double);
    friend QUIVER_API Expression operator>(double, const Expression&);
    friend QUIVER_API Expression operator<(const Expression&, const Expression&);
    friend QUIVER_API Expression operator<(const Expression&, double);
    friend QUIVER_API Expression operator<(double, const Expression&);
    friend QUIVER_API Expression operator>=(const Expression&, const Expression&);
    friend QUIVER_API Expression operator>=(const Expression&, double);
    friend QUIVER_API Expression operator>=(double, const Expression&);
    friend QUIVER_API Expression operator<=(const Expression&, const Expression&);
    friend QUIVER_API Expression operator<=(const Expression&, double);
    friend QUIVER_API Expression operator<=(double, const Expression&);
    friend QUIVER_API Expression operator==(const Expression&, const Expression&);
    friend QUIVER_API Expression operator==(const Expression&, double);
    friend QUIVER_API Expression operator==(double, const Expression&);
    friend QUIVER_API Expression operator!=(const Expression&, const Expression&);
    friend QUIVER_API Expression operator!=(const Expression&, double);
    friend QUIVER_API Expression operator!=(double, const Expression&);

    friend QUIVER_API Expression operator&&(const Expression&, const Expression&);
    friend QUIVER_API Expression operator&&(const Expression&, double);
    friend QUIVER_API Expression operator&&(double, const Expression&);
    friend QUIVER_API Expression operator||(const Expression&, const Expression&);
    friend QUIVER_API Expression operator||(const Expression&, double);
    friend QUIVER_API Expression operator||(double, const Expression&);
    friend QUIVER_API Expression operator!(const Expression&);

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
// `==`/`!=` return an elementwise mask Expression (not a bool, Eigen-style). All operators take the
// three combos explicitly (no reliance on C++20 reversed candidates, which the compiler does not
// synthesize for these non-bool-returning operators).
QUIVER_API Expression operator>(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator>(const Expression& lhs, double rhs);
QUIVER_API Expression operator>(double lhs, const Expression& rhs);
QUIVER_API Expression operator<(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator<(const Expression& lhs, double rhs);
QUIVER_API Expression operator<(double lhs, const Expression& rhs);
QUIVER_API Expression operator>=(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator>=(const Expression& lhs, double rhs);
QUIVER_API Expression operator>=(double lhs, const Expression& rhs);
QUIVER_API Expression operator<=(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator<=(const Expression& lhs, double rhs);
QUIVER_API Expression operator<=(double lhs, const Expression& rhs);
QUIVER_API Expression operator==(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator==(const Expression& lhs, double rhs);
QUIVER_API Expression operator==(double lhs, const Expression& rhs);
QUIVER_API Expression operator!=(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator!=(const Expression& lhs, double rhs);
QUIVER_API Expression operator!=(double lhs, const Expression& rhs);

// Logical operators on boolean-valued expressions (nonzero is true), producing `1.0`/`0.0`; a NaN
// operand propagates as NaN. The result is unitless and `&&`/`||` skip unit-match validation, so
// conditions on different-unit variables compose. Overloading `&&`/`||` drops short-circuiting,
// which is irrelevant for a lazy DAG (both operands are always built).
QUIVER_API Expression operator&&(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator&&(const Expression& lhs, double rhs);
QUIVER_API Expression operator&&(double lhs, const Expression& rhs);
QUIVER_API Expression operator||(const Expression& lhs, const Expression& rhs);
QUIVER_API Expression operator||(const Expression& lhs, double rhs);
QUIVER_API Expression operator||(double lhs, const Expression& rhs);
QUIVER_API Expression operator!(const Expression& operand);

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_H
