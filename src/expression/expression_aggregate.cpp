#include "expression_helpers.h"
#include "quiver/binary/iteration.h"
#include "quiver/expression/expression_node.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

ExpressionAggregate::ExpressionAggregate(Operation operation,
                                         std::shared_ptr<ExpressionNode> operand,
                                         std::string dimension_name,
                                         std::optional<double> parameter)
    : operation_(operation), operand_(std::move(operand)), dimension_name_(std::move(dimension_name)),
      parameter_(parameter) {
    validate_aggregation_param(operation_, parameter_, "aggregate");

    const auto& operand_meta = operand_->metadata();

    const int reduced_idx = find_dim_index(operand_meta.dimensions, dimension_name_);
    if (reduced_idx < 0) {
        throw std::runtime_error("Dimension not found: '" + dimension_name_ + "' in operand metadata");
    }
    reduced_operand_index_ = reduced_idx;

    const auto& reduced_dim = operand_meta.dimensions[reduced_idx];
    const int64_t grandparent_orig_idx =
        reduced_dim.is_time_dimension() ? reduced_dim.time->parent_dimension_index : -1;

    output_meta_ = operand_meta;
    output_meta_.dimensions.erase(output_meta_.dimensions.begin() + reduced_idx);

    operand_to_out_.assign(operand_meta.dimensions.size(), -1);
    for (size_t i = 0; i < operand_meta.dimensions.size(); ++i) {
        if (static_cast<int>(i) == reduced_idx) {
            continue;
        }
        operand_to_out_[i] = (static_cast<int>(i) < reduced_idx) ? static_cast<int>(i) : static_cast<int>(i) - 1;
    }

    for (size_t out_i = 0; out_i < output_meta_.dimensions.size(); ++out_i) {
        auto& out_dim = output_meta_.dimensions[out_i];
        if (!out_dim.is_time_dimension()) {
            continue;
        }
        const int operand_idx =
            (static_cast<int>(out_i) < reduced_idx) ? static_cast<int>(out_i) : static_cast<int>(out_i) + 1;
        const int64_t orig_parent = operand_meta.dimensions[operand_idx].time->parent_dimension_index;
        if (orig_parent < 0) {
            out_dim.time->parent_dimension_index = -1;
        } else if (orig_parent == reduced_idx) {
            if (grandparent_orig_idx < 0) {
                out_dim.time->parent_dimension_index = -1;
            } else {
                out_dim.time->parent_dimension_index =
                    (grandparent_orig_idx < reduced_idx) ? grandparent_orig_idx : grandparent_orig_idx - 1;
            }
        } else {
            out_dim.time->parent_dimension_index = (orig_parent < reduced_idx) ? orig_parent : orig_parent - 1;
        }
    }

    output_meta_.validate();

    operand_dims_buf_.resize(operand_meta.dimensions.size());
    operand_row_buf_.resize(operand_meta.labels.size());
    if (operation_ == Operation::Percentile) {
        percentile_scratch_.resize(operand_meta.labels.size());
    }
}

const BinaryMetadata& ExpressionAggregate::metadata() const {
    return output_meta_;
}

void ExpressionAggregate::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    const auto label_count = operand_row_buf_.size();
    if (out.size() != label_count) {
        out.resize(label_count);
    }

    const auto& operand_meta = operand_->metadata();

    for (size_t i = 0; i < operand_meta.dimensions.size(); ++i) {
        if (static_cast<int>(i) == reduced_operand_index_) {
            continue;
        }
        operand_dims_buf_[i] = dims[operand_to_out_[i]];
    }
    operand_dims_buf_[reduced_operand_index_] = 1;

    const auto& reduced_dim = operand_meta.dimensions[reduced_operand_index_];
    int64_t start = 1;
    int64_t end = reduced_dim.size;
    if (reduced_dim.is_time_dimension()) {
        const auto& tp = *reduced_dim.time;
        const int64_t parent_idx = tp.parent_dimension_index;
        const auto sizes = dimension_sizes_at_values(operand_meta, operand_dims_buf_);
        end = sizes[reduced_operand_index_];
        if (parent_idx < 0) {
            start = tp.initial_value;
        } else {
            const auto& parent_dim = operand_meta.dimensions[parent_idx];
            const int64_t parent_initial = parent_dim.is_time_dimension() ? parent_dim.time->initial_value : 1;
            start = (operand_dims_buf_[parent_idx] == parent_initial) ? tp.initial_value : 1;
        }
    }

    std::vector<double> sum_buf(label_count, 0.0);
    std::vector<int64_t> count_buf(label_count, 0);
    std::vector<double> min_buf(label_count, std::numeric_limits<double>::infinity());
    std::vector<double> max_buf(label_count, -std::numeric_limits<double>::infinity());

    if (operation_ == Operation::Percentile) {
        for (auto& scratch : percentile_scratch_) {
            scratch.clear();
        }
    }

    for (int64_t v = start; v <= end; ++v) {
        operand_dims_buf_[reduced_operand_index_] = v;
        operand_->compute_row(operand_dims_buf_, operand_row_buf_);

        for (size_t k = 0; k < label_count; ++k) {
            const double value = operand_row_buf_[k];
            if (std::isnan(value)) {
                continue;
            }
            switch (operation_) {
            case Operation::Sum:
            case Operation::Mean:
                sum_buf[k] += value;
                ++count_buf[k];
                break;
            case Operation::Min:
                if (value < min_buf[k]) {
                    min_buf[k] = value;
                }
                ++count_buf[k];
                break;
            case Operation::Max:
                if (value > max_buf[k]) {
                    max_buf[k] = value;
                }
                ++count_buf[k];
                break;
            case Operation::Percentile:
                percentile_scratch_[k].push_back(value);
                break;
            }
        }
    }

    const double nan_value = std::numeric_limits<double>::quiet_NaN();
    for (size_t k = 0; k < label_count; ++k) {
        switch (operation_) {
        case Operation::Sum:
            out[k] = (count_buf[k] > 0) ? sum_buf[k] : nan_value;
            break;
        case Operation::Mean:
            out[k] = (count_buf[k] > 0) ? sum_buf[k] / static_cast<double>(count_buf[k]) : nan_value;
            break;
        case Operation::Min:
            out[k] = (count_buf[k] > 0) ? min_buf[k] : nan_value;
            break;
        case Operation::Max:
            out[k] = (count_buf[k] > 0) ? max_buf[k] : nan_value;
            break;
        case Operation::Percentile:
            out[k] = compute_percentile(percentile_scratch_[k], *parameter_);
            break;
        }
    }
}

void ExpressionAggregate::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

}  // namespace quiver
