#include "expression_helpers.h"
#include "quiver/expression/expression_node.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace quiver {

double ExpressionBinary::apply(Operation operation, double lhs, double rhs) {
    switch (operation) {
    case Operation::Add:
        return lhs + rhs;
    case Operation::Subtract:
        return lhs - rhs;
    case Operation::Multiply:
        return lhs * rhs;
    case Operation::Divide:
        return lhs / rhs;
    }
    throw std::runtime_error("Cannot apply: unhandled ExpressionBinary::Operation variant");
}

ExpressionBinary::ExpressionBinary(Operation operation,
                                   std::shared_ptr<ExpressionNode> lhs,
                                   std::shared_ptr<ExpressionNode> rhs)
    : operation_(operation), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {
    const auto& lhs_meta = lhs_->metadata();
    const auto& rhs_meta = rhs_->metadata();

    validate_compatibility(lhs_meta, rhs_meta);
    auto output_labels = compute_output_labels(lhs_meta.labels, rhs_meta.labels);
    broadcast_meta_ = build_broadcast_metadata(lhs_meta, rhs_meta, std::move(output_labels));
    broadcast_meta_.validate();

    lhs_op_ = make_broadcast_operand(lhs_meta, broadcast_meta_.dimensions);
    rhs_op_ = make_broadcast_operand(rhs_meta, broadcast_meta_.dimensions);
}

const BinaryMetadata& ExpressionBinary::metadata() const {
    return broadcast_meta_;
}

void ExpressionBinary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count) {
        out.resize(out_label_count);
    }

    compute_broadcast_operand_row(lhs_op_, *lhs_, dims);
    compute_broadcast_operand_row(rhs_op_, *rhs_, dims);

    for (size_t k = 0; k < out_label_count; ++k) {
        out[k] = apply(operation_,
                       lhs_op_.row_buf[broadcast_label_index(lhs_op_, k)],
                       rhs_op_.row_buf[broadcast_label_index(rhs_op_, k)]);
    }
}

void ExpressionBinary::collect_input_files(std::vector<BinaryFile*>& out) const {
    lhs_->collect_input_files(out);
    rhs_->collect_input_files(out);
}

}  // namespace quiver
