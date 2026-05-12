#ifndef QUIVER_EXPRESSION_H
#define QUIVER_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"
#include "expression_node.h"

#include <memory>
#include <optional>
#include <string>

namespace quiver {

class QUIVER_API Expression {
public:
    Expression(const BinaryFile& file);

    explicit Expression(std::shared_ptr<ExpressionNode> node);

    const BinaryMetadata& metadata() const;

    void save(const std::string& path) const;

    Expression aggregate(const std::string& dimension,
                         const std::string& operation,
                         std::optional<double> parameter = std::nullopt) const;

    Expression aggregate_agents(const std::string& operation, std::optional<double> parameter = std::nullopt) const;

private:
    friend Expression operator+(const Expression&, const Expression&);
    friend Expression operator+(const Expression&, double);
    friend Expression operator+(double, const Expression&);
    friend Expression operator-(const Expression&, const Expression&);
    friend Expression operator-(const Expression&, double);
    friend Expression operator-(double, const Expression&);
    friend Expression operator*(const Expression&, const Expression&);
    friend Expression operator*(const Expression&, double);
    friend Expression operator*(double, const Expression&);
    friend Expression operator/(const Expression&, const Expression&);
    friend Expression operator/(const Expression&, double);
    friend Expression operator/(double, const Expression&);

    friend Expression operator-(const Expression&);
    friend Expression abs(const Expression&);
    friend Expression sqrt(const Expression&);
    friend Expression log(const Expression&);
    friend Expression exp(const Expression&);

    friend Expression ifelse(const Expression&, const Expression&, const Expression&);

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

QUIVER_API Expression
ifelse(const Expression& condition, const Expression& then_value, const Expression& else_value);

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_H
