#include "quiver/expression/expression_node.h"

#include "expression_helpers.h"

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

    const auto& out_dims = broadcast_meta_.dimensions;
    lhs_dim_sizes_.assign(out_dims.size(), 0);
    rhs_dim_sizes_.assign(out_dims.size(), 0);
    lhs_to_out_.assign(lhs_meta.dimensions.size(), -1);
    rhs_to_out_.assign(rhs_meta.dimensions.size(), -1);

    for (size_t out_i = 0; out_i < out_dims.size(); ++out_i) {
        const auto li = find_dim_index(lhs_meta.dimensions, out_dims[out_i].name);
        const auto ri = find_dim_index(rhs_meta.dimensions, out_dims[out_i].name);
        lhs_dim_sizes_[out_i] = (li >= 0) ? lhs_meta.dimensions[li].size : 0;
        rhs_dim_sizes_[out_i] = (ri >= 0) ? rhs_meta.dimensions[ri].size : 0;
        if (li >= 0) {
            lhs_to_out_[li] = static_cast<int>(out_i);
        }
        if (ri >= 0) {
            rhs_to_out_[ri] = static_cast<int>(out_i);
        }
    }

    lhs_label_count_ = lhs_meta.labels.size();
    rhs_label_count_ = rhs_meta.labels.size();

    lhs_dims_buf_.resize(lhs_meta.dimensions.size());
    rhs_dims_buf_.resize(rhs_meta.dimensions.size());
    lhs_buf_.resize(lhs_label_count_);
    rhs_buf_.resize(rhs_label_count_);
}

const BinaryMetadata& ExpressionBinary::metadata() const {
    return broadcast_meta_;
}

void ExpressionBinary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count) {
        out.resize(out_label_count);
    }

    for (size_t li = 0; li < lhs_dims_buf_.size(); ++li) {
        const auto out_i = lhs_to_out_[li];
        auto coord = dims[out_i];
        if (lhs_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        lhs_dims_buf_[li] = coord;
    }
    for (size_t ri = 0; ri < rhs_dims_buf_.size(); ++ri) {
        const auto out_i = rhs_to_out_[ri];
        auto coord = dims[out_i];
        if (rhs_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        rhs_dims_buf_[ri] = coord;
    }

    lhs_->compute_row(lhs_dims_buf_, lhs_buf_);
    rhs_->compute_row(rhs_dims_buf_, rhs_buf_);

    for (size_t k = 0; k < out_label_count; ++k) {
        const size_t li = (lhs_label_count_ == 1) ? 0 : k;
        const size_t ri = (rhs_label_count_ == 1) ? 0 : k;
        out[k] = apply(operation_, lhs_buf_[li], rhs_buf_[ri]);
    }
}

void ExpressionBinary::collect_input_files(std::vector<BinaryFile*>& out) const {
    lhs_->collect_input_files(out);
    rhs_->collect_input_files(out);
}

}  // namespace quiver
