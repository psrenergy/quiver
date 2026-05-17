#include "expression_helpers.h"
#include "quiver/expression/expression_node.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace quiver {

ExpressionAggregateAgents::ExpressionAggregateAgents(Operation operation,
                                                     std::shared_ptr<ExpressionNode> operand,
                                                     std::optional<double> parameter)
    : operation_(operation), operand_(std::move(operand)), parameter_(parameter) {
    validate_aggregation_param(operation_, parameter_, "aggregate_agents");

    const auto& operand_meta = operand_->metadata();
    output_meta_ = operand_meta;
    output_meta_.labels = {aggregation_operation_label(operation_)};
    output_meta_.validate();

    operand_row_buf_.resize(operand_meta.labels.size());
}

const BinaryMetadata& ExpressionAggregateAgents::metadata() const {
    return output_meta_;
}

void ExpressionAggregateAgents::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    if (out.size() != 1) {
        out.resize(1);
    }

    operand_->compute_row(dims, operand_row_buf_);

    const double nan_value = std::numeric_limits<double>::quiet_NaN();

    switch (operation_) {
    case Operation::Sum: {
        double sum = 0.0;
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            sum += v;
            ++count;
        }
        out[0] = (count > 0) ? sum : nan_value;
        break;
    }
    case Operation::Mean: {
        double sum = 0.0;
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            sum += v;
            ++count;
        }
        out[0] = (count > 0) ? sum / static_cast<double>(count) : nan_value;
        break;
    }
    case Operation::Min: {
        double m = std::numeric_limits<double>::infinity();
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            if (v < m) {
                m = v;
            }
            ++count;
        }
        out[0] = (count > 0) ? m : nan_value;
        break;
    }
    case Operation::Max: {
        double m = -std::numeric_limits<double>::infinity();
        int64_t count = 0;
        for (double v : operand_row_buf_) {
            if (std::isnan(v)) {
                continue;
            }
            if (v > m) {
                m = v;
            }
            ++count;
        }
        out[0] = (count > 0) ? m : nan_value;
        break;
    }
    case Operation::Percentile: {
        std::vector<double> scratch;
        scratch.reserve(operand_row_buf_.size());
        for (double v : operand_row_buf_) {
            if (!std::isnan(v)) {
                scratch.push_back(v);
            }
        }
        out[0] = compute_percentile(scratch, *parameter_);
        break;
    }
    }
}

void ExpressionAggregateAgents::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

}  // namespace quiver
