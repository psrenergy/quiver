#include "expression_helpers.h"
#include "quiver/expression/expression_node.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace quiver {

double ExpressionTernary::apply(Operation operation, double condition, double then_value, double else_value) {
    switch (operation) {
    case Operation::IfElse:
        if (std::isnan(condition)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return (condition != 0.0) ? then_value : else_value;
    }
    throw std::runtime_error("Cannot apply: unhandled ExpressionTernary::Operation variant");
}

ExpressionTernary::ExpressionTernary(Operation operation,
                                     std::shared_ptr<ExpressionNode> condition,
                                     std::shared_ptr<ExpressionNode> then_value,
                                     std::shared_ptr<ExpressionNode> else_value)
    : operation_(operation), condition_(std::move(condition)), then_value_(std::move(then_value)),
      else_value_(std::move(else_value)) {
    const auto& condition_meta = condition_->metadata();
    const auto& then_meta = then_value_->metadata();
    const auto& else_meta = else_value_->metadata();

    validate_unit_match(then_meta, else_meta);
    validate_shape_compatibility(condition_meta, then_meta);
    validate_shape_compatibility(then_meta, else_meta);
    validate_shape_compatibility(condition_meta, else_meta);

    auto output_labels = compute_ternary_output_labels(condition_meta.labels, then_meta.labels, else_meta.labels);
    broadcast_meta_ = build_ternary_broadcast_metadata(condition_meta, then_meta, else_meta, std::move(output_labels));
    broadcast_meta_.validate();

    condition_op_ = make_broadcast_operand(condition_meta, broadcast_meta_.dimensions);
    then_op_ = make_broadcast_operand(then_meta, broadcast_meta_.dimensions);
    else_op_ = make_broadcast_operand(else_meta, broadcast_meta_.dimensions);
}

const BinaryMetadata& ExpressionTernary::metadata() const {
    return broadcast_meta_;
}

void ExpressionTernary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count) {
        out.resize(out_label_count);
    }

    compute_broadcast_operand_row(condition_op_, *condition_, dims);
    compute_broadcast_operand_row(then_op_, *then_value_, dims);
    compute_broadcast_operand_row(else_op_, *else_value_, dims);

    for (size_t k = 0; k < out_label_count; ++k) {
        out[k] = apply(operation_,
                       condition_op_.row_buf[broadcast_label_index(condition_op_, k)],
                       then_op_.row_buf[broadcast_label_index(then_op_, k)],
                       else_op_.row_buf[broadcast_label_index(else_op_, k)]);
    }
}

void ExpressionTernary::collect_input_files(std::vector<BinaryFile*>& out) const {
    condition_->collect_input_files(out);
    then_value_->collect_input_files(out);
    else_value_->collect_input_files(out);
}

}  // namespace quiver
