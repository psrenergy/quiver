#ifndef QUIVER_EXPR_EXPRESSION_H
#define QUIVER_EXPR_EXPRESSION_H

#include "../binary/binary_file.h"
#include "../binary/binary_metadata.h"
#include "../export.h"
#include "node.h"

#include <memory>
#include <string>

namespace quiver::expr {

class QUIVER_API Expression {
public:
    Expression(const BinaryFile& file);  // NOT explicit by design — see CORE-01

    explicit Expression(std::shared_ptr<Node> node);

    BinaryMetadata metadata() const;

    void save(const std::string& path) const;

    const std::shared_ptr<Node>& node() const;

private:
    std::shared_ptr<Node> node_;
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

}  // namespace quiver::expr

#endif  // QUIVER_EXPR_EXPRESSION_H
