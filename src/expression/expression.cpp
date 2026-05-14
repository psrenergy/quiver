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

Expression::Expression(const BinaryFile& file) : node_(std::make_shared<ExpressionFile>(file.get_file_path())) {}

Expression::Expression(std::shared_ptr<ExpressionNode> node) : node_(std::move(node)) {}

const BinaryMetadata& Expression::metadata() const {
    return node_->metadata();
}

Expression Expression::aggregate(const std::string& dimension,
                                 const std::string& operation,
                                 std::optional<double> parameter) const {
    auto op = ExpressionAggregate::parse_operation(operation);
    return Expression(std::make_shared<ExpressionAggregate>(op, node_, dimension, parameter));
}

Expression Expression::aggregate_agents(const std::string& operation, std::optional<double> parameter) const {
    auto op = ExpressionAggregateAgents::parse_operation(operation);
    return Expression(std::make_shared<ExpressionAggregateAgents>(op, node_, parameter));
}

Expression Expression::select_agents(const std::vector<std::string>& labels) const {
    return Expression(std::make_shared<ExpressionSelectAgents>(node_, labels));
}

Expression Expression::rename_agents(const std::vector<std::pair<std::string, std::string>>& mapping) const {
    return Expression(std::make_shared<ExpressionRenameAgents>(node_, mapping));
}

void Expression::save(const std::string& path) const {
    std::vector<BinaryFile*> input_files;
    node_->collect_input_files(input_files);

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

    const auto& meta = node_->metadata();
    auto writer = BinaryFile::open_file(path, 'w', meta);

    std::unordered_map<std::string, int64_t> dim_map;
    dim_map.reserve(meta.dimensions.size());

    std::vector<int64_t> dims = first_dimensions(meta);
    std::vector<double> row;
    for (;;) {
        node_->compute_row(dims, row);
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

Expression operator+(const Expression& lhs, const Expression& rhs) {
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Add, lhs.node_, rhs.node_));
}
Expression operator+(const Expression& lhs, double rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(rhs, lhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Add, lhs.node_, scalar));
}
Expression operator+(double lhs, const Expression& rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(lhs, rhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Add, scalar, rhs.node_));
}

Expression operator-(const Expression& lhs, const Expression& rhs) {
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Subtract, lhs.node_, rhs.node_));
}
Expression operator-(const Expression& lhs, double rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(rhs, lhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Subtract, lhs.node_, scalar));
}
Expression operator-(double lhs, const Expression& rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(lhs, rhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Subtract, scalar, rhs.node_));
}

Expression operator*(const Expression& lhs, const Expression& rhs) {
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Multiply, lhs.node_, rhs.node_));
}
Expression operator*(const Expression& lhs, double rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(rhs, lhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Multiply, lhs.node_, scalar));
}
Expression operator*(double lhs, const Expression& rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(lhs, rhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Multiply, scalar, rhs.node_));
}

Expression operator/(const Expression& lhs, const Expression& rhs) {
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Divide, lhs.node_, rhs.node_));
}
Expression operator/(const Expression& lhs, double rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(rhs, lhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Divide, lhs.node_, scalar));
}
Expression operator/(double lhs, const Expression& rhs) {
    auto scalar = std::make_shared<ExpressionScalar>(lhs, rhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(ExpressionBinary::Operation::Divide, scalar, rhs.node_));
}

Expression operator-(const Expression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(ExpressionUnary::Operation::Negate, operand.node_));
}
Expression abs(const Expression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(ExpressionUnary::Operation::Abs, operand.node_));
}
Expression sqrt(const Expression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(ExpressionUnary::Operation::Sqrt, operand.node_));
}
Expression log(const Expression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(ExpressionUnary::Operation::Log, operand.node_));
}
Expression exp(const Expression& operand) {
    return Expression(std::make_shared<ExpressionUnary>(ExpressionUnary::Operation::Exp, operand.node_));
}

Expression ifelse(const Expression& condition, const Expression& then_value, const Expression& else_value) {
    return Expression(std::make_shared<ExpressionTernary>(
        ExpressionTernary::Operation::IfElse, condition.node_, then_value.node_, else_value.node_));
}

}  // namespace quiver
