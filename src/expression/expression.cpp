#include "quiver/expression/expression.h"

#include "quiver/binary/binary_file.h"
#include "quiver/binary/binary_metadata.h"
#include "quiver/binary/iteration.h"
#include "quiver/expression/expression_node.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

const BinaryMetadata& AbstractExpression::get_metadata() const {
    return node()->metadata();
}

Expression AbstractExpression::aggregate(
    const std::string& dimension,
    AggregateOperation operation,
    std::optional<double> parameter
) const {
    return Expression(std::make_shared<ExpressionAggregate>(operation, node(), dimension, parameter));
}

Expression AbstractExpression::aggregate_agents(AggregateOperation operation, std::optional<double> parameter) const {
    return Expression(std::make_shared<ExpressionAggregateAgents>(operation, node(), parameter));
}

Expression AbstractExpression::select_agents(const std::vector<std::string>& labels) const {
    return Expression(std::make_shared<ExpressionSelectAgents>(node(), labels));
}

Expression AbstractExpression::rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const {
    return Expression(std::make_shared<ExpressionRenameAgents>(node(), mapping));
}

void AbstractExpression::save(const std::string& path) const {
    const auto root = node();
    std::vector<BinaryFile*> input_files;
    root->collect_input_files(input_files);

    const auto canonical_out = std::filesystem::weakly_canonical(path).string();
    for (const auto* f : input_files) {
        const auto& in_path = f->get_file_path();
        if (std::filesystem::weakly_canonical(in_path).string() == canonical_out) {
            throw std::runtime_error("Cannot save: output path collides with input file '" + in_path + "'");
        }
    }

    // Guard is in place before any open() so a mid-loop failure still closes whatever did open.
    struct CloseOnExit {
        const std::vector<BinaryFile*>& files;
        ~CloseOnExit() {
            for (auto* f : files) {
                f->close();
            }
        }
    } guard{input_files};

    for (auto* f : input_files) {
        f->open('r');
    }

    const auto& meta = root->metadata();
    auto writer = BinaryFile::open_file(path, 'w', meta);

    std::unordered_map<std::string, int64_t> dim_map;
    dim_map.reserve(meta.dimensions.size());

    std::vector<int64_t> dims = first_dimensions(meta);
    std::vector<double> row;
    for (;;) {
        root->compute_row(dims, row);
        for (size_t i = 0; i < meta.dimensions.size(); ++i) {
            dim_map[meta.dimensions[i].name] = dims[i];
        }
        writer.write(row, dim_map);

        auto nxt = next_dimensions(meta, dims);
        if (!nxt) {
            break;
        }
        dims = std::move(*nxt);
    }
}

Expression::Expression(std::shared_ptr<ExpressionNode> node) : node_(std::move(node)) {}

Expression::Expression(const AbstractExpression& expression) : node_(expression.node()) {}

std::shared_ptr<ExpressionNode> Expression::node() const {
    return node_;
}

namespace {

// One node() call per operand, into locals in declaration order: operands are evaluated left to right.
Expression binary_op(ExpressionBinary::Operation op, const AbstractExpression& lhs, const AbstractExpression& rhs) {
    auto l = lhs.node();
    auto r = rhs.node();
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(l), std::move(r)));
}

// The scalar takes the node's metadata, not get_metadata(): an unopened file's handle metadata is empty.
Expression binary_op(ExpressionBinary::Operation op, const AbstractExpression& lhs, double rhs) {
    auto l = lhs.node();
    auto scalar = std::make_shared<ExpressionScalar>(rhs, l->metadata());
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(l), std::move(scalar)));
}

Expression binary_op(ExpressionBinary::Operation op, double lhs, const AbstractExpression& rhs) {
    auto r = rhs.node();
    auto scalar = std::make_shared<ExpressionScalar>(lhs, r->metadata());
    return Expression(std::make_shared<ExpressionBinary>(op, std::move(scalar), std::move(r)));
}

Expression unary_op(ExpressionUnary::Operation op, const AbstractExpression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(op, operand.node()));
}

}  // namespace

Expression operator+(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Add, lhs, rhs);
}

Expression operator-(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Subtract, lhs, rhs);
}
Expression operator-(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Subtract, lhs, rhs);
}
Expression operator-(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Subtract, lhs, rhs);
}

Expression operator*(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Multiply, lhs, rhs);
}
Expression operator*(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Multiply, lhs, rhs);
}
Expression operator*(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Multiply, lhs, rhs);
}

Expression operator/(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Divide, lhs, rhs);
}
Expression operator/(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Divide, lhs, rhs);
}
Expression operator/(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Divide, lhs, rhs);
}

Expression operator-(const AbstractExpression& operand) {
    return unary_op(ExpressionUnary::Operation::Negate, operand);
}
Expression abs(const AbstractExpression& operand) {
    return unary_op(ExpressionUnary::Operation::Abs, operand);
}
Expression sqrt(const AbstractExpression& operand) {
    return unary_op(ExpressionUnary::Operation::Sqrt, operand);
}
Expression log(const AbstractExpression& operand) {
    return unary_op(ExpressionUnary::Operation::Log, operand);
}
Expression exp(const AbstractExpression& operand) {
    return unary_op(ExpressionUnary::Operation::Exp, operand);
}

Expression ifelse(
    const AbstractExpression& condition,
    const AbstractExpression& then_value,
    const AbstractExpression& else_value
) {
    auto c = condition.node();
    auto t = then_value.node();
    auto e = else_value.node();
    return Expression(
        std::make_shared<ExpressionTernary>(
            ExpressionTernary::Operation::IfElse,
            std::move(c),
            std::move(t),
            std::move(e)
        )
    );
}

Expression operator>(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Gt, lhs, rhs);
}
Expression operator>(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Gt, lhs, rhs);
}
Expression operator>(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Gt, lhs, rhs);
}

Expression operator<(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Lt, lhs, rhs);
}
Expression operator<(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Lt, lhs, rhs);
}
Expression operator<(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Lt, lhs, rhs);
}

Expression operator>=(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Gte, lhs, rhs);
}
Expression operator>=(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Gte, lhs, rhs);
}
Expression operator>=(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Gte, lhs, rhs);
}

Expression operator<=(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Lte, lhs, rhs);
}
Expression operator<=(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Lte, lhs, rhs);
}
Expression operator<=(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Lte, lhs, rhs);
}

Expression operator==(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Eq, lhs, rhs);
}
Expression operator==(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Eq, lhs, rhs);
}
Expression operator==(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Eq, lhs, rhs);
}

Expression operator!=(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Neq, lhs, rhs);
}
Expression operator!=(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Neq, lhs, rhs);
}
Expression operator!=(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Neq, lhs, rhs);
}

Expression operator&&(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::And, lhs, rhs);
}
Expression operator&&(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::And, lhs, rhs);
}
Expression operator&&(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::And, lhs, rhs);
}

Expression operator||(const AbstractExpression& lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Or, lhs, rhs);
}
Expression operator||(const AbstractExpression& lhs, double rhs) {
    return binary_op(ExpressionBinary::Operation::Or, lhs, rhs);
}
Expression operator||(double lhs, const AbstractExpression& rhs) {
    return binary_op(ExpressionBinary::Operation::Or, lhs, rhs);
}

Expression operator!(const AbstractExpression& operand) {
    return unary_op(ExpressionUnary::Operation::Not, operand);
}

}  // namespace quiver
