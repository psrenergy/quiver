#include "quiver/expression/expression_node.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace quiver {

double ExpressionUnary::apply(Operation operation, double x) {
    switch (operation) {
    case Operation::Negate:
        return -x;
    case Operation::Abs:
        return std::abs(x);
    case Operation::Sqrt:
        return std::sqrt(x);
    case Operation::Log:
        return std::log(x);
    case Operation::Exp:
        return std::exp(x);
    case Operation::Not:
        // Logical NOT on a nonzero-is-true operand; a NaN operand propagates as NaN.
        if (std::isnan(x)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return (x == 0.0) ? 1.0 : 0.0;
    }
    throw std::runtime_error("Cannot apply: unhandled ExpressionUnary::Operation variant");
}

ExpressionUnary::ExpressionUnary(Operation operation, std::shared_ptr<ExpressionNode> operand)
    : operation_(operation), operand_(std::move(operand)) {
    operand_row_buf_.resize(operand_->metadata().labels.size());
    if (operation_ == Operation::Not) {
        // NOT yields a unitless boolean; other unary ops pass the operand metadata through.
        output_meta_ = operand_->metadata();
        output_meta_.unit = "";
    }
}

const BinaryMetadata& ExpressionUnary::metadata() const {
    return (operation_ == Operation::Not) ? output_meta_ : operand_->metadata();
}

void ExpressionUnary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto n = operand_row_buf_.size();
    if (out.size() != n) {
        out.resize(n);
    }
    operand_->compute_row(dims, operand_row_buf_);
    for (size_t k = 0; k < n; ++k) {
        out[k] = apply(operation_, operand_row_buf_[k]);
    }
}

void ExpressionUnary::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

}  // namespace quiver
