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

    const auto& out_dims = broadcast_meta_.dimensions;
    condition_dim_sizes_.assign(out_dims.size(), 0);
    then_dim_sizes_.assign(out_dims.size(), 0);
    else_dim_sizes_.assign(out_dims.size(), 0);
    condition_to_out_.assign(condition_meta.dimensions.size(), -1);
    then_to_out_.assign(then_meta.dimensions.size(), -1);
    else_to_out_.assign(else_meta.dimensions.size(), -1);

    for (size_t out_i = 0; out_i < out_dims.size(); ++out_i) {
        const auto ci = find_dim_index(condition_meta.dimensions, out_dims[out_i].name);
        const auto ti = find_dim_index(then_meta.dimensions, out_dims[out_i].name);
        const auto ei = find_dim_index(else_meta.dimensions, out_dims[out_i].name);
        condition_dim_sizes_[out_i] = (ci >= 0) ? condition_meta.dimensions[ci].size : 0;
        then_dim_sizes_[out_i] = (ti >= 0) ? then_meta.dimensions[ti].size : 0;
        else_dim_sizes_[out_i] = (ei >= 0) ? else_meta.dimensions[ei].size : 0;
        if (ci >= 0)
            condition_to_out_[ci] = static_cast<int>(out_i);
        if (ti >= 0)
            then_to_out_[ti] = static_cast<int>(out_i);
        if (ei >= 0)
            else_to_out_[ei] = static_cast<int>(out_i);
    }

    condition_label_count_ = condition_meta.labels.size();
    then_label_count_ = then_meta.labels.size();
    else_label_count_ = else_meta.labels.size();

    condition_dims_buf_.resize(condition_meta.dimensions.size());
    then_dims_buf_.resize(then_meta.dimensions.size());
    else_dims_buf_.resize(else_meta.dimensions.size());
    condition_buf_.resize(condition_label_count_);
    then_buf_.resize(then_label_count_);
    else_buf_.resize(else_label_count_);
}

const BinaryMetadata& ExpressionTernary::metadata() const {
    return broadcast_meta_;
}

void ExpressionTernary::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto out_label_count = broadcast_meta_.labels.size();
    if (out.size() != out_label_count) {
        out.resize(out_label_count);
    }

    for (size_t ci = 0; ci < condition_dims_buf_.size(); ++ci) {
        const auto out_i = condition_to_out_[ci];
        auto coord = dims[out_i];
        if (condition_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        condition_dims_buf_[ci] = coord;
    }
    for (size_t ti = 0; ti < then_dims_buf_.size(); ++ti) {
        const auto out_i = then_to_out_[ti];
        auto coord = dims[out_i];
        if (then_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        then_dims_buf_[ti] = coord;
    }
    for (size_t ei = 0; ei < else_dims_buf_.size(); ++ei) {
        const auto out_i = else_to_out_[ei];
        auto coord = dims[out_i];
        if (else_dim_sizes_[out_i] == 1) {
            coord = 1;
        }
        else_dims_buf_[ei] = coord;
    }

    condition_->compute_row(condition_dims_buf_, condition_buf_);
    then_value_->compute_row(then_dims_buf_, then_buf_);
    else_value_->compute_row(else_dims_buf_, else_buf_);

    for (size_t k = 0; k < out_label_count; ++k) {
        const size_t ck = (condition_label_count_ == 1) ? 0 : k;
        const size_t tk = (then_label_count_ == 1) ? 0 : k;
        const size_t ek = (else_label_count_ == 1) ? 0 : k;
        out[k] = apply(operation_, condition_buf_[ck], then_buf_[tk], else_buf_[ek]);
    }
}

void ExpressionTernary::collect_input_files(std::vector<BinaryFile*>& out) const {
    condition_->collect_input_files(out);
    then_value_->collect_input_files(out);
    else_value_->collect_input_files(out);
}

}  // namespace quiver
