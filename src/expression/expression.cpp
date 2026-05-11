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

Expression::Expression(const BinaryFile& file) : node_(std::make_shared<ExpressionFile>(file)) {}

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

void Expression::save(const std::string& path) const {
    std::vector<std::string> input_paths;
    node_->collect_input_paths(input_paths);
    const auto canonical_out = std::filesystem::weakly_canonical(path).string();
    for (const auto& in : input_paths) {
        if (std::filesystem::weakly_canonical(in).string() == canonical_out) {
            throw std::runtime_error("Cannot save: output path collides with input file '" + in + "'");
        }
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

}  // namespace quiver
