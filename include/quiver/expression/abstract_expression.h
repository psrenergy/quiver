#ifndef QUIVER_ABSTRACT_EXPRESSION_H
#define QUIVER_ABSTRACT_EXPRESSION_H

#include "../binary/binary_metadata.h"
#include "../export.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

class Expression;
class ExpressionNode;

// At namespace scope because this header cannot include expression_node.h (which includes
// binary_file.h, which includes this header).
enum class AggregateOperation { Sum, Mean, Min, Max, Percentile };

class QUIVER_API AbstractExpression {
public:
    virtual ~AbstractExpression() = default;

    // The expression's root node. A BinaryFile builds a fresh path-based leaf on every call, so the
    // caller must not call it on a moved-from file.
    virtual std::shared_ptr<ExpressionNode> node() const = 0;

    // The base body (the node's metadata) is valid while node() returns a node the object keeps alive,
    // as Expression's does. BinaryFile overrides it to return the handle's in-memory metadata.
    virtual const BinaryMetadata& get_metadata() const;

    void save(const std::string& path) const;

    Expression aggregate(
        const std::string& dimension,
        AggregateOperation operation,
        std::optional<double> parameter = std::nullopt
    ) const;

    Expression aggregate_agents(AggregateOperation operation, std::optional<double> parameter = std::nullopt) const;

    Expression select_agents(const std::vector<std::string>& labels) const;

    Expression rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const;

protected:
    AbstractExpression() = default;
    AbstractExpression(const AbstractExpression&) = default;
    AbstractExpression(AbstractExpression&&) noexcept = default;
    AbstractExpression& operator=(const AbstractExpression&) = default;
    AbstractExpression& operator=(AbstractExpression&&) noexcept = default;
};

}  // namespace quiver

#endif  // QUIVER_ABSTRACT_EXPRESSION_H
