#ifndef QUIVER_EXPRESSION_H
#define QUIVER_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"
#include "expression_node.h"

#include <memory>
#include <string>

namespace quiver {

class QUIVER_API Expression {
public:
    explicit Expression(const BinaryFile& file);

    explicit Expression(std::shared_ptr<ExpressionNode> node);

    const BinaryMetadata& metadata() const;

    void save(const std::string& path) const;

    const std::shared_ptr<ExpressionNode>& node() const;

private:
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

}  // namespace quiver

#endif  // QUIVER_EXPRESSION_H
