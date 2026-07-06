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
    case Operation::Gt:
    case Operation::Lt:
    case Operation::Gte:
    case Operation::Lte:
    case Operation::Eq:
    case Operation::Neq: {
        // A NaN operand (missing data) propagates so ifelse(cmp, ...) yields NaN.
        if (std::isnan(lhs) || std::isnan(rhs)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        bool result = false;
        switch (operation) {
        case Operation::Gt:
            result = lhs > rhs;
            break;
        case Operation::Lt:
            result = lhs < rhs;
            break;
        case Operation::Gte:
            result = lhs >= rhs;
            break;
        case Operation::Lte:
            result = lhs <= rhs;
            break;
        case Operation::Eq:
            result = lhs == rhs;
            break;
        case Operation::Neq:
            result = lhs != rhs;
            break;
        default:
            break;
        }
        return result ? 1.0 : 0.0;
    }
    case Operation::And:
    case Operation::Or: {
        // Boolean logic on nonzero-is-true operands; a NaN operand propagates as NaN.
        if (std::isnan(lhs) || std::isnan(rhs)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        const bool l = lhs != 0.0;
        const bool r = rhs != 0.0;
        const bool result = (operation == Operation::And) ? (l && r) : (l || r);
        return result ? 1.0 : 0.0;
    }
    }
    throw std::runtime_error("Cannot apply: unhandled ExpressionBinary::Operation variant");
}

namespace {
bool is_logical(ExpressionBinary::Operation op) {
    return op == ExpressionBinary::Operation::And || op == ExpressionBinary::Operation::Or;
}
}  // namespace

ExpressionBinary::ExpressionBinary(Operation operation,
                                   std::shared_ptr<ExpressionNode> lhs,
                                   std::shared_ptr<ExpressionNode> rhs)
    : operation_(operation), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {
    const auto& lhs_meta = lhs_->metadata();
    const auto& rhs_meta = rhs_->metadata();

    // Logical ops combine booleans, so units are irrelevant: validate only that shapes broadcast,
    // and emit a unitless result. Arithmetic and comparison ops keep the full unit-match check.
    if (is_logical(operation_)) {
        validate_shape_compatibility(lhs_meta, rhs_meta);
    } else {
        validate_compatibility(lhs_meta, rhs_meta);
    }
    auto output_labels = compute_output_labels(lhs_meta.labels, rhs_meta.labels);
    broadcast_meta_ = build_broadcast_metadata(lhs_meta, rhs_meta, std::move(output_labels));
    if (is_logical(operation_)) {
        broadcast_meta_.unit = "";
    }
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
