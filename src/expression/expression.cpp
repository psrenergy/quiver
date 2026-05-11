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

namespace {

void collect_file_paths(const ExpressionNode& node, std::vector<std::string>& out) {
    if (auto* fn = dynamic_cast<const ExpressionFile*>(&node)) {
        out.push_back(fn->path());
        return;
    }
    
    if (auto* bn = dynamic_cast<const ExpressionBinary*>(&node)) {
        collect_file_paths(*bn->lhs(), out);
        collect_file_paths(*bn->rhs(), out);
        return;
    }
}

}  // namespace

Expression::Expression(const BinaryFile& file) : node_(std::make_shared<ExpressionFile>(file)) {}

Expression::Expression(std::shared_ptr<ExpressionNode> node) : node_(std::move(node)) {}

const BinaryMetadata& Expression::metadata() const {
    return node_->metadata();
}

const std::shared_ptr<ExpressionNode>& Expression::node() const {
    return node_;
}

void Expression::save(const std::string& path) const {
    std::vector<std::string> input_paths;
    collect_file_paths(*node_, input_paths);
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

namespace {

Expression make_binop(ExpressionBinary::Operation operation, const Expression& lhs, const Expression& rhs) {
    return Expression(std::make_shared<ExpressionBinary>(operation, lhs.node(), rhs.node()));
}

Expression scalar_left(ExpressionBinary::Operation operation, double lhs, const Expression& rhs) {
    auto scalar_node = std::make_shared<ExpressionScalar>(lhs, rhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(operation, scalar_node, rhs.node()));
}

Expression scalar_right(ExpressionBinary::Operation operation, const Expression& lhs, double rhs) {
    auto scalar_node = std::make_shared<ExpressionScalar>(rhs, lhs.metadata());
    return Expression(std::make_shared<ExpressionBinary>(operation, lhs.node(), scalar_node));
}

}  // namespace

Expression operator+(const Expression& lhs, const Expression& rhs) {
    return make_binop(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(const Expression& lhs, double rhs) {
    return scalar_right(ExpressionBinary::Operation::Add, lhs, rhs);
}
Expression operator+(double lhs, const Expression& rhs) {
    return scalar_left(ExpressionBinary::Operation::Add, lhs, rhs);
}

Expression operator-(const Expression& lhs, const Expression& rhs) {
    return make_binop(ExpressionBinary::Operation::Subtract, lhs, rhs);
}
Expression operator-(const Expression& lhs, double rhs) {
    return scalar_right(ExpressionBinary::Operation::Subtract, lhs, rhs);
}
Expression operator-(double lhs, const Expression& rhs) {
    return scalar_left(ExpressionBinary::Operation::Subtract, lhs, rhs);
}

Expression operator*(const Expression& lhs, const Expression& rhs) {
    return make_binop(ExpressionBinary::Operation::Multiply, lhs, rhs);
}
Expression operator*(const Expression& lhs, double rhs) {
    return scalar_right(ExpressionBinary::Operation::Multiply, lhs, rhs);
}
Expression operator*(double lhs, const Expression& rhs) {
    return scalar_left(ExpressionBinary::Operation::Multiply, lhs, rhs);
}

Expression operator/(const Expression& lhs, const Expression& rhs) {
    return make_binop(ExpressionBinary::Operation::Divide, lhs, rhs);
}
Expression operator/(const Expression& lhs, double rhs) {
    return scalar_right(ExpressionBinary::Operation::Divide, lhs, rhs);
}
Expression operator/(double lhs, const Expression& rhs) {
    return scalar_left(ExpressionBinary::Operation::Divide, lhs, rhs);
}

}  // namespace quiver
