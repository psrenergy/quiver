#include "quiver/expression/expression_node.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace quiver {

ExpressionSelectAgents::ExpressionSelectAgents(std::shared_ptr<ExpressionNode> operand, std::vector<std::string> labels)
    : operand_(std::move(operand)) {
    const auto& operand_meta = operand_->metadata();

    std::unordered_map<std::string, size_t> label_to_index;
    label_to_index.reserve(operand_meta.labels.size());
    for (size_t i = 0; i < operand_meta.labels.size(); ++i) {
        label_to_index.emplace(operand_meta.labels[i], i);
    }

    selected_indices_.reserve(labels.size());
    for (const auto& label : labels) {
        auto it = label_to_index.find(label);
        if (it == label_to_index.end()) {
            throw std::runtime_error("Cannot select_agents: label not found: '" + label + "'");
        }
        selected_indices_.push_back(it->second);
    }

    output_meta_ = operand_meta;
    output_meta_.labels = std::move(labels);
    output_meta_.validate();

    operand_row_buf_.resize(operand_meta.labels.size());
}

const BinaryMetadata& ExpressionSelectAgents::metadata() const {
    return output_meta_;
}

void ExpressionSelectAgents::compute_row(const std::vector<int64_t>& dims, std::vector<double>& out) const {
    operand_->compute_row(dims, operand_row_buf_);
    if (out.size() != selected_indices_.size()) {
        out.resize(selected_indices_.size());
    }
    for (size_t i = 0; i < selected_indices_.size(); ++i) {
        out[i] = operand_row_buf_[selected_indices_[i]];
    }
}

void ExpressionSelectAgents::collect_input_files(std::vector<BinaryFile*>& out) const {
    operand_->collect_input_files(out);
}

}  // namespace quiver
