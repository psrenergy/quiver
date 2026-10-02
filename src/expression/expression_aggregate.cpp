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

ExpressionAggregate::ExpressionAggregate(
    Operation operation,
    std::shared_ptr<ExpressionNode> operand,
    std::string dimension_name,
    std::optional<double> parameter
)
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

    // Removing the outermost time dimension promotes its time child to outermost, but compute_row still
    // forwards the child's coordinate to the operand unchanged: output month 3 is the operand's month 3,
    // i.e. March. So the output must start where the removed dimension's period holding initial_datetime
    // starts (year x month from 2025-03-01 -> 2025-01-01; day x hour from 06:00 -> 00:00). It runs before
    // derive_initial_values(), which reads this start. With no time dimension left there is nothing to label.
    if (reduced_dim.is_time_dimension() && reduced_dim.time->parent_dimension_index == -1 &&
        output_meta_.number_of_time_dimensions() > 0) {
        output_meta_.initial_datetime = reduced_dim.time->add_offset_from_int(operand_meta.initial_datetime, 1);
    }

    output_meta_.validate();
    output_meta_.derive_initial_values();

    operand_dims_buf_.resize(operand_meta.dimensions.size());
    operand_row_buf_.resize(operand_meta.labels.size());
    percentile_scratch_.resize(operand_meta.labels.size());
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

    // Reduce from where next_dimensions starts the reduced dimension at this coordinate to its
    // actual size here (Feb = 28, ...). Reducing the outermost time dimension also reads the first
    // period's cells before initial_datetime, which the walk never writes: NaN, so skipped.
    const int64_t start = dimension_start_at_values(operand_meta, operand_dims_buf_, reduced_operand_index_);
    const int64_t end = dimension_sizes_at_values(operand_meta, operand_dims_buf_)[reduced_operand_index_];

    std::vector<AggregationState> states(label_count);
    for (auto& scratch : percentile_scratch_) {
        scratch.clear();
    }

    for (int64_t v = start; v <= end; ++v) {
        operand_dims_buf_[reduced_operand_index_] = v;
        operand_->compute_row(operand_dims_buf_, operand_row_buf_);

        for (size_t k = 0; k < label_count; ++k) {
            aggregation_accumulate(operation_, states[k], percentile_scratch_[k], operand_row_buf_[k]);
        }
    }

    for (size_t k = 0; k < label_count; ++k) {
        out[k] = aggregation_finalize(operation_, states[k], percentile_scratch_[k], parameter_);
    }
}

void ExpressionAggregate::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

}  // namespace quiver
