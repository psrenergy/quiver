#ifndef QUIVER_EXPRESSION_H
#define QUIVER_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"
#include "abstract_expression.h"
#include "expression_node.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

class QUIVER_API Expression final : public AbstractExpression {
public:
    explicit Expression(std::shared_ptr<ExpressionNode> node);

    explicit Expression(const AbstractExpression& expression);

    std::shared_ptr<ExpressionNode> node() const override;

private:
    std::shared_ptr<ExpressionNode> node_;
};

QUIVER_API Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator+(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator+(double lhs, const AbstractExpression& rhs);

QUIVER_API Expression operator-(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator-(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator-(double lhs, const AbstractExpression& rhs);

QUIVER_API Expression operator*(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator*(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator*(double lhs, const AbstractExpression& rhs);

QUIVER_API Expression operator/(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator/(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator/(double lhs, const AbstractExpression& rhs);

QUIVER_API Expression operator-(const AbstractExpression& operand);
QUIVER_API Expression abs(const AbstractExpression& operand);
QUIVER_API Expression sqrt(const AbstractExpression& operand);
QUIVER_API Expression log(const AbstractExpression& operand);
QUIVER_API Expression exp(const AbstractExpression& operand);

QUIVER_API Expression
ifelse(const AbstractExpression& condition, const AbstractExpression& then_value, const AbstractExpression& else_value);

// Element-wise comparisons producing 1.0 (true) / 0.0 (false); a NaN operand propagates as NaN.
// `==`/`!=` return an elementwise mask Expression (not a bool, Eigen-style). Every `==`/`!=` overload
// has a partner with the same parameters, so C++20 forms no rewritten candidate and `a == b` calls the
// declared overload with the operands in the order written.
QUIVER_API Expression operator>(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator>(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator>(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator<(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator<(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator<(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator>=(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator>=(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator>=(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator<=(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator<=(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator<=(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator==(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator==(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator==(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator!=(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator!=(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator!=(double lhs, const AbstractExpression& rhs);

// Logical operators on boolean-valued expressions (nonzero is true), producing `1.0`/`0.0`; a NaN
// operand propagates as NaN. The result is unitless and `&&`/`||` skip unit-match validation, so
// conditions on different-unit variables compose. Overloading `&&`/`||` drops short-circuiting,
// which is irrelevant for a lazy DAG (both operands are always built).
QUIVER_API Expression operator&&(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator&&(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator&&(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator||(const AbstractExpression& lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator||(const AbstractExpression& lhs, double rhs);
QUIVER_API Expression operator||(double lhs, const AbstractExpression& rhs);
QUIVER_API Expression operator!(const AbstractExpression& operand);

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_H
